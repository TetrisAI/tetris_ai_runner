# Cobra 思路移植执行方案

> 关键字: cobra-port-execution-plan tetris-context-deprecation movegen-rewrite

## 已拍板的先决条件（用户 2026-05-26）

| 决策 | 拍板 |
|-----|------|
| C++ 标准升级 | ✅ 接受升 C++20（先 20，必要时再 23）|
| GCC 扩展（vector_size） | ❌ 必须规避，使用 portable SIMD |
| MSVC 兼容 | ✅ 必须保留 |
| Search 改写 | ✅ 接受，但保留**当前仓库的可订制性** |
| 矩阵尺寸 | ✅ N=4, W=10, H=40 时与 cobra 等价即可，**不要求超越** |
| AI 接口 | ✅ eval/get 全部模板化，可改签名 |
| 落地方式 | ✅ **直接原地改写** `src/tetris_core.h`，不另建 `src/cobra/` 子目录 |
| 命名风格 | ✅ **沿用现有命名**：`Map` / `Search` / `LandPoints`（不引入 cobra 的 Board/MoveList）|
| Engine 模板风格 | ✅ 模板模板参数 `<Rule, template<int,int> class AI, template<class,int,int> class Search>`，约束严格 |
| Search 数量 | ✅ **统一为单个模板**，通过模板参数调控覆盖现有 6 套效果，并支持扩展 |

## GCC 扩展规避方案

cobra 用 `using Bitboard [[gnu::vector_size(Tn * sizeof(T))]] = T;` 让 GCC/Clang 自动 SIMD。MSVC 不支持，必须替换。

**统一抽象层 `BoardVec<T, N>`**（手工封装），三层后端：
1. **MSVC + x86**：`__m128i / __m256i` intrinsics（`<emmintrin.h>` / `<immintrin.h>`）
2. **GCC/Clang + x86**：复用 1 的 intrinsics（统一代码路径）
3. **fallback**：标量 `std::array<T, N>` 循环（保 portability）

接口最小集（cobra movegen 实际用到）：
```cpp
template<typename T, size_t N>
struct BoardVec {
    void store(T*) const;
    static BoardVec load(T const*);
    BoardVec operator|(BoardVec) const;
    BoardVec operator&(BoardVec) const;
    BoardVec operator~() const;
    BoardVec operator==(BoardVec) const;     // 元素级
    template<int Sx, int Sy> BoardVec shifted() const;  // 跨 lane 移位
    bool any() const;
    bool none() const;
    int popcount() const;
    T& operator[](size_t i);
};
```

**实现优先级**：先做 fallback 标量版让逻辑跑通，再加 SSE2/AVX2 特化。

## "保留当前仓库可订制性"的具体形态

### 当前仓库可订制的维度
1. **Rule**：10 个 rule_*.h（SRS/SRSX/QQ/ST/TAG/ASRS/Botris/C2/PPT/TOJ）
2. **Search**：6 套（simple/path/cautious/simulate/tag/tspin/aspin）
3. **AI**：8 个评估函数（Attack/Dig/TOJ/TOJ_PC/Botris/Botris_PC/C2/...）
4. **TetrisEngine** 通过模板参数 `<TetrisRule, AI, Search>` 自由组合

### 移植后仍需保留的可订制性
- ✅ Rule 仍然通过 `rule_*.h` 定义（已经基于 `RuleSpec` 编译期描述）
- ✅ AI 仍然通过 `ai_*.h/cpp` 提供，每个 AI 是独立模板类
- ✅ Search 仍然通过模板参数注入到 Engine
- ✅ Engine 顶层接口形态不变，仍然 `TetrisEngine<Rule, AI, Search>`
- ❌ **已删除的不再支持**：TetrisNode 指针网、TetrisContext 工厂、build_snap 等遗留接口

## 新架构骨架

