#pragma once

#include "tetris_shape.h"
#include "bb_state.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace m_tetris2
{
    namespace ai_rule_spec
    {
        template<class Spec>
        using piece_info_t = bb::PieceIndexInfo<typename Spec::ops>;

        template<class Spec>
        constexpr std::size_t piece_count() noexcept
        {
            return static_cast<std::size_t>(piece_info_t<Spec>::count);
        }

        template<class Spec>
        constexpr int piece_index(char t) noexcept
        {
            return piece_info_t<Spec>::index_of(t);
        }

        template<class Spec>
        constexpr char piece_type_at(std::size_t i) noexcept
        {
            return piece_info_t<Spec>::type_at(i);
        }

        template<class Spec>
        constexpr std::uint32_t row_mask() noexcept
        {
            return Spec::width >= 32 ? 0xffffffffu : ((std::uint32_t(1) << Spec::width) - 1u);
        }

        template<class Cache>
        void reset_piece_index_cache(Cache &cache)
        {
            for (auto &v : cache)
                v = -1;
        }

        template<class Spec, class Cache, std::size_t I = 0>
        void fill_piece_index_cache_impl(Cache &cache)
        {
            if constexpr (I < piece_count<Spec>())
            {
                cache[static_cast<unsigned char>(piece_type_at<Spec>(I))] = static_cast<int>(I);
                fill_piece_index_cache_impl<Spec, Cache, I + 1>(cache);
            }
        }

        template<class Spec, class Cache>
        void fill_piece_index_cache(Cache &cache)
        {
            reset_piece_index_cache(cache);
            fill_piece_index_cache_impl<Spec>(cache);
        }

        template<class Spec, char T, std::uint8_t R = 0, int DropY = 0>
        constexpr int spawn_min_row() noexcept
        {
            constexpr auto cells = shape::piece_cells<Spec, T, R>;
            constexpr auto sp = Spec::spawn(T, Spec::width, Spec::height);
            return sp.second - static_cast<int>(cells.origin.y) - DropY;
        }

        template<class Data>
        void accumulate_danger_rows(Data &data)
        {
            for (int y = 0; y < 3; ++y)
                data.data[y + 1] |= data.data[y];
        }

        template<class Spec, char T, std::uint8_t R, class Data, int BaseRow, int DropY>
        void add_spawn_rotation_rows(Data &data)
        {
            constexpr auto cells = shape::piece_cells<Spec, T, R>;
            constexpr auto sp = Spec::spawn(T, Spec::width, Spec::height);
            constexpr int bbox_x = sp.first + static_cast<int>(cells.origin.x);
            constexpr int bbox_y = sp.second - static_cast<int>(cells.origin.y) - DropY;
            for (std::size_t i = 0; i < cells.count; ++i)
            {
                int x = bbox_x + cells.cells[i].x;
                int y = bbox_y + cells.cells[i].y;
                if (x >= 0 && x < Spec::width && y >= BaseRow && y < BaseRow + 4)
                    data.data[y - BaseRow] |= (std::uint32_t(1) << x);
            }
        }

        template<class Spec, char T, class Data, int BaseRow, int DropY, std::size_t R = 0>
        void add_spawn_piece_rows(Data &data)
        {
            if constexpr (R < shape::rotation_count<Spec, T>)
            {
                add_spawn_rotation_rows<Spec, T, static_cast<std::uint8_t>(R), Data, BaseRow, DropY>(data);
                add_spawn_piece_rows<Spec, T, Data, BaseRow, DropY, R + 1>(data);
            }
        }

        template<class Spec, class Data, int BaseRow = static_cast<int>(Spec::height) - 4, int DropY = 0, std::size_t I = 0>
        void fill_spawn_danger_data(std::vector<Data> &out)
        {
            if constexpr (I == 0)
                out.resize(piece_count<Spec>());
            if constexpr (I < piece_count<Spec>())
            {
                Data data{};
                add_spawn_piece_rows<Spec, piece_type_at<Spec>(I), Data, BaseRow, DropY>(data);
                accumulate_danger_rows(data);
                out[I] = data;
                fill_spawn_danger_data<Spec, Data, BaseRow, DropY, I + 1>(out);
            }
        }
    }
}
