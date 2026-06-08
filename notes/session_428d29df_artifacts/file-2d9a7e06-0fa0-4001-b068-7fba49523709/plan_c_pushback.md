# Plan C 实证范围 (2026-05-28)

## Mewtwo 实测结论
**总改动 ≈ 480 行, 跨 3 个解耦模块**, 不是我先前估计的 1000+ 行/模块. 用户"没那么多, 一起做"在量级上是对的, 但跨模块这一点仍然成立, 因此**风险评估**而不是**工作量**才是反驳的依据.

## 必须改的硬编码 (来自 Mewtwo 实测)

### A. 位板容器层 (W>32 大盘)
- `tetris_map.h:75` `static_assert(W >= 1 && W <= 32)`
- `tetris_core.cpp:266` `if (width > 32 || height > max_height)`
- `tetris_core.h:44` `int32_t top[32]`
- `ai_zzz.cpp:145, 146` `HoleNum[32], WellNum[32]` 栈数组

### B. 节点布局层 (N!=4 / 5+ rotation / 5+ cell)
- `tetris_core.h:238, 240, 242` `TetrisNode::data[4]/top[4]/bottom[4]`
- `tetris_core.h:2575` `op_create_bridge static_assert(Lines::size == 4)`
- `tetris_core.cpp:116` `map.count += 4` 假定 4-cell
- `tetris_core.cpp:67-77, 88-114` `attach switch(height/width)` 硬编码 4
- `tetris_core.cpp:388, 530` `node_block_ * 4`

### C. 搜索分发层 (5+ rot LUT / 7-piece switch)
- `movegen_search.h:337, 644` `usable_arr[4]`
- `movegen_search.h:946` `PathMark::kR = 4`
- `movegen_search.h:725` `make_path_20g_native mark[4 * kW * kH]`
- `movegen_search.h:226-286, 312-317, 638-643` `search/make_path` 内 'O'~'T' switch
- `rule_c2.cpp:22-28` 等 rule `get_generate` 硬编码 7-piece bag

## 可不改 (评估质量降级即可)
- `ai_zzz.cpp` `ItemTable[7]` 权重表
- `TOJ` T-slot 识别
- `Search::init` 'T' 块特化 (无 'T' 时自动跳过)

## 反驳理由 (即使总量只有 480 行)
1. **回归归因困难**: 布局层 (B) 改 4 → N 后, AI 评估器若出现差异, 难以判断是
   "格子算错了"还是 "rotation 维度填错了"; 与搜索分发层 (C) 合并后归因更难.
2. **当前环境不可编译**: A/B 都是机械重构 (zero behavior change), 单 commit
   无编译可接受; C 是引入新行为, 至少应该让每个 commit 内部具有"独立的语义
   闭包" (即可单独编译并跑过 oracle_diff).
3. **跨模块解耦原则**: TetrisNode 布局变化要求 TetrisContext init 大幅重写,
   与 MoveGenSearch 改 LUT 维度是两个独立的设计决策, 单 commit 把决策耦死,
   后续若要回退某一层须整块回退.

## 建议拆法 (4 个 commit, 每个 ~80~150 行)
- **C-A**: 位板容器层 W>32 解锁 (tetris_map / tetris_core W check / AI 栈数组) ~80 行
- **C-B1**: TetrisNode N!=4 解锁 (data/top/bottom[4] + attach + node_block_ index) ~150 行
- **C-B2**: op_create_bridge / TetrisContext::generate 桥接层 N!=4 ~50 行
- **C-C**: MoveGenSearch 分发层 (PathMark::kR / usable_arr / piece switch) ~150 行
合计 ~430 行, 与 Mewtwo 估算 480 一致, 单 commit 粒度可控.

## 边界提醒
**还需要用户给规则文件目标**才能真正"接通". 即使 C-A~C-C 都做完, 仓库里仍然没有
具体的"大盘规则定义" / "新 piece OpDesc" / "5-rot kick 表". 这些是规则作者层
的输入, 不是框架层能凭空生成的.
