#pragma once

// TETR.IO Season 2 game rules (attack, B2B surge, garbage queue, randomizer) as pure, allocation-free logic.
// Formulas and order of operations follow .research/tetrio-s2-rules/rules.md §3, §7, §8 and §9.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace io_rules
{
    enum class Spin : std::uint8_t
    {
        None,
        Mini,
        Full
    };

    enum class GarbageBlocking : std::uint8_t
    {
        ComboBlocking,
        LimitedBlocking,
        None
    };
    enum class Passthrough : std::uint8_t
    {
        Zero,
        Limited,
        Consistent,
        Full
    };
    enum class ComboTable : std::uint8_t
    {
        None,
        Multiplier,
        ClassicGuideline,
        ModernGuideline
    };
    enum class RoundMode : std::uint8_t
    {
        Down,
        Rng
    };
    enum class SpinBonuses : std::uint8_t
    {
        TSpins,
        TSpinsPlus,
        All,
        AllPlus,
        AllMini,
        AllMiniPlus,
        MiniOnly,
        Handheld,
        Stupid,
        None
    };

    namespace detail
    {
        template<class E, std::size_t N>
        constexpr bool parse_enum(std::string_view text, std::array<std::string_view, N> const &names, E &out)
        {
            for (std::size_t i = 0; i < N; ++i)
            {
                if (names[i] == text)
                {
                    out = static_cast<E>(i);
                    return true;
                }
            }
            return false;
        }
    }

    // TETR.IO option strings, indexed by enumerator value.
    inline constexpr std::array<std::string_view, 3> kGarbageBlockingNames = {"combo blocking", "limited blocking", "none"};
    inline constexpr std::array<std::string_view, 4> kPassthroughNames = {"zero", "limited", "consistent", "full"};
    inline constexpr std::array<std::string_view, 4> kComboTableNames = {"none", "multiplier", "classic guideline", "modern guideline"};
    inline constexpr std::array<std::string_view, 2> kRoundModeNames = {"down", "rng"};
    inline constexpr std::array<std::string_view, 10> kSpinBonusesNames = {"T-spins", "T-spins+", "all", "all+", "all-mini", "all-mini+", "mini-only", "handheld", "stupid", "none"};

    constexpr bool parse(std::string_view text, GarbageBlocking &out)
    {
        return detail::parse_enum(text, kGarbageBlockingNames, out);
    }
    constexpr bool parse(std::string_view text, Passthrough &out)
    {
        return detail::parse_enum(text, kPassthroughNames, out);
    }
    constexpr bool parse(std::string_view text, ComboTable &out)
    {
        return detail::parse_enum(text, kComboTableNames, out);
    }
    constexpr bool parse(std::string_view text, RoundMode &out)
    {
        return detail::parse_enum(text, kRoundModeNames, out);
    }
    constexpr bool parse(std::string_view text, SpinBonuses &out)
    {
        return detail::parse_enum(text, kSpinBonusesNames, out);
    }

    // Rule-relevant TETR.IO game options, named after the game's option keys.
    // Member defaults are the engine defaults (the server only sends overrides); tetra_league() is ranked TL.
    struct Options
    {
        bool b2bchaining = false;
        bool b2bcharging = false;
        int b2bcharge_at = 4;
        int b2bcharge_base = 0;
        bool b2bextras = false;
        bool allclears = true;
        int allclear_garbage = 10;
        int allclear_b2b = 0;
        bool allclear_b2b_sends = false;
        bool allclear_b2b_dupes = true;
        bool allclear_charges = false;
        bool garbagespecialbonus = false;
        int openerphase = 0;
        double garbagemultiplier = 1;
        double garbageincrease = 0;
        int garbagemargin = 0;
        double garbagecap = 8;
        double garbagecapincrease = 0;
        double garbagecapmax = 40;
        int garbagecapmargin = 0;
        int garbagespeed = 20;
        GarbageBlocking garbageblocking = GarbageBlocking::ComboBlocking;
        Passthrough passthrough = Passthrough::Zero;
        ComboTable combotable = ComboTable::Multiplier;
        RoundMode roundmode = RoundMode::Down;
        SpinBonuses spinbonuses = SpinBonuses::TSpins;
        bool allow180 = false;
        bool nolockout = false;
        bool clutch = true;
        double messiness_change = 1;
        double messiness_inner = 0;
        int garbageholesize = 1;

        static constexpr Options tetra_league()
        {
            Options o;
            o.b2bcharging = true;
            o.b2bcharge_base = 3;
            o.allclear_garbage = 5;
            o.allclear_b2b = 1;
            o.allclear_b2b_sends = true;
            o.allclear_b2b_dupes = false;
            o.garbagespecialbonus = true;
            o.openerphase = 14;
            o.garbageincrease = 0.008;
            o.garbagemargin = 10800;
            o.spinbonuses = SpinBonuses::AllMiniPlus;
            o.allow180 = true;
            o.nolockout = true;
            return o;
        }
    };

    // Name of the first option whose value these rules do not implement, or nullptr when all are supported.
    // The spin class itself comes from the search, so spinbonuses only matters here for handheld's halved non-T spins.
    constexpr char const *unsupported_option(Options const &o)
    {
        if (o.b2bchaining)
            return "b2bchaining";
        if (o.garbageblocking != GarbageBlocking::ComboBlocking)
            return "garbageblocking";
        if (o.passthrough != Passthrough::Zero)
            return "passthrough";
        if (o.combotable != ComboTable::Multiplier && o.combotable != ComboTable::None)
            return "combotable";
        if (o.roundmode != RoundMode::Down)
            return "roundmode";
        if (o.spinbonuses == SpinBonuses::Handheld)
            return "spinbonuses";
        if (o.garbageholesize != 1)
            return "garbageholesize";
        return nullptr;
    }

    inline double garbage_multiplier(Options const &o, double frame)
    {
        return o.garbagemultiplier + (frame > o.garbagemargin ? o.garbageincrease * (frame - o.garbagemargin) / 60 : 0.0);
    }

    // Lines of garbage that may enter the board on one placement.
    inline int garbage_cap(Options const &o, double frame)
    {
        double cap = o.garbagecap + (frame > o.garbagecapmargin ? o.garbagecapincrease * (frame - o.garbagecapmargin) / 60 : 0.0);
        return static_cast<int>(std::floor(std::min(o.garbagecapmax, cap)));
    }

    // TETR.IO client counters: btb 0 = no chain, first B2B-eligible clear -> 1, display "B2B xN" with N = btb - 1;
    // combo 0 = none, first clear -> 1.
    struct Counters
    {
        int btb = 0;
        int combo = 0;
    };

    struct Placement
    {
        int lines = 0;
        Spin spin = Spin::None;
        bool all_clear = false;
        bool garbage_cleared = false; // at least one cleared row contains garbage cells
    };

    struct Attack
    {
        std::array<int, 3> surge{}; // in send order; zero parts are not sent
        int lines = 0;               // line attack after multiplier and special bonus
        int all_clear = 0;
        bool blocks = false; // the placement fights instead of tanking (combo blocking)
        bool chain_broken = false;
        Counters counters;

        constexpr int total() const { return surge[0] + surge[1] + surge[2] + lines + all_clear; }
    };

    // Breaking the chain releases surge only above b2bcharge_at (B2B x4 and up in TL).
    constexpr bool is_charged(Options const &o, int btb)
    {
        return o.b2bcharging && btb > o.b2bcharge_at;
    }

    inline int surge_amount(Options const &o, int btb, double multiplier)
    {
        return is_charged(o, btb) ? static_cast<int>(std::floor((btb - o.b2bcharge_at + o.b2bcharge_base) * multiplier)) : 0;
    }

    // JS Math.round split: [round(S/3), round(S/3), S - 2*round(S/3)].
    inline std::array<int, 3> split_surge(int amount)
    {
        int third = static_cast<int>(std::floor(amount / 3.0 + 0.5));
        return {third, third, amount - 2 * third};
    }

    inline constexpr std::array<std::array<int, 3>, 6> kBaseAttack = {{
        {0, 0, 0},
        {0, 0, 2},
        {1, 1, 4},
        {2, 2, 6},
        {4, 4, 10},
        {5, 12, 12},
    }};

    inline double combo_multiplied(Options const &o, double d, int combo)
    {
        if (o.combotable != ComboTable::Multiplier || combo <= 1)
            return d;
        d *= 1 + 0.25 * (combo - 1);
        if (combo > 2)
            d = std::max(d, std::log(1 + 1.25 * (combo - 1)));
        return d;
    }

    // One locked piece, rules.md §7.2 (tetrio.js `function i(s,i,o)`). `multiplier` is garbage_multiplier() at the lock frame.
    inline Attack resolve_attack(Options const &o, Counters counters, Placement const &p, double multiplier)
    {
        Attack r;
        bool const pc = p.all_clear && o.allclears;
        bool const spin = p.spin != Spin::None;
        int a = pc ? o.allclear_b2b : 0;
        if (p.lines > 0)
        {
            if (o.combotable != ComboTable::None)
                ++counters.combo;
            if (p.lines >= 4 || spin)
                ++a;
            if (pc && !o.allclear_b2b_dupes)
                a = std::max(o.allclear_b2b, a - o.allclear_b2b);
            if (pc && o.allclear_charges)
                a = std::max(a, o.b2bcharge_at + 1 - counters.btb);
        }
        else
        {
            counters.combo = 0;
        }
        if (a > 0)
        {
            counters.btb += a;
        }
        else if (p.lines > 0)
        {
            int const surge = surge_amount(o, counters.btb, multiplier);
            if (surge > 0)
                r.surge = split_surge(surge);
            r.chain_broken = counters.btb > 0;
            counters.btb = 0;
        }
        double d = kBaseAttack[std::min(p.lines, 5)][static_cast<int>(p.spin)];
        bool const b2b_sends = o.allclear_b2b_sends || !(pc && a == o.allclear_b2b);
        if ((p.lines > 0 || a > 0) && counters.btb > 1 && b2b_sends)
            d += o.b2bextras && (p.lines == 4 || (p.lines >= 2 && spin)) ? 2 : 1;
        d = combo_multiplied(o, d, counters.combo);
        r.lines = static_cast<int>(std::floor(d * multiplier));
        if (o.garbagespecialbonus && p.garbage_cleared && (p.lines == 4 || spin))
            ++r.lines;
        r.blocks = p.lines > 0 || r.lines > 0;
        if (pc)
            r.all_clear = static_cast<int>(std::floor(o.allclear_garbage * multiplier));
        r.counters = counters;
        return r;
    }

    // Park–Miller minimal standard LCG used by TETR.IO for the bag (`rng`) and garbage holes (`rngex`).
    class Rng
    {
    public:
        explicit constexpr Rng(std::int64_t seed)
        {
            seed %= 2147483647;
            if (seed <= 0)
                seed += 2147483646;
            seed_ = seed;
        }
        constexpr std::int64_t next()
        {
            seed_ = 16807 * seed_ % 2147483647;
            return seed_;
        }
        constexpr double next_float() { return static_cast<double>(next() - 1) / 2147483646; }
        template<class T>
        constexpr void shuffle(std::span<T> items)
        {
            for (std::size_t i = items.size(); i-- > 1;)
            {
                auto j = static_cast<std::size_t>(next_float() * static_cast<double>(i + 1));
                std::swap(items[i], items[j]);
            }
        }
        constexpr std::int64_t seed() const { return seed_; }

    private:
        std::int64_t seed_;
    };

    // TETR.IO 7-bag: each bag is a shuffle of the literal order z,l,o,s,i,j,t. Pieces are upper-case.
    class Bag7
    {
    public:
        explicit constexpr Bag7(std::int64_t seed) : rng_(seed) {}
        constexpr char next()
        {
            if (index_ == bag_.size())
            {
                bag_ = {'Z', 'L', 'O', 'S', 'I', 'J', 'T'};
                rng_.shuffle(std::span<char>(bag_));
                index_ = 0;
            }
            return bag_[index_++];
        }

    private:
        Rng rng_;
        std::array<char, 7> bag_{};
        std::size_t index_ = 7;
    };

    // Garbage hole column state driven by `rngex` (rules.md §9: RerollColumn / TakeAllDamage).
    class HoleColumns
    {
    public:
        explicit constexpr HoleColumns(std::int64_t seed, int width = 10) : rng_(seed), width_(width) {}

        // Hole column of the next garbage row that enters the board.
        constexpr int row_column(Options const &o)
        {
            if ((last_ < 0 || rng_.next_float() < o.messiness_inner) && !changed_)
                reroll();
            changed_ = false;
            return last_;
        }
        // An incoming entry has been used up, by tanking or by cancelling.
        constexpr void entry_done(Options const &o)
        {
            if (rng_.next_float() < o.messiness_change)
            {
                reroll();
                changed_ = true;
            }
        }
        constexpr int last_column() const { return last_; }

    private:
        constexpr void reroll() { last_ = static_cast<int>(rng_.next_float() * width_); }

        Rng rng_;
        int width_;
        int last_ = -1;
        bool changed_ = false;
    };

    struct FightResult
    {
        int sent = 0;
        int cancelled = 0;
    };

    // Incoming garbage of one player. Entries become active (may enter the board) once frames_until_active <= 0.
    // When full, a new entry is merged into the last one (total is kept; its activation moves earlier).
    template<std::size_t Capacity>
    class PendingQueue
    {
    public:
        struct Entry
        {
            int amount;
            int frames_until_active;
        };

        constexpr void push(int amount, int frames_until_active)
        {
            if (amount <= 0)
                return;
            total_ += amount;
            if (size_ == Capacity)
            {
                entries_[size_ - 1].amount += amount;
                return;
            }
            entries_[size_++] = Entry{amount, frames_until_active};
        }
        constexpr void advance(int frames)
        {
            for (std::size_t i = 0; i < size_; ++i)
                entries_[i].frames_until_active -= frames;
        }
        constexpr int total() const { return total_; }
        constexpr int active_total() const
        {
            int sum = 0;
            for (std::size_t i = 0; i < size_; ++i)
                sum += entries_[i].frames_until_active <= 0 ? entries_[i].amount : 0;
            return sum;
        }
        constexpr bool empty() const { return size_ == 0; }
        constexpr std::span<Entry const> entries() const { return {entries_.data(), size_}; }

        // FightLines for one outgoing attack part. `pieces_placed` includes the piece just locked;
        // `lines_sent` (lines actually sent this round) is updated.
        constexpr FightResult fight(Options const &o, int lines, int pieces_placed, int &lines_sent, HoleColumns *columns = nullptr)
        {
            FightResult r;
            int extra = pieces_placed <= o.openerphase && total_ >= lines_sent ? lines : 0;
            while ((lines > 0 || extra > 0) && size_ > 0)
            {
                --total_;
                ++r.cancelled;
                if (--entries_[0].amount == 0)
                {
                    pop_front();
                    if (columns != nullptr)
                        columns->entry_done(o);
                }
                if (lines > 0)
                    --lines;
                else
                    --extra;
            }
            r.sent = lines;
            lines_sent += lines;
            return r;
        }

        // TakeAllDamage: up to `cap` lines from active entries enter the board. Returns the number of rows;
        // when `columns` is given, the hole column of each row (bottom row last) is written to `holes`.
        constexpr int tank(Options const &o, int cap, HoleColumns *columns = nullptr, std::span<std::int8_t> holes = {})
        {
            int rows = 0;
            while (rows < cap)
            {
                std::size_t i = 0;
                while (i < size_ && entries_[i].frames_until_active > 0)
                    ++i;
                if (i == size_)
                    break;
                --total_;
                if (columns != nullptr)
                {
                    int column = columns->row_column(o);
                    if (static_cast<std::size_t>(rows) < holes.size())
                        holes[rows] = static_cast<std::int8_t>(column);
                }
                ++rows;
                if (--entries_[i].amount == 0)
                {
                    erase(i);
                    if (columns != nullptr)
                        columns->entry_done(o);
                }
            }
            return rows;
        }

    private:
        constexpr void pop_front() { erase(0); }
        constexpr void erase(std::size_t i)
        {
            std::copy(entries_.begin() + static_cast<std::ptrdiff_t>(i) + 1, entries_.begin() + static_cast<std::ptrdiff_t>(size_), entries_.begin() + static_cast<std::ptrdiff_t>(i));
            --size_;
        }

        std::array<Entry, Capacity> entries_{};
        std::size_t size_ = 0;
        int total_ = 0;
    };

    struct ExchangeResult
    {
        int sent = 0;
        int surge_sent = 0; // part of `sent`
        int cancelled = 0;
        int tanked = 0;
        std::array<int, 5> parts{}; // amounts actually sent, one incoming entry each for the opponent
        int part_count = 0;
    };

    // Applies one placement's attack to the own queue in client order: surge parts, line attack (if it blocks),
    // all-clear attack, then tanking when nothing blocked. Rows tanked are reported (and their holes written)
    // exactly like PendingQueue::tank.
    template<std::size_t Capacity>
    constexpr ExchangeResult exchange(Options const &o, Attack const &attack, PendingQueue<Capacity> &queue, int pieces_placed, int &lines_sent, int cap,
                                      HoleColumns *columns = nullptr, std::span<std::int8_t> holes = {})
    {
        ExchangeResult r;
        auto fight = [&](int lines)
        {
            FightResult f = queue.fight(o, lines, pieces_placed, lines_sent, columns);
            r.sent += f.sent;
            r.cancelled += f.cancelled;
            if (f.sent > 0)
                r.parts[r.part_count++] = f.sent;
        };
        for (int part : attack.surge)
        {
            if (part > 0)
                fight(part);
        }
        r.surge_sent = r.sent;
        if (attack.blocks)
            fight(attack.lines);
        if (attack.all_clear > 0)
            fight(attack.all_clear);
        if (!attack.blocks)
            r.tanked = queue.tank(o, cap, columns, holes);
        return r;
    }
}
