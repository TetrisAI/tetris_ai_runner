#include "tetris_core.h"
#include "rule_io.h"
#include "search_amini.h"
#include "search_path.h"
#include "io_rules.h"
#include "ai_zzz_io.h"

#include <nlohmann/json.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;
    using Engine = m_tetris2::TetrisThreadEngine<rule_io::TetrisRule, ai_zzz::IO, amini::Search>;
    using Map = Engine::map_t;
    using Spec = rule_io::TetrisRule::rule_spec;
    using Helpers = m_tetris2::Helpers<Spec>;

    constexpr int kProtocolVersion = 2;
    constexpr int kDefaultThinkMs = 50;
    constexpr std::uint64_t kMemoryLimit = 512ull << 20;
    constexpr std::string_view kPieces = "IJLOSTZ";
    constexpr int kMaxNext = 64;

    constexpr char kUsage[] = R"(Usage: io_uds [--think-ms N] [--verbose]

TETR.IO bot server for botconnect (protocol v2): newline-delimited JSON over an AF_UNIX stream socket.

Socket:
  $HOME/$TETRIS_AI_SOCKET (default $HOME/tetris_ai); a stale socket file is replaced.

Options:
  --think-ms N   search time per move in milliseconds (default 50; env TETRIS_AI_THINK_MS)
  --verbose      log every request and reply
  --help         show this help

Commands (one JSON object per line: {"bot_id", "command", "data"}):
  create    start a bot engine for bot_id
  prepare   data = {"protocol": 2, "options": <TETR.IO round options>, "pps": <pieces/s>, "params": <param file>?}
  move      data = board state; replies {"bot_id", "command": "move", "data": "<path>V"}
  dismiss   drop the bot
