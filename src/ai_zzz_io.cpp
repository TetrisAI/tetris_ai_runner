#include "ai_zzz_io.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using m_tetris2::AIEnv;

namespace ai_zzz
{
    namespace
    {
        constexpr IO::ParamField field(std::string_view name, double IO::Param::*member, double lower, double upper)
        {
            return {name, member, IO::Param{}.*member, lower, upper, (upper - lower) * 0.2};
        }

        constexpr std::array<IO::ParamField, 33> kParamFields = {{
            field("roof", &IO::Param::roof, 0, 512),
            field("col_trans", &IO::Param::col_trans, 0, 512),
            field("row_trans", &IO::Param::row_trans, 0, 512),
            field("hole_count", &IO::Param::hole_count, 0, 512),
            field("hole_line", &IO::Param::hole_line, 0, 1024),
            field("well_depth", &IO::Param::well_depth, 0, 512),
            field("hole_depth", &IO::Param::hole_depth, 0, 256),
            field("b2b", &IO::Param::b2b, 0, 1024),
            field("attack", &IO::Param::attack, 0, 8),
            field("hold_t", &IO::Param::hold_t, -16, 16),
            field("hold_i", &IO::Param::hold_i, -16, 16),
            field("waste_t", &IO::Param::waste_t, -16, 16),
            field("waste_i", &IO::Param::waste_i, -16, 16),
            field("clear_1", &IO::Param::clear_1, -16, 16),
            field("clear_2", &IO::Param::clear_2, -16, 16),
            field("clear_3", &IO::Param::clear_3, -16, 16),
            field("clear_4", &IO::Param::clear_4, -16, 16),
            field("t2_slot", &IO::Param::t2_slot, 0, 8),
            field("t3_slot", &IO::Param::t3_slot, 0, 8),
            field("tspin_mini", &IO::Param::tspin_mini, -16, 16),
            field("tspin_1", &IO::Param::tspin_1, -16, 32),
            field("tspin_2", &IO::Param::tspin_2, -16, 32),
            field("tspin_3", &IO::Param::tspin_3, -16, 48),
            field("combo", &IO::Param::combo, 0, 128),
            field("ratio", &IO::Param::ratio, 1, 2.5),
            field("surge", &IO::Param::surge, 0, 16),
            field("b2b_gain", &IO::Param::b2b_gain, 0, 16),
            field("cancel", &IO::Param::cancel, 0, 8),
            field("pending", &IO::Param::pending, 0, 8),
            field("opponent_surge", &IO::Param::opponent_surge, 0, 2),
            field("kill", &IO::Param::kill, 0, 64),
            field("spin_garbage", &IO::Param::spin_garbage, -16, 16),
            field("allspin", &IO::Param::allspin, -16, 16),
        }};

        std::string_view trim(std::string_view text)
        {
            auto const first = text.find_first_not_of(" \t\r");
            if (first == std::string_view::npos)
                return {};
            auto const last = text.find_last_not_of(" \t\r");
            return text.substr(first, last - first + 1);
        }

        // Without charging the B2B bonus is a flat +1 whenever the chain is active, so only being active has value.
        double chain_value(IO::Param const &p, io_rules::Options const &rules, int btb, double multiplier)
        {
            if (!rules.b2bcharging)
                return btb > 0 ? p.b2b * 16 : 0.0;
            double const shown = std::max(0, btb - 1);
            double const charge = io_rules::is_charged(rules, btb) ? (btb - rules.b2bcharge_at + rules.b2bcharge_base) * multiplier : 0.0;
            return p.b2b * 16 * shown + p.surge * 256 * charge;
        }
    }

    std::span<IO::ParamField const> IO::param_fields()
    {
        return kParamFields;
    }

