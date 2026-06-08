# Bitboard Refactor Checklist

## Commit 3-B: ai_ax / ai_farter bitboard semantics
- [x] ai_ax.cpp / ai_farter.cpp 1=occupied 语义修正 (9cb4e72)

## Commit 5: ActivePiecePolicy + TetrisEngine2
- [x] 5-1  ActivePiecePolicy 三类型定义 (ActiveNonePolicy / ActiveTOnlyPolicy / ActiveAllPolicy)
- [x] 5-2-1  DeduceSpinPolicy<AI> 元函数实现
- [x] 5-2-2  TetrisEngine2<Rule, AI, Search> alias
- [x] 5-2-3  TSpinHook::active_for_piece 双参重载
- [x] 5-2-4  policy_deduction_test.cpp static_assert 全量验证 ← **完成**
             - TOJ / TOJ_v08 → ActiveTOnlyPolicy ✓
             - Botris / Botris_PC → ActiveAllPolicy ✓
             - ai_easy::AI → ActiveNonePolicy ✓
             - TSpinHook::active_for_piece 双参 / 单参行为 ✓
             - NoSpinHook 双参 / 单参 ✓
             - TetrisEngine2 == manual rebind_policy ✓
             - BUILD PASSED: EXIT 0 (2026-06-03)

## Commit 4 Phase 1: for_each_typed_r 工具层
- [x] `src/typed_dispatch.h`：`for_each_typed_r<R_count>` + `TypedLandPoint<T,R>` 实现
- [x] `tests/typed_dispatch_test.cpp`：static_assert + 运行时 assert 全量验证
- [x] `CMakeLists.txt`：typed_dispatch_test 构建目标 + ctest 注册
- [x] BUILD + RUN PASSED: EXIT 0 (2026-06-03)
     - TypedLandPoint<T,R> 编译期常量字段 ✓
     - for_each_typed_r<4>(r): 精确 dispatch 到 r==R 的槽 ✓
     - for_each_typed_r<4>(99): 超界返回 false，fn 不调 ✓

## Commit 4 Phase 1b: BBNode<Spec,T,R,SpinT>
- [x] `src/bb_node.h`：BBNodeBase + BBNode 通用特化 + std::monostate 偏特化
- [x] `tests/bb_node_test.cpp`：static_assert + 运行时 assert 全量验证
- [x] `CMakeLists.txt`：bb_node_test 构建目标 + ctest 注册
- [x] v1 BUILD + RUN PASSED: EXIT 0 (2026-06-03)
     - kHeight == 4（SRS 全量 OpLines::size）✓
     - kOriginY == piece_cells.origin.y（编译期对拍）✓
     - row() == y - kOriginY（各 piece/rotation 运行时对拍）✓
     - sizeof(BBNode<monostate>) == 2，sizeof(BBNode<FakeSpinType>) == 3 ✓
     - has_spin 探针：monostate 无 spin，SpinT 特化有 spin ✓
- [x] v2 补齐旧 TetrisNode 全量字段扫描（ai_tag / ai_zzz / ai_farter / ai_ax / ai_misaka）
- [x] v2 BUILD + RUN PASSED: EXIT 0 (2026-06-03)
     - land_height() == row() + kHeight（y-up bbox 落点高度；≠ y+1 unless cy_min==0）✓
     - kWidth（编译期 bbox 列数，cx_max-cx_min+1）：T=3, I-竖=1, O=2 ✓
     - line(i) y-down 翻转对拍 OpLines::data（T R=0 kLines=[0,0,7,2]）✓
- 关键坐标结论：
     - origin.y = H - 1 - cy_min（不是 H-1；cy_min 因 piece 形状不同而异）
     - emit: lp.y = yb + origin_y  →  row() = y - kOriginY = yb（bbox 底行）
     - row() ≠ y - kHeight + 1（之前草案有误，已修正）
     - land_height() = row() + kHeight（对应旧 node.row + node.height）
     - line(i) = OpLines::data[kHeight-1-i]（y-down 翻转，对应旧 node.data[i]）

## Commit 3 剩余 AI 迁移 (已完成): ax / farter / zzz
- [x] ai_ax.h / ai_ax.cpp：eval 迁移到 BBNode<Details,monostate> + Map<W,H>（1=occupied）
- [x] ai_farter.h / ai_farter.cpp：同上
- [x] ai_zzz.h / ai_zzz.cpp：同上（含多 AI 结构体全量迁移）
- [x] bb_node.h：BBNodeDetails<Spec,T,R> 引入，三参打包为单一类型
- [x] tests/bb_node_test.cpp：对应 BBNodeDetails 接口更新
- [x] BUILD PASSED: EXIT 0 (2026-06-04) — commit 5734756

## Commit 4 Phase 2 (已完成): BBCallEval compile-time dispatch
- [x] TetrisCore::eval() 替换为 BBCallEval<AI, SearchRuleSpecOf<Search>::type>::eval()
- [x] TetrisCallAI eval 侧基础设施（eval_function_traits / CallEval / eval()）移除
- [x] TetrisAIInfo::Result 改为直接引用 AI::Result alias
- [x] m_tetris::TSpinType / ASpinType 独立定义，断开循环 include 链
- [x] bb_eval_bridge.h 移至 namespace m_tetris 块外，防止内层 namespace 嵌套
- [x] Legacy AI fallback（EvalIsLegacyNodeEx concept）保证向后兼容
- [x] BUILD PASSED: EXIT 0 (2026-06-03) — commit e2718d8

## Commit 4 Phase 2 扩展 (已完成): MapT concept 探针
- [x] EvalWantsTSpinMapT concept：检测 AI eval(BBNode<Det,TSpinType>, Map<W,H>, Map<W,H>, int)
- [x] EvalWantsASpinMapT concept：检测 AI eval(BBNode<Det,ASpinType>, Map<W,H>, Map<W,H>, int)
- [x] call_eval_typed 分发优先级：MapT 版先于 TetrisMap fallback
- [x] ai_tag (TSpin) / ai_misaka 现可通过 EvalWantsTSpinMapT 路径，无需 TetrisMap 中转
- [x] BUILD PASSED: EXIT 0 (2026-06-04) — commit 5734756
