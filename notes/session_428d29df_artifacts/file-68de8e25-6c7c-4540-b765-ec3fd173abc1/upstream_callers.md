# 上层接入调研：search_tspin 调用点 + 字段消费矩阵（refactor-zzz / flip-bits 收尾）

## A. search_tspin::Search 公开 API（src/search_tspin.h）

```cpp
class Search {
    enum TSpinType { None, TSpin, TSpinMini };
    struct Config { allow_rotate_move/allow_180/allow_d/allow_D/allow_LR/is_20g/last_rotate; };
    struct TetrisNodeWithTSpinType {
        m_tetris::TetrisNode const *node;
        m_tetris::TetrisNode const *last;
        TSpinType type;
        union { struct { bool is_check, is_last_rotate, is_ready, is_mini_ready; }; uint32_t flags; };
        // 隐式转 TetrisNode const* + operator->()
    };
    void init(m_tetris::TetrisContext const *context, Config const *config);
    std::vector<TetrisNodeWithTSpinType> const *search(map, node, depth);
    std::vector<char> make_path(node, land_point, map);  // 返回 'L/R/c/x/z/d/D' 指令链
};
```

`TetrisCore<TetrisAI, TetrisSearch>::LandPoint` 用 `decltype(TetrisSearch().search(...))` 推导出 entry 类型（`tetris_core.h:821`），下游一路把 `TetrisNodeWithTSpinType` 当 `LandPoint` 使。

## B. 直接调用点（不含 oracle_diff 自身）

| 文件 | 行 | 形式 |
|---|---|---|
| `src/ai.cpp` | 104 / 106 | `TetrisEngine<rule_toj, ai_zzz::TOJ, search_tspin::Search> srs_ai` |
| `src/ai.cpp` | 110 / 112 | `TetrisEngine<rule_toj, ai_zzz::TOJ_v08, search_tspin::Search> srs_ai`（USE_V08） |
| `src/ai.cpp` | 116 / 165 | `TetrisThreadEngine<rule_toj, ai_zzz::TOJ_PC, search_tspin::Search> srs_pc`（USE_PC） |
| `src/pso.cpp` | 135 / 154 / 605 | `TetrisEngine<rule_srs, ai_zzz::TOJ, search_tspin::Search>`（参数演化） |
| `src/ppt_pso.cpp` | 153 / 173 / 745 | 同上（PPT 调参） |
| `src/ai_zzz.h` | 96-99, 185-188, 298-301 | `typedef search_tspin::Search::TSpinType / TetrisNodeWithTSpinType` 给 TOJ / TOJ_v08 / TOJ_PC 三个 AI |
| `src/ai_misaka.h` | 10-11 | 同上 typedef 给 ai_misaka |
| `tests/oracle_diff.cpp` | 188 / 204-212 | 我们自己用作 oracle |

注意：`ai_tag.cpp` 也消费 `is_check / is_ready / is_last_rotate`（line 520, 674），但它的 search 实现是独立的 `search_tag` 而**不是** `search_tspin`。同源问题但两套 search。

`TetrisCore` 调 search 只有两个点（`tetris_core.h:2157, 2171`）：
- `engine.search(node, map, result)` → `search_.search(map, node, 0)`
- `engine.make_path(node, land_point, map)` → `search_.make_path(...)` + 截尾 'd/D'

## C. TetrisNodeWithTSpinType 字段消费矩阵

| 字段 | ai_zzz::TOJ (792-805) | ai_zzz::TOJ_v08 (1037-1052) | ai_zzz::TOJ_PC (1394-1409) | ai_misaka (145-160) | ai_tag (520, 674) | search_tspin 自己写入 (1087-1091) |
|---|---|---|---|---|---|---|
| `is_check` | R | R | R | R | R | W (true at land) |
| `is_last_rotate` | R | R | R | R | R | W |
| `is_ready` | R | R | R | R | R | W (= check_ready) |
| `is_mini_ready` | R | R | R | R | — | W (= check_mini_ready) |
| `type` | W (None/TSpin/TSpinMini) | W | W | W | W (TSpin) | init=None |
| `last` | — | — | — | — | — | W (前驱节点) |
| `node` | R (隐式 -> TetrisNode*) | R | R | R | R | W |

**关键发现**：所有 AI 是先 `eval` 再回写 `node.type`；search 阶段只填 4 个 bool，不写 `type`。`type` 是 AI 阶段产物，不是 MoveGen 责任。

## D. make_path 详细分析

**签名**：`std::vector<char> Search::make_path(TetrisNode const *node, TetrisNodeWithTSpinType const &land_point, TetrisMap const &map)`

