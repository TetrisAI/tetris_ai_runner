# Search Hook 方案 v2 (响应用户 2026-05-28 三点反驳)

> 状态: 等用户拍板. 0 commit, 0 dirty.
> 关键字: search hook trait MarkData NodeEx active_for_piece NoHook DefaultTSpinHook DefaultASpinHook

## 0. 用户反驳消化
1. **"Hook 也是模板, 其应该可以定制仅对某些 T 触发 hook"**
   → 我 v1 提的 `Trait::piece_needs_spin<T>::value` 是多此一举.
   Hook **本身就是模板**, `Hook::template active_for_piece<T>` 编译期 bool 就够,
   框架对 piece 字面量完全零知识.
2. **"这个插件支持后也是可以做到与 master 行为相同"**
   → DefaultTSpinHook 仅对 'T' 激活, 内部算法搬迁不改, oracle_diff baseline 保持.
   v1 那种"H1 改动小, H2 风险高所以先做 H1"的折中**不必要**.
3. **"编译时应该匹配 Search 的 MarkData 类型与 AI 需要 MarkData 是否匹配, 不匹配则直接不启用 Hook"**
   → 编译期 NodeEx 类型匹配, 不匹配走 NoHook 退化. 这是 hook 体系真正的"插件化".

## 1. 新架构 (取代 v1 的 H1/H2/H3)

### 1.1 概念分层 (4 层, 上层完全不感知下层细节)

```
+----------------------------------------------------+
|  AI 模块 (ai_zzz / ai_misaka / botris)            |
|  typedef TetrisNodeEx = ChosenHook::NodeEx        |
+------------------------ + -------------------------+
                          |
+----------------------------------------------------+
|  MoveGenSearch<RuleSpec, Hook = DefaultTSpinHook>  |
|  using NodeEx = Hook::NodeEx                        |
+------------------------ + -------------------------+
                          |
+----------------------------------------------------+
|  MoveGen<RuleSpec, T, Hook = NoHook>                |
|  BFS 主体: 零 piece / 零 spin 知识                  |
|    if constexpr (Hook::template active_for_piece<T>)|
|       维护 Hook::RotState, 调 Hook::on_*            |
+------------------------ + -------------------------+
                          |
+----------------------------------------------------+
|  Hook trait (DefaultTSpinHook / DefaultASpinHook /  |
|  NoHook / 用户自定义)                               |
|    using MarkData       — BFS Mark slot 私有 payload|
|    using RotState       — per-rotation 状态(last_rotate,corners3)|
|    using Payload        — emit 输出 landing 数据    |
|    using NodeEx         — AI 消费的 land_point 类型 |
|    template<char T> static constexpr bool active_for_piece;|
|    static void on_init(board, RotState&);           |
|    static void on_expand(parent_md, child_md, action);|
|    static void on_rotate_reach(src_r,dst_r,RotState&,...);|
|    static void on_emit(landing_state,RotState,Payload&);|
|    static NodeEx make_node_ex(TetrisNode const *, Payload const&);|
+----------------------------------------------------+
```

### 1.2 关键编译期约束

#### (a) Hook 决定 piece 是否激活, 框架不问 T
```cpp
template<class Spec, char T, class Hook = NoHook>
class MoveGen {
    static constexpr bool kHookActive = Hook::template active_for_piece<T>;
    // 不再有 (T == 'T'). 不再有 EnableMini.
    ...
    if constexpr (kHookActive) {
        // 维护 RotState (last_rotate / corners3 / 任意 hook 私有状态)
        // 调 Hook::on_emit 写 Payload
    } else {
        // 极简: 只 emit (x, y, r)
    }
};
```

#### (b) AI 与 Search 的 NodeEx 必须类型匹配, 否则编译失败 / 退化
```cpp
template<class Rule, class AI, class Search>
class TetrisEngine {
    using AIWantsNodeEx     = typename AI::TetrisNodeEx;
    using SearchProvidesNodeEx = typename Search::NodeEx;

    // === 选项 A (严格): 不匹配立即编译失败 ===
    static_assert(std::is_same_v<AIWantsNodeEx, SearchProvidesNodeEx>,
                  "AI::TetrisNodeEx must match Search::NodeEx; "
                  "use a Search whose Hook produces the matching NodeEx, "
                  "or wrap with NoHook for plain TetrisNode*.");

    // === 选项 B (柔性): 不匹配时框架自动用 NoHook 替换 Hook ===
    using EffectiveSearch = std::conditional_t<
        std::is_same_v<AIWantsNodeEx, SearchProvidesNodeEx>,
        Search,
        typename Search::template Rebind<NoHook>
    >;
};
```

> ⚠ 用户原话 "不匹配则直接不启用 Hook" 字面对应**选项 B**.
> 我担心选项 B 可能掩盖配置错误 (AI 期望 TSpin 但意外拿到 NoHook, 静默退化, 行为差异不易察觉).
> **建议选 A** (严格 static_assert). 选 B 的话需要再加一个 explicit opt-in 标志.
> 详见第 4 节"待确认 1".

### 1.3 与 master 行为相同的保证
- `DefaultTSpinHook::active_for_piece<'T'> = true`, 其余 = false.
- `DefaultTSpinHook::RotState` 携带现有 `corners3_arr / last_rotate_arr`.
- `DefaultTSpinHook::Payload` = 现 `LandingPos { spin / last_x/y/r / has_last_rot }` 的字段.
- `DefaultTSpinHook::NodeEx` = 现 `TetrisNodeWithTSpinType` (字段不变, 命名空间换).
- 算法 (corners3/emit_with_spin/check_mini_ready_native/spin_block_) 全部**搬迁不改写**.
- oracle_diff baseline 保持.

