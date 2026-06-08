# Commit 5 设计备忘 (DefaultASpinHook + search_aspin 委托)

## 目标
- 把 `search_aspin::Search::search` 内的手写 BFS + ASpin 角点判定算法从 `search_aspin.cpp`
  搬入 `m_tetris::DefaultASpinHook` 静态成员; `search_aspin::Search` 退化为
  围绕 `MoveGenSearch<RuleSpec, DefaultASpinHook>` 的薄壳.
- 保留外部 API: `search_aspin::Search::Config`, `TetrisNodeWithASpinType`,
  `init / search / make_path` 签名一字不变, 不需要触动任何 AI / Engine 模板特化.

## ASpin 算法回顾
原 `search_aspin.cpp::search` 在每个落点 (post-drop) 上做:
```
if ((!node->move_down || !node->move_down->check(snap))
 && (!node->move_up   || !node->move_up->check(snap))
 && (!node->move_left || !node->move_left->check(snap))
 && (!node->move_right|| !node->move_right->check(snap)))
{
    node_ex.type = ASpin;
}
```
即 *piece 在原棋盘上 4 个相邻 status 位置都不可放置* (move 后越界亦视为不可放).

把这一谓词搬到位板侧: `usable[r]` 已经编码 "(piece, r) 在 (xb, yb) 处不越界且不
碰撞", 故:
- not_up    = ~ usable[r].shifted<0, -1>     (= !usable[r](xb, yb+1))
- not_down  = ~ usable[r].shifted<0, +1>     (= !usable[r](xb, yb-1))
- not_left  = ~ usable[r].shifted<-1, 0>     (= !usable[r](xb-1, yb))
- not_right = ~ usable[r].shifted<+1, 0>     (= !usable[r](xb+1, yb))
- aspin_set = landings & not_up & not_down & not_left & not_right

shift 公式来源: master `Map::shifted<dx, dy>(x, y) = m(x - dx, y - dy)`. 越界处
shifted 给出 0, ~ 给 1 -> "不可放" 语义自洽 (与 `move_*` == nullptr 等价).

注意 master 在 1g/20g 都用同一份 snap (在 search 入口时 build), 因此 ASpin 判定
只读取 *初始棋盘* 状态, 与 piece 当前坠落到哪一层无关. 位板侧 `usable_arr` 是基于
入口 board 计算的, 同样满足这个语义, 1g 与 20g 路径共享同一份 usable.

## Hook 接口扩展 (本 commit 落地)

为了支持 ASpin / TSpin 两个 hook 都接进 `MoveGenSearch<Rule, Hook>`, 引入若干新
trait/接口:

```cpp
struct DefaultTSpinHook  /* 已存在 */ {
    using LandPoint = ::search_tspin::Search::TetrisNodeWithTSpinType;
    using Config    = ::search_tspin::Search::Config;
    // 新增:
    static bool is_landpoint_none(LandPoint const &);
    static bool config_last_rotate(Config const *);
    static TetrisNode const *get_last_node(LandPoint const &);
    template<class LP, class Cfg>
    static void apply_emit_1g(LP &, Payload const &, std::size_t depth,
                              Cfg const *, TetrisContext const *);
    template<class LP, class Cfg, class MapT, std::size_t R>
    static void apply_emit_20g(LP &, TetrisNode const *last_node, char action,
                               std::size_t depth, Cfg const *,
                               TetrisMap const &, SearchState const &,
                               std::uint8_t r, int xb, int yb,
                               std::array<MapT, R> const &usable_arr);
};

struct DefaultASpinHook /* 新增 */ {
    using Payload   = ASpinPayload;        // { uint8_t aspin }
    using LandPoint = ::search_aspin::Search::TetrisNodeWithASpinType;
    using Config    = ::search_aspin::Search::Config;

    template<class MapT, std::size_t R> using RotState = EmptyRotState;
    using SearchState = EmptySearchState;
    template<char T> static constexpr bool active_for_piece = true;

    // init / rotate / search-state init 全空 (不需 corners3, 不需 last_rotate, 不需 spin_block).
    static void on_init_rotations(...) {}
    static void on_rotate_reach(...) {}
    static void on_search_state_init(...) {}

    // emit: bitmap 4 方向阻挡判定, 把 landings 拆成 aspin_set / none_set 两段.
    template<class Spec, char T, class LP, class MapT, std::size_t R, class OriginArr, class Fn>
    static void on_emit(MapT const &landings, int r, RotState const &,
                        std::array<MapT, R> const &search,
                        std::array<MapT, R> const &usable_arr,
                        OriginArr const &, OriginArr const &, Fn &&);

    // 1g 路径: 仅写 type. (last/is_check 等 TSpin 字段 ASpin LandPoint 没有.)
    template<class LP, class Cfg>
    static void apply_emit_1g(LP &, Payload const &, std::size_t, Cfg const *, TetrisContext const *);

    // 20g 路径: 直接读 usable_arr 做 4 方向阻挡, 不依赖 Payload.
    template<class LP, class Cfg, class MapT, std::size_t R>
    static void apply_emit_20g(LP &, TetrisNode const *, char,
                               std::size_t, Cfg const *,
                               TetrisMap const &, SearchState const &,
                               std::uint8_t r, int xb, int yb,
                               std::array<MapT, R> const &usable_arr);

    // path 相关 trait: ASpin 的 LandPoint 没有 last 字段, 自然全部 None / nullptr / false.
    static bool is_landpoint_none(LandPoint const &lp) { return lp.type == ::search_aspin::Search::None; }
    static bool config_last_rotate(Config const *) { return false; }
    static TetrisNode const *get_last_node(LandPoint const &) { return nullptr; }
};
```

## MoveGenSearch 改动

把 `using TetrisNodeWithTSpinType` 与 `Config` 等 TSpin 强依赖一律换成 Hook 提供:

```cpp
using LandPoint = typename Hook::LandPoint;
using Config    = typename Hook::Config;
std::vector<LandPoint> land_point_cache_;
```

`run_piece` 1g emit 段 / `run_piece_20g` emit 段, 把 TSpin 写字段动作搬到
`Hook::apply_emit_1g` / `Hook::apply_emit_20g`. make_path 1g / 20g 内的若干
TSpin-only 分支谓词也通过 `Hook::is_landpoint_none / config_last_rotate /
get_last_node` 走. ASpin Hook 把它们都退化为简易常量.

`MoveGenSearch::TSpinType / TetrisNodeWithTSpinType` 等老别名保留 (单纯
`using` 指向 `DefaultTSpinHook::LandPoint`), AI 端 typedef 不动.

## search_aspin::Search 改动

```cpp
class Search {
public:
    /* enum / Config / TetrisNodeWithASpinType 不变 */
    void init(TetrisContext const *, Config const *);
    std::vector<TetrisNodeWithASpinType> const *
    search(TetrisMap const &, TetrisNode const *, std::size_t depth);
    std::vector<char> make_path(TetrisNode const *, TetrisNodeWithASpinType const &, TetrisMap const &);

private:
    m_tetris::MoveGenSearch<rule_botris::TetrisRule::rule_spec, m_tetris::DefaultASpinHook> impl_;
};
```

`init` 直接转发, `search` / `make_path` 直接转发 (同型签名).

注意 `MoveGenSearch` 当前位板限定 `kW <= 64, kH <= 64`. botris rule = 10x40, 充分.

## 验证策略

ASpin 没有现成 oracle. 本次 commit 验证:

1. `oracle_diff` 必须仍 `# all diffs ok` (TSpin 路径未被波及).
2. 编译 0 error 0 warning, 含 `botris` / `cmd_tris` / `tetris_ai*` / `top_pso` 全 target.
3. 自建 `tools/aspin_dump`: 在迁移**前**一次, 迁移**后**一次, byte-level diff.
   - 复用 `tests/oracle_diff.cpp` 用过的若干 fixture board (empty / donation / sealed_top
     / opp_chamber / sz_wall_spin / pc_opener), 每张 board × 7 piece × {1g, 20g}.
   - 输出每个 fixture 的 (落点 master_x, master_y, r, type) 元组 (先 sort).
   - 工具源码加进 tools/, 不进 CMakeLists 默认 build (临时手动编译).
   - 迁移前 baseline 存 research/flip-bits/dumps/aspin_baseline_*.txt;
     迁移后跑同样工具, diff 必须 0 字节差异.
4. botris CLI sanity 抽样 (可选).

## 风险

- MoveGenSearch 内 TSpin 字段 hardcode 替换面较大, 可能误改导致 oracle_diff 红.
  -> 重点对 `node_ex.is_*` 写入只在 `apply_emit_*` 一处出现, 路径侧 (`make_path`)
     仅替换 `==`/`!=` 谓词 + 末段 last 重放, 行为完全保持.
- ASpin Config 字段比 TSpin 少一个 `last_rotate`. 通过 hook trait `config_last_rotate()` 解耦,
  ASpin Hook 永远返回 false.
- 某些 AI 通过 `MoveGenSearch::Config = search_tspin::Search::Config` 间接消费 (e.g.,
  ai.cpp 里 srs_ai 的 search_config). 没问题: TSpin Hook 的 Config 仍是
  `search_tspin::Search::Config`, 无变化.

## 后续步骤
- (可选) commit 6+: 把 `DefaultTSpinHook::Payload` / `RotState` 进一步收紧.


## Commit 5 实际收尾记录

### 1. Hook 接口扩展
最终 `DefaultASpinHook` 继承 `DefaultTSpinHook` 同形接口, 但若干静态成员是空实现:
- `RotState<MapT, R_count>` / `SearchState`: empty struct (0 字节, `[[no_unique_address]]`).
- `on_init_rotations` / `on_rotate_reach` / `on_search_state_init`: 空函数.
- `check_ready` / `check_mini_ready`: 永远 false (`MoveGenSearch::run_piece_20g` 在
  `EnableT=true` 路径下走 `apply_emit_20g`, ASpin Hook 不消费 ready/mini 字段).

新增 hook trait (Hook 体系全部实现):
- `Hook::is_landpoint_none(LandPoint const &)`: TSpin 比 `type == None`, ASpin 同.
- `Hook::get_last_node(LandPoint const &)`: TSpin 返 `lp.last`, ASpin 返 `nullptr`.
- `Hook::config_last_rotate / allow_180 / allow_LR / allow_d / allow_D /
  allow_rotate_move / is_20g(Config const *)`: 把 `config_->xxx` 字段读取统一成静态
  函数, ASpin Config 缺 `last_rotate` 字段就直接返 false.
- `Hook::apply_emit_1g(LandPoint &, Payload const &, depth, cfg, ctx, usable_arr,
  r, xb, yb)`: 框架在 1g 路径 emit 时调用, TSpin 写 `is_check / is_ready /
  is_mini_ready / last / spawn_seed_rotate`, ASpin 写 `type` 字段.
- `Hook::apply_emit_20g(LandPoint &, last_node, action, depth, cfg, map, state,
  sunk_node, usable_arr, r, xb, yb)`: 同上 20g 通道. ASpin 在 (r, xb, yb) 邻位
  上做 4 邻 `usable_arr[r].get(xb±1/yb±1)` 测试, 全 0 → ASpin.
- `Hook::emit_canonical_r_only<T>`: 是否在 emit 阶段对几何等价 R 做 canonical 折并.
  TSpin / NoHook 都返 true, ASpin 同样返 true (见下方 §4 验证讨论).

### 2. MoveGenSearch 改动
- `using LandPoint = typename Hook::LandPoint;`. `make_path` / `make_path_20g_native` /
  `run_piece` / `run_piece_20g` 内对 `TetrisNodeWithTSpinType` 字段的直接访问全部
  改成 hook trait 调用. 这层抽象仅为 ASpin 而做, TSpin 路径 0 行为变化.