    bool IO::parse_params(std::string_view text, Param &param)
    {
        Param parsed = param;
        while (!text.empty())
        {
            auto const end = text.find('\n');
            std::string_view line = text.substr(0, end);
            text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
            line = trim(line.substr(0, line.find('#')));
            if (line.empty())
                continue;
            auto const eq = line.find('=');
            if (eq == std::string_view::npos)
                return false;
            std::string_view const name = trim(line.substr(0, eq));
            std::string const value(trim(line.substr(eq + 1)));
            auto const it = std::ranges::find(kParamFields, name, &ParamField::name);
            if (it == kParamFields.end() || value.empty())
                return false;
            char *parsed_end = nullptr;
            double const number = std::strtod(value.c_str(), &parsed_end);
            if (parsed_end != value.c_str() + value.size())
                return false;
            parsed.*(it->member) = number;
        }
        param = parsed;
        return true;
    }

    std::string IO::format_params(Param const &param)
    {
        std::string text;
        for (ParamField const &f : kParamFields)
        {
            std::array<char, 32> number{};
            std::snprintf(number.data(), number.size(), "%.17g", param.*(f.member));
            text.append(f.name).append(" = ").append(number.data()).append("\n");
        }
        return text;
    }

    bool IO::load_params(std::string const &path, Param &param)
    {
        std::ifstream in(path);
        if (!in)
            return false;
        std::ostringstream content;
        content << in.rdbuf();
        return parse_params(content.str(), param);
    }

    bool IO::save_params(std::string const &path, Param const &param)
    {
        std::ofstream out(path);
        out << format_params(param);
        return static_cast<bool>(out);
    }

    bool IO::Status::operator<(Status const &other) const
    {
        return value < other.value;
    }

    std::string IO::ai_name() const
    {
        return "ZZZ TETR.IO S2";
    }

    IO::Status IO::make_root_status(RootState const &root, Config const &config)
    {
        Status status;
        for (PendingGarbage const &g : root.pending)
            status.pending.push(g.amount, g.frames_until_active);
        status.frame = root.frame;
        status.btb = root.btb;
        status.combo = root.combo;
        status.pieces_placed = root.pieces_placed;
        status.lines_sent = root.lines_sent;
        status.garbage_rows = root.garbage_rows;
        status.root_chain = chain_value(config.param, config.rules, root.btb, io_rules::garbage_multiplier(config.rules, root.frame));
        return status;
    }

