#pragma once

#include <type_traits>

namespace m_tetris2
{
    struct ActiveAllPolicy
    {
        template<char T>
        static constexpr bool value = true;
    };

    struct ActiveNonePolicy
    {
        template<char T>
        static constexpr bool value = false;
    };

    struct ActiveTOnlyPolicy
    {
        template<char T>
        static constexpr bool value = (T == 'T');
    };

    // A spin enum may select its policy itself by specialising this next to the enum
    // (search_amini.h: AMiniType -> ActiveAllPolicy). void = deduce from AI::TSpinType / AI::ASpinType.
    template<class SpinT>
    struct SpinTypePolicy
    {
        using type = void;
    };

    namespace detail
    {
        template<class AI, class = void>
        struct has_eval_spin_type : std::false_type
        {
        };

        template<class AI>
        struct has_eval_spin_type<AI, std::void_t<typename AI::EvalSpinType>> : std::true_type
        {
        };

        template<class AI, class = void>
        struct has_t_spin_type : std::false_type
        {
        };

        template<class AI>
        struct has_t_spin_type<AI, std::void_t<typename AI::TSpinType>> : std::true_type
        {
        };

        template<class AI, class = void>
        struct has_a_spin_type : std::false_type
        {
        };

        template<class AI>
        struct has_a_spin_type<AI, std::void_t<typename AI::ASpinType>> : std::true_type
        {
        };

        template<class AI, class SpinT, class = void>
        struct match_t_spin_type : std::false_type
        {
        };

        template<class AI, class SpinT>
        struct match_t_spin_type<AI, SpinT, std::void_t<typename AI::TSpinType>>
            : std::bool_constant<std::is_same_v<SpinT, typename AI::TSpinType>>
        {
        };

        template<class AI, class SpinT, class = void>
        struct match_a_spin_type : std::false_type
        {
        };

        template<class AI, class SpinT>
        struct match_a_spin_type<AI, SpinT, std::void_t<typename AI::ASpinType>>
            : std::bool_constant<std::is_same_v<SpinT, typename AI::ASpinType>>
        {
        };

        template<class AI, bool HasSpin>
        struct DeduceSpinPolicyImpl
        {
            using type = ActiveNonePolicy;
        };

        template<class AI>
        struct DeduceSpinPolicyImpl<AI, true>
        {
        private:
            using SpinT = typename AI::EvalSpinType;
            using OwnPolicy = typename SpinTypePolicy<SpinT>::type;

        public:
            using type = std::conditional_t<
                !std::is_void_v<OwnPolicy>,
                OwnPolicy,
                std::conditional_t<
                    match_t_spin_type<AI, SpinT>::value,
                    ActiveTOnlyPolicy,
                    std::conditional_t<
                        match_a_spin_type<AI, SpinT>::value,
                        ActiveAllPolicy,
                        ActiveNonePolicy>>>;
        };
    } // namespace detail

    template<class AI>
    using DeduceSpinPolicy = typename detail::DeduceSpinPolicyImpl<AI, detail::has_eval_spin_type<AI>::value>::type;
} // namespace m_tetris2
