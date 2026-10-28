# eval/get 框架升级方案（修订版：Config-aware eval + 按需 get）

## 0. 状态与边界（2026-10-28 更新）

- **状态（2026-10-29，full Debug build rc0）**：框架头 `src/tetris_eval_get.h` 已落地，全部 15 个 src AI 已迁到新契约；换位剪枝已实际全删除，无剪枝重复展开由独立 repeats-500 测试覆盖。engine 组 3+5+7 全 pass；逐 AI 数值对拍 A 384 字段、B 52、D 扩大（misaka 121500 / ax 18000 / farter 3600）、C golden 独立注册 PASS（241 行 golden，240 cases / 2460 obs matched，其余字段 bit-exact；旧 tag 边界 UB 不语义承诺）；core counter/wrap/tree scale 80→160 测试 pass。full Debug rc0 最终 41 tests / 40 pass / 1 fail（唯一 pre-existing cultris_safe_regression 旧 BPM-generation；41 = 37 原 + negative + 两 failure-API 各 46 + Config v5 23）。已部署生产身份 code 3770535e / 四库 36c6febe（1843728，name C2 ZZZ / 3770535e346f-dirty，8.4.0-generation-10ms）；8 strict 两轮 load OK、12 Native 专项补正后 fresh 12/12 PASS（初次增量非 fresh，已强制 rebuild 重跑）。multi-widen 固定 64 步 scored 30914 / hits 65 vs 0。不声称 ASan dataRace 洁净、全搜索 path 等价、wall-ms 强度、最优桶数、perf/GUI/TSan。旧 4b2/34f546 为历史身份。明细见 `docs/eval_get_validation.html` 与主项目报告。
- 源码审计基线为 `dev@7af0537a5f4da7490b95019c36d90fcdc93885cb`；oracle 冻结，已有 `src/ai_ax.h` 未提交内容原样保留。
- **契约（已落地）**：eval 响应 Config；Config 改变时按 `generation` 纪元使 eval 结果集失效；固定容量、单桶直接映射 hash 表，无冲突链、无开放寻址；get 参数因 AI 而异，Engine 按 `GetArgs` 类型表模板按需萃取，不计算未列出的数据。
- 早期草案已淘汰：eval 禁止读取 Config、强制全部权重移 get、统一全量 TransitionContext / 逐节点旧配置 EvalSnapshot、以及"父 map 各 lane + lp_key"缓存键。附录保留源码依赖取证，迁移节以本版为准。
- 迁移目标是**固定 Config 下评分、状态传播、运算顺序与搜索逻辑不变**。Config 改变后统一使旧结果失效，是新生命周期语义，不再兼容旧的"树节点混用旧 eval / 新 get 配置"现象。

## 1. eval 与 get 的新契约

### 1.1 eval(map)：Config 固定期内的盘面函数

```cpp
// 已落地于 src/tetris_eval_get.h 与各 AI；Map 来自 RuleSpec。
EvalResult eval(Map const& map) const;
```

- 显式参数只有落块并消行后的 `map`；允许通过 AI 已绑定的 `Config const&/const*` 读取当前配置。
- 数学语义为 `E = f<AI, RuleSpec>(map, config)`。相同 AI/规则、同一 Config 版本、相同盘面得到相同结果，与到达路径无关。
- 可保留配置加权后的 value/map/attack、Config 选路扫描、IO spin_slots 规则等。**不要求结果仅为原始特征**，不把本来能缓存的工作迁到 get 逐节点重复算。
- 不能读取本次落块、clear、spin、父 Status、src_map、深度、hold/next；不能写外部状态或计时/随机；Config 指向的 eval 依赖数据在固定期内也必须不可变。
- 模板仅依赖 AI/RuleSpec/Map 类型，不依赖落块 T/R；get 的 typed landing 桥继续保留原 T/R 几何语义。
- 各 AI 的 EvalResult 自由定义，可为结构体或标量。Dig 保留 `double`；enemy/farter 没有可独立盘面计算的部分时，可用空类型作为 EvalResult，不强行发明评分。

### 1.2 get：AI 定义参数类型表，Engine 可变参展开

