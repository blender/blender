/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: MIT */

/**
 * Small SIMD library to avoid duplicating code.
 */

#pragma once

#include <cassert>
#include <cstdint>

#if defined(__ARM_NEON)
#  define USE_NEON
#  include <arm_neon.h>
#endif

#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_2__)
#  define USE_SSE4_2
#  include <immintrin.h>
#endif

#if defined(USE_NEON) || defined(USE_SSE4_2)

namespace lexit::simd {

/* Size must be power of 2. */
template<int Size> struct u8_base {
#  if defined(USE_NEON)
  uint8x16_t lanes[Size];
#  elif defined(USE_SSE4_2)
  __m128i lanes[Size];
#  endif

  u8_base() = default;

  explicit u8_base(const uint8_t scalar)
  {
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      lanes[i] = vdupq_n_u8(scalar);
#  elif defined(USE_SSE4_2)
      lanes[i] = _mm_set1_epi8(scalar);
#  endif
    }
  }

  static u8_base load_unaligned(const uint8_t *src)
  {
    u8_base result;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      result.lanes[i] = vld1q_u8(src + i * 16);
#  elif defined(USE_SSE4_2)
      result.lanes[i] = _mm_loadu_si128((const __m128i *)src + i);
#  endif
    }
    return result;
  }

  static u8_base load(const uint8_t *src)
  {
    assert((intptr_t(src) & (16 - 1)) == 0);
    u8_base result;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      result.lanes[i] = vld1q_u8(src + i * 16);
#  elif defined(USE_SSE4_2)
      result.lanes[i] = _mm_load_si128((const __m128i *)src + i);
#  endif
    }
    return result;
  }

  void store_unaligned(uint8_t *dst) const
  {
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      vst1q_u8(dst + i * 16, lanes[i]);
#  elif defined(USE_SSE4_2)
      _mm_storeu_si128((__m128i *)dst + i, lanes[i]);
#  endif
    }
  }

  void store(uint8_t *dst) const
  {
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      vst1q_u8(dst + i * 16, lanes[i]);
#  elif defined(USE_SSE4_2)
      _mm_store_si128((__m128i *)dst + i, lanes[i]);
#  endif
    }
  }

  u8_base<1> lane(int i) const
  {
    u8_base<1> result;
    result.lanes[0] = lanes[i];
    return result;
  }

  /* Get content of end lane */
  uint8_t last() const
  {
#  if defined(USE_NEON)
    return vgetq_lane_u8(lanes[Size - 1], 15);
#  elif defined(USE_SSE4_2)
    auto lane = lanes[Size - 1];
    return _mm_extract_epi8(lane, 15);
#  endif
  }

  /* Get content of end lane */
  uint8_t first() const
  {
#  if defined(USE_NEON)
    return vgetq_lane_u8(lanes[0], 0);
#  elif defined(USE_SSE4_2)
    return _mm_extract_epi8(lanes[0], 0);
#  endif
  }

  /* --- Bitwise Operators --- */

  friend u8_base operator^(u8_base a, u8_base b)
  {
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = veorq_u8(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_xor_si128(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }

  friend u8_base operator|(u8_base a, u8_base b)
  {
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vorrq_u8(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_or_si128(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }

  u8_base &operator|=(u8_base b)
  {
    *this = *this | b;
    return *this;
  }

  friend u8_base operator&(u8_base a, u8_base b)
  {
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vandq_u8(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_and_si128(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }

  friend u8_base operator&(u8_base a, uint8_t b)
  {
#  if defined(USE_NEON)
    uint8x16_t ref = vdupq_n_u8(b);
#  elif defined(USE_SSE4_2)
    __m128i ref = _mm_set1_epi8(b);
#  endif
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vandq_u8(a.lanes[i], ref);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_and_si128(a.lanes[i], ref);
#  endif
    }
    return res;
  }

  u8_base operator~() const
  {
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vmvnq_u8(lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_xor_si128(lanes[i], _mm_set1_epi8(-1));
#  endif
    }
    return res;
  }

  /* --- Arithmetic Operators --- */

  friend u8_base operator+(u8_base a, u8_base b)
  {
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vaddq_u8(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_add_epi8(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }

  friend u8_base operator+(u8_base a, uint8_t b)
  {
#  if defined(USE_NEON)
    uint8x16_t ref = vdupq_n_u8(b);
#  elif defined(USE_SSE4_2)
    __m128i ref = _mm_set1_epi8(b);
#  endif
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vaddq_u8(a.lanes[i], ref);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_add_epi8(a.lanes[i], ref);
#  endif
    }
    return res;
  }

  friend u8_base operator-(u8_base a, u8_base b)
  {
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vsubq_u8(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_sub_epi8(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }

  /* --- Comparison Operators --- */

  /** WARNING: Signed comparison on SSE. Will not work for input greater than 127. */
  friend u8_base operator>(u8_base a, u8_base b)
  {
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vcgtq_u8(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_cmpgt_epi8(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }

  /** WARNING: Signed comparison on SSE. Will not work for input greater than 127. */
  friend u8_base operator<(u8_base a, u8_base b)
  {
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vcltq_u8(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_cmplt_epi8(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }

  /** WARNING: Signed comparison on SSE. Will not work for input greater than 127. */
  friend u8_base operator<(u8_base a, uint8_t b)
  {
#  if defined(USE_NEON)
    uint8x16_t ref = vdupq_n_u8(b);
#  elif defined(USE_SSE4_2)
    __m128i ref = _mm_set1_epi8(b);
#  endif
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vcltq_u8(a.lanes[i], ref);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_cmplt_epi8(a.lanes[i], ref);
#  endif
    }
    return res;
  }

  friend u8_base operator==(u8_base a, uint8_t b)
  {
#  if defined(USE_NEON)
    uint8x16_t ref = vdupq_n_u8(b);
#  elif defined(USE_SSE4_2)
    __m128i ref = _mm_set1_epi8(b);
#  endif
    u8_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vceqq_u8(a.lanes[i], ref);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_cmpeq_epi8(a.lanes[i], ref);
#  endif
    }
    return res;
  }
};

using u8x16 = u8_base<1>;
using u8x32 = u8_base<2>;
using u8x64 = u8_base<4>;

struct u8x16_table {
  u8x16 table;

  u8x16_table() = default;
  u8x16_table(u8x16 table) : table(table) {}

  static u8x16_table load_unaligned(const uint8_t *src)
  {
    u8x16_table table;
    table.table = u8x16::load_unaligned(src);
    return table;
  }

  static u8x16_table load(const uint8_t *src)
  {
    u8x16_table table;
    table.table = u8x16::load(src);
    return table;
  }

  template<int Size> u8_base<Size> operator[](u8_base<Size> index) const
  {
    u8_base<Size> result;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      result.lanes[i] = vqtbl1q_u8(table.lanes[0], index.lanes[i]);
#  elif defined(USE_SSE4_2)
      /* Make sure to mimic the NEON behavior and return 0 on overflow.
       * _mm_shuffle_epi8 will only return zero if the MSB is high. */
      __m128i out_of_range = _mm_cmpgt_epi8(index.lanes[i], _mm_set1_epi8(15));
      __m128i safe_indices = _mm_or_si128(index.lanes[i], out_of_range);
      result.lanes[i] = _mm_shuffle_epi8(table.lanes[0], safe_indices);
#  endif
    }
    return result;
  }

  /* Only valid if all indices are less than 16. */
  template<int Size> u8_base<Size> unsafe_shuffle(u8_base<Size> index) const
  {
    u8_base<Size> result;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      result.lanes[i] = vqtbl1q_u8(table.lanes[0], index.lanes[i]);
#  elif defined(USE_SSE4_2)
      result.lanes[i] = _mm_shuffle_epi8(table.lanes[0], index.lanes[i]);
#  endif
    }
    return result;
  }
};

struct u8x64_table {
#  if defined(USE_NEON)
  uint8x16x4_t table;
#  elif defined(USE_SSE4_2)
  u8x16_table tables[4];
#  endif

  u8x64_table() = default;
#  if defined(USE_NEON)
  u8x64_table(u8x64 table)
      : table({table.lanes[0], table.lanes[1], table.lanes[2], table.lanes[3]})
  {
  }
#  elif defined(USE_SSE4_2)
  u8x64_table(u8x64 table)
  {
    tables[0].table = table.lane(0);
    tables[1].table = table.lane(1);
    tables[2].table = table.lane(2);
    tables[3].table = table.lane(3);
  }
#  endif

  static u8x64_table load_unaligned(const uint8_t *src)
  {
    u8x64_table table;
#  if defined(USE_NEON)
    table.table = vld1q_u8_x4(src);
#  elif defined(USE_SSE4_2)
    for (int i = 0; i < 4; ++i) {
      table.tables[i] = u8x16_table::load_unaligned(src + i * 16);
    }
#  endif
    return table;
  }

  static u8x64_table load(const uint8_t *src)
  {
    assert((intptr_t(src) & 63) == 0);
    u8x64_table table;
#  if defined(USE_NEON)
    table.table = vld1q_u8_x4(src);
#  elif defined(USE_SSE4_2)
    for (int i = 0; i < 4; ++i) {
      table.tables[i] = u8x16_table::load(src + i * 16);
    }
#  endif
    return table;
  }

  template<int Size> u8_base<Size> operator[](u8_base<Size> index) const
  {
    u8_base<Size> result;
#  if defined(USE_NEON)
    for (int i = 0; i < Size; ++i) {
      result.lanes[i] = vqtbl4q_u8(table, index.lanes[i]);
    }
#  elif defined(USE_SSE4_2)
    result = tables[0][index];
    result |= tables[1][index ^ u8_base<Size>(0x10)];
    result |= tables[2][index ^ u8_base<Size>(0x20)];
    result |= tables[3][index ^ u8_base<Size>(0x30)];
#  endif
    return result;
  }
};

struct u8x128_table {
  u8x64_table tables[2];

  static u8x128_table load_unaligned(const uint8_t *src)
  {
    u8x128_table table;
    for (int i = 0; i < 2; ++i) {
      table.tables[i] = u8x64_table::load_unaligned(src + i * 64);
    }
    return table;
  }

  static u8x128_table load(const uint8_t *src)
  {
    u8x128_table table;
    for (int i = 0; i < 2; ++i) {
      table.tables[i] = u8x64_table::load(src + i * 64);
    }
    return table;
  }

  /* Perform a 128 bytes table lookup for each lane of the input vector.*/
  template<int Size> u8_base<Size> operator[](u8_base<Size> index) const
  {
    /* https://lemire.me/blog/2019/07/23/arbitrary-byte-to-byte-maps-using-arm-neon/
     * Table lookup on NEON will return 0 on overflow. Leverage this using XOR to swap which range
     * we are looking up and combine result using OR.
     * Note we make sure that SSE lookup have the same behavior Which is more costly
     * (more than 3x the number of instructions) it is then preferable to avoid this path. */
    return tables[0][index] | tables[1][index ^ u8_base<Size>(0x40)];
  }
};

/* Select A if mask is 0, B otherwise.
 * Mask is expected to be 0xFF or 0x00 for each component. */
template<int Size>
inline u8_base<Size> select(u8_base<Size> a, u8_base<Size> b, u8_base<Size> mask)
{
  u8_base<Size> result;
  for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
    result.lanes[i] = vbslq_u8(mask.lanes[i], b.lanes[i], a.lanes[i]);
#  elif defined(USE_SSE4_2)
    result.lanes[i] = _mm_blendv_epi8(a.lanes[i], b.lanes[i], mask.lanes[i]);
#  endif
  }
  return result;
}

/* fill_value is the lanes to shift in. */
template<int Shift, int Size>
inline u8_base<Size> shift_lanes_right(u8_base<Size> a, uint8_t fill_value)
{
  u8_base<Size> result;
  for (int i = Size - 1; i > 0; --i) {
#  if defined(USE_NEON)
    result.lanes[i] = vextq_u8(a.lanes[i - 1], a.lanes[i], 16 - Shift);
#  elif defined(USE_SSE4_2)
    result.lanes[i] = _mm_alignr_epi8(a.lanes[i], a.lanes[i - 1], 16 - Shift);
#  endif
  }
  u8_base<1> fill{fill_value};
#  if defined(USE_NEON)
  result.lanes[0] = vextq_u8(fill.lanes[0], a.lanes[0], 16 - Shift);
#  elif defined(USE_SSE4_2)
  result.lanes[0] = _mm_alignr_epi8(a.lanes[0], fill.lanes[0], 16 - Shift);
#  endif
  return result;
}

template<int Size> inline u8_base<Size> is_zero(u8_base<Size> a)
{
#  if defined(USE_SSE4_2)
  __m128i zero = _mm_setzero_si128();
#  endif
  u8_base<Size> result;
  for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
    result.lanes[i] = vceqzq_u8(a.lanes[i]);
#  elif defined(USE_SSE4_2)
    result.lanes[i] = _mm_cmpeq_epi8(a.lanes[i], zero);
#  endif
  }
  return result;
}

/* Create a bitmask from a u8x64 mask containing 0xFF or 0x00 inside each lane. */
inline uint64_t movemask(u8x64 mask)
{
  uint64_t result;

#  if defined(USE_NEON)
  const uint8x16_t bits = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
  /* Merge lanes with their neighbors (e.g. [1, 2, 4, 8, ...] > [3, 12, 48, ...]).
   * This is equivalent to merging using | and >> operations.
   * Doing it 3 time to collapse the 8 bits. */
  uint8x16_t sum0 = vpaddq_u8(vandq_u8(mask.lanes[0], bits), vandq_u8(mask.lanes[1], bits));
  uint8x16_t sum1 = vpaddq_u8(vandq_u8(mask.lanes[2], bits), vandq_u8(mask.lanes[3], bits));
  sum0 = vpaddq_u8(sum0, sum1);
  sum0 = vpaddq_u8(sum0, sum0);
  result = vgetq_lane_u64(vreinterpretq_u64_u8(sum0), 0);
#  elif defined(USE_SSE4_2)
  result = _mm_movemask_epi8(mask.lanes[3]);
  result = _mm_movemask_epi8(mask.lanes[2]) | (result << 16);
  result = _mm_movemask_epi8(mask.lanes[1]) | (result << 16);
  result = _mm_movemask_epi8(mask.lanes[0]) | (result << 16);
#  endif
  return result;
}

/* Create a bitmask from a u8x32 mask containing 0xFF or 0x00 inside each lane. */
inline uint32_t movemask(u8x32 mask)
{
  uint32_t result;

#  if defined(USE_NEON)
  const uint8x16_t bits = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
  /* Merge lanes with their neighbors (e.g. [1, 2, 4, 8, ...] > [3, 12, 48, ...]).
   * This is equivalent to merging using | and >> operations.
   * Doing it 3 time to collapse the 8 bits. */
  uint8x16_t sum = vpaddq_u8(vandq_u8(mask.lanes[0], bits), vandq_u8(mask.lanes[1], bits));
  sum = vpaddq_u8(sum, sum);
  sum = vpaddq_u8(sum, sum);
  result = vgetq_lane_u64(vreinterpretq_u64_u8(sum), 0);
#  elif defined(USE_SSE4_2)
  result = _mm_movemask_epi8(mask.lanes[1]);
  result = _mm_movemask_epi8(mask.lanes[0]) | (result << 16);
#  endif
  return result;
}

/* Create a bitmask from a u8x64 mask containing 0xFF or 0x00 inside each lane. */
inline uint16_t movemask(u8x16 mask)
{
  uint16_t result;

#  if defined(USE_NEON)
  const uint8x16_t bits = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
  uint8x16_t sum = vandq_u8(mask.lanes[0], bits);
  /* Merge lanes with their neighbors (e.g. [1, 2, 4, 8, ...] > [3, 12, 48, ...]).
   * This is equivalent to merging using | and >> operations.
   * Doing it 3 time to collapse the 8 bits. */
  sum = vpaddq_u8(sum, sum);
  sum = vpaddq_u8(sum, sum);
  sum = vpaddq_u8(sum, sum);
  result = vgetq_lane_u64(vreinterpretq_u64_u8(sum), 0);
#  elif defined(USE_SSE4_2)
  result = _mm_movemask_epi8(mask.lanes[0]);
#  endif
  return result;
}

/* Size must be power of 2. */
template<int Size> struct u16_base {
#  if defined(USE_NEON)
  uint16x8_t lanes[Size];
#  elif defined(USE_SSE4_2)
  __m128i lanes[Size];
#  endif

  u16_base() = default;

  explicit u16_base(const u8_base<Size / 2> v)
  {
#  if defined(USE_SSE4_2)
    __m128i zero = _mm_setzero_si128();
#  endif
    for (int i = 0; i < Size / 2; ++i) {
#  if defined(USE_NEON)
      lanes[i * 2 + 0] = vmovl_u8(vget_low_u8(v.lanes[i]));
      lanes[i * 2 + 1] = vmovl_u8(vget_high_u8(v.lanes[i]));
#  elif defined(USE_SSE4_2)
      lanes[i * 2 + 0] = _mm_unpacklo_epi8(v.lanes[i], zero);
      lanes[i * 2 + 1] = _mm_unpackhi_epi8(v.lanes[i], zero);
#  endif
    }
  }

  void store(uint16_t *dst) const
  {
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      vst1q_u16(dst + i * 8, lanes[i]);
#  elif defined(USE_SSE4_2)
      _mm_storeu_si128((__m128i *)dst + i, lanes[i]);
#  endif
    }
  }
};

using u16x16 = u16_base<2>;
using u16x64 = u16_base<8>;

/* Size must be power of 2. */
template<int Size> struct u32_base {
#  if defined(USE_NEON)
  uint32x4_t lanes[Size];
#  elif defined(USE_SSE4_2)
  __m128i lanes[Size];
#  endif

  u32_base() = default;

  explicit u32_base(const u8_base<Size / 4> v)
  {
    for (int i = 0; i < Size / 4; ++i) {
#  if defined(USE_NEON)
      uint16x8_t tmp_lo = vmovl_u8(vget_low_u8(v.lanes[i]));
      uint16x8_t tmp_hi = vmovl_u8(vget_high_u8(v.lanes[i]));
      lanes[i * 4 + 0] = vmovl_u16(vget_low_u16(tmp_lo));
      lanes[i * 4 + 1] = vmovl_u16(vget_high_u16(tmp_lo));
      lanes[i * 4 + 2] = vmovl_u16(vget_low_u16(tmp_hi));
      lanes[i * 4 + 3] = vmovl_u16(vget_high_u16(tmp_hi));
#  elif defined(USE_SSE4_2)
      lanes[i * 4 + 0] = _mm_cvtepu8_epi32(v.lanes[i]);
      lanes[i * 4 + 1] = _mm_cvtepu8_epi32(_mm_srli_si128(v.lanes[i], 4));
      lanes[i * 4 + 2] = _mm_cvtepu8_epi32(_mm_srli_si128(v.lanes[i], 8));
      lanes[i * 4 + 3] = _mm_cvtepu8_epi32(_mm_srli_si128(v.lanes[i], 12));
#  endif
    }
  }

  /* NOTE: This does a signed saturation on SSE. */
  explicit operator u8_base<Size / 4>() const
  {
    u8_base<Size / 4> res;
    for (int i = 0; i < Size / 4; ++i) {
#  if defined(USE_NEON)
      uint16x4_t n0 = vmovn_u32(lanes[i * 4 + 0]);
      uint16x4_t n1 = vmovn_u32(lanes[i * 4 + 1]);
      uint16x4_t n2 = vmovn_u32(lanes[i * 4 + 2]);
      uint16x4_t n3 = vmovn_u32(lanes[i * 4 + 3]);
      uint16x8_t q01 = vcombine_u16(n0, n1);
      uint16x8_t q23 = vcombine_u16(n2, n3);
      res.lanes[i] = vcombine_u8(vmovn_u16(q01), vmovn_u16(q23));
#  elif defined(USE_SSE4_2)
      __m128i pack_16_lo = _mm_packs_epi32(lanes[i * 4 + 0], lanes[i * 4 + 1]);
      __m128i pack_16_hi = _mm_packs_epi32(lanes[i * 4 + 2], lanes[i * 4 + 3]);
      res.lanes[i] = _mm_packs_epi16(pack_16_lo, pack_16_hi);
#  endif
    }
    return res;
  }

  static u32_base load_unaligned(const uint32_t *src)
  {
    u32_base result;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      result.lanes[i] = vld1q_u32(src + i * 4);
#  elif defined(USE_SSE4_2)
      result.lanes[i] = _mm_loadu_si128((const __m128i *)src + i);
#  endif
    }
    return result;
  }

  static u32_base load(const uint32_t *src)
  {
    assert((intptr_t(src) & (16 - 1)) == 0);
    u32_base result;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      result.lanes[i] = vld1q_u32(src + i * 4);
#  elif defined(USE_SSE4_2)
      result.lanes[i] = _mm_load_si128((const __m128i *)src + i);
#  endif
    }
    return result;
  }

  void store_unaligned(uint32_t *dst) const
  {
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      vst1q_u32(dst + i * 4, lanes[i]);
#  elif defined(USE_SSE4_2)
      _mm_storeu_si128((__m128i *)dst + i, lanes[i]);
#  endif
    }
  }

  void store(uint32_t *dst) const
  {
    assert((intptr_t(dst) & (16 - 1)) == 0);
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      vst1q_u32(dst + i * 4, lanes[i]);
#  elif defined(USE_SSE4_2)
      _mm_store_si128((__m128i *)dst + i, lanes[i]);
#  endif
    }
  }

  /* --- Arithmetic Operators --- */

  /* Note: Signed comparison on SSE. */
  friend u32_base operator<(u32_base a, u32_base b)
  {
    u32_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vcltq_u32(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_cmplt_epi32(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }

  /* Note: Signed comparison on SSE. */
  friend u32_base operator<(u32_base a, uint32_t b)
  {
#  if defined(USE_NEON)
    uint32x4_t tmp = vdupq_n_u32(b);
#  elif defined(USE_SSE4_2)
    __m128i tmp = _mm_set1_epi32(b);
#  endif

    u32_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vcltq_u32(a.lanes[i], tmp);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_cmplt_epi32(a.lanes[i], tmp);
#  endif
    }
    return res;
  }

  friend u32_base operator>(u32_base a, uint32_t b)
  {
#  if defined(USE_NEON)
    uint32x4_t tmp = vdupq_n_u32(b);
#  elif defined(USE_SSE4_2)
    __m128i tmp = _mm_set1_epi32(b);
#  endif

    u32_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vcgtq_u32(a.lanes[i], tmp);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_cmpgt_epi32(a.lanes[i], tmp);
#  endif
    }
    return res;
  }

  friend u32_base operator+(u32_base a, uint32_t b)
  {
#  if defined(USE_NEON)
    uint32x4_t tmp = vdupq_n_u32(b);
#  elif defined(USE_SSE4_2)
    __m128i tmp = _mm_set1_epi32(b);
#  endif

    u32_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vaddq_u32(a.lanes[i], tmp);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_add_epi32(a.lanes[i], tmp);
#  endif
    }
    return res;
  }

  friend u32_base operator-(u32_base a, u32_base b)
  {
    u32_base res;
    for (int i = 0; i < Size; ++i) {
#  if defined(USE_NEON)
      res.lanes[i] = vsubq_u32(a.lanes[i], b.lanes[i]);
#  elif defined(USE_SSE4_2)
      res.lanes[i] = _mm_sub_epi32(a.lanes[i], b.lanes[i]);
#  endif
    }
    return res;
  }
};

using u32x16 = u32_base<4>;
using u32x32 = u32_base<8>;
using u32x64 = u32_base<16>;

}  // namespace lexit::simd

#endif
