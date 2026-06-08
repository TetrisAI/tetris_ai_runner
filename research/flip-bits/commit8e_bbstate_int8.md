# Commit 8e — BBState / MarkSlot 字段压到 int8_t

## 背景
BBState.xb/yb 实际取值 [-7, 14] / [-3, 44], 远在 int8 范围内. int16
的存储是 commit 5g-2 引入时为安全余量留的. 中间运算已经走 `int` 中转
再回填, 字段宽度变窄不影响算术.

## 改动
- `BBState.xb / yb` 与 MarkSlot.parent_xb/yb (1g make_path 与 20g
  run_piece_20g 各一份) 由 `std::int16_t` 改 `std::int8_t`.
- 所有 `static_cast<std::int16_t>` 同步改 `std::int8_t`. `std::uint16_t`
  (CellsKey::c) 不动, 与本次无关.

## 内存收益
- BBState: 8 -> 4 字节
- MarkSlot.parent: -2 字节
- BFS 队列 / mark 表条目内存约减半, cache 局部性更好.

## 验证
- `oracle_diff` 全场景通过 (`# all diffs ok`).
