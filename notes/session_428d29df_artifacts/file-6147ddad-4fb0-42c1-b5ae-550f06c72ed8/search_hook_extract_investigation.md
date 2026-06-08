# tetris_ai_runner Search 层额外元数据计算解耦调研报告

## 背景
本报告旨在调研 `search_tspin` / `search_aspin` 与框架层 (`movegen_search.h`, `tetris_movegen.h`) 之间的额外元数据计算耦合现状，并提出通过 Search 自定义钩子 (Hook) 进行解耦的架构方向。

---

## A. 框架层与 search_tspin 的现状耦合

### 1. EnableT / EnableMini / kCheckTSpin 的分布与作用

#### tetris_movegen.h 中的耦合
- **`kCheckTSpin` (line 191)**: `static constexpr bool kCheckTSpin = EnableMini && (T == 'T');`。这是核心开关，仅当 `EnableMini` 为 true 且当前 piece 为 'T' 时开启 T-spin 逻辑。
- **`corners3_arr` (line 310, 313, 660-681)**:
    - 定义：存储每个坐标 (x,y) 和旋转 r 下，T 块 4 个角是否至少有 3 个被占据（含墙外）。
    - 作用：作为 `is_ready` 的必要条件。
- **`last_rotate_arr` (line 333, 364, 722-831)**:
    - 作用：记录每个可达格子是否是**最后一步通过旋转**到达的。
    - 细节：在 `apply_kicks` 中，如果 `kCheckTSpin` 为 true，会点亮 `last_rotate_arr[DstR] |= rotate_reached` (line 824)。
- **`emit_with_spin` (line 383, 559-579)**:
    - 作用：计算最终的 `spin` 字段（0=None, 1=Mini, 2=Full）。
    - 逻辑：`ready = landings & last_rotate_arr[r] & corners3_arr[r]`。
    - 字段写入：写入 `LandingPos.spin` (line 540) 和 `LandingPos.last_x/y/r` (line 543)（如果是旋转到达）。

#### movegen_search.h 中的耦合
- **`run_piece_20g<T, EnableT>` (line 1903-1927)**:
    - `EnableT` 分支：当为 true 时，`MarkSlot` (line 1925) 会存储 `parent_r/xb/yb` 和 `action` 用于回溯，并追踪旋转状态。
    - **`run_piece_20g_dispatch<T>` (line 1881-1889)**: 硬编码了 `constexpr bool kEnable = (T == 'T')`。
- **`run_piece_dispatch<T>` (line 1891-1896)**: 同样硬编码了 T 块的特殊逻辑分发。
- **`search()` (line 221-242, 247-253)**: 在 20g 和 1g 路径中，根据 piece 类型派发到带 `EnableT` 的实现。

### 2. TetrisNodeWithTSpinType 的构造与填充
- **定义**: `search_tspin.h` line 27-71。包含 `node`, `last`, `type` (TSpinType), 和 `flags` (is_ready, is_mini_ready等)。
- **构造路径**:
    - **MoveGenSearch (1g)**: 走 `run_piece` -> `MoveGen::generate` -> `emit_with_spin` -> `LandingPos`。然后在 `run_piece` 的回调中，将 `LandingPos` 转换回 `TetrisNodeWithTSpinType`。
    - **MoveGenSearch (20g)**: 走 `run_piece_20g`。在 `run_piece_20g` 内部，通过 `state_to_node` 反查 `TetrisNode` 指针，并根据 `mark` 表中的 `action` 填充 `is_last_rotate`。
- **旧路径 (search_tspin::Search)**:
    - `search_t` (search_tspin.cpp: 976-1126): 手动维护 `node_mark_`，在 `cover_if` 中处理旋转优先级。

### 3. TetrisNodeEx 别名传播链路
- **`movegen_search.h` line 153**: `using TetrisNodeWithTSpinType = ::search_tspin::Search::TetrisNodeWithTSpinType;`
- **`ai_zzz.h`**:
    - `TOJ_PC`, `TOJ_v08`, `TOJ` 类中：`typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;` (lines 97, 186, 299)。
- **`ai_misaka.h` line 11**: `typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx;`
- **AI 消费字段**:
    - `node->type` (TSpinType) 用于判定是否是 T-Spin。
    - `node.is_ready` / `node.is_mini_ready` 在某些 AI 的 `eval` 函数中被引用以计算分数。

---

## B. search_aspin 的现状

### 1. 实现机制
- **`ASpinType`**: 只有 `None` 和 `ASpin` (search_aspin.h: 13-16)。
- **`TetrisNodeWithASpinType`**: 仅包含 `node` 和 `type` (search_aspin.h: 26-59)。
- **`search()` (search_aspin.cpp: 368-496)**:
    - 逻辑：执行一个标准 BFS。
    - **A-Spin 判定 (line 411-414)**:
      ```cpp
      if ((!node->move_down || !node->move_down->check(snap)) && 
          (!node->move_up || !node->move_up->check(snap)) && 
          (!node->move_left || !node->move_left->check(snap)) && 
          (!node->move_right || !node->move_right->check(snap)))
      {
          node_ex.type = ASpin;
      }
      ```
      即：如果方块在当前位置无法进行任何上下左右移动（被完全卡住），则标记为 `ASpin`。