AI 自己声明 `GetArgs = type_list<...>`，这是萃取需求的唯一事实来源；不再靠 get 参数个数猜来源，也不尝试对任意泛型 get 做函数签名反射。参数表中的类型表达数据语义，Engine 按类型调用 provider、以可变参展开传给 get：

```cpp
// 已落地：args 类型表达来源，不是全量 context；具体 provider 见 docs/eval_get_framework.md。
using GetArgs = type_list<args::Eval<EvalResult>, args::Clear,
                          args::Parent<Status>>;

// AI 可用具体参数实现，或用受 GetArgs 约束的模板 get。
// 框架的可变参 dispatcher：
template<class... P>
Status invoke_get(type_list<P...>, /* engine-owned call source */);
// 准备所需 providers 后：ai.get(extracted<P>()...)
```

- `get` 返回各 AI 自己的 Status；参数表允许长度不同、类型不同，含空表，数据按表顺序传入。
- 重复来源类型只萃取一次，但可在声明位置重复传入；共享昂贵依赖只计算一次。
- 原 get 的累积、比较、iterate、副作用和运算顺序保留；缓存命中不减少 get/scored 调用。
- `Clear`、`Depth` 等不能用相同裸整数类型表述来源；按语义包装类型区别。引用只在当前调用生命周期内有效，不指向可覆盖 bucket。
- 框架对参数表与 get 的可调用性、返回类型做 static_assert 校验；不存在两份独立的需求清单。

## 2. 单桶直接映射大 hash 表

### 2.1 所有权、容量与 entry

- 每个 `TetrisEngine<Rule, AI, Search>` 的 Context 一张表，不跨 AI/RuleSpec/Engine 共享；在 prepare 时一次分配，之后容量固定、不 rehash、不按负载扩容。
- 预设 `bucket_count` 为 2 的幂且非零，支持构建默认值或 prepare 前 EngineOptions 配置，运行中不得改变。建议起始候选 `2^16` 桶，**这是待测设计默认，不是已测最优值**；实施时按 `sizeof(Entry)`、典型对局命中率与内存预算确定最终默认，不改变单桶替换算法。
- 每桶只存一个 `{valid, hash, map_key, EvalResult}`。结果可持固定数组；非平凡类型须正确构造、析构、替换，禁止以 memset 清非平凡对象。
- 盘面键含所有有效棋盘数据；第一阶段沿现有 raw lane 表示做精确相等，保持原盘面语义。无效位归一化只作单独优化，先验证不会改 eval 输出。
- `hash` 仅用于定位/快速排除，不能作为正确性的唯一证据：即使完整 hash 碰撞，仍须比较 map_key 的全部 lane。单桶精确比较不需要冲突链。
- 节点可保留 EvalResult 的按值副本和 eval_generation，**不得持可被其它落点覆盖的 bucket 引用**。

### 2.2 唯一查询流程

```cpp
// 伪代码：结果按值交给节点，bucket 不是节点的长期引用。
auto lookup_eval(Map const& map) {
    const auto hash = hash_map(map);
    auto& bucket = table[hash & (bucket_count - 1)];
    if (bucket.valid && bucket.hash == hash && equal_map(bucket.map_key, map)) {
        ++stats.hits;
        return bucket.result;
    }
    ++stats.misses;
    auto result = ai.eval(map);   // Config 已绑定，固定期内不变
    bucket.replace(hash, map, result);
    return result;
}
```

未命中就在此桶当场计算并覆盖；没有链表、邻桶 probing、开放地址、LRU、多候选 eviction 或后台清理。其它盘面覆盖旧 entry 只影响命中率，不能影响评分正确性。空桶显式 valid，空棋盘是正常键，不以 hash=0 代表空槽。

### 2.3 缓存维护

- prepare / AI init / Rule 重建：清空有效项并建立新的 Config 固定期。
- Config 明确改变（包括只影响 get 的配置，初版按用户要求保守全清）：清空整张表的有效项；大表可独立清 validity bitmap，不必重分配，也不必销毁仍可复用的存储。
- 普通状态、next/hold、搜索深度、危险队列变化不自动等于 Config 改变；走原评分更新/树复用语义。**凡该 AI eval 实际读取并放入 Config 的值变化，都必须走 Config 改变入口。** C2 danger/BPM 是否当前属于 Config 按源码保持，不能为了避免清表偷偷改数据归属。
- 度量记录 eval requests/hits/misses/replacements、实际 eval 次数、scored/get 次数和内存；统计不参与选择或 tie-break。

