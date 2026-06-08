# 7-Namespace Search 形态统一 — 多 Commit 计划

> 父 commit: `47b8bf8 refactor(engine): collapse bitboard search instantiation to single SearchTag arg`
> 分支: `flip-bits-clean`
> 目标: 让所有位板 search 形态对齐, namespace 平铺到顶层 (不在 `m_tetris::` 下), 与 oracle namespace `search_xxx` 风格保持镜像.

## 顶层决定 (用户拍板)

1. **search namespace 不在 m_tetris 内**, 顶层 `path::Search` / `aspin::Search` / `tspin::Search` / `cautious::Search` / `simple::Search` / `simulate::Search` / `tag::Search`. 与 oracle 的 `search_path::Search` 形成镜像 (oracle 用 `search_xxx` 前缀, 位板用 `xxx` 简称).
2. 7 个 namespace **统一支持** `Search` (默认) / `SearchWith<Config>` (可配).
3. **oracle Config 字段**用户写一份对象, 既能用于 oracle 也能用于位板 `SearchWith`.
4. cautious / aspin / tspin 是 PathStrategy + 不同 Hook, **物理上仍各自独立头文件** (用户要求), 但内部就是 `path::SearchWith<XxxConfig>` alias.

## Commit 序列

### Commit 1 (本会话执行): oracle 归档 search_cautious

**目标**: 从 flip-bits 历史打捞 `search_cautious.{h,cpp}`, 放到 `oracle/`, namespace 改 `search_cautious_oracle::`.

**步骤**:
1. 从 commit `b5a6ca3` (version 158) 取出 `src/search_cautious.cpp` (~473 行) 与 `src/search_cautious.h` (~27 行).
2. 落到 `oracle/search_cautious.cpp` / `oracle/search_cautious.h`.
3. 文件中 `namespace search_cautious` → `namespace search_cautious_oracle`. include 路径 / 内部交叉引用同步.
4. CMakeLists.txt: `oracle/search_cautious.cpp` 追加到 `tetris_oracle` 静态库源列表.
5. **不**新增 cautious_diff 测试 (oracle_diff 框架是否已有 cautious 路径以现状为准, 不引入新对比 target).

**验收**:
- `cmake --build build -j` 通过
- 现有 `oracle_diff` / `extreme_rule_diff` / `path_node_diff` / `simulate_node_diff` / `tag_node_diff` 不退化, 全绿
- clang-format -i 已执行新文件
- mode bit 100644 (新文件) / 既有文件保持
- commit message 英文, ASCII only, 描述本 commit 相对父 (`47b8bf8`) 变更了什么 (oracle 归档 cautious)

**不要做**: 任何 src/ 改动. 仅 oracle/ 新增 + CMakeLists.

---

### Commit 2 (下一会话执行): 7 namespace 形态统一

**前置: 读取本文件恢复上下文, 同时读 .research/flip-bits-cleanup/strategy_purity_audit.md / next_session_remove_context.md**.

**目标**:
1. 7 个 namespace 顶层暴露 `Search` / `SearchWith<Config>`.
2. aspin / tspin / cautious 都是 `path::SearchWith<XxxConfig>` alias, 物理独立头.
3. Config 字段与 oracle 同名同语义.

**改动清单**:

#### A. 新建 / 重写头文件 (顶层 namespace)

```
src/search_path.h      [改写]
  namespace path { struct DefaultConfig; struct Search; template SearchWith; }
  内部仍可保留 m_tetris::movegen::Searcher 的 Strategy/Hook 实现, 但暴露面切到顶层 path 命名空间.
  移除当前末尾的 m_tetris::aspin / m_tetris::cautious alias 与 path::ASpinConfig / path::CautiousConfig.

src/search_aspin.h     [替换当前 facade]
  #include "search_path.h"
  namespace aspin {
    struct DefaultConfig {
      using HookType = m_tetris::ASpinHook;
      // 透传 oracle 字段:
      static constexpr bool allow_rotate_move = true;
      static constexpr bool allow_180 = true;
      static constexpr bool allow_d = true;
      static constexpr bool allow_D = true;
      static constexpr bool allow_LR = true;
      static constexpr bool is_20g = false;
    };
    using Search = path::SearchWith<DefaultConfig>;
    template<class Cfg> using SearchWith = path::SearchWith<detail::merge<DefaultConfig, Cfg>>;
  }

src/search_tspin.h     [新增]
  #include "search_path.h"
  namespace tspin {
    struct DefaultConfig {
      using HookType = m_tetris::TSpinHook;
      static constexpr bool allow_rotate_move = false;
      static constexpr bool allow_180 = false;
      static constexpr bool allow_d = true;
      static constexpr bool allow_D = true;
      static constexpr bool allow_LR = true;
      static constexpr bool is_20g = false;
      static constexpr bool last_rotate = true;
    };
    using Search = path::SearchWith<DefaultConfig>;
    template<class Cfg> using SearchWith = path::SearchWith<detail::merge<DefaultConfig, Cfg>>;
  }
  注: TSpinHook 默认 Config 字段需对照 movegen_hook.h 复核; 上面是 oracle search_tspin 的字段, 实际默认值需要看 ai.cpp 现有 PathTSpinConfig 设的什么.

src/search_cautious.h  [新增]
  #include "search_path.h"
  namespace cautious {
    struct DefaultConfig {
      using HookType = m_tetris::CautiousHook;
      static constexpr bool fast_move_down = false;
    };
    using Search = path::SearchWith<DefaultConfig>;
    template<class Cfg> using SearchWith = path::SearchWith<detail::merge<DefaultConfig, Cfg>>;
  }

src/search_simple.h    [改]: namespace 从 m_tetris::simple 提到顶层 simple
src/search_simulate.h  [改]: namespace 从 m_tetris::simulate 提到顶层 simulate
src/search_tag.h       [改]: namespace 从 m_tetris::tag 提到顶层 tag
```

