# root_eval_fix_20261029 — Configtree discard 后 root eval 副本残留回归

- 日期：2026-10-29
- 唯一 writer：`tetris_ai_runner/src/tetris_core.h`（`tetris_eval_get.h` 未改）
- Source freeze（修复后）：`src/tetris_core.h` = `5f1fbe8512b73b2dd0b5cecc47066c37f750cfb5f41662cfc9891fc5ed20915a`
- 保护 `src/ai_ax.h`（未动）：`caf3117f25cb91206c21f2e4d88400619ed0fda0dd89ea2df4a115281e6e9bf8`
- 编译：`/opt/homebrew/opt/llvm/bin/clang++`（Homebrew clang 22.1.8, arm64），`-std=c++20 -O0 -g -w`

## 根因
`discard_search_tree()` 把 `*root_ = TreeNode{}` 重置成空壳（map 空、eval_slot gen0）。
下一轮 `run → TetrisTreeNode::update → update_root`：root->map(空) ≠ 当前盘面，children 已清空，
于是走 `new_root = context->alloc(nullptr)`。

`Context::alloc` 从 free_list 回收 discard 前的旧子节点，但 `reclaim()`（tetris_core.h ~:712）只清
children/node_flag/flag/node/hold/level，**不清 `eval_slot`**。回收节点带着旧 Config、旧 map 的 eval
副本成为新 root，随后 `new_root->map = _map`。

关键：**root 的 Status 由 host 直写，从不调用 root 的 `get`**，所以 `args::Eval`（tetris_core.h:465）
那条 gen/era 自检刷新路径永远不会在 root 上触发。root eval_slot 就此残留旧副本。

旧副本特征（diag 实测）：`node=T@0,4r0 level0 gen3 pop26 exp65 got30 cells30 par=nullptr`，
即 pop30×cfg1.weight1.0=30（旧子节点 map，旧 Config），而非 pop26×cfg2.weight2.5=65。
总 619545 节点中仅此 1 root bad，619544 非 root 全 good。

## 最小 diff（`TetrisTreeNode::update_root`，新建/复用两支就绪后、`dealloc(this)` 前）
```cpp
// root 的 Status 由 host 直写、永不经 args::Eval 取 eval；alloc 回收的旧子节点会带着旧
// Config/旧 map 的 eval 副本成为新 root。建立（新建/复用）时按当前 map 与纪元补齐 root
// eval_slot，使其与当前 Config 一致；不请求 Eval provider 的 AI 该成员为 monostate，不产生 eval。
if constexpr (GetTraits<TetrisAI>::kWantEval)
{
    if (new_root->eval_slot.eval_gen != context->eval_cache.generation() ||
        new_root->eval_slot.eval_era != context->eval_era)
    {
        new_root->eval_slot.result = context->eval_cache.lookup(*context->ai, new_root->map);
        new_root->eval_slot.eval_gen = context->eval_cache.generation();
        new_root->eval_slot.eval_era = context->eval_era;
    }
}
```
- 与 `args::Eval`（tetris_core.h:465-472）同一套 gen/era + `eval_cache.lookup`，**不是清零默认值，不过滤 root 断言**。
- 只在 root 建立漏斗刷新；`map==_map` 快速路径（root 未换盘面）不重复 eval。
- `kWantEval=false` 的 AI 该成员为 `std::monostate`，`if constexpr` 丢弃分支，**不产生额外 eval**。
- 未动：root hostStatus/raw/iterate 语义、AI 运算、C2 权重、root combo/time/DP、21row 尺度、浮点顺序；oracle 未碰；transposition_key 保持删除。

## 复现命令（从 tetris_ai_runner 目录）
```sh
CXX=/opt/homebrew/opt/llvm/bin/clang++

# 1) diag：复现并 dump 残留 root（修前 bad=1，修后 bad=0）
$CXX -std=c++20 -O0 -g -w -Isrc \
  test/research/config_resume_20261029/diag_config_tree_stale.cpp \
  src/ai_zzz.cpp src/integer_utils.cpp src/random.cpp \
  -o test/research/config_resume_20261029/diag_config_tree
./test/research/config_resume_20261029/diag_config_tree

# 2) 原 framework 测试（修前 Phase5 eval 1/619545，修后全绿）
$CXX -std=c++20 -O0 -g -w -Isrc \
  test/framework_config_tree_reference_test.cpp \
  src/ai_zzz.cpp src/integer_utils.cpp src/random.cpp -o /tmp/fw_config_tree
/tmp/fw_config_tree

# 3) C2 Config invalidation（单 bucket=1，reused==fresh 全位）
$CXX -std=c++20 -O0 -g -w -Isrc \
  test/test_c2_config_invalidation.cpp \
  src/ai_zzz.cpp src/integer_utils.cpp src/random.cpp -o /tmp/c2_inval
/tmp/c2_inval
```

## Old bad → New good
- diag：
  - 修前：`BAD node=T@0,4r0 level=0 gen=3 pop=26 exp=65 got=30 cells=30 par=0x0` … `tot=619545 bad=1 cur_gen=5`
  - 修后：`tot=619545 bad=0 cur_gen=5`（rc=0）
- framework_config_tree_reference_test：
  - 修前：Phase5 `eval 619544/619545`（root bad）
  - 修后：`passes=23 failures=0`，`eval 619545/619545`，`raw 619544/619544`，virtual 3630/3630，root raw 4 字段两轮 PASS；get 计数 e1cfg1/e1cfg2/e2 同为 641337（root 补的是 eval 不是 get，未污染计数）；rc=0
- test_c2_config_invalidation：
  - `PASS c2_config_invalidation (reachable=24813 stale=0 failures=0)` rc=0（target/value/attack/map/combo/combo_limit reused==fresh 全位匹配；real_get/virtual_get>0）

## 证据边界（什么被证明 / 什么 pending）
- **Static 机制事实（本次改动）**：仅在 `update_root` 建立漏斗新增一个 `kWantEval` 门控的 root eval 刷新；
  非 root 节点路径、AI 运算、C2 权重、root combo/time/DP、21row 尺度、浮点顺序、root hostStatus/raw/iterate
  语义均未改；无 Eval provider 的 AI 该成员为 `std::monostate`，`if constexpr` 丢弃分支不产生 eval；oracle 未碰；
  transposition_key 保持删除。
- **Target tests 实际通过（rc=0，本代理实跑）**：
  - diag_config_tree_stale：`tot=619545 bad=0 cur_gen=5`
  - framework_config_tree_reference_test：`passes=23 failures=0`，eval 619545/619545、raw 619544/619544
  - test_c2_config_invalidation：`reachable=24813 stale=0 failures=0`
- **完整 Debug ctest suite：pending**，不在本报告作通过结论；由独立 build 实跑 ctest 得出（known-safe
  old-generation 保留项不削）。本报告不以"改动小 / 跨 TU 编译通过"作为全 suite 不退的证明。

## Verifier text gate
- `rubric check-file-format` → `passed: true`（text size=4850）。
- `text must-contain --mode all` → `passed: true`（matched 8/8：freeze/保护 hash、`update_root`、`bad=1`、`bad=0`、`619545`、门控 hunk、`clang++`）。
