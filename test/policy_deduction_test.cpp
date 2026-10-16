// policy_deduction_test: 编译期验证 DeduceSpinPolicy + TetrisEngine 推导链.
//
// 本文件只做 static_assert, 不运行任何运行时逻辑.
// 通过编译 = 验证通过; 编译失败 = 推导逻辑有误.
//
// 验证点:
//   1. TSpinType AI (TOJ / TOJ_v08)         → ActiveTOnlyPolicy
//      ASpinType AI (Botris / Botris_PC)    → ActiveAllPolicy
//      NoSpin AI    (ai_zzz::qq::Attack)    → ActiveNonePolicy
//   2. TSpinHook::active_for_piece 双参: hook 内部始终限于 T 件
//      (T==T) && Policy::value<T>, 故 I + ActiveAllPolicy 仍为 false
//   3. TSpinHook::active_for_piece 单参向后兼容
//   4. NoSpinHook::active_for_piece Policy 参数下永远 false
//   5. TetrisEngine<Rule, TOJ, tspin::Search> == TetrisEngine + manual rebind_policy
//   6. EvalSpinType == search_amini::Search::AMiniType → ActiveAllPolicy (no AI alias needed);
//      AMiniHook 对所有 piece 激活, amini::Search 的 rebind_policy 透传到 Searcher

#include "../src/tetris_core.h"
#include "../src/ai_zzz.h"
#include "../src/movegen_hook.h"
#include "../src/rule_io.h"
#include "../src/rule_srs.h"
#include "../src/search_amini.h"
#include "../src/search_path.h"
#include "../src/search_tspin.h"

#include <type_traits>

using namespace m_tetris2;

// ─── 1. DeduceSpinPolicy 推导结果 ────────────────────────────────────────────

static_assert(std::is_same_v<DeduceSpinPolicy<ai_zzz::TOJ>,      ActiveTOnlyPolicy>,
    "TOJ (TetrisNodeWithTSpinType) must deduce ActiveTOnlyPolicy");

static_assert(std::is_same_v<DeduceSpinPolicy<ai_zzz::TOJ_v08>,  ActiveTOnlyPolicy>,
    "TOJ_v08 must deduce ActiveTOnlyPolicy");

// Botris / Botris_PC 用 TetrisNodeWithASpinType → ActiveAllPolicy
static_assert(std::is_same_v<DeduceSpinPolicy<ai_zzz::Botris>,    ActiveAllPolicy>,
    "Botris (TetrisNodeWithASpinType) must deduce ActiveAllPolicy");

static_assert(std::is_same_v<DeduceSpinPolicy<ai_zzz::Botris_PC>, ActiveAllPolicy>,
    "Botris_PC (TetrisNodeWithASpinType) must deduce ActiveAllPolicy");

// ai_zzz::qq::Attack: eval 只接受 TetrisNode<Details, std::monostate>，无 TSpinType / ASpinType
// → DeduceSpinPolicy 应推导出 ActiveNonePolicy
static_assert(std::is_same_v<DeduceSpinPolicy<ai_zzz::qq::Attack>, ActiveNonePolicy>,
    "ai_zzz::qq::Attack (TetrisNode<Details,monostate> only) must deduce ActiveNonePolicy");

// 只声明 EvalSpinType 的 AI: AMiniType 自带 SpinTypePolicy 特化.
struct AMiniAI
{
    using EvalSpinType = search_amini::Search::AMiniType;
};
static_assert(std::is_same_v<DeduceSpinPolicy<AMiniAI>, ActiveAllPolicy>,
              "EvalSpinType == AMiniType must deduce ActiveAllPolicy");
static_assert(AMiniHook::active_for_piece<'T', ActiveAllPolicy> && AMiniHook::active_for_piece<'I', ActiveAllPolicy> &&
                  AMiniHook::active_for_piece<'O', ActiveAllPolicy>,
              "AMiniHook + ActiveAllPolicy: every piece is active");
static_assert(!AMiniHook::active_for_piece<'S', ActiveNonePolicy>,
              "AMiniHook + ActiveNonePolicy: no piece is active");