## 3. Config 固定期与全结果失效

### 3.1 显式 Config 变更 API（推荐落地契约）

```cpp
Config const& ai_config() const;             // 只读访问
void set_ai_config(Config const& next);       // 显式安装新配置并失效结果
void notify_ai_config_changed();              // 已绑定的间接依赖被外部改变时显式通知
void update();                               // 现有普通 root/status 重评分，不等同清 eval 表
```

- `set_ai_config` / `notify_ai_config_changed` 每次调用都视为配置边界，绑定/准备新配置后清 eval 集、提升 generation、使旧评分失效；不偷偷通过 memcmp 或指针地址判断“没有改变”。未改变就由调用方不调用，初次安装与运行期变更统一契约。
- 不再暴露搜索中的裸可写 `ai_config()`；旧“拿指针改字段后 update”调用方迁成：从只读配置复制到局部、集中修改、一次 `set_ai_config`。仅清评分的原 `update()` 保留独立语义。
- 同地址的权重表/规则依赖内容被修改也必须显式通知；Config 指向的 eval 数据在两个变更调用之间不可变、且寿命覆盖搜索。通知必须在受保护的编辑事务中，不能先在运行中的后台搜索旁无锁改指向内存，再补通知。
- Config storage 由 Engine 稳定持有；AI 的 Config 引用/指针在显式更新后必须有效。配置依赖的 AI 辅助表按显式入口更新，无需每帧 init。无 Config 的 AI 用空配置策略，不虚构快照。
- enemy get 通过指针写穿的外部累计输出不是 Config 变更；该输出不属于 eval 输入，不触发逐节点清表循环。
- ThreadEngine 的显式 setter/通知在 PauseBackground 下完成；Cultris 的实际入口在 engine_mutex 下完成。一次入口统一安装配置、清表、作废旧结果与聚合，再恢复搜索，不允许半更新状态。
- AI 配置与 Search 配置区分：搜索规则改变必须刷新落点/路径相关缓存；不把 search_config 的变化仅当 eval 清表处理。同步变更可在同一互斥事务完成，仍保持不同失效职责。

### 3.2 清表不够，树副本也要失效

旧代码把 eval 结果保存在 `child->result`（`tetris_core.h:1088`），rescan 可复用旧 child（:1128），评分版本更新只重跑 get（:1519–1547）。因此 Config 改变不能只清 bucket 而继续把旧节点结果送进新 get。

推荐**一张缓存 + 一个引擎级 eval_generation + 条件性的节点 generation**：

1. Config 改变：清 bucket.valid，提升 eval_generation，并沿既有 engine.update 路径作废所有依赖旧评分的 frontier / status 聚合。
2. 从宿主真实 root 状态出发，沿父→子顺序更新依赖评分；status_raw/status、虚拟分支 iterate 聚合都不能遗留旧配置分数，也不能伪造 root。下次需要 args::Eval 时：若节点 eval_generation 不匹配，使用当前节点 map 走 lookup_eval；更新该节点结果副本与 generation，再调用 get。
3. get 不需要 Eval 的 AI，不为其重复计算无用 eval，也不生成额外的 eval 副本字段。
4. 同 generation 下沿用原节点结果/树复用行为，不增加无条件刷新或 reset。
5. generation 溢出走明确全失效处理，不可让极旧节点被误判当前；实现可以用 uint64 并覆盖测试。

这不需要每节点保存旧 Config，也不需要旧 EvalSnapshot/current_epoch 双轨、投影 memo 或第三个加权函数。新 Config 只对应一套有效结果；旧副本在逻辑上全失效，按需重算避免同步重扫整棵树。

## 4. C++ 模板按 get 参数需求萃取

### 4.1 单一事实来源：AI 的参数类型表

