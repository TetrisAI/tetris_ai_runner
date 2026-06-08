// policy_deduction_test: 编译期验证 DeduceSpinPolicy + TetrisEngine2 推导链.
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
//   5. TetrisEngine2<Rule, TOJ, tspin::Search> == TetrisEngine + manual rebind_policy

#include "../src/tetris_core.h"
#include "../src/ai_zzz.h"
#include "../src/movegen_hook.h"
#include "../src/rule_srs.h"
#include "../src/search_path.h"
#include "../src/search_tspin.h"
#include "../src/tetris_engine2.h"

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

// ai_zzz::qq::Attack: eval 只接受 BBNode<Details, std::monostate>，无 TSpinType / ASpinType
// → DeduceSpinPolicy 应推导出 ActiveNonePolicy
static_assert(std::is_same_v<DeduceSpinPolicy<ai_zzz::qq::Attack>, ActiveNonePolicy>,
    "ai_zzz::qq::Attack (BBNode<Details,monostate> only) must deduce ActiveNonePolicy");

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

// ─── 5. TetrisEngine2 实例化 (类型可见性验证) ─────────────────────────────────

using E2_TOJ     = TetrisEngine2<rule_srs::TetrisRule, ai_zzz::TOJ,        tspin::Search>;
using E2_NoSpin  = TetrisEngine2<rule_srs::TetrisRule, ai_zzz::qq::Attack,  path::Search>;

// TetrisEngine2<..., TOJ, tspin::Search> 与手工注入等价:
// TetrisEngine<Rule, TOJ, tspin::SearchWith<>::rebind_policy<ActiveTOnlyPolicy>>
using E2_Manual  = TetrisEngine<
    rule_srs::TetrisRule,
    ai_zzz::TOJ,
    tspin::SearchWith<>::rebind_policy<ActiveTOnlyPolicy>>;

static_assert(std::is_same_v<E2_TOJ, E2_Manual>,
    "TetrisEngine2<Rule, TOJ, tspin::Search> must equal manual rebind_policy injection");

// ─── main (dummy, never called) ──────────────────────────────────────────────

int main() { return 0; }