### 1. Board（替代 TetrisMap）
```cpp
namespace m_tetris {

template<int W, int H>
struct Board {
    static_assert(W <= 10);  // 当前 cobra 等价目标
    using row_t = uint16_t;  // W=10 用 uint16_t；后续按 W 缩窄
    static constexpr int VecLanes = 64 / sizeof(row_t);   // 一个 64bit 通道塞几行
    static constexpr int VecCount = (H + VecLanes - 1) / VecLanes;

    BoardVec<uint64_t, VecCount> data;  // 主存储

    // 必备接口（参考 cobra board.hpp 但裁剪到当前需求）
    void set(int x, int y);
    bool get(int x, int y) const;
    int max_y() const;
    template<int Dx, int Dy> Board shifted() const;
    Board operator|(Board) const;
    Board operator&(Board) const;
    Board operator~() const;
    int row_popcount(int y) const;
    row_t row(int y) const;       // 兼容老 AI 取整行
    int height() const { return H; }
    int width() const { return W; }
};

}
```

### 2. PieceShape（替代 TetrisNode 静态部分）
```cpp
template<class Piece, Rotation R>
struct PieceShape {
    static constexpr int W;       // bbox 宽
    static constexpr int H;       // bbox 高
    static constexpr int OffsetX; // bbox 起点
    static constexpr int OffsetY;
    static constexpr std::array<row_t, H> data;  // 形状位图（编译期）
};
```

`PieceShape` 完全编译期，对应 cobra 的 `piece_table<P,R>()`。形状数据从 `Rule::rule_spec::OpDesc` 抽出（已存在）。

### 3. PiecePos（替代 TetrisNode 动态部分）
```cpp
struct PiecePos {
    int8_t x, y;
    Piece piece;
    Rotation r;
    SpinType spin;
};
// 16 字节，POD，AI/Search 之间传值
```

### 4. Search 接口
```cpp
namespace m_tetris {

// 新接口：吐 PiecePos 列表
template<class Rule, int W, int H>
class SearchSimple { ... };

template<class Rule, int W, int H>
class SearchPath { ... };

// ...每套 Search 单独类型
}
```

Search 内部用 cobra 思路（基于 Board 的 SIMD 位运算 movegen），但保留每个 Search 类的独立性以维持可订制性。

### 5. AI 接口（全模板化）
```cpp
template<int W, int H>
class Attack {
public:
    static constexpr row_t row_mask_ = (W >= 16) ? row_t(-1) : row_t((row_t(1) << W) - 1);
    static constexpr row_t col_mask_ = row_mask_ & ~row_t(1);

    Result eval(PiecePos const &pos, Board<W,H> const &map,
                Board<W,H> const &src_map, size_t clear) const;
    Status get(PiecePos const &pos, Result const &eval_result, ...) const;
};
```

旧 `node->row / node->height / node->status / node->open / node->move_down / node->attach` 调用全部翻译：
- `node->row` → `pos.y`
- `node->status.x/.y/.t/.r` → `pos.x/pos.y/pos.piece/pos.r`
- `node->height` → `PieceShape<pos.piece, pos.r>::H`（运行期通过 `piece.route([]<Piece P>{...})` 派发）
- `node->open(map)` → `Board::is_open(pos)`（重新实现）
- `node->move_down->attach(context, map)` → `board.do_move_down<P,R>(pos)`（重新实现）

### 6. Engine 顶层
```cpp
template<class Rule, template<int,int> class AI, template<class,int,int> class Search>
class TetrisEngine {
    static constexpr int W = Rule::rule_spec::width;
    static constexpr int H = Rule::rule_spec::height;
    AI<W, H> ai_;
    Search<Rule, W, H> search_;
    Board<W, H> map_;
    // ...
};
```

**TetrisContext / TetrisNode / TetrisNodeMark / TetrisMapSnap / build_snap 全部下线**。

## 执行阶段拆分（顺序不可逆）