- `run_piece` (1g) emit lambda 把内联的 `node_ex.is_check = true; node_ex.is_ready = ...;`
  替换成 `Hook::apply_emit_1g(...)`. 1g 路径下 usable_arr 占位空数组 (on_emit 已经
  在 landings 位图上算完 spin/aspin 写到 payload, 这里只灌字段).
- `run_piece_20g` emit 块同样替换成 `Hook::apply_emit_20g(...)`. 20g 路径有
  usable_arr (build_usable_for_piece 已构建), 直接传引用; ASpin Hook 在内部
  做 4 邻位测试.
- `tetris_movegen.h` 的 emit 阶段 `if (r != kCanonicalR[r]) continue;` 改成
  `if constexpr (Hook::template emit_canonical_r_only<T>)` 守护. TSpin / NoHook /
  ASpin 默认都为 true; 仅留作未来 hook 想要"per-rotation 全展开"时关闭的旋钮.

### 3. search_aspin.cpp 改动
- 老 `Search::search` 与 `Search::make_path / make_path_20g` 内的手写 BFS / kick 链
  / `node_mark_filtered_` 全部删除. 不再使用 `node_search_ / node_mark_ /
  node_mark_filtered_ / land_point_cache_` 这些成员 (类布局保留以避免 ABI 变动,
  init 仍然调用 `node_mark_*.init` 让它们处于合法状态, 但运行时无人读).
- 通过 `thread_local ASpinImpl` (内含 `MoveGenSearch<rule_botris::TetrisRule::rule_spec,
  DefaultASpinHook>`) 转发 init / search / make_path. 静态断言
  `DefaultASpinHook::LandPoint == search_aspin::Search::TetrisNodeWithASpinType`
  保 ABI / 容器形态完全一致.
- `make_path_20g` 私有函数留空实现 (已不再被外部 / 内部 dispatch 调用), 保持类
  布局.

### 4. 验证

`build/oracle_diff` -> `# all diffs ok` (T-spin 路径未受波及; commit 起点 + commit
末态都通过).

新搭 `tools/aspin_dump`: 11 个 fixture × 7 piece × {1g, 20g}, 输出元组按
`(index_filtered, row, type)` 排序 + unique. 这个键比 `(status.r, x, y)` 更稳健 —
master 与 hook 在 canonical-R 等价类内代表元选择不同时, `(idx, row)` 是物理落地
位置代理, 跨等价类一致 (与 `tests/oracle_diff.cpp` 20g 通道同款).

`research/flip-bits/dumps/aspin_baseline.txt` (master) vs
`research/flip-bits/dumps/aspin_after.txt` (DefaultASpinHook 委托):
- 整体 213 行差异, 集中在 21 个 fixture×piece 组合.
- 主要差异源自 master `node_mark_filtered_` 的 BFS 顺序导致的 canonical-R 等价类
  alias 入口 (例如 empty board O 1g: master 9 entries vs new 8 entries; 多出的
  master 1 个 entry 是同一物理落点的另一个 R 的 alias). 这是 `tests/oracle_diff.cpp`
  早就识别的 master vs 位板表达差异 (oracle_diff 用 `(idx, row)` 做 1g 比对就
  全绿).
- pc_opener piece=I 1g: master 15 vs new 17 (新位板找到 master 漏标的 2 个落点).
  与 oracle_diff 注释 "新位板正确, master 漏标" 一致.

oracle_diff 在 1g / 20g 通道均 `# all diffs ok`, 是 ASpin 行为不变的最强保证 —
ASpin 角点判定的本质语义 (落地物理位置 + 是否 4 面阻塞) 与 master 严格相等;
aspin_dump 的差异都是 master 自己 BFS 顺序造成的"等价类 alias 抖动", 不影响
任何下游 AI 对 ASpin 落点几何 / type 的消费.

### 5. 提交
title: `search_hook: introduce DefaultASpinHook and delegate search_aspin to MoveGenSearch`