#### B. 引入 `detail::merge<Default, Override>` 工具

放在 `src/search_path.h` 的 `path::detail` 内, 或 `movegen_hook.h` 内.

实现要点:
- 用 detection idiom 检测 `Override::HookType` 是否存在
- 对每个字段 (HookType / allow_180 / fast_move_down / ...) 做 fallback
- 返回的合并结果是一个新结构体或 trait 集合, 喂给 `path::SearchWith`

C++17 `std::void_t` 配合 `if constexpr requires{}` (取决于编译器版本) 实现.

#### C. 删除文件

- `src/search_aspin.cpp` (旧 facade pImpl 实现, 整文件删)
- 当前 `src/search_aspin.h` 内的 facade pImpl 接口替换为新内容

#### D. CMakeLists.txt 改动

- 移除 `src/search_aspin.cpp` (从 src 静态库)
- 不需要为新增的 .h 加任何 target (头-only)

#### E. ai.cpp 实例化迁移

| 行 | 当前 | 目标 |
|---|---|---|
| L32 (Dig) | `m_tetris::path::Search` | `path::Search` (顶层, 不带 m_tetris::) |
| L103-111 (TOJ / TOJ_v08 / TOJ_PC) | `m_tetris::path::SearchWith<PathTSpinConfig>` | `tspin::Search` (默认配置即可) 或 `tspin::SearchWith<UserCfg>` 看是否有非默认字段 |
| L164 unique_ptr | 同 L103-111 | 同步 |
| L333/334/349 (aspin) | `search_aspin::Search` (旧 facade) | `aspin::Search` |
| L579 (qq_ai QQTetrisSearch) | 复合, 检查 | 内部成员若是 simple, 改 `simple::Search` |
| L655 (c2_ai) | `m_tetris::cautious::Search` | `cautious::Search` |

注: ai.cpp 还有可能在中段透传 `tetris_ai.context().get(...)` 等代码, 改 namespace 不影响这些调用.

#### F. 兼容期 alias

**不保留**. 用户已确认顶层 namespace, 旧 `m_tetris::aspin::Search` / `m_tetris::cautious::Search` 命名直接消失.

ai.cpp 若有其他文件 (例: `ai_zzz.cpp`, `tetris_ai_dll.vcxproj` 内引用) 也需要同步改.

#### G. 验收

- `cmake --build build -j` 通过
- `build/oracle_diff` byte-equal: `# all diffs ok` exit=0
- `extreme_rule_diff` / `path_node_diff` / `simulate_node_diff` / `tag_node_diff` 全绿
- clang-format -i 6+ 文件
- mode bit 保留
- commit message 英文 ASCII only

#### H. 风险

1. **PathTSpinConfig → tspin::Search 迁移**: 必须确认 `tspin::DefaultConfig` 与 ai.cpp 当前传给 `path::SearchWith<PathTSpinConfig>` 的字段值完全一致. 不一致就要在 tspin::DefaultConfig 调对应字段, 或 ai.cpp 用 `tspin::SearchWith<...>` 显式覆盖.
2. **detail::merge 字段查找**: 一个字段一个 detection 模板, 维护成本中. 推荐用 macro 或可变参数 trait 减少样板代码.
3. **m_tetris namespace 内 search 类型**: 当前 `m_tetris::path::SearchWith` 内部有 `template<class RuleType> using type = m_tetris::movegen::Searcher<...>` 形态. 提到顶层 `path::` 后, 内部仍可引用 `m_tetris::movegen::Searcher`, 不需要把 movegen 整套搬到顶层.
4. **SearchTagResolve**: `core/tetris_core.h::detail::SearchTagResolve` 探测 `T::template type<RuleType>`, 顶层 namespace 只要暴露同协议即可, 模板零改动.
5. **Visual Studio 工程文件**: `tetris_ai_runner.vcxproj` / `tetris_ai_dll.vcxproj` 若枚举 search_*.cpp 文件, 删除 `search_aspin.cpp` 时要同步.

---

## 当前栈状态 (落盘时点)

```
47b8bf8 refactor(engine): collapse bitboard search instantiation to single SearchTag arg
7a331ad refactor(search): drop master rotate_*/land_point usage from bitboard tag/simple strategies
485ba69 refactor(search): drop TetrisContext->get from bitboard run_piece path
4d9086d refactor: retire search_*_node BfsEngine implementations
9bfffa0 oracle: archive search_aspin master-graph BFS
c8d331e refactor(aspin): drop master-graph members from Search facade
9240004 refactor(bb): inline PathMarkBit bitset into PathMark, memset whole object on clear
```

未 push, 全部干净, 等 Commit 1 / 2 落盘.

## Commit 2 启动模板 (复制即可)

```
读 .research/flip-bits-cleanup/seven_namespace_plan.md, 按照 Commit 2 章节执行.
环境可以编译, 不要偷懒, 立即开始, 没有新会话, 强行做, 崩了我接受. 一个 Commit 完成.
```
