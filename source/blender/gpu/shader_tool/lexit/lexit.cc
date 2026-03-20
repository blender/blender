/* SPDX-FileCopyrightText: 2026 Clement Foucault
 *
 * SPDX-License-Identifier: MIT */

#include "lexit.hh"
#include "identifier.hh"
#include "simd.hh"

#include <algorithm>
#include <cassert>
#include <cstring>
#ifdef _MSC_VER
#  include <intrin.h>
#endif

#if defined(__clang__) || defined(__GNUC__)
#  define count_bits_i(i) __builtin_popcount(i)
#elif defined(_MSC_VER)
#  define count_bits_i(i) __popcnt(i)
#else
#  include <bitset>
#  define count_bits_i(i) (std::bitset<8>{i}.count())
#endif

namespace lexit {

static int builtin_ctzll(uint64_t a)
{
#ifdef _MSC_VER
  unsigned long ctz;
  _BitScanForward64(&ctz, a);
  return ctz;
#else
  return __builtin_ctzll(a);
#endif
}  // namespace lexit

static uint32_t divide_ceil(uint32_t a, uint32_t b)
{
  return (a + b - 1) / b;
}

/* Helper function to realloc aligned array keeping elem_count data. */
template<typename T>
void realloc_aligned_array(AlignedArrayPtr<T> &ptr, size_t elem_count, size_t new_size)
{
  assert(new_size >= elem_count);
  AlignedArrayPtr<T> new_ptr(new_size);
  if (ptr.get()) {
    std::memcpy(new_ptr.get(), ptr.get(), elem_count * sizeof(T));
  }
  ptr = std::move(new_ptr);
}

void TokenBuffer::clear()
{
  size_ = 0;
}

void TokenBuffer::reserve(const uint32_t count)
{
  if (allocated_size_ >= count + 1) {
    return;
  }
  allocated_size_ = count + 1;
  realloc_aligned_array(types_, size_ + 1, allocated_size_);
  realloc_aligned_array(offsets_, size_ + 1, allocated_size_);
  realloc_aligned_array(offsets_end_, size_ + 1, allocated_size_);
  realloc_aligned_array(atoms_, size_ + 1, allocated_size_);
  realloc_aligned_array(lengths_, size_ + 1, allocated_size_);
}

#if defined(USE_NEON) || defined(USE_SSE4_2)

/* Shuffle table used for stream compaction.
 * For a given 8bit pattern (where each 1 bit represents an element to keep)
 * encode the index of the source register for each of the 8 destination registers.
 * Every 0 bit (representing a discarded element) will be sourced from the 0th element.
 * This is to be used with table. */
alignas(16) static const uint8_t shuffle_table_8[256][8] = {
    /* [0b00000000] = */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* [0b00000001] = */ {0, 0, 0, 0, 0, 0, 0, 0},
    /* [0b00000010] = */ {1, 0, 0, 0, 0, 0, 0, 0},
    /* [0b00000011] = */ {0, 1, 0, 0, 0, 0, 0, 0},
    /* [0b00000100] = */ {2, 0, 0, 0, 0, 0, 0, 0},
    /* [0b00000101] = */ {0, 2, 0, 0, 0, 0, 0, 0},
    /* [0b00000110] = */ {1, 2, 0, 0, 0, 0, 0, 0},
    /* [0b00000111] = */ {0, 1, 2, 0, 0, 0, 0, 0},
    /* [0b00001000] = */ {3, 0, 0, 0, 0, 0, 0, 0},
    /* [0b00001001] = */ {0, 3, 0, 0, 0, 0, 0, 0},
    /* [0b00001010] = */ {1, 3, 0, 0, 0, 0, 0, 0},
    /* [0b00001011] = */ {0, 1, 3, 0, 0, 0, 0, 0},
    /* [0b00001100] = */ {2, 3, 0, 0, 0, 0, 0, 0},
    /* [0b00001101] = */ {0, 2, 3, 0, 0, 0, 0, 0},
    /* [0b00001110] = */ {1, 2, 3, 0, 0, 0, 0, 0},
    /* [0b00001111] = */ {0, 1, 2, 3, 0, 0, 0, 0},
    /* [0b00010000] = */ {4, 0, 0, 0, 0, 0, 0, 0},
    /* [0b00010001] = */ {0, 4, 0, 0, 0, 0, 0, 0},
    /* [0b00010010] = */ {1, 4, 0, 0, 0, 0, 0, 0},
    /* [0b00010011] = */ {0, 1, 4, 0, 0, 0, 0, 0},
    /* [0b00010100] = */ {2, 4, 0, 0, 0, 0, 0, 0},
    /* [0b00010101] = */ {0, 2, 4, 0, 0, 0, 0, 0},
    /* [0b00010110] = */ {1, 2, 4, 0, 0, 0, 0, 0},
    /* [0b00010111] = */ {0, 1, 2, 4, 0, 0, 0, 0},
    /* [0b00011000] = */ {3, 4, 0, 0, 0, 0, 0, 0},
    /* [0b00011001] = */ {0, 3, 4, 0, 0, 0, 0, 0},
    /* [0b00011010] = */ {1, 3, 4, 0, 0, 0, 0, 0},
    /* [0b00011011] = */ {0, 1, 3, 4, 0, 0, 0, 0},
    /* [0b00011100] = */ {2, 3, 4, 0, 0, 0, 0, 0},
    /* [0b00011101] = */ {0, 2, 3, 4, 0, 0, 0, 0},
    /* [0b00011110] = */ {1, 2, 3, 4, 0, 0, 0, 0},
    /* [0b00011111] = */ {0, 1, 2, 3, 4, 0, 0, 0},
    /* [0b00100000] = */ {5, 0, 0, 0, 0, 0, 0, 0},
    /* [0b00100001] = */ {0, 5, 0, 0, 0, 0, 0, 0},
    /* [0b00100010] = */ {1, 5, 0, 0, 0, 0, 0, 0},
    /* [0b00100011] = */ {0, 1, 5, 0, 0, 0, 0, 0},
    /* [0b00100100] = */ {2, 5, 0, 0, 0, 0, 0, 0},
    /* [0b00100101] = */ {0, 2, 5, 0, 0, 0, 0, 0},
    /* [0b00100110] = */ {1, 2, 5, 0, 0, 0, 0, 0},
    /* [0b00100111] = */ {0, 1, 2, 5, 0, 0, 0, 0},
    /* [0b00101000] = */ {3, 5, 0, 0, 0, 0, 0, 0},
    /* [0b00101001] = */ {0, 3, 5, 0, 0, 0, 0, 0},
    /* [0b00101010] = */ {1, 3, 5, 0, 0, 0, 0, 0},
    /* [0b00101011] = */ {0, 1, 3, 5, 0, 0, 0, 0},
    /* [0b00101100] = */ {2, 3, 5, 0, 0, 0, 0, 0},
    /* [0b00101101] = */ {0, 2, 3, 5, 0, 0, 0, 0},
    /* [0b00101110] = */ {1, 2, 3, 5, 0, 0, 0, 0},
    /* [0b00101111] = */ {0, 1, 2, 3, 5, 0, 0, 0},
    /* [0b00110000] = */ {4, 5, 0, 0, 0, 0, 0, 0},
    /* [0b00110001] = */ {0, 4, 5, 0, 0, 0, 0, 0},
    /* [0b00110010] = */ {1, 4, 5, 0, 0, 0, 0, 0},
    /* [0b00110011] = */ {0, 1, 4, 5, 0, 0, 0, 0},
    /* [0b00110100] = */ {2, 4, 5, 0, 0, 0, 0, 0},
    /* [0b00110101] = */ {0, 2, 4, 5, 0, 0, 0, 0},
    /* [0b00110110] = */ {1, 2, 4, 5, 0, 0, 0, 0},
    /* [0b00110111] = */ {0, 1, 2, 4, 5, 0, 0, 0},
    /* [0b00111000] = */ {3, 4, 5, 0, 0, 0, 0, 0},
    /* [0b00111001] = */ {0, 3, 4, 5, 0, 0, 0, 0},
    /* [0b00111010] = */ {1, 3, 4, 5, 0, 0, 0, 0},
    /* [0b00111011] = */ {0, 1, 3, 4, 5, 0, 0, 0},
    /* [0b00111100] = */ {2, 3, 4, 5, 0, 0, 0, 0},
    /* [0b00111101] = */ {0, 2, 3, 4, 5, 0, 0, 0},
    /* [0b00111110] = */ {1, 2, 3, 4, 5, 0, 0, 0},
    /* [0b00111111] = */ {0, 1, 2, 3, 4, 5, 0, 0},
    /* [0b01000000] = */ {6, 0, 0, 0, 0, 0, 0, 0},
    /* [0b01000001] = */ {0, 6, 0, 0, 0, 0, 0, 0},
    /* [0b01000010] = */ {1, 6, 0, 0, 0, 0, 0, 0},
    /* [0b01000011] = */ {0, 1, 6, 0, 0, 0, 0, 0},
    /* [0b01000100] = */ {2, 6, 0, 0, 0, 0, 0, 0},
    /* [0b01000101] = */ {0, 2, 6, 0, 0, 0, 0, 0},
    /* [0b01000110] = */ {1, 2, 6, 0, 0, 0, 0, 0},
    /* [0b01000111] = */ {0, 1, 2, 6, 0, 0, 0, 0},
    /* [0b01001000] = */ {3, 6, 0, 0, 0, 0, 0, 0},
    /* [0b01001001] = */ {0, 3, 6, 0, 0, 0, 0, 0},
    /* [0b01001010] = */ {1, 3, 6, 0, 0, 0, 0, 0},
    /* [0b01001011] = */ {0, 1, 3, 6, 0, 0, 0, 0},
    /* [0b01001100] = */ {2, 3, 6, 0, 0, 0, 0, 0},
    /* [0b01001101] = */ {0, 2, 3, 6, 0, 0, 0, 0},
    /* [0b01001110] = */ {1, 2, 3, 6, 0, 0, 0, 0},
    /* [0b01001111] = */ {0, 1, 2, 3, 6, 0, 0, 0},
    /* [0b01010000] = */ {4, 6, 0, 0, 0, 0, 0, 0},
    /* [0b01010001] = */ {0, 4, 6, 0, 0, 0, 0, 0},
    /* [0b01010010] = */ {1, 4, 6, 0, 0, 0, 0, 0},
    /* [0b01010011] = */ {0, 1, 4, 6, 0, 0, 0, 0},
    /* [0b01010100] = */ {2, 4, 6, 0, 0, 0, 0, 0},
    /* [0b01010101] = */ {0, 2, 4, 6, 0, 0, 0, 0},
    /* [0b01010110] = */ {1, 2, 4, 6, 0, 0, 0, 0},
    /* [0b01010111] = */ {0, 1, 2, 4, 6, 0, 0, 0},
    /* [0b01011000] = */ {3, 4, 6, 0, 0, 0, 0, 0},
    /* [0b01011001] = */ {0, 3, 4, 6, 0, 0, 0, 0},
    /* [0b01011010] = */ {1, 3, 4, 6, 0, 0, 0, 0},
    /* [0b01011011] = */ {0, 1, 3, 4, 6, 0, 0, 0},
    /* [0b01011100] = */ {2, 3, 4, 6, 0, 0, 0, 0},
    /* [0b01011101] = */ {0, 2, 3, 4, 6, 0, 0, 0},
    /* [0b01011110] = */ {1, 2, 3, 4, 6, 0, 0, 0},
    /* [0b01011111] = */ {0, 1, 2, 3, 4, 6, 0, 0},
    /* [0b01100000] = */ {5, 6, 0, 0, 0, 0, 0, 0},
    /* [0b01100001] = */ {0, 5, 6, 0, 0, 0, 0, 0},
    /* [0b01100010] = */ {1, 5, 6, 0, 0, 0, 0, 0},
    /* [0b01100011] = */ {0, 1, 5, 6, 0, 0, 0, 0},
    /* [0b01100100] = */ {2, 5, 6, 0, 0, 0, 0, 0},
    /* [0b01100101] = */ {0, 2, 5, 6, 0, 0, 0, 0},
    /* [0b01100110] = */ {1, 2, 5, 6, 0, 0, 0, 0},
    /* [0b01100111] = */ {0, 1, 2, 5, 6, 0, 0, 0},
    /* [0b01101000] = */ {3, 5, 6, 0, 0, 0, 0, 0},
    /* [0b01101001] = */ {0, 3, 5, 6, 0, 0, 0, 0},
    /* [0b01101010] = */ {1, 3, 5, 6, 0, 0, 0, 0},
    /* [0b01101011] = */ {0, 1, 3, 5, 6, 0, 0, 0},
    /* [0b01101100] = */ {2, 3, 5, 6, 0, 0, 0, 0},
    /* [0b01101101] = */ {0, 2, 3, 5, 6, 0, 0, 0},
    /* [0b01101110] = */ {1, 2, 3, 5, 6, 0, 0, 0},
    /* [0b01101111] = */ {0, 1, 2, 3, 5, 6, 0, 0},
    /* [0b01110000] = */ {4, 5, 6, 0, 0, 0, 0, 0},
    /* [0b01110001] = */ {0, 4, 5, 6, 0, 0, 0, 0},
    /* [0b01110010] = */ {1, 4, 5, 6, 0, 0, 0, 0},
    /* [0b01110011] = */ {0, 1, 4, 5, 6, 0, 0, 0},
    /* [0b01110100] = */ {2, 4, 5, 6, 0, 0, 0, 0},
    /* [0b01110101] = */ {0, 2, 4, 5, 6, 0, 0, 0},
    /* [0b01110110] = */ {1, 2, 4, 5, 6, 0, 0, 0},
    /* [0b01110111] = */ {0, 1, 2, 4, 5, 6, 0, 0},
    /* [0b01111000] = */ {3, 4, 5, 6, 0, 0, 0, 0},
    /* [0b01111001] = */ {0, 3, 4, 5, 6, 0, 0, 0},
    /* [0b01111010] = */ {1, 3, 4, 5, 6, 0, 0, 0},
    /* [0b01111011] = */ {0, 1, 3, 4, 5, 6, 0, 0},
    /* [0b01111100] = */ {2, 3, 4, 5, 6, 0, 0, 0},
    /* [0b01111101] = */ {0, 2, 3, 4, 5, 6, 0, 0},
    /* [0b01111110] = */ {1, 2, 3, 4, 5, 6, 0, 0},
    /* [0b01111111] = */ {0, 1, 2, 3, 4, 5, 6, 0},
    /* [0b10000000] = */ {7, 0, 0, 0, 0, 0, 0, 0},
    /* [0b10000001] = */ {0, 7, 0, 0, 0, 0, 0, 0},
    /* [0b10000010] = */ {1, 7, 0, 0, 0, 0, 0, 0},
    /* [0b10000011] = */ {0, 1, 7, 0, 0, 0, 0, 0},
    /* [0b10000100] = */ {2, 7, 0, 0, 0, 0, 0, 0},
    /* [0b10000101] = */ {0, 2, 7, 0, 0, 0, 0, 0},
    /* [0b10000110] = */ {1, 2, 7, 0, 0, 0, 0, 0},
    /* [0b10000111] = */ {0, 1, 2, 7, 0, 0, 0, 0},
    /* [0b10001000] = */ {3, 7, 0, 0, 0, 0, 0, 0},
    /* [0b10001001] = */ {0, 3, 7, 0, 0, 0, 0, 0},
    /* [0b10001010] = */ {1, 3, 7, 0, 0, 0, 0, 0},
    /* [0b10001011] = */ {0, 1, 3, 7, 0, 0, 0, 0},
    /* [0b10001100] = */ {2, 3, 7, 0, 0, 0, 0, 0},
    /* [0b10001101] = */ {0, 2, 3, 7, 0, 0, 0, 0},
    /* [0b10001110] = */ {1, 2, 3, 7, 0, 0, 0, 0},
    /* [0b10001111] = */ {0, 1, 2, 3, 7, 0, 0, 0},
    /* [0b10010000] = */ {4, 7, 0, 0, 0, 0, 0, 0},
    /* [0b10010001] = */ {0, 4, 7, 0, 0, 0, 0, 0},
    /* [0b10010010] = */ {1, 4, 7, 0, 0, 0, 0, 0},
    /* [0b10010011] = */ {0, 1, 4, 7, 0, 0, 0, 0},
    /* [0b10010100] = */ {2, 4, 7, 0, 0, 0, 0, 0},
    /* [0b10010101] = */ {0, 2, 4, 7, 0, 0, 0, 0},
    /* [0b10010110] = */ {1, 2, 4, 7, 0, 0, 0, 0},
    /* [0b10010111] = */ {0, 1, 2, 4, 7, 0, 0, 0},
    /* [0b10011000] = */ {3, 4, 7, 0, 0, 0, 0, 0},
    /* [0b10011001] = */ {0, 3, 4, 7, 0, 0, 0, 0},
    /* [0b10011010] = */ {1, 3, 4, 7, 0, 0, 0, 0},
    /* [0b10011011] = */ {0, 1, 3, 4, 7, 0, 0, 0},
    /* [0b10011100] = */ {2, 3, 4, 7, 0, 0, 0, 0},
    /* [0b10011101] = */ {0, 2, 3, 4, 7, 0, 0, 0},
    /* [0b10011110] = */ {1, 2, 3, 4, 7, 0, 0, 0},
    /* [0b10011111] = */ {0, 1, 2, 3, 4, 7, 0, 0},
    /* [0b10100000] = */ {5, 7, 0, 0, 0, 0, 0, 0},
    /* [0b10100001] = */ {0, 5, 7, 0, 0, 0, 0, 0},
    /* [0b10100010] = */ {1, 5, 7, 0, 0, 0, 0, 0},
    /* [0b10100011] = */ {0, 1, 5, 7, 0, 0, 0, 0},
    /* [0b10100100] = */ {2, 5, 7, 0, 0, 0, 0, 0},
    /* [0b10100101] = */ {0, 2, 5, 7, 0, 0, 0, 0},
    /* [0b10100110] = */ {1, 2, 5, 7, 0, 0, 0, 0},
    /* [0b10100111] = */ {0, 1, 2, 5, 7, 0, 0, 0},
    /* [0b10101000] = */ {3, 5, 7, 0, 0, 0, 0, 0},
    /* [0b10101001] = */ {0, 3, 5, 7, 0, 0, 0, 0},
    /* [0b10101010] = */ {1, 3, 5, 7, 0, 0, 0, 0},
    /* [0b10101011] = */ {0, 1, 3, 5, 7, 0, 0, 0},
    /* [0b10101100] = */ {2, 3, 5, 7, 0, 0, 0, 0},
    /* [0b10101101] = */ {0, 2, 3, 5, 7, 0, 0, 0},
    /* [0b10101110] = */ {1, 2, 3, 5, 7, 0, 0, 0},
    /* [0b10101111] = */ {0, 1, 2, 3, 5, 7, 0, 0},
    /* [0b10110000] = */ {4, 5, 7, 0, 0, 0, 0, 0},
    /* [0b10110001] = */ {0, 4, 5, 7, 0, 0, 0, 0},
    /* [0b10110010] = */ {1, 4, 5, 7, 0, 0, 0, 0},
    /* [0b10110011] = */ {0, 1, 4, 5, 7, 0, 0, 0},
    /* [0b10110100] = */ {2, 4, 5, 7, 0, 0, 0, 0},
    /* [0b10110101] = */ {0, 2, 4, 5, 7, 0, 0, 0},
    /* [0b10110110] = */ {1, 2, 4, 5, 7, 0, 0, 0},
    /* [0b10110111] = */ {0, 1, 2, 4, 5, 7, 0, 0},
    /* [0b10111000] = */ {3, 4, 5, 7, 0, 0, 0, 0},
    /* [0b10111001] = */ {0, 3, 4, 5, 7, 0, 0, 0},
    /* [0b10111010] = */ {1, 3, 4, 5, 7, 0, 0, 0},
    /* [0b10111011] = */ {0, 1, 3, 4, 5, 7, 0, 0},
    /* [0b10111100] = */ {2, 3, 4, 5, 7, 0, 0, 0},
    /* [0b10111101] = */ {0, 2, 3, 4, 5, 7, 0, 0},
    /* [0b10111110] = */ {1, 2, 3, 4, 5, 7, 0, 0},
    /* [0b10111111] = */ {0, 1, 2, 3, 4, 5, 7, 0},
    /* [0b11000000] = */ {6, 7, 0, 0, 0, 0, 0, 0},
    /* [0b11000001] = */ {0, 6, 7, 0, 0, 0, 0, 0},
    /* [0b11000010] = */ {1, 6, 7, 0, 0, 0, 0, 0},
    /* [0b11000011] = */ {0, 1, 6, 7, 0, 0, 0, 0},
    /* [0b11000100] = */ {2, 6, 7, 0, 0, 0, 0, 0},
    /* [0b11000101] = */ {0, 2, 6, 7, 0, 0, 0, 0},
    /* [0b11000110] = */ {1, 2, 6, 7, 0, 0, 0, 0},
    /* [0b11000111] = */ {0, 1, 2, 6, 7, 0, 0, 0},
    /* [0b11001000] = */ {3, 6, 7, 0, 0, 0, 0, 0},
    /* [0b11001001] = */ {0, 3, 6, 7, 0, 0, 0, 0},
    /* [0b11001010] = */ {1, 3, 6, 7, 0, 0, 0, 0},
    /* [0b11001011] = */ {0, 1, 3, 6, 7, 0, 0, 0},
    /* [0b11001100] = */ {2, 3, 6, 7, 0, 0, 0, 0},
    /* [0b11001101] = */ {0, 2, 3, 6, 7, 0, 0, 0},
    /* [0b11001110] = */ {1, 2, 3, 6, 7, 0, 0, 0},
    /* [0b11001111] = */ {0, 1, 2, 3, 6, 7, 0, 0},
    /* [0b11010000] = */ {4, 6, 7, 0, 0, 0, 0, 0},
    /* [0b11010001] = */ {0, 4, 6, 7, 0, 0, 0, 0},
    /* [0b11010010] = */ {1, 4, 6, 7, 0, 0, 0, 0},
    /* [0b11010011] = */ {0, 1, 4, 6, 7, 0, 0, 0},
    /* [0b11010100] = */ {2, 4, 6, 7, 0, 0, 0, 0},
    /* [0b11010101] = */ {0, 2, 4, 6, 7, 0, 0, 0},
    /* [0b11010110] = */ {1, 2, 4, 6, 7, 0, 0, 0},
    /* [0b11010111] = */ {0, 1, 2, 4, 6, 7, 0, 0},
    /* [0b11011000] = */ {3, 4, 6, 7, 0, 0, 0, 0},
    /* [0b11011001] = */ {0, 3, 4, 6, 7, 0, 0, 0},
    /* [0b11011010] = */ {1, 3, 4, 6, 7, 0, 0, 0},
    /* [0b11011011] = */ {0, 1, 3, 4, 6, 7, 0, 0},
    /* [0b11011100] = */ {2, 3, 4, 6, 7, 0, 0, 0},
    /* [0b11011101] = */ {0, 2, 3, 4, 6, 7, 0, 0},
    /* [0b11011110] = */ {1, 2, 3, 4, 6, 7, 0, 0},
    /* [0b11011111] = */ {0, 1, 2, 3, 4, 6, 7, 0},
    /* [0b11100000] = */ {5, 6, 7, 0, 0, 0, 0, 0},
    /* [0b11100001] = */ {0, 5, 6, 7, 0, 0, 0, 0},
    /* [0b11100010] = */ {1, 5, 6, 7, 0, 0, 0, 0},
    /* [0b11100011] = */ {0, 1, 5, 6, 7, 0, 0, 0},
    /* [0b11100100] = */ {2, 5, 6, 7, 0, 0, 0, 0},
    /* [0b11100101] = */ {0, 2, 5, 6, 7, 0, 0, 0},
    /* [0b11100110] = */ {1, 2, 5, 6, 7, 0, 0, 0},
    /* [0b11100111] = */ {0, 1, 2, 5, 6, 7, 0, 0},
    /* [0b11101000] = */ {3, 5, 6, 7, 0, 0, 0, 0},
    /* [0b11101001] = */ {0, 3, 5, 6, 7, 0, 0, 0},
    /* [0b11101010] = */ {1, 3, 5, 6, 7, 0, 0, 0},
    /* [0b11101011] = */ {0, 1, 3, 5, 6, 7, 0, 0},
    /* [0b11101100] = */ {2, 3, 5, 6, 7, 0, 0, 0},
    /* [0b11101101] = */ {0, 2, 3, 5, 6, 7, 0, 0},
    /* [0b11101110] = */ {1, 2, 3, 5, 6, 7, 0, 0},
    /* [0b11101111] = */ {0, 1, 2, 3, 5, 6, 7, 0},
    /* [0b11110000] = */ {4, 5, 6, 7, 0, 0, 0, 0},
    /* [0b11110001] = */ {0, 4, 5, 6, 7, 0, 0, 0},
    /* [0b11110010] = */ {1, 4, 5, 6, 7, 0, 0, 0},
    /* [0b11110011] = */ {0, 1, 4, 5, 6, 7, 0, 0},
    /* [0b11110100] = */ {2, 4, 5, 6, 7, 0, 0, 0},
    /* [0b11110101] = */ {0, 2, 4, 5, 6, 7, 0, 0},
    /* [0b11110110] = */ {1, 2, 4, 5, 6, 7, 0, 0},
    /* [0b11110111] = */ {0, 1, 2, 4, 5, 6, 7, 0},
    /* [0b11111000] = */ {3, 4, 5, 6, 7, 0, 0, 0},
    /* [0b11111001] = */ {0, 3, 4, 5, 6, 7, 0, 0},
    /* [0b11111010] = */ {1, 3, 4, 5, 6, 7, 0, 0},
    /* [0b11111011] = */ {0, 1, 3, 4, 5, 6, 7, 0},
    /* [0b11111100] = */ {2, 3, 4, 5, 6, 7, 0, 0},
    /* [0b11111101] = */ {0, 2, 3, 4, 5, 6, 7, 0},
    /* [0b11111110] = */ {1, 2, 3, 4, 5, 6, 7, 0},
    /* [0b11111111] = */ {0, 1, 2, 3, 4, 5, 6, 7},
};

/* Popcount for a uint8_t. */
alignas(16) static const uint8_t mask_popcount[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3,
    4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4,
    4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4,
    5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5,
    4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2,
    3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5,
    5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4,
    5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6,
    4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

template<int Size> struct ShuffleIndicesResult {
  simd::u8_base<Size> indices;
  int popcount;
};

inline ShuffleIndicesResult<1> shuffle_indices_from_emit_mask(uint16_t emit_mask)
{
  const uint8_t emit_mask_lo = emit_mask & 0xFFu;
  const uint8_t emit_mask_hi = emit_mask >> 8;
  const uint8_t mask_popcount_lo = mask_popcount[emit_mask_lo];
  const uint8_t mask_popcount_hi = mask_popcount[emit_mask_hi];
  /* Lookup the shuffle vector in 2 halves. */
  uint64_t v0 = *(uint64_t *)shuffle_table_8[emit_mask_lo];
  uint64_t v1 = *(uint64_t *)shuffle_table_8[emit_mask_hi] | uint64_t(0x0808080808080808);
  /* Combine the two halves into one contiguous index array.
   * We don't care about values after the last valid index. */
  alignas(16) uint8_t combined[16];
  *(uint64_t *)combined = v0;
  *(uint64_t *)(combined + mask_popcount_lo) = v1;

  return {simd::u8x16::load((const uint8_t *)&combined), mask_popcount_lo + mask_popcount_hi};
}

inline ShuffleIndicesResult<4> shuffle_indices_from_emit_mask(uint64_t emit_mask)
{
  const uint8_t emit_mask_0 = (emit_mask >> 0) & 0xFFu;
  const uint8_t emit_mask_1 = (emit_mask >> 8) & 0xFFu;
  const uint8_t emit_mask_2 = (emit_mask >> 16) & 0xFFu;
  const uint8_t emit_mask_3 = (emit_mask >> 24) & 0xFFu;
  const uint8_t emit_mask_4 = (emit_mask >> 32) & 0xFFu;
  const uint8_t emit_mask_5 = (emit_mask >> 40) & 0xFFu;
  const uint8_t emit_mask_6 = (emit_mask >> 48) & 0xFFu;
  const uint8_t emit_mask_7 = (emit_mask >> 56) & 0xFFu;
  const uint8_t mask_popcount_0 = mask_popcount[emit_mask_0];
  const uint8_t mask_popcount_1 = mask_popcount[emit_mask_1];
  const uint8_t mask_popcount_2 = mask_popcount[emit_mask_2];
  const uint8_t mask_popcount_3 = mask_popcount[emit_mask_3];
  const uint8_t mask_popcount_4 = mask_popcount[emit_mask_4];
  const uint8_t mask_popcount_5 = mask_popcount[emit_mask_5];
  const uint8_t mask_popcount_6 = mask_popcount[emit_mask_6];
  const uint8_t mask_popcount_7 = mask_popcount[emit_mask_7];
  /* Lookup the shuffle vector in multiple part. */
  uint64_t v0 = *(uint64_t *)shuffle_table_8[emit_mask_0];
  uint64_t v1 = *(uint64_t *)shuffle_table_8[emit_mask_1] | uint64_t(0x0808080808080808);
  uint64_t v2 = *(uint64_t *)shuffle_table_8[emit_mask_2] | uint64_t(0x1010101010101010);
  uint64_t v3 = *(uint64_t *)shuffle_table_8[emit_mask_3] | uint64_t(0x1818181818181818);
  uint64_t v4 = *(uint64_t *)shuffle_table_8[emit_mask_4] | uint64_t(0x2020202020202020);
  uint64_t v5 = *(uint64_t *)shuffle_table_8[emit_mask_5] | uint64_t(0x2828282828282828);
  uint64_t v6 = *(uint64_t *)shuffle_table_8[emit_mask_6] | uint64_t(0x3030303030303030);
  uint64_t v7 = *(uint64_t *)shuffle_table_8[emit_mask_7] | uint64_t(0x3838383838383838);
  /* Combine the parts into one contiguous index array.
   * We don't care about values after the last valid index. */
  alignas(64) uint8_t combined[64];
  int popcount = 0;
  *(uint64_t *)combined = v0, popcount += mask_popcount_0;
  *(uint64_t *)(combined + popcount) = v1, popcount += mask_popcount_1;
  *(uint64_t *)(combined + popcount) = v2, popcount += mask_popcount_2;
  *(uint64_t *)(combined + popcount) = v3, popcount += mask_popcount_3;
  *(uint64_t *)(combined + popcount) = v4, popcount += mask_popcount_4;
  *(uint64_t *)(combined + popcount) = v5, popcount += mask_popcount_5;
  *(uint64_t *)(combined + popcount) = v6, popcount += mask_popcount_6;
  *(uint64_t *)(combined + popcount) = v7, popcount += mask_popcount_7;

  return {simd::u8x64::load((const uint8_t *)&combined), popcount};
}
#endif

inline TokenType select(char char_value, char char_class, bool cond)
{
  return TokenType((cond) ? char_class : char_value);
}

inline void TokenBuffer::tokenize_scalar(uint32_t &__restrict offset,
                                         uint32_t &__restrict cursor_begin,
                                         uint32_t &__restrict cursor_end,
                                         CharClass &__restrict prev_char_class,
                                         bool &__restrict prev_whitespace,
                                         uint32_t end,
                                         const CharClass char_class_table[128])
{
  for (; offset < end; offset += 1) {
    const char c = str_[offset];
    const CharClass curr_char_class = char_class_table[c];
    const TokenType curr_tok_type = select(
        c, char(curr_char_class), curr_char_class > CharClass::ClassToTypeThreshold);
    /* It is faster to overwrite the previous value with the same value
     * as having a condition. */
    types_[cursor_begin] = curr_tok_type;
    offsets_[cursor_begin] = offset;
    offsets_end_[cursor_end] = offset;
    /**
     * Split if no class in common.
     * Example:
     * str  : i n t   i 2   =   0 . 0 f ;   i 2 + + ;
     * emit : 1 0 0 1 1 0 1 1 1 1 1 1 0 1 1 1 0 1 0 1
     */
    const bool emit = (uint8_t(curr_char_class) & uint8_t(prev_char_class) &
                       uint8_t(CharClass::CanMerge)) == 0;
    prev_char_class = curr_char_class;

    /**
     * These are the emit mask we want to achieve:
     * str          : i n t   i 2   =   0 . 0 f ;   i 2 + + ;
     * emit start   : 1 0 0 0 1 0 0 1 0 1 1 1 0 1 0 1 0 1 0 1
     * emit end     : 0 0 0 1 0 0 1 0 1 0 1 1 0 1 1 0 0 1 0 1
     */
    /*              : 1 1 1 0 1 1 0 1 0 1 1 1 1 1 0 1 1 1 1 1  */
    const bool curr_ws = (curr_char_class == CharClass::WhiteSpace);
    /*              : 0 0 0 1 0 0 1 0 1 0 0 0 0 0 1 0 0 0 0 0  */
    const bool emit_ws = emit && curr_ws;
    /*              : 1 0 0 0 1 0 0 1 0 1 1 1 0 1 0 1 0 1 0 1  */
    const bool emit_start = emit && !curr_ws;
    /*              : 0 0 0 0 0 0 0 0 0 0 1 1 0 1 0 0 0 1 0 1  */
    const bool follow_non_ws = emit_start && !prev_whitespace;
    /*              : 0 0 0 1 0 0 1 0 1 0 1 1 0 1 1 0 0 1 0 1  */
    const bool emit_end = emit_ws || follow_non_ws;

    prev_whitespace = curr_ws;

#ifdef LEXIT_DEBUG
    if (emit_start) {
      int start = offsets_[cursor_begin - 1];
      int end = offsets_[cursor_begin];
      token_str_with_whitespace_debug_.emplace_back(str_.data() + start, end - start);
    }
    if (emit_end) {
      int start = offsets_[cursor_end];
      int end = offsets_end_[cursor_end];
      token_str_debug_.emplace_back(str_.data() + start, end - start);
    }
#endif

    cursor_begin += emit_start;
    cursor_end += emit_end;
  }
}

void TokenBuffer::tokenize(const CharClass char_class_table[128])
{
  /* Ensure enough space for the worse scenario, which is one token per character.
   * This is done in order to avoid allocation and check inside the hot loop. */
  reserve(str_.size());

  if (str_.size() == 0) {
    size_ = 0;
    types_[0] = EndOfFile;
    return;
  }

  /**
   * The goal of this function is to build the offset and type buffers describing the tokens.
   * The type of the token is decided by the character class of its first character. This is a
   * simple lookup from the `char_class_table` combined with a `select`. It is then merged with the
   * preceding character if the classes are compatible (see `CharClass::CanMerge`).
   *
   * The offsets define the boundaries of the tokens.
   * If whitespaces are treated as tokens, one offset is emitted for the beginning of each token.
   *
   * str:      i n t   a = 0 ;   EndOfFile
   * emit:     1 0 0 0 1 1 1 1 1 1
   * offsets:  0       4   6 7 8 9
   *
   * If whitespaces are merged with preceding tokens, one offset is emitted for the begining of
   * each token *and* at either the start of the next token or the start of the next whitespace.
   *
   * str:           i n t   a = 0 ;   EndOfFile
   * emit start:    1 0 0 0 1 1 1 1 0 1
   * emit end:      1 0 0 1 0 1 1 1 1 1
   * offsets start: 0       4 5 6 7   9
   * offsets end:         3   5 6 7 8 9
   */

  CharClass prev_value = CharClass::None;
  bool prev_whitespace;
  {
    /* First iteration needs to always emit start of the token. */
    const CharClass curr = char_class_table[str_[0]];
    types_[0] = select(str_[0], char(curr), curr > CharClass::ClassToTypeThreshold);
    offsets_[0] = 0;
    offsets_end_[0] = 0;
    prev_value = curr;
    prev_whitespace = curr == CharClass::WhiteSpace;
  }

  uint32_t offset = 1, cursor = 1, cursor_end = prev_whitespace ? 1 : 0;

  int stride = 64;

  /* Process until alignment to SIMD size is met. */
  size_t index_at_align = ((stride - (uintptr_t(str_.data() + 1) & (stride - 1))) & (stride - 1)) +
                          1;
  tokenize_scalar(offset,
                  cursor,
                  cursor_end,
                  prev_value,
                  prev_whitespace,
                  str_.size() > index_at_align ? index_at_align : str_.size(),
                  char_class_table);

#if defined(USE_NEON) || defined(USE_SSE4_2)
  using namespace lexit::simd;
  const uint8_t *str = (const uint8_t *)str_.data();
  const u8x128_table char_to_class = u8x128_table::load((const uint8_t *)char_class_table);

  for (; offset + stride <= str_.size(); offset += stride) {
#  ifdef LEXIT_DEBUG
    const std::string_view char_simd{str_.data() + offset, size_t(stride)};
#  endif
    const u8x64 c = u8x64::load(str + offset);
    const u8x64 curr_char_class = char_to_class[c];
    /* Shift and add the last iteration end token at the start of the vector. */
    const u8x64 prev_char_class = shift_lanes_right<1>(curr_char_class, uint8_t(prev_value));

    const u8x64 to_type_threshold{uint8_t(CharClass::ClassToTypeThreshold)};
    const u8x64 can_merge{uint8_t(CharClass::CanMerge)};
    const u8x64 curr_tok_type = select(c, curr_char_class, curr_char_class > to_type_threshold);
    const u8x64 emit = is_zero(curr_char_class & prev_char_class & can_merge);
    /* Store for next iteration. */
    prev_value = CharClass(curr_char_class.last());
    /* Start and end of token. See scalar version for documentation. */
    const u8x64 curr_ws = (curr_char_class == uint8_t(CharClass::WhiteSpace));
    const u8x64 curr_non_ws = ~curr_ws;
    const u8x64 emit_ws = emit & curr_ws;
    const u8x64 emit_start = emit & curr_non_ws;
    const u8x64 prev_non_ws = shift_lanes_right<1>(curr_non_ws, prev_whitespace ? 0x0 : 0xFF);
    const u8x64 follow_non_ws = emit_start & prev_non_ws;
    const u8x64 emit_end = emit_ws | follow_non_ws;
    /* Store for next iteration. */
    prev_whitespace = !curr_non_ws.last();
    uint64_t emit_end_mask = movemask(emit_end);
    uint64_t emit_start_mask = movemask(emit_start);

    /* Stream compaction of data based on the emit mask (0xFF == emit, 0x00 == skip).
     * Stores `data` compacted inside `data_out` starting from `data_out + cursor` and advance
     * `cursor` by the number of element compacted. */
    {
      auto [shuffle, popcount] = shuffle_indices_from_emit_mask(emit_start_mask);
      /* Move data to destination elements (compaction). */
      const u8x64 data_packed = u8x64_table(curr_tok_type)[shuffle];
      /* Write 16 types in the stream. */
      data_packed.store_unaligned((uint8_t *)&types_[cursor]);
      /* The offsets are contained inside the 8 bit shuffle vector.
       * We need to promote it to 32 bit before adding the base offset. */
      (u32x16(shuffle.lane(0)) + offset).store_unaligned(&offsets_[cursor + 0]);
      (u32x16(shuffle.lane(1)) + offset).store_unaligned(&offsets_[cursor + 16]);
      (u32x16(shuffle.lane(2)) + offset).store_unaligned(&offsets_[cursor + 32]);
      (u32x16(shuffle.lane(3)) + offset).store_unaligned(&offsets_[cursor + 48]);
#  ifdef LEXIT_DEBUG
      for (int i = cursor; i < cursor + popcount; i++) {
        int start = offsets_[i - 1];
        int end = offsets_[i];
        token_str_with_whitespace_debug_.emplace_back(str_.data() + start, end - start);
      }
#  endif
      cursor += popcount;
    }
    {
      auto [shuffle, popcount] = shuffle_indices_from_emit_mask(emit_end_mask);
      /* The offsets are contained inside the 8 bit shuffle vector.
       * We need to promote it to 32 bit before adding the base offset. */
      (u32x16(shuffle.lane(0)) + offset).store_unaligned(&offsets_end_[cursor_end + 0]);
      (u32x16(shuffle.lane(1)) + offset).store_unaligned(&offsets_end_[cursor_end + 16]);
      (u32x16(shuffle.lane(2)) + offset).store_unaligned(&offsets_end_[cursor_end + 32]);
      (u32x16(shuffle.lane(3)) + offset).store_unaligned(&offsets_end_[cursor_end + 48]);
#  ifdef LEXIT_DEBUG
      for (int i = cursor_end; i < cursor_end + popcount; i++) {
        int start = offsets_[i];
        int end = offsets_end_[i];
        token_str_debug_.emplace_back(str_.data() + start, end - start);
      }
#  endif
      cursor_end += popcount;
    }
    assert(cursor_end == cursor || cursor_end + 1 == cursor);
  }
#endif

  assert(cursor_end == cursor || cursor_end + 1 == cursor);

  /* Finish tail using scalar loop. */
  tokenize_scalar(
      offset, cursor, cursor_end, prev_value, prev_whitespace, str_.size(), char_class_table);

  assert(cursor_end == cursor || cursor_end + 1 == cursor);

  /* Set end of last token. */
  offsets_[cursor] = str_.size();
  offsets_end_[cursor_end] = str_.size();
  /* Set end of file token. */
  types_[cursor] = EndOfFile;
  size_ = cursor;
}

void TokenBuffer::compute_lengths()
{
  uint32_t tok_id = 0;
#if defined(USE_NEON) || defined(USE_SSE4_2)
  using namespace simd;
  static constexpr int stride = 64;
  for (; tok_id + stride <= size_; tok_id += stride) {
    const u32x64 str_start = u32x64::load(&offsets_[tok_id]);
    const u32x64 str_end = u32x64::load(&offsets_end_[tok_id]);
    const u32x64 str_size_32 = str_end - str_start;
    const u8x64 str_large = u8x64(str_size_32 > 127);
    /* Saturate the size to max int8_t since SSE comparison is signed. */
    const u8x64 str_size = select(u8x64(str_size_32), u8x64(127), str_large);
    str_size.store(&lengths_[tok_id]);
  }
  /* Finish tail using scalar loop. */
#endif
  for (; tok_id < size_; ++tok_id) {
    const uint32_t str_start = offsets_[tok_id];
    const uint32_t str_end = offsets_end_[tok_id];
    const uint32_t str_size_32 = str_end - str_start;
    /* Saturate the size to max int8_t since SSE comparison is signed. */
    const uint8_t str_size = select(uint8_t(str_size_32), 127, str_size_32 > 127);
    lengths_[tok_id] = str_size;
  }
}

void TokenBuffer::atomize_words(IdentifierMap &identifiers, const KeywordTable &keywords)
{
  static constexpr int stride = 64;

  struct Masks {
    uint64_t mask8, mask16, mask24, mask32;
  };
  /* First scan to create a bitmap of potential matches to avoid wasting cycles iterating over
   * non-words. */
  lexit::Vector<Masks> masks_small_id;
  lexit::Vector<uint64_t> masks_large_id;
  masks_small_id.resize(divide_ceil(size_, stride));
  masks_large_id.resize(divide_ceil(size_, stride));

  {
    uint32_t chunk_id = 0;
    uint32_t tok_id = 0;
#if defined(USE_NEON) || defined(USE_SSE4_2)
    using namespace simd;
    for (; tok_id + stride <= size_; tok_id += stride, ++chunk_id) {
      const u8x64 size = u8x64::load(&lengths_[tok_id]);
      const u8x64 type = u8x64::load((uint8_t *)&types_[tok_id]);
      const uint64_t is_word = movemask(type == Word);
      const uint64_t less_8 = movemask(size < 9);
      const uint64_t less_16 = movemask(size < 17);
      const uint64_t less_24 = movemask(size < 25);
      const uint64_t less_32 = movemask(size < 33);

      Masks small;
      small.mask8 = is_word & less_8;
      small.mask16 = is_word & less_16 & ~less_8;
      small.mask24 = is_word & less_24 & ~less_16;
      small.mask32 = is_word & less_32 & ~less_24;
      masks_small_id[chunk_id] = small;
      masks_large_id[chunk_id] = is_word & ~less_32;
    }
#endif
    for (; tok_id < size_; tok_id += stride, ++chunk_id) {
      uint64_t is_word = 0, less_8 = 0, less_16 = 0, less_24 = 0, less_32 = 0;

      for (int i = 0; i < stride && tok_id + i < size_; ++i) {
        const uint8_t size = lengths_[tok_id + i];
        const TokenType type = types_[tok_id + i];
        const uint64_t bit = uint64_t(1) << i;
        /* Generate a masks of all 1s if true, all 0s if false. */
        is_word |= bit & -uint64_t(type == Word);
        less_8 |= bit & -uint64_t(size < 9);
        less_16 |= bit & -uint64_t(size < 17);
        less_24 |= bit & -uint64_t(size < 25);
        less_32 |= bit & -uint64_t(size < 33);
      }

      Masks small;
      small.mask8 = is_word & less_8;
      small.mask16 = is_word & less_16 & ~less_8;
      small.mask24 = is_word & less_24 & ~less_16;
      small.mask32 = is_word & less_32 & ~less_24;
      masks_small_id[chunk_id] = small;
      masks_large_id[chunk_id] = is_word & ~less_32;
    }
  }
  {
    /* Iterate over the bitmasks. */
    const int end = divide_ceil(size_, stride);
    /* The atomize_short_tokens_in_mask can read past the end of each token by 8 bytes.
     * For this reason we process the last chunks that contains the last 8 tokens separately. */
    const int end_safe = divide_ceil(int(size_) - 8, stride) - 1;
    int chunk = 0;
    for (; chunk < end_safe; ++chunk) {
      const Masks small = masks_small_id[chunk];
      atomize_short_tokens_in_mask<1>(small.mask8, chunk * stride, identifiers, keywords);
      atomize_short_tokens_in_mask<2>(small.mask16, chunk * stride, identifiers, keywords);
      atomize_short_tokens_in_mask<3>(small.mask24, chunk * stride, identifiers, keywords);
      atomize_short_tokens_in_mask<4>(small.mask32, chunk * stride, identifiers, keywords);
      atomize_tokens_in_mask(masks_large_id[chunk], chunk * stride, identifiers, keywords);
    }
    for (; chunk < end; ++chunk) {
      const Masks small = masks_small_id[chunk];
      atomize_tokens_in_mask(small.mask8, chunk * stride, identifiers, keywords);
      atomize_tokens_in_mask(small.mask16, chunk * stride, identifiers, keywords);
      atomize_tokens_in_mask(small.mask24, chunk * stride, identifiers, keywords);
      atomize_tokens_in_mask(small.mask32, chunk * stride, identifiers, keywords);
      atomize_tokens_in_mask(masks_large_id[chunk], chunk * stride, identifiers, keywords);
    }
  }
}

INLINE_METHOD void TokenBuffer::atomize_tokens_in_mask(uint64_t mask,
                                                       uint32_t tok_id_base,
                                                       IdentifierMap &id_map,
                                                       const KeywordTable &kw_table)
{
  if (mask == 0) [[likely]] {
    return;
  }
  while (mask != 0) {
    const int index = builtin_ctzll(mask);
    const int tok_id = tok_id_base + index;

    const int str_start = offsets_[tok_id];
    const int str_size = offsets_end_[tok_id] - str_start;
    const std::string_view str = {str_.data() + str_start, size_t(str_size)};

    const TokenAtom atom = id_map.lookup_or_add(str);
    atoms_[tok_id] = atom;
    types_[tok_id] = kw_table[atom];
    /* Pop last bit. */
    mask &= (mask - 1);
  }
}

template<int Size>
INLINE_METHOD void TokenBuffer::atomize_short_tokens_in_mask(uint64_t mask,
                                                             uint32_t tok_id_base,
                                                             IdentifierMap &id_map,
                                                             const KeywordTable &kw_table)
{
  while (mask != 0) {
    const int index = builtin_ctzll(mask);
    const int tok_id = tok_id_base + index;

    const int str_start = offsets_[tok_id];
    const int str_size = lengths_[tok_id]; /* PaddedString is for small identifier. */

    const std::string_view str = {str_.data() + str_start, size_t(str_size)};
    const PaddedString<Size> padded_str(str);

    const TokenAtom atom = id_map.lookup_or_add(padded_str);
    atoms_[tok_id] = atom;
    types_[tok_id] = kw_table[atom];
    /* Pop last bit. */
    mask &= (mask - 1);
  }
}

template void TokenBuffer::atomize_short_tokens_in_mask<1>(uint64_t,
                                                           uint32_t,
                                                           IdentifierMap &,
                                                           const KeywordTable &);
template void TokenBuffer::atomize_short_tokens_in_mask<2>(uint64_t,
                                                           uint32_t,
                                                           IdentifierMap &,
                                                           const KeywordTable &);
template void TokenBuffer::atomize_short_tokens_in_mask<3>(uint64_t,
                                                           uint32_t,
                                                           IdentifierMap &,
                                                           const KeywordTable &);
template void TokenBuffer::atomize_short_tokens_in_mask<4>(uint64_t,
                                                           uint32_t,
                                                           IdentifierMap &,
                                                           const KeywordTable &);

static void lex_string(const TokenType *types, uint32_t &cursor)
{
  const TokenType *ptr = types + cursor;
  while (true) {
    cursor++;
    ptr++;
    if (*ptr == '\\') {
      /* Escaped character. Skip next. */
      cursor++;
      ptr++;
      continue;
    }
    if (*ptr == String || *ptr == EndOfFile) {
      return;
    }
  }
}

static void lex_float(const std::string_view str,
                      const TokenType *types,
                      const uint32_t *offsets,
                      const uint32_t *offsets_end_,
                      uint32_t &cursor)
{
  const TokenType *type = types + cursor;
  const uint32_t *offset = offsets + cursor;
  const uint32_t *offset_end_ = offsets_end_ + cursor;

  /* If number is followed by whitespace itself. */
  const bool followed_by_whitespace = offset_end_[0] != offset[1];
  if (followed_by_whitespace) {
    return;
  }

  while (true) {
    cursor++;
    type++;
    offset++;
    offset_end_++;

    /* Check if the previous char was an exponent "e" char. */
    if ((*type == '+' || *type == '-') && str[*offset - 1] != 'e') {
      break;
    }
    if (!(*type == Word || *type == Number || *type == '.' || *type == '+' || *type == '-')) {
      break;
    }
    /* Break if this token is part of the number but followed by a whitespace. */
    const bool followed_by_whitespace = offset_end_[0] != offset[1];
    if (followed_by_whitespace) {
      /* Note: we don't want to do the cursor roll back since we want to merge this token. */
      return;
    }
  }
  /* We need to evaluate the token we broke on. */
  cursor--;
}

static void lex_comment(const std::string_view str,
                        const TokenType *types,
                        const uint32_t *offsets,
                        uint32_t &cursor,
                        TokenType &out_type)
{
  const uint32_t start = offsets[cursor];
  const TokenType *type = types + cursor;
  const char c = str[start + 1];

  size_t end_pos;
  if (c == '/') {
    /* Single-line comment. Search for end of line. */
    end_pos = str.find('\n', start + 2);
  }
  else if (c == '*') {
    /* Multi-line comment. Search for termination. */
    end_pos = str.find("*/", start + 2);
    /* Search for the closing slash. */
    end_pos += (end_pos != std::string::npos) ? 1 : 0;
  }
  else {
    /* Not a comment. */
    return;
  }

  out_type = Comment;

  while (*type != EndOfFile) {
    const uint32_t tok_start = offsets[cursor];
    /* Skip tokens until we find the one that starts after the end of the comment. */
    if (tok_start > end_pos) {
      break;
    }
    cursor++;
    type++;
  }
  /* We need to evaluate the token we broke on. */
  cursor--;
}

/* Ideally this should not be needed and the `tokenize` step should just parse them correctly.
 * But this would much harder to implement. */
void TokenBuffer::merge_complex_literals()
{
  const TokenType *in_types = types_.get();
  TokenType *out_type = types_.get();
  const uint32_t *in_offsets = offsets_.get();
  const uint32_t *in_offset_end = offsets_end_.get();
  uint32_t *out_offset = offsets_.get();
  uint32_t *out_offset_end = offsets_end_.get();

  for (uint32_t i = 0; i < size_; i++, out_type++, out_offset++, out_offset_end++) {
    const TokenType type = in_types[i];
    const uint32_t offset = in_offsets[i];
    const uint32_t offset_end = in_offset_end[i];
    *out_type = type;
    *out_offset = offset;
    *out_offset_end = offset_end;

    switch (type) {
      case String:
        lex_string(in_types, i);
        break;
      case Number:
        lex_float(str_, in_types, in_offsets, in_offset_end, i);
        break;
      case Slash:
        lex_comment(str_, in_types, in_offsets, i, *out_type);
        break;
      default:
        continue;
    }
    /* Set the correct end for the complex token that have just been lexed. */
    *out_offset_end = in_offset_end[i];
  }

  assert(in_types <= out_type);
  assert(out_type - in_types < 0xFFFFFFFFu);
  size_ = out_type - in_types;
  types_[size_] = EndOfFile;
  offsets_[size_] = str_.size();
}

}  // namespace lexit
