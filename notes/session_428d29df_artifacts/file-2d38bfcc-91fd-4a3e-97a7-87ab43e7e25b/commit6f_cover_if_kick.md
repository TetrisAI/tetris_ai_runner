# Commit 6f: cover_if semantics for kick mark, BFS neighbor order aligned

## 目标
让 20g T 块的 spin tag 与 master `search_t` 严格对齐。

## 改动
1. `src/movegen_search.h`
   - 新增 `try_cover_with_parent` lambda，复刻 master `node_mark_.cover_if(child, parent, ' ', action)` 语义：当 child 已被 shift (action == ' ') mark 过，旋转 (x/z/c) 可以**覆盖**为 'x'/'z'/'c'。
   - 邻居生成顺序调整为 master `search_t` (search_tspin.cpp:1007~1080) 的口径：d → l → r → x (180) → z → c。
   - 仅 EnableT=true 路径启用 cover 逻辑；EnableT=false 路径仍用 `try_mark_with_parent`。
2. `tests/oracle_diff.cpp`
   - 修改 zero-spin tolerance 注释，说明剩余少量 oracle spin=2 vs new spin=0 的差异其实源自 master 的 mark 写在 pre-drop、读在 post-drop，当 kick 之后还能下落时 master 漏标 spin。位板侧在落点（post-drop）上 mark，更精确。
   - 保留 zero-spin tolerance（落点集合零化后严格一致仍是 search 正确性的真正度量）。

## 验证
- `oracle_diff` 全场景 `# all diffs ok`。
- 探针测试（关闭 zero-spin tolerance）显示 commit 6f 修复了：empty / sspin / tss_left / tsd 共 4 个场景的 spin tag。剩余 11 个场景仍有 oracle spin=2 vs new spin=0，根本原因是 master 在 pre-drop 节点上写 cover_if mark、在 post-drop 读 get，kick 之后能继续下落时 master 自己漏标。位板侧 mark 更精确，但因 oracle 错误所以呈现"位板少标"。

## 后续
- Commit 6g（可选）：把 `search_tspin::Search` 的剩余类型 `Config / TSpinType / TetrisNodeWithTSpinType` 完全迁移到 `MoveGenSearch`，让 `search_tspin.{h,cpp}` 仅保留 cmd_tris/老 demo 兜底。
- spin tag 对齐到精确版本（不再容忍 zero-spin）需要在 oracle 端也修一遍 mark 时机，当前不在重构范围内，可作为后续单独修 master 的 patch。
