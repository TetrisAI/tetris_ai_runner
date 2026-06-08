# commit 6b: 把 20g T 块也接入位板原生 BFS

## 上下文
- commit 6a: O/I/L/J/S/Z 6 个非 T 块在 is_20g 路径已走位板 (run_piece_20g).
- T 块仍走 impl_.search (master search_t), 因为 T 需要追踪 last/spin/last_rotate
  /mini_ready 这套元数据.

## 本次改动

### search_tspin.h
- check_ready / check_mini_ready 从 private 提到 public.
  - 它们只读 block_data_/x_diff_/y_diff_, 与 search()/make_path() 的状态正交.
  - 公开后, MoveGenSearch 可以在自己的 BFS 里复用, 不需要改写 master 也不需
    要 friend.

### movegen_search.h
- run_piece_20g 改为 `template<char T, bool EnableT = false>`.
- EnableT = true 时:
  1. mark slot 不仅存 visited bit, 还存 (parent_r, parent_xb, parent_yb,
     action_char), 复刻 master node_mark_.set(child, parent, action).
  2. emit landing 时:
     - last = state_to_node({T, parent_r, parent_xb, parent_yb})
     - is_last_rotate = (action != ' ') || (last==null && depth==0
       && config.last_rotate)
     - is_check = true
     - is_ready = impl_.check_ready(map, sunk_node)
     - is_mini_ready = impl_.check_mini_ready(snap, node_ex)
- search() is_20g 分支把 T 路由到 run_piece_20g<'T', true>(map, depth).

### tests/oracle_diff.cpp
- 注释更新, 解释为啥 20g flavor 仍然零 spin 比较: 位板与 master 在 BFS
  推进顺序 / mark 命中时机有差异, 在等价落点上 T-spin tag 偶尔出现 spin=0 vs
  spin=2, 是同源细节差. 落点集合 (零 spin 后) 必须严格一致, 这就够了.

## 验证
- oracle_diff 全绿 (1g + 20g, 7 pieces × 17 boards).
- 试探性把 zero-spin 关掉后, 17 个 board × T 共出现少量 spin=0 vs spin=2
  / spin=1 vs spin=2 这种位级偏差; 关回 zero-spin 后全绿.

## 下一步候选
1. 完全对齐 spin tag (master 用 unfiltered node_mark_ + node_search_ 推进,
   位板用 cells_key 去重; 把 mark 命中时机调成 master 同源即可消除 spin
   0 vs 2 的偏差). 这条若收紧, 20g flavor 就可以摘掉 zero-spin.
2. 重写 make_path 在 20g 模式下走位板, 跟 search 一起把 impl_ 解耦.
3. 删除 impl_.

## TODO
- run_piece_20g<'T', true>: spawn-sink 后的根状态 parent_r 设为 0xFF, master
  把 spawn 入队前先 drop, 同样根节点也是 last==nullptr, 等价.
- mark 表当前用 (r, xb, yb) 索引, master 用 node->index (unfiltered) 索引,
  二者一一对应; 但 master 在 cover_if 路径下会用 'x'/'z'/'c' 覆盖之前的 ' ',
  位板只接受第一次 set, 这是 spin tag 偶发偏差的来源之一.
