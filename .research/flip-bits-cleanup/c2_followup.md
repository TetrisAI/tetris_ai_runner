# C2 Follow-up Notes

提交: `Collapse hook payloads and introduce BaseSpinHook CRTP` (待 commit, 见末尾 git log).

## ASpin 短路修正在测试上的表现

### 预期
反转 `is_landpoint_none` → `lp_requires_last_rotate` 时, ASpinHook 从 "返回 true (= 不可短路)" 改为 "返回 false (= 可短路)". 这让 ASpin 路径 "起点 == 终点" 在 `make_path_*_native` 入口直接 return 空 path.

### 实测
- `oracle_diff` 全部 byte-equal (T-spin baseline 不受影响, 因为 TSpinHook 仍然 cfg && cfg->last_rotate && lp.type != None 时返回 true).
- `extreme_rule_diff` 全部 ok (phase-0 跑 NoHook 路径, 与 ASpin 修复无关).
- `aspin_dump` 输出与 baseline byte-equal (3725 行全部一致).

### 解释
ASpin 路径在 `oracle` 测试用例下没有触发 "起点 cells_key == 终点 cells_key" 的 corner case. ASpin 的判定是 "落点 4 邻几何阻挡", 一般落点都在棋盘中下方, 而 spawn 节点在棋盘顶部 (y≈21). 即使存在 spawn 落点恰好在中下区域且 4 邻全阻塞的扭曲案例, oracle 自身在该 corner case 下也回 build_path(start), 通过 self-prev 协议返回空 path. 所以新旧两条路径在 oracle 路径下都 emit 空 path, byte-equal 自然成立.

bug 修复体现在: 之前 ASpin 路径 "起点 == 终点" 时多走一遍 BFS 才能拿到空 path, 修复后直接短路返回. 在性能敏感场景 (尤其 BFS 启动开销不可忽略) 上有微小收益, 不改变正确性.

## TagStrategy 的 lp_requires_last_rotate 不可消费问题

### 现象
`lp_requires_last_rotate(cfg, lp)` 内联了 `cfg->last_rotate` 检查. TagStrategy 走 oracle/search_tag 路径时不消费 Config (默认 `cfg->last_rotate = false`), 但 Tag 仍依赖 "lp.type != None" 来决定是否需要末段旋转后缀 (z/c).

直接调用 `SpinHook::lp_requires_last_rotate(ctx.config_, land_point)` 会让 Tag 在 `cfg->last_rotate=false` 下永远拿到 false, 末段反查路径被切掉, 导致 T-spin 用例 oracle vs new tag 失配.

### 解决
Tag 改用 `SpinHook::get_last_node(land_point) != nullptr` 作 lp.type 是否为 None 的 proxy:

* TSpinHook (Tag 唯一消费者) 在 lp.type != None 时 emit_set 调 find_last_rotate_pred + apply_emit_* 会写 lp.last; lp.type = None 时 lp.last = nullptr.
* 二者等价, 不依赖 ctx.config_.

代码: `src/search_tag.h:1103`.

### 设计权衡
保留 `lp_requires_last_rotate` 内联 cfg 查询是合理的: PathStrategy 端两个查询 (cfg->last_rotate 与 lp.type != None) 必须同时成立才需要后缀, 合并成单一 trait 可以减少 strategy 调用面. Tag 不消费 cfg 是这条 trait 之外的特例, 用 `get_last_node` 做 proxy 不破坏抽象.

后续如果 Tag 也需要消费 cfg (例如想让 Tag 支持非默认 last_rotate=true 的快路径), 可以再考虑把 trait 拆成 `lp_has_spin_landpoint(lp)` + `cfg_demands_last_rotate(cfg)` 两条独立查询. 当前阶段无此需求.

## NoSpinHook / CautiousHook config_* 显式覆盖

### 现象
BaseSpinHook 默认 7 项 `config_*` 全部返回 false. 但历史 NoSpinHook 这 5 项 (allow_180 / allow_LR / allow_d / allow_D / allow_rotate_move) 全 true; CautiousHook 在 `fast_move_down=false` 路径下也是全 true (config_allow_d 取决于 fast_move_down).

直接派生 + 不覆盖会导致 path 1g/20g 邻居枚举字符集大幅收缩, 与 baseline 不 byte-equal.

### 解决
NoSpinHook 显式覆盖 5 项返回 true (movegen_hook.h:293-312); CautiousHook 显式覆盖 4 项 (allow_180/allow_LR/allow_D 直接 true, allow_d 看 fast_move_down). 其余 (config_last_rotate / config_is_20g) 历史就是 false, 继承 base 默认.

### 设计权衡
本想让 BaseSpinHook 默认 5 项返回 true (匹配 NoSpinHook 的 "无配置全开" 形态), 但这与 TSpinHook/ASpinHook 必须显式查 cfg 字段冲突 — 它们的默认应当是 "未提供 cfg 即视作不开启" (false). 让 base 默认为 false 反映了 trait 的语义 ("未指明 = 不开"), NoSpinHook/CautiousHook 选择显式开启就显式覆盖, 一致性更强.

## 测试用例更新

* `tests/oracle_diff.cpp`: `p.extra.spin` → `p.extra.type` (2 处).
* `tests/perft_movegen.cpp`: `p.extra.spin` → `p.extra.type` (1 处).
* `src/tetris_movegen.h`: 注释从 `lp.extra.spin` → `lp.extra.type` (commit C2 N.1 把 spin 收敛到 SpinTypePayload.type).

## 遗留事项 (留给 C3)

### N.2 PlainLandPoint
NoSpinHook / CautiousHook 当前 LandPoint = `::search_tspin::Search::TetrisNodeWithTSpinType`. 这是为了满足 MoveGenSearch 内 `cells_key_for(.node)` 的成员访问约定. 但 NoSpinHook 路径根本不消费 spin/last/is_check 等字段, 用 search_tspin 类型只是历史包袱.

C3 计划引入 `PlainLandPoint`: 仅含 `node` 一个成员, 让 NoSpinHook / CautiousHook 切换过去, 砍掉 LandPoint 的 90% 字段.

### N.3 hook_interface_matrix.md 复核
本次落地实际接口面 vs 设计文档的对照尚未做最终一遍复核. 改动稳定后 (C3 / C4 也落定), 把 hook_interface_matrix.md 翻新一遍.