Path keys: l r L R D z c x (L/R = DAS to the wall), leading v = hold, trailing V = hard drop.
)";

    struct ServerSettings
    {
        int think_ms = kDefaultThinkMs;
        bool verbose = false;
    };
    ServerSettings settings;

    std::mutex log_mutex;

    template<class... Args>
    void log(int bot_id, std::format_string<Args...> format, Args &&...args)
    {
        std::lock_guard lock(log_mutex);
        std::clog << "[bot " << bot_id << "] " << std::format(format, std::forward<Args>(args)...) << std::endl;
    }

    template<class T>
    bool read_value(Json const &json, T &out)
    {
        if constexpr (std::is_same_v<T, bool>)
        {
            if (!json.is_boolean())
                return false;
            out = json.get<bool>();
        }
        else if constexpr (std::is_enum_v<T>)
        {
            return json.is_string() && io_rules::parse(json.get_ref<std::string const &>(), out);
        }
        else if constexpr (std::is_integral_v<T>)
        {
            if (json.is_number_unsigned())
            {
                auto const value = json.get<std::uint64_t>();
                if (!std::in_range<T>(value))
                    return false;
                out = static_cast<T>(value);
            }
            else if (json.is_number_integer())
            {
                auto const value = json.get<std::int64_t>();
                if (!std::in_range<T>(value))
                    return false;
                out = static_cast<T>(value);
            }
            else
            {
                return false;
            }
        }
        else
        {
            static_assert(std::is_floating_point_v<T>);
            if (!json.is_number())
                return false;
            out = json.get<T>();
        }
        return true;
    }

    template<class T>
    bool read_field(Json const &object, std::string_view key, T &out)
    {
        auto it = object.find(key);
        return it != object.end() && read_value(*it, out);
    }

    bool read_piece(Json const &object, std::string_view key, char &out, bool allow_none)
    {
        auto it = object.find(key);
        if (it == object.end() || !it->is_string())
            return false;
        auto const &text = it->get_ref<std::string const &>();
        if (text.size() != 1)
            return false;
        out = text[0];
        return kPieces.find(out) != std::string_view::npos || (allow_none && out == ' ');
    }

    struct OptionField
    {
        std::string_view key;
        bool (*read)(Json const &, io_rules::Options &);
        void (*reset_to_tetra_league)(io_rules::Options &);
    };

    template<auto kMember>
    constexpr OptionField option_field(std::string_view key)
    {
        return {key,
                [](Json const &json, io_rules::Options &options)
                { return read_value(json, options.*kMember); },
                [](io_rules::Options &options)
                { options.*kMember = io_rules::Options::tetra_league().*kMember; }};
    }

    using O = io_rules::Options;
    constexpr std::array kOptionFields = {
        option_field<&O::b2bchaining>("b2bchaining"),
        option_field<&O::b2bcharging>("b2bcharging"),
        option_field<&O::b2bcharge_at>("b2bcharge_at"),
        option_field<&O::b2bcharge_base>("b2bcharge_base"),
        option_field<&O::b2bextras>("b2bextras"),
        option_field<&O::allclears>("allclears"),
        option_field<&O::allclear_garbage>("allclear_garbage"),
        option_field<&O::allclear_b2b>("allclear_b2b"),
        option_field<&O::allclear_b2b_sends>("allclear_b2b_sends"),
        option_field<&O::allclear_b2b_dupes>("allclear_b2b_dupes"),
        option_field<&O::allclear_charges>("allclear_charges"),
        option_field<&O::garbagespecialbonus>("garbagespecialbonus"),
        option_field<&O::openerphase>("openerphase"),
        option_field<&O::garbagemultiplier>("garbagemultiplier"),
        option_field<&O::garbageincrease>("garbageincrease"),
        option_field<&O::garbagemargin>("garbagemargin"),
        option_field<&O::garbagecap>("garbagecap"),
        option_field<&O::garbagecapincrease>("garbagecapincrease"),
        option_field<&O::garbagecapmax>("garbagecapmax"),
        option_field<&O::garbagecapmargin>("garbagecapmargin"),
        option_field<&O::garbagespeed>("garbagespeed"),
        option_field<&O::garbageblocking>("garbageblocking"),
        option_field<&O::passthrough>("passthrough"),
        option_field<&O::combotable>("combotable"),
        option_field<&O::roundmode>("roundmode"),
        option_field<&O::spinbonuses>("spinbonuses"),
        option_field<&O::allow180>("allow180"),
        option_field<&O::nolockout>("nolockout"),
        option_field<&O::clutch>("clutch"),
        option_field<&O::messiness_change>("messiness_change"),
        option_field<&O::messiness_inner>("messiness_inner"),
        option_field<&O::garbageholesize>("garbageholesize"),
    };
    static_assert(io_rules::unsupported_option(O::tetra_league()) == nullptr);

    // Options outside io_rules that rule_io hard-codes (SRS+ on a 10-wide board with 20 visible rows).
    struct FixedOption
    {
        std::string_view key;
        Json expected;
    };

    std::string json_text(Json const &object, std::string_view key)
    {
        auto it = object.find(key);
        return it == object.end() ? "absent" : it->dump();
    }

    io_rules::Options parse_options(int bot_id, Json const &json)
    {
        io_rules::Options options;
        for (auto const &field : kOptionFields)
        {
            auto it = json.find(field.key);
            if (it != json.end() && !field.read(*it, options))
                log(bot_id, "error: option {} has invalid value {}; using the engine default", field.key, it->dump());
        }
        while (char const *key = io_rules::unsupported_option(options))
        {
            auto field = std::find_if(kOptionFields.begin(), kOptionFields.end(), [&](OptionField const &f)
                                      { return f.key == key; });
            assert(field != kOptionFields.end());
            log(bot_id, "warning: option {} = {} is not supported; playing it as ranked Tetra League", key, json_text(json, key));
            field->reset_to_tetra_league(options);
        }
        static FixedOption const kFixedOptions[] = {{"kickset", "SRS+"}, {"boardwidth", 10}, {"boardheight", 20}};
        for (auto const &option : kFixedOptions)
        {
            auto it = json.find(option.key);
            if (it != json.end() && *it != option.expected)
                log(bot_id, "warning: option {} = {} is not supported; playing it as {}", option.key, it->dump(), option.expected.dump());
        }
        return options;
    }

    using SpinRule = search_amini::Search::SpinRule;

    SpinRule search_spin_rule(int bot_id, io_rules::SpinBonuses spinbonuses)
    {
        ai_zzz::IO::SearchSpinRule const mapped = ai_zzz::IO::search_spin_rule(spinbonuses);
        if (!mapped.approximation.empty())
            log(bot_id, "warning: spinbonuses {} is searched as {}", io_rules::kSpinBonusesNames[static_cast<std::size_t>(spinbonuses)], mapped.approximation);
        return mapped.rule;
    }

    struct MoveState
    {
        Map board{};
        char active = ' ';
        int row = 0; // spawn row minus `fallen`; may exceed the spawn row after a clutch raise
        char hold = ' ';
        bool can_hold = true;
        std::string next;
        ai_zzz::IO::RootState root{};
    };

    struct Bot
    {
        std::mutex mutex;
        std::unique_ptr<Engine> engine;
        ai_zzz::IO::Config config;
        MoveState move;
        std::vector<ai_zzz::IO::PendingGarbage> pending;

        Bot()
        {
            reset_engine(config.rules.allow180, SpinRule::AllMiniPlus);
            move.next.reserve(kMaxNext);
        }

        // The search tree caches landings classified under the movement and spin settings it was built with, so a
        // new round gets a fresh engine instead of a reconfigured one.
        void reset_engine(bool allow_180, SpinRule spin_rule)
        {
            engine = std::make_unique<Engine>();
            [[maybe_unused]] bool const prepared = engine->prepare(Map::width, Map::height);
            assert(prepared);
            engine->memory_limit(kMemoryLimit);
            auto &search = *engine->search_config();
            search.allow_180 = allow_180;
            // Both executors soft drop instantly (SDF 41) and map L/R to DAS-to-wall.
            search.allow_d = false;
            search.allow_LR = true;
            search.is_20g = false;
            search.spin_rule = spin_rule;
        }
    };

    std::mutex bots_mutex;
    std::unordered_map<int, std::shared_ptr<Bot>> bots;

    std::shared_ptr<Bot> find_bot(int bot_id)
    {
        std::lock_guard lock(bots_mutex);
        auto it = bots.find(bot_id);
        return it == bots.end() ? nullptr : it->second;
    }

    bool create_bot(int bot_id)
    {
        std::lock_guard lock(bots_mutex);
        return bots.try_emplace(bot_id, std::make_shared<Bot>()).second;
    }

    void dismiss_bot(int bot_id)
    {
        std::lock_guard lock(bots_mutex);
        bots.erase(bot_id);
    }

    void prepare_bot(int bot_id, Bot &bot, Json const &data)
    {
        int protocol = 0;
        if (!read_field(data, "protocol", protocol) || protocol != kProtocolVersion)
            log(bot_id, "error: prepare protocol {} is not {}; reading it as protocol {}", json_text(data, "protocol"), kProtocolVersion, kProtocolVersion);

        auto options = data.find("options");
        if (options == data.end() || !options->is_object())
        {
            log(bot_id, "error: prepare without an options object; using ranked Tetra League");
            bot.config.rules = io_rules::Options::tetra_league();
        }
        else
        {
            bot.config.rules = parse_options(bot_id, *options);
        }

        double pps = 0;
        if (read_field(data, "pps", pps) && pps > 0)
            bot.config.pps = pps;
        else
            log(bot_id, "error: prepare pps {} is not a positive number; keeping {}", json_text(data, "pps"), bot.config.pps);

        bot.config.param = ai_zzz::IO::Param{};
        auto params = data.find("params");
        if (params != data.end())
        {
            if (!params->is_string() || !ai_zzz::IO::load_params(params->get_ref<std::string const &>(), bot.config.param))
                log(bot_id, "error: cannot load params {}; using the built-in parameters", params->dump());
        }

        auto const &rules = bot.config.rules;
        bot.reset_engine(rules.allow180, search_spin_rule(bot_id, rules.spinbonuses));
        log(bot_id, "prepared: spinbonuses {}, allow180 {}, b2bcharging {}, pps {}, params {}",
            io_rules::kSpinBonusesNames[static_cast<std::size_t>(rules.spinbonuses)], rules.allow180, rules.b2bcharging,
            bot.config.pps, json_text(data, "params"));
    }

    // Returns the first missing or invalid field of a move request, or an empty view.
    std::string_view parse_move(Json const &data, Bot &bot)
    {
        auto &move = bot.move;
        auto &root = move.root;

        auto board = data.find("board");
        if (board == data.end() || !board->is_array() || board->size() != Map::height)
            return "board";
        for (int y = 0; y < Map::height; ++y)
        {
            int row = 0;
            if (!read_value((*board)[y], row) || row < 0 || row > Map::row_full)
                return "board";
            move.board.set_row(y, static_cast<Map::row_t>(row));
        }
        if (!read_field(data, "garbage_rows", root.garbage_rows) || root.garbage_rows < 0 || root.garbage_rows > Map::height)
            return "garbage_rows";
        if (!read_piece(data, "active", move.active, false))
            return "active";
        int fallen = 0;
        if (!read_field(data, "fallen", fallen) || fallen < -Map::height || fallen > Map::height)
            return "fallen";
        move.row = Spec::spawn(move.active, Map::width, Map::height).second - fallen;
        if (move.row < 0 || move.row >= Map::height)
            return "fallen";
        if (!read_piece(data, "hold", move.hold, true))
            return "hold";
        if (!read_field(data, "can_hold", move.can_hold))
            return "can_hold";
        if (!read_field(data, "can_pc", bot.config.allow_pc))
            return "can_pc";

        auto queue = data.find("queue");
        int next_size = 0;
        if (queue == data.end() || !queue->is_string())
            return "queue";
        if (!read_field(data, "next_size", next_size) || next_size < 0)
            return "next_size";
        auto const &pieces = queue->get_ref<std::string const &>();
        move.next.assign(pieces, 0, std::min({pieces.size(), static_cast<std::size_t>(next_size), static_cast<std::size_t>(kMaxNext)}));
        if (move.next.find_first_not_of(kPieces) != std::string::npos)
            return "queue";

        if (!read_field(data, "b2b", root.btb) || root.btb < 0)
            return "b2b";
        if (!read_field(data, "combo", root.combo) || root.combo < 0)
            return "combo";
        if (!read_field(data, "frame", root.frame) || root.frame < 0)
            return "frame";
        if (!read_field(data, "pieces_placed", root.pieces_placed) || root.pieces_placed < 0)
            return "pieces_placed";
        if (!read_field(data, "lines_sent", root.lines_sent) || root.lines_sent < 0)
            return "lines_sent";

        auto garbage = data.find("garbage");
        if (garbage == data.end() || !garbage->is_array())
            return "garbage";
        bot.pending.clear();
        for (auto const &entry : *garbage)
        {
            ai_zzz::IO::PendingGarbage packet{};
            if (!entry.is_object() || !read_field(entry, "amount", packet.amount) || packet.amount < 0 ||
                !read_field(entry, "frames_until_active", packet.frames_until_active) || packet.frames_until_active < 0)
                return "garbage";
            bot.pending.push_back(packet);
        }
        root.pending = bot.pending;

        auto &opponent = bot.config.opponent;
        opponent = {};
        auto opponent_json = data.find("opponent");
        if (opponent_json != data.end())
        {
            if (!opponent_json->is_object() || !read_field(*opponent_json, "free_rows", opponent.free_rows) ||
                !read_field(*opponent_json, "pending", opponent.pending) || !read_field(*opponent_json, "b2b", opponent.btb))
                return "opponent";
            opponent.known = true;
        }
        return {};
    }

    std::string run_move(int bot_id, Bot &bot)
    {
        auto &engine = *bot.engine;
        auto const &move = bot.move;

        auto const spawn_x = Spec::spawn(move.active, Map::width, Map::height).first;
        m_tetris2::TetrisBlockStatus const status(move.active, static_cast<std::int8_t>(spawn_x), static_cast<std::int8_t>(move.row), 0);
        std::array<Map, Helpers::kMaxR> usable;
        Helpers::build_usable_for_piece(move.active, move.board, usable);
        if (!Helpers::check_T(move.active, status.x, status.y, 0, usable))
        {
            log(bot_id, "error: active {} at row {} overlaps the board or leaves it", move.active, move.row);
            return "V";
        }

        *engine.ai_config() = bot.config;
        *engine.status() = ai_zzz::IO::make_root_status(move.root, bot.config);
        engine.update();

        std::string path;
        // An empty path means "hard drop from the start"; anything else that yields no keys is a make_path failure.
        auto append_path = [&](m_tetris2::TetrisBlockStatus const &from, Engine::RunResult const &result)
        {
            auto const keys = engine.make_path(from, result.target, move.board);
            path.append(keys.begin(), keys.end());
            if (!keys.empty())
                return;
            std::array<Map, Helpers::kMaxR> from_usable;
            Helpers::build_usable_for_piece(from.t, move.board, from_usable);
            auto const dropped = Helpers::drop(Helpers::state_from_status(from.t, from.r, from.x, from.y), from_usable);
            auto const &target = result.target;
            if (!dropped || target.type != search_amini::Search::None ||
                Helpers::cells_key_for_state(*dropped) != Helpers::cells_key_for_state(target.state))
                log(bot_id, "error: no path from {} at ({}, {}) to the chosen landing r{} ({}, {}) type {}", from.t, from.x,
                    from.y, target.state.r, target.state.xb, target.state.yb, static_cast<int>(target.type));
        };
        auto no_placement = [&]
        { log(bot_id, "error: the search found no placement for {}", move.active); };
        if (move.can_hold)
        {
            auto const result = engine.run_hold(move.board, status, move.hold, true, move.next.data(), move.next.size(), settings.think_ms);
            if (result.change_hold)
            {
                path += 'v';
                if (result.target != nullptr)
                {
                    char const piece = static_cast<char>(result.target.state.t);
                    auto const [x, y] = Spec::spawn(piece, Map::width, Map::height);
                    append_path(m_tetris2::TetrisBlockStatus(piece, static_cast<std::int8_t>(x), static_cast<std::int8_t>(y), 0), result);
                }
            }
            else if (result.target != nullptr)
            {
                append_path(status, result);
            }
            else
            {
                no_placement();
            }
        }
        else
        {
            auto const result = engine.run(move.board, status, move.next.data(), move.next.size(), settings.think_ms);
            if (result.target != nullptr)
                append_path(status, result);
            else
                no_placement();
        }
        path += 'V';
        return path;
    }

    std::string handle_move(int bot_id, Json const &data)
    {
        auto bot = find_bot(bot_id);
        if (!bot)
        {
            log(bot_id, "error: move for a bot that was not created");
            return "V";
        }
        std::lock_guard lock(bot->mutex);
        if (!data.is_object())
        {
            log(bot_id, "error: move without a data object");
            return "V";
        }
        if (auto field = parse_move(data, *bot); !field.empty())
        {
            log(bot_id, "error: move field {} is missing or invalid: {}", field, json_text(data, field));
            return "V";
        }
        return run_move(bot_id, *bot);
    }

    // Handles one request line; `reply` is left empty for commands without a reply.
    void handle_message(std::string_view line, std::vector<int> &owned_bots, std::string &reply)
    {
        reply.clear();
        Json const message = Json::parse(line, nullptr, false);
        int bot_id = 0;
        if (message.is_discarded() || !message.is_object() || !read_field(message, "bot_id", bot_id))
        {
            log(-1, "error: message without a valid bot_id: {}", line);
            return;
        }
        auto command = message.find("command");
        if (command == message.end() || !command->is_string())
        {
            log(bot_id, "error: message without a command: {}", line);
            return;
        }
        static Json const kNoData = Json::object();
        auto data_it = message.find("data");
        Json const &data = data_it == message.end() ? kNoData : *data_it;
        auto const &name = command->get_ref<std::string const &>();
        if (settings.verbose)
            log(bot_id, "<- {}", line);

        if (name == "create")
        {
            if (create_bot(bot_id))
            {
                owned_bots.push_back(bot_id);
                log(bot_id, "created");
            }
            else
            {
                log(bot_id, "error: create for an existing bot; keeping it");
            }
        }
        else if (name == "prepare")
        {
            auto bot = find_bot(bot_id);
            if (!bot)
            {
                log(bot_id, "error: prepare for a bot that was not created");
                return;
            }
            if (!data.is_object())
            {
                log(bot_id, "error: prepare without a data object");
                return;
            }
            std::lock_guard lock(bot->mutex);
            prepare_bot(bot_id, *bot, data);
        }
        else if (name == "move")
        {
            reply = Json{{"bot_id", bot_id}, {"command", "move"}, {"data", handle_move(bot_id, data)}}.dump();
            if (settings.verbose)
                log(bot_id, "-> {}", reply);
        }
        else if (name == "dismiss")
        {
            dismiss_bot(bot_id);
            std::erase(owned_bots, bot_id);
            log(bot_id, "dismissed");
        }
        else
        {
            log(bot_id, "error: unknown command {}", name);
        }
    }

    bool send_all(int fd, std::string_view bytes)
    {
        while (!bytes.empty())
        {
            ssize_t const sent = ::send(fd, bytes.data(), bytes.size(), 0);
            if (sent < 0 && errno == EINTR)
                continue;
            if (sent <= 0)
                return false;
            bytes.remove_prefix(static_cast<std::size_t>(sent));
        }
        return true;
    }

    void serve_client(int fd)
    {
        std::vector<int> owned_bots;
        std::string buffer, reply;
        char chunk[4096];
        bool connected = true;
        while (connected)
        {
            ssize_t const received = ::read(fd, chunk, sizeof chunk);
            if (received < 0 && errno == EINTR)
                continue;
            if (received <= 0)
                break;
            buffer.append(chunk, static_cast<std::size_t>(received));
            std::size_t start = 0;
            for (std::size_t end; connected && (end = buffer.find('\n', start)) != std::string::npos; start = end + 1)
            {
                handle_message(std::string_view(buffer).substr(start, end - start), owned_bots, reply);
                if (!reply.empty())
                {
                    reply += '\n';
                    connected = send_all(fd, reply);
                }
            }
            buffer.erase(0, start);
        }
        for (int bot_id : owned_bots)
        {
            dismiss_bot(bot_id);
            log(bot_id, "dismissed (client disconnected)");
        }
        ::close(fd);
    }

    char socket_path[sizeof(sockaddr_un::sun_path)];

    extern "C" void remove_socket_and_exit(int)
    {
        ::unlink(socket_path);
        ::_exit(0);
    }

    bool parse_int(std::string_view text, int &out)
    {
        auto const [end, error] = std::from_chars(text.data(), text.data() + text.size(), out);
        return error == std::errc() && end == text.data() + text.size();
    }
} // namespace