### Phase 1：基础设施
1. CMake 升 C++20（先不上 23，看 MSVC 支持度；如必要再上 C++23）
2. 实现 `BoardVec<T,N>` 抽象（先 fallback 标量版）
3. 实现 `Board<W,H>` + 基本位运算 / shifted / set / get / max_y
4. **不动现有代码**，新增的代码全部在 `src/cobra/` 子目录
5. 写一组单测验证 Board 与现有 TetrisMap 在 attach/clear 等价
6. **里程碑**：Board 在 N=4 W=10 H=40 单测通过

### Phase 2：Movegen 移植
1. 实现 `PieceShape<P,R>` 编译期数据，从 `RuleSpec::OpDesc` 抽出
2. 实现 cobra 风格 `MoveList<Rules, P, Board<W,H>>`，输出 `std::vector<PiecePos>`
3. 用现有 SRS rule + cmd_tris 跑 perft 对拍 cobra
4. **里程碑**：在 N=4 W=10 H=40 时与 cobra perft 等价（数量正确，性能 ≥ cobra 80%）

### Phase 3：Search 重写（保留 6 套）
1. `SearchSimple` 重写为 `template<class Rule, int W, int H>`，内部用 MoveList
2. `SearchPath` 同上 + finesse pathfinding
3. `SearchTSpin / SearchASpin` 同上 + spin 判定
4. `SearchCautious / SearchSimulate / SearchTag` 同上
5. **里程碑**：6 套 Search 全部基于 cobra movegen，单测通过

### Phase 4：AI 模板化
1. 定义 `PiecePos`，确保它能装下所有 AI 用到的 node 字段
2. AI 类全部加 `<int W, int H>` 模板，eval/get 改吃 `PiecePos + Board<W,H>`
3. 各 AI 内部 `row_mask_/col_mask_/full_count_` 全部 `static constexpr`
4. 旧的 `context->...` 调用全部清空（context 已不存在）
5. **里程碑**：8 个 AI 全部基于新接口，对拍旧 AI 输出一致

### Phase 5：TetrisContext / TetrisNode 下线
1. 删除 `TetrisContext / TetrisNode / TetrisMap / TetrisNodeMark / TetrisMapSnap / build_snap`
2. 删除 `tetris_core.cpp` 中所有指针网 build 逻辑
3. `tetris_core.h` 只保留 Engine 模板 + 通用 helper
4. **里程碑**：仓库不再出现 "TetrisNode" / "TetrisContext" 标识符

### Phase 6：TetrisEngine 重接线
1. Engine 顶层模板改成 `<Rule, template<int,int> class AI, template<class,int,int> class Search>`
2. cmd_tris.cpp / ai.cpp / tetris_ai.cpp 全部 typedef 适配
3. 5 个 build target 全部 Built，0 error 0 新 warning
4. **里程碑**：完整链路打通

### Phase 7：SIMD 后端
1. `BoardVec` SSE2 / AVX2 特化
2. NEON 特化（ARM）
3. 性能调优至 cobra 等价

## 风险与回滚

- **每个 Phase 是单一 commit**（用户偏好"单一提交"）
- **Phase 之间编译可通过**，不引入半成品状态
- 任何 Phase 失败可独立 revert，不污染前序 Phase
- 各 Phase 间隔需要用户确认对拍正确性，再开始下一阶段

## 待用户确认

1. ✅ 阶段拆分顺序合理吗？
2. ✅ Phase 1 起步策略（新代码独立目录、不动旧代码）合理吗？
3. ✅ Engine 顶层模板 `<Rule, template<int,int> class AI, template<class,int,int> class Search>` 风格 OK 吗？还是 `<Rule, class AI, class Search>` 让 user 自己写完整类型？
4. ✅ C++20 起步还是直接 C++23？
5. ✅ "保留 6 套 Search 独立类型"够不够，还是允许合并精简？

确认后从 Phase 1 开始落地。
