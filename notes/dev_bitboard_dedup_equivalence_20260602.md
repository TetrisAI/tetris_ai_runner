# dev 分支位板去重组件调研（2026-06-02）

## 调研目标

- 找出 `dev` 分支里 oracle 迁移到位板后的“去重组件”
- 说明它如何等价于旧 oracle 版本的去重机制
- 本次只做临时调研记录，不写长期记忆

## 结论快照

- 位板版主去重组件在 `src/tetris_movegen.h`，核心类型是 `m_tetris::movegen::MoveGen<Spec, T, EnableMini>`。
- 去重分成两层：
  1. **搜索级去重**：`std::array<map_t, R_count> search` 按 `(rotation, x, y)` 记录已达状态；通过位图 OR 合并，天然去掉同一旋转下重复到达的状态。
  2. **输出级对称去重**：`kCanonicalR` 只保留每个几何等价类的 canonical rotation，等价于旧 oracle 里围绕 `index_filtered` / `TetrisNodeMarkFiltered` 的对称件去重。
- `kCanonicalR` 不是任意挑代表，而是按 **master/oracle 的 CCW 旋转访问顺序** 挑“先到先得”的代表，注释已明确写出这是为了对齐旧 `IndexFilter` 语义。

## 代码定位

### 1. 位板版主去重组件

文件：`src/tetris_movegen.h`

- `MoveGen<...>::generate()`：主入口
- `search[r]`：每个旋转一个位图，保存该旋转下所有已达 anchor/basepoint
- `expand_rotations(...)`：旋转扩张，但**不**做 canonical 剪枝，保留各旋转独立可达性和 `last_rotate` 语义
- `kVisitOrder` / `canonical_for()` / `kCanonicalR`：编译期生成对称旋转的 canonical 映射
- 输出阶段 `if (r != kCanonicalR[r]) continue;`：最终去掉对称重复落点

关键位置：

- `tetris_movegen.h:219-269`
  - 注释直接写明：`canonical_r` 是“几何对称 rotation 去重”，并且与 master `IndexFilter` 的 CCW 访问顺序对齐。
- `tetris_movegen.h:318-356`
  - `search` 数组做 BFS 收敛；`next = search[r] | expand` 是搜索级去重本体。
- `tetris_movegen.h:363-367`
  - `if (r != kCanonicalR[r]) continue;` 是输出级去重。

### 2. 几何等价判定基础

文件：`src/tetris_shape.h`

- `CellList::same_geometry()`：逐元素比较规范化后的 `cells` 序列
- 因为 `cells_impl` 已把同几何形状规范化到同一坐标系并按固定顺序生成，所以这里不需要额外排序

关键位置：

- `tetris_shape.h:211-221`

### 3. 旧 oracle 的去重载体

文件：`src/tetris_core.h`

- `TetrisNode::index_filtered`：旧指针网里“去重后等价类索引”
- `TetrisNodeMarkFiltered = TetrisNodeMarkTemplate<true>`：旧搜索器落点级去重标记器

关键位置：

- `tetris_core.h:251-252`
- `tetris_core.h:310-337`

### 4. 旧 oracle 搜索侧的典型使用

文件：`src/search_simple.cpp`

- `push(node_mark_filtered_, land_point_cache_, land_point)`
- 通过 `mark.mark(land_point)` 只把同一个 filtered 等价类的第一个落点放进结果集

关键位置：

- `search_simple.cpp:56-61`
- `search_simple.cpp:72-100`

文件：`src/search_tspin.cpp`

- 搜索/路径恢复时大量拿 `node->index_filtered == index` 作为“目标等价类是否一致”的判断
- 落点收集阶段也使用 `node_mark_filtered_.mark(node)` 去重

关键位置：

- `search_tspin.cpp:482-485`
- `search_tspin.cpp:524-527`
- 以及全文件大量 `index_filtered` 判断

## 等价关系拆解

### A. 旧 oracle：按 filtered node 等价类去重

旧版不是按 `(x, y, r)` 原始状态直接对用户暴露结果，而是先把几何对称、旋转折叠后的状态收敛到 `index_filtered`：

- `index`：原始节点索引
- `index_filtered`：去重后的等价类索引

于是：

- 搜索内部仍可以走不同节点/不同旋转
- 但最终输出落点、路径命中目标时，很多地方都以 `index_filtered` 为准
- `TetrisNodeMarkFiltered` 则保证“同一个 filtered 等价类只收一次”

### B. 位板版：把“filtered 等价类”改写成 `kCanonicalR`

位板版不再有旧指针网节点，也就没有现成的 `index_filtered` 字段；它改成两步表达同样的语义：

1. **先保留所有旋转的可达性**
   - `search[r]` 对每个 `r` 独立扩张
   - 这样不会丢掉某些只在非 canonical 旋转上出现的可达路径信息，尤其是 `last_rotate` 这种按旋转记录的语义

2. **在输出阶段折叠几何等价旋转**
   - `kCanonicalR[r]` 给出该旋转所属几何等价类的代表旋转
   - 只有 `r == kCanonicalR[r]` 的旋转才允许输出
   - 效果上等于：把多个对称旋转合并成旧版的一个 `index_filtered` 等价类

### C. 为什么说它与 oracle 等价，而不是只像 cobra

关键不只是“也做了 canonical rotation”，而是 **canonical 的选择顺序刻意按 oracle 来**。

`tetris_movegen.h` 的注释已经写明：

- `kVisitOrder` 按 master/oracle 的 **CCW 旋转链** 构造
- 同一几何等价类里，按这个顺序“先出现的 rotation”被选为 canonical
- 这是为了与旧 `IndexFilter` 的“先到先得”访问顺序一致

这意味着：

- 对 O/I/S/Z 这类对称件，dev 位板版不是只做“数学上任取一个代表”
- 而是尽量选出 **与旧 oracle 同一个代表 rotation**
- 这样最终导出的 `(r, x, y, spin)` 集合才能与 `oracle_diff` 对拍保持一致

## 一句话判断

- **位板版主去重组件就是 `MoveGen` 里的 `kCanonicalR + generate()` 输出级过滤；搜索级辅助去重则是 `search[r]` 位图收敛。**
- 它对 oracle 的等价方式是：**用“每旋转独立 BFS + 输出时按 oracle 顺序选 canonical rotation”替代旧指针网里的 `index_filtered + TetrisNodeMarkFiltered`。**
- 所以本质上是“表示法变了”，不是“去重语义变了”。
