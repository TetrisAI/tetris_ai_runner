
#pragma once


#include <cstddef>
#include <cstdint>
#if __SSE4__
#   include <nmmintrin.h>
#endif

#if __SSE4__
#   define ZZZ_BitCount(n) size_t(_mm_popcnt_u32(n))
#else
#   define ZZZ_BitCount(n) ::zzz::BitCountImpl(n)
#endif

namespace zzz
{
    size_t BitCountImpl(uint32_t n);
    size_t NumberOfTrailingZeros(uint32_t i);
}