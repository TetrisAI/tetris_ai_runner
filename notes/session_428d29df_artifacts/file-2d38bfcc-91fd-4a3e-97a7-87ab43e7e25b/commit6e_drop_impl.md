# Commit 6e: Remove impl_ field; MoveGenSearch fully decoupled from search_tspin::Search

## 目标
彻底拔除 `MoveGenSearch::impl_` (`search_tspin::Search`) 字段与所有委托调用，让位板原生引擎完全独立。

## 改动
1. `src/movegen_search.h`
   - 删除字段 `::search_tspin::Search impl_;`。
   - `init()` 中删除 `impl_.init(...)`。
   - `search()` 20g 路径 default 分支由 `return impl_.search(...)` 改为 `assert + return &land_point_cache_;`，与非 20g 路径口径一致 (5h 已对非 20g 路径收窄到 SRS-7)。
2. `src/search_tspin.h`
   - `check_ready` / `check_mini_ready` 由 `public` 恢复为 `private`，去掉 6b 时为外部访问添加的注释。

## 验证
- `cmake --build . -j 4` 全量目标 100% 构建通过。
- `./oracle_diff` 全场景通过 (`# all diffs ok`)。
- `./perft_movegen` 抽样不报错。

## 后续
- Commit 6f：处理 20g T 块 spin tag 与 master BFS 顺序差异，移除 `oracle_diff` 中的 `t.spin = 0` 容忍逻辑。
- 还需考虑：把 `search_tspin::Search` 完全从 movegen 头依赖中拆离 (`Config / TetrisNodeWithTSpinType / TSpinType` 仍以 alias 暴露; 若 AI 端不再触达 `search_tspin::Search`，可将这些类型源迁移到 `movegen_search.h` 自身, 让 `search_tspin.{h,cpp}` 仅作为 cmd_tris/老 demo 的兜底). 暂未做, 不影响 oracle_diff 行为.