### 1.4 search_aspin 的归宿
- `DefaultASpinHook::active_for_piece<*> = true` (与 piece 无关).
- `RotState = empty`. `MarkData = empty`.
- `Payload = { ASpinType type }`, `NodeEx = TetrisNodeWithASpinType` 字段不变.
- `on_emit` 内部做 4-direction collision 染色.
- `search_aspin::Search` 类签名保留, 内部委托 `MoveGenSearch<Rule, DefaultASpinHook>`. 上层 (`botris.cpp/cmd_tris.cpp/ai.cpp`) typedef 不变.
- ⚠ 现有 search_aspin 走的是**老 TetrisNode 指针图 BFS**, 而不是位板原生; 接入 MoveGenSearch 等于把 ASpin 也升级到位板 BFS, **无 oracle baseline**, 是真实风险.
- 建议: 接入 ASpinHook 但保留 `search_aspin::Search` 旧实现一段时间, 二者跑回归对比. 详见第 4 节"待确认 3".

## 2. 落地步骤 (commit 计划)

### Commit 1: Hook 抽象骨架 + NoHook + MoveGen 模板参数切换
- 新增 `src/search_hook.h`: `struct NoHook { ... }`.
- `MoveGen<Spec, T, EnableMini=false>` → `MoveGen<Spec, T, Hook=NoHook>`.
- `LandingPos` 拆为 `LandingPos<Hook>` (基础 x/y/r + Hook::Payload extra, 用 `[[no_unique_address]]`).
- 把所有 `if constexpr ((T=='T') && EnableMini)` 替换为 `if constexpr (Hook::template active_for_piece<T>)`.
- corners3 / last_rotate / emit_with_spin / target_blocked_mask 全部搬入 `DefaultTSpinHook` (本 commit 暂不引入 DefaultTSpinHook, 这些函数先变成 hook-aware 的 free template 函数, 等 commit 2 再搬).
- 验证: 当前 cautious/simple search (kCheckTSpin=false 等价 NoHook) 路径跑通.

### Commit 2: DefaultTSpinHook + search_tspin 委托
- 新增 `src/default_tspin_hook.h`: `struct DefaultTSpinHook { active_for_piece<'T'>=true; ... }`.
- 把 commit 1 留下的 free function 全部内化到 DefaultTSpinHook 静态成员.
- `MoveGenSearch<Rule>` → `MoveGenSearch<Rule, Hook=DefaultTSpinHook>`.
- `MoveGenSearch::TetrisNodeWithTSpinType` → `MoveGenSearch::NodeEx = Hook::NodeEx`.
- `search_tspin::Search` 内部委托 `MoveGenSearch<Rule, DefaultTSpinHook>`.
- 删除 `movegen_search.h` 中的 `kEnable=(T=='T')` 与 `EnableT` 模板参数.
- **必须跑过 oracle_diff**.

### Commit 3: DefaultASpinHook + search_aspin 委托 (高风险)
- 新增 `src/default_aspin_hook.h`.
- `search_aspin::Search` 内部委托 `MoveGenSearch<Rule, DefaultASpinHook>`.
- 无 oracle baseline. 跑 botris.cpp / cmd_tris.cpp 的回归 (如果有).
- ⚠ 如果回归手段不够, 这一 commit 应该被推迟 / 不做.

### Commit 4: AI typedef 收尾 + Engine static_assert
- ai_zzz / ai_misaka 的 `typedef search_tspin::Search::TetrisNodeWithTSpinType TetrisNodeEx` → `typedef DefaultTSpinHook::NodeEx TetrisNodeEx` (或保留 search_tspin alias 不动, 因为 search_tspin 已委托过去).
- TetrisEngine 加 `static_assert(AI::TetrisNodeEx == Search::NodeEx)`.
- 整理注释.

## 3. 与 v1 (H1/H2/H3) 对应关系
| v1 | v2 |
|---|---|
| H1 (trait piece_needs_spin) | 废弃, 抽象冗余 |
| H2 (LandingPos 模板化) | = commit 1 + commit 2 |
| H3 (search_aspin hook 化) | = commit 3 |

v2 比 v1 干净, 因为认了"hook 是模板", 不再纠结 trait/dispatch 两层.

## 4. 待确认 (动手前最后 3 个问题)

### 待确认 1: NodeEx 不匹配时的语义
- 选项 A: 严格 `static_assert` 编译失败.
- 选项 B: 静默回退 NoHook (用户原话字面对应这个).
- 我的建议: A. 因为 B 容易掩盖配置错误, 比如把 ai_zzz::TOJ 接到 ASpinHook 上, AI 收到的 NodeEx 退化成只有 (TetrisNode*), 那 AI 的 eval 函数读 spin/is_ready 字段会**编译失败**而不是静默退化 — 因为 AI 端 typedef 写的就是 spin payload 类型.
- 等价命题: 选 B 等价于"AI 必须写两套 eval, 一套读 spin, 一套不读". 这违反 SRP.
- 等你拍.

### 待确认 2: commit 拆分粒度
- 4 个 commit (推荐) — 每 commit 都跑 oracle_diff.
- 1 个大 commit — 高风险, 出事难二分.
- 等你拍.

### 待确认 3: search_aspin 这次是否真做
- A. 做. 跟 search_tspin 一起进 hook 体系, 一致性最好.
- B. 不做. 本次只做 search_tspin 侧 (commit 1+2+4), search_aspin 留给后续会话, ASpinHook 作为 placeholder 文档存在.
- 我的建议: B. 因为 ASpin 没 oracle baseline, 一旦回归不可见, 静默引入 bug 极麻烦. ASpin 可以独立做.
- 等你拍.
