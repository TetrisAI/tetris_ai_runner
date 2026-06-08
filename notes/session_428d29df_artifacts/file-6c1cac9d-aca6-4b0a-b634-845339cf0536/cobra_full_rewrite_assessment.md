# 用 Cobra 思路全量重构 tetris_ai_runner 的可行性评估

> 关键字: cobra rewrite feasibility movegen ai-framework refactor architecture

## 结论（先说人话）

**不建议整体重构。** Cobra 的设计目标和本框架在工程定位上是错位的，全量套用 Cobra 思路会丢失本框架的核心资产，而获得的优化只在 movegen 这一段。建议在保持框架骨架不变的前提下，对 movegen 子系统做"可选后端"接入，把 Cobra 的 Bitmask BFS 作为 `search_*` 系列里的一个新成员，对比落地。

## 一、两套代码的工程定位差异（这是反驳的根本）

| 维度 | tetris_ai_runner | cobra-movegen |
|---|---|---|
| 项目定位 | 含规则系统/AI 评估/树搜索/移动生成的**完整 AI 框架** | **纯粹的 movegen 引擎**，不含 AI 评估和规则抽象 |
| 规则支持 | 10+ 种：SRS / Botris / TOJ / QQ / C2 / PPT / ASRS / SRSX / ST / TAG | 1 种：SRS（big-board 分支也只是放宽尺寸） |
| 盘面宽度 | 运行时可变，最大 32（`uint32_t` 行） | 编译期常量 W，且 `Tn` 模板路由 H |
| 搜索器变体 | 6+ 种：simple / cautious / path / tspin / aspin / simulate / tag | 1 种：mask BFS |
| 状态识别 | 集成 TSpin/ASpin/T-slot/Hold/Combo/B2B 等高阶语义 | 仅 movegen，不识别 spin 类型 |
| 上层 AI | TetrisEngine + sb_tree + 多种 AI 实现（ax / tag / zzz / misaka / farter / ppt） | 无 |

**框架核心资产 ≠ movegen**。本仓库真正吃成本的是 AI/搜索/规则抽象，movegen 只是底座的一部分。

## 二、Cobra 思路在本框架内"水土不服"的具体点

1. **规则抽象层 vs Consteval 强约束**
   - 本框架靠 `TetrisOpertion` 函数指针族（`rotate_clockwise` / `wall_kick_*` / `generate`）支持多规则。
   - Cobra 把方块/踢墙完全 `consteval` 化，换框架等于把这个抽象层全部废弃，10 种规则要全部重写为模板特化或代码生成。
   - 实施成本极高，但带来的"性能" 90% 集中在 movegen，AI 那一侧不受益。

2. **盘面行宽 32 vs 垂直打包**
   - 本框架最大支持 W=32，且行宽参与运行时判断。
   - Cobra 的 `uint64_t` 垂直打包对 W 上限敏感（W*H ≤ 64 才能装一个 word），W=32 时垂直打包失去意义。
   - 想兼顾就要写一套带宽度路由的二级分发，复杂度爆炸。

3. **TSpin / ASpin 路径识别**
   - 本框架的 `search_tspin.cpp`(1125 行) / `search_aspin.cpp`(692 行) 把路径来源（旋转/踢墙的最后一步是哪种）作为 spin 判定输入。
   - 这是**节点层**的语义，依赖 `wall_kick_clockwise[]` 等指针网保留来源信息。
   - Cobra 的 mask BFS 一次性扩展整层前沿，丢失"我从哪条边/哪次踢墙过来"的来源链，**TSpin 类型判定要从头设计**。

4. **AI 评估的接口契约**
   - `TetrisCallAI` / `TetrisTreeNode` 直接消费 `TetrisNode const *`（指针即身份），通过 `index_filtered` 做去重。
   - 改成 mask 表达后，落点是 (mask, x, y, r) 四元组，所有 AI 接口（`ai_ax/ai_tag/ai_zzz/ai_misaka/...`）的 `eval/get_value/iterate` 都要重新对接，且评估器内部的 `land_point->status` 字段含义会改。

5. **指针网内存 vs CPU 用法**
   - 调研报告提到"Cobra 内存极低、计算型"。但本框架的指针网在 W=10 时数十 MB 量级，对桌面/服务端是白菜价；换来的是 `move_left = node->move_left` 一次解引用、零分支。
   - mask BFS 对应 3 次 shift + 2 次 mask + 1 次 popcount/test，两者数量级接近——只有当**搜索分支无法被指针网压扁**（例如大盘面、运行时变形地形）时，mask BFS 才有显著优势。

## 三、如果硬要"重构"，实际会发生的代价

- **代码量**：~7700 行核心 + 数千行 AI/规则/搜索特化几乎全部要改写或重新对接接口；
- **回归风险**：现有 10 种规则、6 种搜索器、5+ AI 全部要重新对齐基线；
- **测试覆盖**：本仓库没有看到完整的 movegen/AI 回归测试集，重构后正确性验证基本只能靠对局自测；
- **收益**：movegen 段在 W=10 / 标准 SRS 下可能 1.5–3x，但 AI 评估这一侧仍然是瓶颈，端到端节点/秒提升有限。

## 四、我推荐的路线（保留架构、可落地）

### 选项 A：把 Cobra 思路装进新搜索器（强烈推荐）
- 新增 `search_bitmask.cpp`，实现一个**等价于 `search_path`** 的搜索器，内部用 mask BFS 跑可达性，输出仍是 `TetrisNode const *` 列表（通过 `(status -> node*)` 反查 `TetrisContext::node_cache_`）。
- 优点：完全不动规则/AI/树搜索，可基线对比、可灰度切换；TSpin/ASpin 仍走旧搜索器。
- 风险：低；最差的情况是新搜索器不如旧搜索器，回退即可。

### 选项 B：仅吃 build_snap 一段
- 在 `TetrisMap` 上挂一个"smeared usable map"缓存（或 `TetrisMapSnap` 增加 mask 字段），`build_snap` 只要 carry 一次 OR 就能服务多个旋转态。
- 我们之前已经把 `check` 改成 `~A & B` 的 BMI 路径，配合 smear 后 `check` 可以退化为 1 次 `test`。
- 风险：低；改动量小；对 AI 评估侧零影响。

### 选项 C（不推荐）：全量重构
- 仅当未来需要支持非矩形地形 / 动态宽度 / 服务端高并发 movegen-only 服务时再考虑。
- 即便如此，仍建议以 fork 形式先做 PoC，再考虑回主干。

## 五、对你下一步的建议

1. **先 benchmark**：拉一份 movegen-heavy 的 profile（perf / vtune），看 build_snap、search_path、AI 评估三段的真实占比。如果 AI 评估占 60%+，重构 movegen 收益边际。
2. **走选项 B**：低风险、低代码量、和你已经做的 `~A & B` 重构方向一致，先把 movegen 这一段拧到指令级最优。
3. **想验证 mask BFS**：走选项 A，写一个 `search_bitmask` 与 `search_path` 做 A/B，把对比数据沉淀到本仓库的 research 目录。