- 每个 AI 定义 `GetArgs = type_list<P...>`；GetTraits 萃取这个类型包，Engine 以可变参展开调用 provider 和 get，而不是对 get 签名做 arity 分派。
- 有 typed Landing 时，参数表可按 Spec/T/R 定义对应模板别名；编译期 dispatch 选中类型表后统一展开。eval 仍不带落块 T/R。
- parameter provider 的 type→source 映射唯一，未知参数类型、缺 provider、返回类型错误或 get 不可调用时给出 static_assert 错误，不静默忽略参数。
- AI 可实现具体 get，或实现受参数表约束的模板 get；框架只保证按所声明类型表调用，不支持含歧义的任意重载自动择参。Status 可声明，或从所选参数包 get 调用的返回类型推导。
- Config 不自动成为参数包的额外隐藏字段：AI 已绑定 const Config；若 AI 明确声明配置参数类型，其 provider 提供同一当前 Config，不构造另一版本。

### 4.2 ParameterProvider 与依赖闭包

每个受支持参数类型有一个模板 provider，例如：

- `args::Eval<E>`：节点结果引用，必要时 lookup_eval，唯一 map+Config 缓存入口。
- `args::Parent<S>`：父节点原始 Status，不强制要求所有 AI 有嵌套 Status。
- `args::Depth`：保留旧传入的 parent.level / depth 口径，不重新定义起算。
- `args::Clear`：attach 已返回的消行数，不再扫描；只有实际 get 需要时才在节点存储。
- 序列参数细粒度标签 `args::NextView` / `NextLength` / `NextNode` / `Hold` / `IsHold`：按旧 env 五字段各自来源萃取，只有请求 NextView 才形成队列视图。`args::Env` 可作为旧适配的全字段便利请求，但新迁移优先按实际使用拆分；例如 C2 只请求 NextNode/NextLength，不为它构造 next 指针、hold 或扫描队列。
- `args::Landing<L>`：沿原 T/R 桥构造 typed 几何和 spin；仅 Map-only AI 不实例化此桥。
- `args::MapBefore<M>` / `MapAfter<M>`：借用父/子 map，不复制或为了包装计算新字段。
- AI 专属参数（C2 源盘洞口径、软降几何、IO 消行行位图等）：放专属 provider/adapter，声明其必要源依赖；通用 Engine 不硬编码 C2/IO 评分公式。

编译期对 GetArgs 类型包取依赖闭包并去重，`if constexpr` 只实例化所需 provider；序列长度采用既有索引差/O(1)来源，不为获取长度去展开内容。共享的中间量在**本次调用局部的 typed storage** 中只计算一次；不要构造全量 context、tuple 的全字段再丢掉无用项，也不要靠运行时 bool 掩饰仍已执行的扫描。

对于几何常量、map 引用等廉价取值可直接传递；昂贵派生必须受静态需求控制。GetArgs 声明未使用的参数就会造成框架请求它，因此迁移时根据源码从 GetArgs 去掉真死参，而不是保留所有参数“以防未来”。provider 计算次序由计划固定，不能依赖 C++ 函数实参求值未指定顺序；先按显式顺序构造所需参数，再调用 get。get 原有写穿只发生于 get，不放到 provider 中。

### 4.3 条件节点存储

结构通过 partial specialization / `[[no_unique_address]]` 空 holder 按 GetTraits 裁剪：

- 仅需要 Clear 的 AI 才保存 clear；clear 无请求时仍必须执行实际 attach 消行，但丢弃返回数即可。
- 仅需要 Eval 的 AI 才保存结果/generation；只需要 Parent/Depth 的 AI 无此 holder。
- map/identity 等本来搜索树必需的数据不删；“get 不需要”不等于“movegen/search 也不需要”。
- 块型/span/旋转 spin 的搜索 hook 由独立 SearchSpinType/Policy trait 驱动，不再从 eval(node,...) 的旧签名推断。需求裁剪不得删除实际生成落点/复原路径需要的 spin 分类。

## 5. 全部 15 个 AI 的迁移映射

以下需求是按当前源码的迁移建议，具体 GetArgs 类型包以实现阶段逐项对拍为准；保留各 AI 的 Status/iterate/comparator，不统一成一种打分结构。源码行锚见附录。

