# Plan C 单 commit 请求 — 反驳与边界 (2026-05-28)

## 用户当前要求
> 开始 Plan C，一个 commit 完成

## 反驳要点

Plan C 的真实范围 (来自 user_pushback_assessment.md):
- **C1**: C2 规则接通 (rule_c2 / MoveGenSearch C2 路径 / TetrisContext / AI / generator)
- **C2**: 大盘 (>10 列 / >40 行) 接通 (row_t 宽度自适应 / 越界 / AI 常量)
- **C3**: 新 piece (10 piece / 5 rot) 真正接通

三件事互相正交且每件 1000+ 行级, 单 commit 不可行原因:
1. 与 commit message 准则"聚焦相对目标分支差异"冲突
2. 回滚粒度过粗 (任一回归拖累其他两块)
3. **目标未明确** -- 大盘多大? 哪些新 piece? kick 表? rotation 数?
   不确定输入根本写不出代码
4. 当前环境无法编译, 引入新行为单 commit 推送风险高于 A/B (那两块是
   机械重构, 行为零变化)

## 已等待用户拍板的边界
- 目前停在向用户提问 "Plan C 内先落哪一项 + 具体目标"
- 没有写任何代码
- 调研记录见此文件, 后续恢复工作直接读

## 建议拆法 (供后续拍板后参考)
- C1 子计划 (3~4 commit): search 路径解锁 -> LUT 填表 -> generator -> AI 兼容
- C2 子计划 (2~3 commit): row_t 宽度推导 -> shape 越界 -> AI 常量
- C3 子计划 (2~3 commit): 规则文件落地 -> generator entry -> AI 默认评估

每个子 commit ~150 行级, 与 A/B 阶段同粒度.