int main(int argc, char *argv[])
{
    if (char const *think = std::getenv("TETRIS_AI_THINK_MS"); think && (!parse_int(think, settings.think_ms) || settings.think_ms <= 0))
    {
        std::cerr << "io_uds: TETRIS_AI_THINK_MS must be a positive integer\n";
        return 2;
    }
    for (int i = 1; i < argc; ++i)
    {
        std::string_view const arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            std::cout << kUsage;
            return 0;
        }
        if (arg == "--verbose" || arg == "-v")
        {
            settings.verbose = true;
        }
        else if (arg == "--think-ms" && i + 1 < argc && parse_int(argv[i + 1], settings.think_ms) && settings.think_ms > 0)
        {
            ++i;
        }
        else
        {
            std::cerr << "io_uds: invalid argument " << arg << "\n\n"
                      << kUsage;
            return 2;
        }
    }

    char const *home = std::getenv("HOME");
    char const *name = std::getenv("TETRIS_AI_SOCKET");
    if (!home)
    {
        std::cerr << "io_uds: HOME is not set\n";
        return 1;
    }
    std::string_view socket_name = name ? name : "tetris_ai";
    while (socket_name.starts_with('/'))
        socket_name.remove_prefix(1);
    std::string const path = std::string(home) + '/' + std::string(socket_name);
    if (path.size() >= sizeof socket_path)
    {
        std::cerr << "io_uds: socket path " << path << " is too long\n";
        return 1;
    }
    std::memcpy(socket_path, path.c_str(), path.size() + 1);

    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT, remove_socket_and_exit);
    std::signal(SIGTERM, remove_socket_and_exit);

    int const server = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socket_path, sizeof socket_path);
    ::unlink(socket_path);
    if (server < 0 || ::bind(server, reinterpret_cast<sockaddr const *>(&address), sizeof address) < 0 || ::listen(server, 16) < 0)
    {
        std::cerr << "io_uds: cannot listen on " << path << ": " << std::strerror(errno) << '\n';
        return 1;
    }
    std::cout << "io_uds: listening on " << path << " (think " << settings.think_ms << " ms)" << std::endl;

    for (;;)
    {
        int const client = ::accept(server, nullptr, nullptr);
        if (client < 0)
        {
            if (errno != EINTR)
                std::cerr << "io_uds: accept failed: " << std::strerror(errno) << '\n';
            continue;
        }
        std::thread(serve_client, client).detach();
    }
}