### 2. 影响范围
- **全 Piece 适用**: `search_aspin` 的判定算法与 piece 类型无关。只要满足 "四向阻塞" 条件，任何方块（I, O, L, J, S, Z, T）都可以被标记为 `ASpin`。

### 3. 与框架关系
- **平行系统**: `search_aspin::Search` 是一个完全独立的类，拥有自己的 BFS 实现和 mark 表。目前它**没有**接入 `movegen_search` 或位板 `MoveGen` 体系。
- **调用方式**: 在 `ai.cpp` 中通过模板参数选择。

---

## C. 用户的目标 = "Search 自定义钩子"

### 1. 框架层扩展点建议
为了剥离 piece-specific 计算，框架层（`MoveGen` / `movegen_search`）应提供以下扩展点：

- **`Trait::NodeMetadata`**: 存储在 BFS Mark 表中的额外数据（如 `search_tspin` 需要 `last_action`, `search_aspin` 可能不需要额外数据）。
- **`Trait::on_expand(parent_data, child_data, action)`**: 在 BFS 扩展边界时触发，允许 Hook 更新子节点的元数据。
- **`Trait::on_emit(bb_state, node_data, landing_pos_out)`**: 在发现落点时触发，允许 Hook 填充 `spin` 或 `type` 等自定义字段。
- **`Trait::Priority decide_priority(existing_data, new_data)`**: 替代目前的 `cover_if` 逻辑，决定是否允许新路径覆盖旧路径（例如旋转优先于平移）。

### 2. 钩子签名示例 (C++ Trait 模式)
```cpp
template<class RuleSpec>
struct TSpinHook {
    struct MarkData {
        uint8_t last_action;
        // ... 其他 T-spin 专用数据
    };
    
    // 框架传给 Hook 的环境信息
    struct Context {
        map_t const& board;
        std::array<map_t, kMaxR> const& usable_arr;
    };

    static void on_expand(MarkData const& parent, MarkData& child, char action) {
        child.last_action = action;
    }

    static void on_emit(BBState const& s, MarkData const& data, Context const& ctx, LandingPos& out) {
        // 在这里计算 corners3, mini-ready 等，填充 out.spin
    }
};
```

### 3. 框架需提供的信息
- **必须提供**: `BBState` (r, xb, yb), `usable_arr` (用于快速碰撞检查), `board` 位图。
- **可反算信息**: `corners` 数据建议由 Hook 自行根据 `board` 计算，以保持框架纯净；`last_action` 必须由框架在 BFS 过程中维护并喂给 Hook。

### 4. search_tspin 重写风险与损失
- **`cover-if-kick` 语义**: `search_tspin` 依赖旋转动作覆盖平移动作来记录正确的 `last_rotate`。如果 Hook 接口不支持优先级控制，可能会丢失 T-Spin 标记。
- **性能**: 如果在 BFS 内部进行复杂的 Hook 调用，可能会降低 MoveGen 的速度。

---

## D. 风险评估

### 1. Oracle Diff 影响 (master vs MoveGenSearch)
- **硬编码逻辑依赖**: 目前 `movegen_search.h` 中的许多 `if constexpr (EnableT)` (如 line 1903) 守卫了 20g 路径下的 `MarkSlot` 结构变化。如果统一 Hook 接口，需要确保非 T 块的 `MarkSlot` 依然保持轻量（不存储冗余的 parent 信息），否则会导致内存占用增加。
- **Sunk Emission**: `EnableT` 决定了是否在 `sunk emission` 时写入 `spin`。如果 Hook 默认开启，可能会在非 T 块上产生意外的 `spin` 标记，导致 `oracle_diff` 不一致。

### 2. 输出风险
- **误触发**: 如果 `search_aspin` 的 "全阻塞" 逻辑作为一个 default hook 注入，那么原本只关心 T-spin 的 AI 可能会因为收到非 T 块的 `ASpin` 标记而产生行为偏差。
- **路径重放**: `make_path` 依赖 mark 表。如果 Hook 改变了 mark 表的覆盖策略，`make_path` 生成的按键序列可能会变化。

---

## 附件：关键引用位置
- `movegen_search.h`:
    - `run_piece_20g` 定义: 1904-2035 行
    - `run_piece` 派发: 1891-1896 行
- `tetris_movegen.h`:
    - `MoveGen::generate` 主循环: 293-413 行
    - `emit_with_spin` 逻辑: 559-579 行
    - `compute_corners3`: 660-681 行
- `search_aspin.cpp`:
    - `ASpin` 判定逻辑: 411-414 行