static_assert(std::is_same_v<amini::Search::rebind_policy<ActiveAllPolicy>::type<rule_io::TetrisRule>,
                             movegen::Searcher<movegen::AMiniStrategy, AMiniHook, rule_io::TetrisRule::rule_spec,
                                               ActiveAllPolicy>>,
              "amini::Search::rebind_policy must reach the Searcher's Policy");

// ─── 2. TSpinHook::active_for_piece 双参行为 ─────────────────────────────────

// ActiveTOnlyPolicy: T 件激活, 非 T 件不激活
static_assert( TSpinHook::active_for_piece<'T', ActiveTOnlyPolicy>,
    "TSpinHook + ActiveTOnlyPolicy: T must be active");
static_assert(!TSpinHook::active_for_piece<'I', ActiveTOnlyPolicy>,
    "TSpinHook + ActiveTOnlyPolicy: I must NOT be active");
static_assert(!TSpinHook::active_for_piece<'S', ActiveTOnlyPolicy>,
    "TSpinHook + ActiveTOnlyPolicy: S must NOT be active");

// ActiveAllPolicy: TSpinHook 双参逻辑 = (T == 'T') && Policy::value<T>
// 故 T 件激活, 非 T 件即使 Policy::value == true 也不激活 (hook 内部限 T)
static_assert( TSpinHook::active_for_piece<'T', ActiveAllPolicy>,
    "TSpinHook + ActiveAllPolicy: T must be active");
static_assert(!TSpinHook::active_for_piece<'I', ActiveAllPolicy>,
    "TSpinHook + ActiveAllPolicy: I must NOT be active (hook limits to T)");

// ActiveNonePolicy: 无论什么件都不激活
static_assert(!TSpinHook::active_for_piece<'T', ActiveNonePolicy>,
    "TSpinHook + ActiveNonePolicy: T must NOT be active");

// ─── 3. TSpinHook::active_for_piece 单参向后兼容 ─────────────────────────────

static_assert( TSpinHook::active_for_piece<'T'>,
    "TSpinHook single-param: T must be active (backward compat)");
static_assert(!TSpinHook::active_for_piece<'I'>,
    "TSpinHook single-param: I must NOT be active (backward compat)");

// ─── 4. NoSpinHook::active_for_piece: Policy 参数下永远 false ────────────────

static_assert(!NoSpinHook::active_for_piece<'T', ActiveAllPolicy>,
    "NoSpinHook + ActiveAllPolicy: must remain false");
static_assert(!NoSpinHook::active_for_piece<'T', ActiveTOnlyPolicy>,
    "NoSpinHook + ActiveTOnlyPolicy: must remain false");
static_assert(!NoSpinHook::active_for_piece<'T'>,
    "NoSpinHook single-param: must remain false (backward compat)");

// ─── 5. TetrisEngine 自动注入结果验证 ─────────────────────────────────────────

using E2_TOJ        = TetrisEngine<rule_srs::TetrisRule, ai_zzz::TOJ,       tspin::Search>;
using E2_NoSpin     = TetrisEngine<rule_srs::TetrisRule, ai_zzz::qq::Attack, path::Search>;
using E2_TOJ_Search = typename E2_TOJ::TetrisSearch;

// TetrisEngine<..., TOJ, tspin::Search> 解析后的实际 Search 类型应等价于手工注入:
// tspin::SearchWith<>::rebind_policy<ActiveTOnlyPolicy>
using E2_ManualSearch = typename TetrisEngine<
    rule_srs::TetrisRule,
    ai_zzz::TOJ,
    tspin::SearchWith<>::rebind_policy<ActiveTOnlyPolicy>>::TetrisSearch;

static_assert(std::is_same_v<E2_TOJ_Search, E2_ManualSearch>,
    "TetrisEngine<Rule, TOJ, tspin::Search> must resolve to the same concrete search type as manual rebind_policy injection");

// ─── main (dummy, never called) ──────────────────────────────────────────────

int main() { return 0; }