| AI | 留在 Config-aware eval(map) | get 必需参数 / 移出项 |
|---|---|---|
| qq::Attack | 原盘面扫描、Config.mode 选 low_x/AttackDepth/Danger、原 map 加权；保留必要盘面指标供混合表达式同序合成 | Eval、Landing、Clear、Parent、Depth；落高/Middle/DeadZone 与 clear 项移 get，不构造 Env/src_map |
| Dig | 原加权 eval 整体保留，Config.p[] 内层累积顺序不变，EvalResult 仍 double | 仅 Eval；恒等 get 不要求 Parent/Depth/Env/Clear，无 raw event 或权重搬迁 |
| TOJ_PC | 原 value/roof | Eval、Clear、spin 类型、Parent；去掉未读 depth/env/src_map，tspin_for_clear 在 get 原位置重放 |
| Botris_PC | 原 value/roof | Eval、Clear、ASpin 类、Parent；不把 ASpin 改成 TSpin 转换规则 |
| TOJ_v08 | 原 value/count/roof/safe_cache/t2/t3 | Eval、Clear、Landing(row/spin)、Parent、Depth、Env；top_out 和本次 spin/clear 移 get |
| Botris | 原 value/count/roof/safe_cache | Eval、Clear、Landing(row/spin)、Parent、Depth、Env；保留累积 clear，无额外 T2/T3 扫描 |
| TOJ | 原 Config.param 加权、count/roof/t2/t3/safe_cache/piece-independent 数据 | Eval、Clear、Landing(row/type/spin)、Parent、Env；删未使用 depth/src_map，保留原 get Config 计算 |
| C2 | 原 Config.p[0..23]/p_rate/mode 的盘面加权与内层累积、fill(safe)、cells、WideWell 等，以及源盘计算所需按 map 定义的洞/列顶指标 | Eval、Landing、Clear、MapBefore（或同口径源盘 Eval）、Parent、NextNode/NextLength；原 depth 未使用，不请求；hole/new_hole/soft_drop 移 get/provider；combo DP 仍只影响返回 continuation |
| IO | 原 Config.param 盘面评分、Config.rules 下 spin_slots、spawn_blocked/cells/roof 等 | Eval、Clear、Landing(type/spin/row)、MapBefore、Parent、Depth、Env；cleared_rows/all_clear/lock_out 移 get/provider；pending/exchange 顺序不变 |
| the_ai_games_old | 原配置无关 map 加权及 safe_cache/full/count/low_y/tilt | Eval、Landing、Clear、Parent、Depth；保留 building/combo 与整数除法、root up 原口径 |
| the_ai_games | 原 Config 八项 map 加权、map_low/tbuild/roof/safe_cache | Eval、Landing、Clear、Parent、Depth；原 get 六项攻击权重保留，不请求 Env/src_map |
| the_ai_games_enemy | 无必要独立盘面结果，可为空类型 | Clear、spin、Parent、Depth；无需 Eval/MapBefore/Env；get 指针写穿次数/条件与原顺序不变 |
| misaka | 原 raw map_rows[22] 拷贝及 map_count | Eval、Clear、Landing(type/spin)、Parent、Env；保留 row 原始极性与 get VirtualRow 翻转组合，无 MapBefore |
| ai_ax::AI | 原 map 加权/危险度 | Eval、Landing、Clear、Parent、Depth；保留 lp.y+1，不替换为 land_height，保持已有未提交修复 |
| ai_farteryhr::AI | 无必要纯 post-map 工作，可为空类型 | MapBefore、Landing、Parent、Depth；原 FarteryhrMap(src_map+lp) 及评分整体进 get，clear/Eval/Env 不请求 |

混合浮点表达式（例如 Attack 把落块与 BoardDeadZone 放同一表达式）不能先拆成两个加权总和再相加；eval 可输出必要的中间量，由 get 按原表达式原位置合成。板面固定加权不必全部挪走。迁移要逐 AI 选择最小必要数据，不为了“统一特征”使结果变大。

## 6. 验收与实施阶段

