# dev 分支对齐 6cf67260 操作记录

- 时间：2026-06-02 14:32
- 目标：在沙盒仓库创建 `dev` 分支，并将其状态对齐到用户本机仓库提交 `6cf67260dcfe9002b956b4f14633d367d3c68da7`
- 本机仓库：`/Users/zhaoming.274/Work/tetris_ai_runner`
- 沙盒仓库：`tetris_ai_runner`

## 已确认状态

- 本机当前分支：`flip-bits`
- 本机当前 HEAD：`429cef80771d6d1a78fdbb521c43db5e228be3bc`
- 本机目标提交存在：`6cf67260dcfe9002b956b4f14633d367d3c68da7`
- 本机工作区状态：`?? research/`
- 沙盒初始分支：`flip-bits`
- 沙盒初始 HEAD：`e3b909bf8e1df1e10231bda78c7e2227a5b0f5ab`
- 沙盒工作区状态：`?? notes/`

## 调研结果

1. 沙盒最初为浅克隆，已执行 `git fetch --unshallow origin` 补齐远端公开历史。
2. 直接从远端无法拿到目标提交 `6cf67260dcfe9002b956b4f14633d367d3c68da7`。
3. 本机确认该提交仅位于本地分支：
   - `flip-bits`
   - `flip-bits-full-from-sandbox`
   - `recactor-zzz`
4. 继续在沙盒 `notes/session_428d29df_artifacts/` 内搜索历史产物，发现可复用 bundle：
   - `notes/session_428d29df_artifacts/file-b7c37fb6-910d-4161-aaf9-63e04f4f8b78/flip-bits.bundle`
5. 已验证该 bundle：
   - 包含 `6cf67260dcfe9002b956b4f14633d367d3c68da7 refs/heads/flip-bits`
   - 标记为 complete history，可直接导入完整提交历史

## 执行结果

1. 已从 bundle 导入提交历史到沙盒本地分支 `dev`。
2. 已切换到沙盒分支 `dev`。
3. 当前 `dev` HEAD：`6cf67260dcfe9002b956b4f14633d367d3c68da7`
4. 校验通过：
   - `git merge-base e3b909bf8e1df1e10231bda78c7e2227a5b0f5ab dev` 返回 `e3b909bf8e1df1e10231bda78c7e2227a5b0f5ab`
   - `git rev-list --count e3b909bf8e1df1e10231bda78c7e2227a5b0f5ab..dev` 返回 `33`
5. 当前工作区状态仍只有未跟踪目录：`?? notes/`

## 当前 dev 顶部提交

- `6cf6726` Extend oracle_diff with SRS kick edge case boards
- `c063a12` Cover MoveGen oracle_diff with TSD STSD SSpin and high stack boards
- `ba9b42f` Align T-spin detection with master search_tspin semantics
- `580cb10` Skip non-canonical symmetric rotations during MoveGen emit
- `d45f612` Fix oracle_diff TetrisMap polarity and add per-piece landing dumps