    IO::Status IO::get(Result const &eval_result, size_t depth, Status const &status, AIEnv const &env) const
    {
        Config const &config = *config_;
        io_rules::Options const &rules = config.rules;
        Param const &p = config.param;
        Status result = status;

        result.frame = depth == 0 ? status.frame : status.frame + 60 / config.pps;
        result.pending.advance(static_cast<int>(std::floor(result.frame) - std::floor(status.frame)));
        ++result.pieces_placed;
        double const multiplier = io_rules::garbage_multiplier(rules, result.frame);

        std::uint64_t const garbage_mask = status.garbage_rows >= 64 ? ~std::uint64_t(0) : (std::uint64_t(1) << status.garbage_rows) - 1;
        int const garbage_cleared = std::popcount(eval_result.cleared_rows & garbage_mask);
        result.garbage_rows -= garbage_cleared;

        io_rules::Placement const placement{eval_result.clear, eval_result.spin, eval_result.all_clear, garbage_cleared > 0};
        io_rules::Attack const attack = io_rules::resolve_attack(rules, {status.btb, status.combo}, placement, multiplier);
        io_rules::ExchangeResult const exchange = io_rules::exchange(rules, attack, result.pending, result.pieces_placed, result.lines_sent, io_rules::garbage_cap(rules, result.frame));
        result.btb = attack.counters.btb;
        result.combo = attack.counters.combo;
        result.map_rise += exchange.tanked;
        result.sent += exchange.sent;
        result.cancelled += exchange.cancelled;
        int const b2b_gain = std::max(0, result.btb - status.btb);
        result.b2b_gained += rules.b2bcharging ? b2b_gain : (status.btb == 0 && result.btb > 0 ? 1 : 0);
        result.max_combo = std::max(result.combo, result.max_combo);

        bool const t_piece = eval_result.piece == 'T';
        double const waste = (eval_result.piece == 'I' ? p.waste_i : 0) + (t_piece ? p.waste_t : 0);
        switch (eval_result.clear)
        {
        case 0:
            if (exchange.tanked > 0)
                result.like += waste;
            break;
        case 1:
        case 2:
        case 3:
            if (eval_result.spin == io_rules::Spin::Mini)
            {
                result.like += t_piece ? p.tspin_mini : p.allspin;
            }
            else if (eval_result.spin == io_rules::Spin::Full)
            {
                std::array<double, 3> const tspin = {p.tspin_1, p.tspin_2, p.tspin_3};
                result.like += tspin[eval_result.clear - 1];
            }
            else
            {
                std::array<double, 3> const plain = {p.clear_1, p.clear_2, p.clear_3};
                result.like += plain[eval_result.clear - 1] + (eval_result.clear < 3 ? waste : 0);
            }
            break;
        default:
            result.like += (result.combo + std::min(std::max(0, result.btb - 1), 5)) * (1 + result.sent + result.cancelled) * p.clear_4;
            break;
        }
        if (eval_result.clear > 0 && eval_result.spin != io_rules::Spin::None && garbage_cleared > 0)
            result.like += p.spin_garbage;
        if (env.hold == 'T' && eval_result.spin == io_rules::Spin::None)
            result.like += p.hold_t;
        if (env.hold == 'I' && eval_result.clear != 4)
            result.like += p.hold_i;
        result.like += result.sent;

        if (eval_result.all_clear && !config.allow_pc)
            result.death = true;
        if (!rules.nolockout && eval_result.lock_out)
            result.death = true;
        if (eval_result.clear == 0 || !rules.clutch)
        {
            int const next_index = env.length > 0 ? piece_index_[static_cast<unsigned char>(*env.next)] : -1;
            std::uint64_t blocked = 0;
            for (int i = 0; i < kPieceTypes; ++i)
            {
                if (next_index < 0 || next_index == i)
                    blocked |= eval_result.spawn_blocked[i];
            }
            if (result.map_rise >= 64 || ((blocked >> result.map_rise) & 1u) != 0)
                result.death = true;
        }

        int const t_expect = [&]
        {
            if (env.hold == 'T')
                return 0;
            for (size_t i = 0; i < env.length; ++i)
            {
                if (env.next[i] == 'T')
                    return static_cast<int>(i);
            }
            return 13;
        }();
        double const rate = 1. / static_cast<double>(depth + 1) + 3;
        double const fullness = std::max(0.05, (full_count_ - eval_result.cells - result.map_rise * (width_ - 1)) / static_cast<double>(full_count_));
        int const placement_attack = attack.total();
        double const offense = result.sent * 256 * rate * p.attack +
                               result.cancelled * 256 * p.cancel +
                               eval_result.t2_value * (t_expect < 4 ? (3 - t_expect) * 256 : 128) * p.t2_slot +
                               eval_result.t3_value * (t_expect < 2 ? 64 : 32) * p.t3_slot +
                               (b2b_gain > 0 ? placement_attack * 16 * (22 - eval_result.roof) : 0) +
                               chain_value(p, rules, result.btb, multiplier) +
                               result.b2b_gained * 256 * p.b2b_gain +
                               result.like * 32;

        double incoming = result.pending.total();
        if (config.opponent.known)
            incoming += p.opponent_surge * io_rules::surge_amount(rules, config.opponent.btb, multiplier);
        double const uncovered = std::max(0.0, incoming - io_rules::surge_amount(rules, result.btb, multiplier));
        bool const kill = config.opponent.known && config.opponent.pending + result.sent >= config.opponent.free_rows;

        result.value = eval_result.value + offense * fullness +
                       result.max_combo * (result.max_combo - 1) * (status.btb <= result.btb ? placement_attack : 0) * p.combo -
                       uncovered * 256 * p.pending +
                       (kill ? p.kill * 4096 + result.root_chain : 0) -
                       (result.death ? 999999999.0 : 0);
        return result;
    }
}