1. **P1 接口与迁移**：Config-aware eval(map)，参数语义标签、GetArgs 可变类型包/providers、按需节点字段；固定 Config 与 HEAD 逐位对拍。先保留当前搜索策略，不混入剪枝删除。
2. **P2 直接映射缓存**：预设容量、单 bucket overwrite、full equality、valid 管理；cache 开关对拍；重复到达同 map 共享 Eval，不共享 get/Status。
3. **P3 Config 生命周期**：清表 + 全树旧 eval 逻辑失效 + 评分版本更新，线程互斥；检测同地址依赖数据变化通知、未调用配置变更入口时不清表、generation 溢出。
4. **P4 单独删除同深度换位剪枝**：属于行为变更，io_sim 配对种子/交换座位以 think_ms 测强度，并报告 scored/eval 统计。

### 必测矩阵

- 固定 Config：全部 AI 旧/new eval 盘面部分与 get 最终 Status 逐字段/逐位一致，尤其 Dig 原循环、Attack混合表达式、C2 root/当前块/返回continuation、enemy 条件写穿。
- 同 map 不同 hold/next/lp/clear/spin：共享相同 EvalResult，但 get 保留路径差异；相同 hash/同 bucket 不同 map 必须重算并覆盖，空棋盘不能当 invalid。
- Config 变更：旧 bucket 全失效；已存在节点在第一次再次请求 Eval 时重算；旧 frontier/聚合不被当作新分数。与“当前 Config 直接 eval + 同样 get”对照，**不是与旧混配置现象对齐**。
- 按需萃取：为不需要的数据 provider 加调用计数，必须为零；不请求 Env 的 AI 不扫 next；两参数共享一个昂贵依赖只计算一次；编译测试证明禁用 provider 不实例化。
- 全 AI contract 显式实例化，覆盖模板 get 选中特化、无 Config/标量 Status/空 Eval、GetArgs 可调用性/空参数表/模板别名；不把 include 当模板覆盖。
- 固定 get/scored 预算、确定性调度下 cache on/off 目标/path/Status 一致；think_ms 下执行量可不同，只用于性能/强度，不设逐位轨迹门槛。
- C2 已有 cells 真实 eval 断言（test/c2_combo_policy_test.cpp:54–62），主要形状与 soft_drop 补专用 golden；其它测试缺口见附录。
- 内存统计包括 cache 数组、实际桶 sizeof、节点条件字段；桶覆盖不产生悬空引用，Config 修改与搜索互斥。

## 7. 当前产物与仍待验证项

- 15 AI 契约迁移与引擎接线已完成并过当前验证（见状态行与 `docs/eval_get_validation.html`）。
- 待测 Open：最优桶数（命中率/内存）、wall-ms 强度、缓存 on/off 全搜索 path 等价、完整对局/GUI/perf/TSan；基线预存 safe-BPM old-generation 唯一 fail；上层 App 历史误覆盖（备份已匹配、用户本轮明确不恢复、对上层零额外写入）。
- 未提交、未推送、未部署。

## 8. 配套资料

`framework_audit_materials/` 四文件保留源码基线、依赖、运算顺序和测试缺口。各文件的新迁移节已按本轮 Config-aware 契约修订，不再包含 EvalSnapshot、强制 raw-features-only、Config 不失效或统一全量 get 参数的旧设计。

## 9. 清晰程度与未完成项（本轮回答）

**架构目标、数据职责与失效规则已清楚，没有需要用户补齐才能继续设计的阻塞信息。** 参数表采用可变参、按用户声明的参数类型萃取；Config 变更独立显式调用，不依赖猜测。

仍需实施验证的事项，不是新的业务选择：
- hash 表默认桶数与内存/命中率测量；容量固定、单桶覆盖的算法已定。
- Config 间接依赖的寿命与调用方迁移清单，后台修改互斥、旧树 generation 检查与前后评分顺序。
- 全 AI GetArgs/providers 的精确类型、细粒度序列字段和特殊叠块输入，并验证无用 provider 调用为零。
- 固定 Config 的旧/new 逐位回归（A 组已过单元级；其余 AI 与 full-engine 待最终结果）、显式 Config 更新后的新结果验证、已知遗留测试缺口。

文档已随实施更新；代码迁移、提交、部署以最终测试数据为准。