**返回值字符集**：`'L'`（左移多步）/ `'R'` / `'l'`（左 1）/ `'r'` / `'x'`（CW）/ `'z'`（CCW）/ `'c'`（180）/ `'d'`（软降 1）/ `'D'`（硬降）等（在 `tetris_core` 与 `search_tspin.cpp` BFS reverse path 时填入）。

**调用点**：仅 `TetrisCore::make_path` 一处（`tetris_core.h:2157`），上游再被 `srs_ai.make_path(...)` 包装（`ai.cpp:280, 289, 311`）输出最终 piece-by-piece replay 串。

**内部依赖**：
- `land_point.last` —— BFS 父指针，反向回溯
- `land_point.is_last_rotate` —— 决定回溯尾段是否带旋转
- 20g 分支用独立 `make_path_20g`

**接入新 MoveGen 的硬阻塞**：当前新 `MoveGen<RuleSpec, EnableMini>` 只产 land set + 4 个 bool flag，**完全没有 BFS 父指针**，无法产 `make_path`。这是必须先解决的 gap。

## E. 模板装配链路示意

```
TetrisEngine<Rule, AI, Search>
└─ TetrisCore<AI, Search>
    └─ LandPoint = decltype(Search().search(...))::value_type   // 即 TetrisNodeWithTSpinType
    └─ AI::eval(identity=LandPoint, map, src_map, clear)         // ai_zzz / ai_misaka 在这里读 4 bool
    └─ AI::get(identity, result, ...)                            // 在这里写回 type
└─ search_  // TetrisSearch 实例
    └─ .init(context, config)
    └─ .search(map, node, 0) -> vector<TetrisNodeWithTSpinType>*
    └─ .make_path(node, land_point, map) -> vector<char>
```

接入策略关键：要么提供一个 `MoveGenAdapter` 满足同样的 `init / search / make_path` + `LandPoint = TetrisNodeWithTSpinType` 接口，要么把 `LandPoint` 类型改造成新 MoveGen 自己产的 entry，再改 ai_zzz / ai_misaka 的 `eval` 适配。前者增量小、风险低、一次替换。

## F. 接入策略备选

### 方案 1：Adapter 同接口替换（推荐）
- 写 `class MoveGenSearch` 满足 `init / search / make_path`，内部持 `MoveGen<Spec, EnableMini>` + 一份 `vector<TetrisNodeWithTSpinType>` 缓存。
- `search()` 把 MoveGen emit 出的 (Piece, flags) 翻译回 `TetrisNode const *` + 4 个 bool（context 已有 `node_index<status>` 哈希反查）。
- `make_path()` 暂时调老 `search_tspin::make_path` 兜底（反正 land 一致），或者新 MoveGen 补 BFS 父指针后自产。
- 改动面：`ai.cpp / pso.cpp / ppt_pso.cpp / ai_zzz.h / ai_misaka.h` 把模板第三参 `search_tspin::Search` 换成 `MoveGenSearch`。
- 优点：oracle_diff 已锁死 land 等价性，4 bool 无差异；AI eval 0 改动；可单 commit 完成。
- 风险：node 反查 (status → TetrisNode*) 必须高效，否则吞性能。`TetrisContext::generate(status)` 已经是 O(1) 哈希。

### 方案 2：编译期开关并行（保守）
- 加 `#define USE_NEW_MOVEGEN`，两套 `srs_ai`/`srs_pc` 各 link 一边。
- 优点：好 A/B 对拍、回滚安全。
- 缺点：宏污染，pso/ppt_pso 也得改，长期维护成本高，最后还是得删一边。

### 方案 3：双轨实例 + 运行期开关
- search 阶段两边都跑、内部 assert 等价。仅用于 stress test，不上线。
- 适合作为方案 1 替换前的最后一关 CI。

### 我的建议
**方案 1（Adapter 同接口替换）+ 方案 3 做一次 stress 验证后下线**。具体步骤：
1. 实现 `MoveGenSearch::search` —— 把 MoveGen 的 Piece + flags 翻译成 `TetrisNodeWithTSpinType`（status 反查 TetrisNode）
2. `make_path` 暂走老 search_tspin（同实例持一份），或者优先实现新版反向回溯（看时间）
3. 跑 oracle_diff（已经在跑）+ 一次 stress（pso 单局 1000+ piece，比较两版 srs_ai 的 evaluate path）
4. 全绿后 ai.cpp / pso.cpp / ppt_pso.cpp 三处替换，一笔 commit
5. 老 search_tspin 暂保留（备份 + tests/oracle_diff 持续）

这样能让"接入"动作变成可逆的小补丁，万一线上发现 bug 一行 revert。
