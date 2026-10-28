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
            return {name, member, IO::Param{}.*member, lower, upper};
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
            field("hold_i", &IO::Param::hold_i, -16, 16),
            field("waste_i", &IO::Param::waste_i, -16, 16),
            field("clear_1", &IO::Param::clear_1, -16, 16),
            field("clear_2", &IO::Param::clear_2, -16, 16),
            field("clear_3", &IO::Param::clear_3, -16, 16),
            field("clear_4", &IO::Param::clear_4, -16, 16),
            field("mini_1", &IO::Param::mini_1, -16, 16),
            field("mini_2", &IO::Param::mini_2, -16, 16),
            field("mini_3", &IO::Param::mini_3, -16, 16),
            field("full_1", &IO::Param::full_1, -16, 32),
            field("full_2", &IO::Param::full_2, -16, 32),
            field("full_3", &IO::Param::full_3, -16, 48),
            field("combo", &IO::Param::combo, 0, 128),
            field("ratio", &IO::Param::ratio, 1, 2.5),
            field("surge", &IO::Param::surge, 0, 16),
            field("b2b_gain", &IO::Param::b2b_gain, 0, 16),
            field("cancel", &IO::Param::cancel, 0, 8),
            field("pending", &IO::Param::pending, 0, 8),
            field("opponent_surge", &IO::Param::opponent_surge, 0, 2),
            field("surge_release", &IO::Param::surge_release, 0, 1),
            field("spin_garbage", &IO::Param::spin_garbage, -16, 16),
            field("slot_b2b", &IO::Param::slot_b2b, 0, 16),
            field("slot_attack", &IO::Param::slot_attack, 0, 8),
            field("slot_decay", &IO::Param::slot_decay, 0, 1),
        }};

        // A piece in neither hold nor preview counts as one 7-bag away.
        constexpr int kUnseenSlotDistance = 7;

        std::string_view trim(std::string_view text)
        {
            auto const first = text.find_first_not_of(" \t\r");
            if (first == std::string_view::npos)
                return {};
            auto const last = text.find_last_not_of(" \t\r");
            return text.substr(first, last - first + 1);
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
        return status;
    }


    double IO::spin_slot_value(std::array<SpinSlot, kPieceTypes> const &slots, AIEnv const &env) const
    {
        Param const &p = config_->param;
        std::array<int, kPieceTypes> distance;
        distance.fill(kUnseenSlotDistance);
        for (size_t i = env.length; i-- > 0;)
        {
            int const index = piece_index_[static_cast<unsigned char>(env.next[i])];
            if (index >= 0)
                distance[index] = std::min(static_cast<int>(i), kUnseenSlotDistance);
        }
        if (int const index = piece_index_[static_cast<unsigned char>(env.hold)]; index >= 0)
            distance[index] = 0;
        std::array<double, kUnseenSlotDistance + 1> decay{};
        decay[0] = 1;
        for (int d = 1; d <= kUnseenSlotDistance; ++d)
            decay[d] = decay[d - 1] * p.slot_decay;
        double value = 0;
        for (int i = 0; i < kPieceTypes; ++i)
        {
            SpinSlot const slot = slots[i];
            if (slot.spin != io_rules::Spin::None)
                value += (p.slot_b2b + p.slot_attack * io_rules::kBaseAttack[slot.lines][static_cast<int>(slot.spin)]) * decay[distance[i]];
        }
        return value;
    }
}
