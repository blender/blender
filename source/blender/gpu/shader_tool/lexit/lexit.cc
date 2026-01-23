/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "lexit.hh"

#include <cassert>

#if defined(__ARM_NEON)
#  define USE_NEON
#  include <arm_neon.h>
#endif

#if (defined(__x86_64__) || defined(_M_X64))
#  define USE_SSE2
#  include <immintrin.h>
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

#if defined(USE_NEON) || defined(USE_SSE2)

/* Shuffle table used for stream compaction. */
static const uint8_t shuffle_table_8[256][8] = {
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

#endif

#if defined(USE_NEON)
static inline uint8x16_t simd_transform16_ascii(uint8x16x4_t table[2], uint8x16_t input)
{
  uint8x16_t t1 = vqtbl4q_u8(table[0], input);
  uint8x16_t t2 = vqtbl4q_u8(table[1], veorq_u8(input, vdupq_n_u8(0x40)));
  return vorrq_u8(t1, t2);
}
#endif

#ifdef USE_SSE2
static inline __m128i simd_transform16_ascii(const __m128i table[8], __m128i input)
{
  __m128i result = _mm_setzero_si128();
  __m128i high_nibble_mask = _mm_set1_epi8(0xF0);

  /* This replaces both vqtbl4q_u8 calls and the XOR/OR logic.
   * It covers the full ASCII range (0-127). */
  for (int i = 0; i < 8; ++i) {
    /* 1. Identify which bytes in 'input' fall in the current 16-byte range
     * Range i=0 is 0-15 (0x00), i=1 is 16-31 (0x10), ..., i=7 is 112-127 (0x70) */
    __m128i range_match = _mm_cmpeq_epi8(_mm_and_si128(input, high_nibble_mask),
                                         _mm_set1_epi8(i << 4));

    /* 2. Perform the shuffle. _mm_shuffle_epi8 only uses the low 4 bits of the index. */
    __m128i lookup = _mm_shuffle_epi8(table[i], input);

    /* 3. Mask the lookup so we only keep values that were actually in this range. */
    result = _mm_or_si128(result, _mm_and_si128(lookup, range_match));
  }

  return result;
}
#endif

void TokenBuffer::tokenize(const CharClass char_class_table[128])
{
  uint32_t offset = 0, cursor = 0;

#if defined(USE_SSE2)
  __m128i map_v[8];
  for (int i = 0; i < 8; ++i) {
    map_v[i] = _mm_loadu_si128((const __m128i *)char_class_table + i);
  }

  const __m128i mask_last = _mm_set_epi8(0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

  __m128i prev = _mm_set1_epi8(uint8_t(CharClass::None));

  for (; offset + 16 <= str_len_; offset += 16) {
    const __m128i c = _mm_loadu_si128((const __m128i *)(str_ + offset));
    const __m128i curr = simd_transform16_ascii(map_v, c);
    /* Check if token needs to always split. */
    const __m128i mask_t = _mm_cmpgt_epi8(curr,
                                          _mm_set1_epi8(int8_t(CharClass::ClassToTypeThreshold)));
    const __m128i type = _mm_blendv_epi8(c, curr, mask_t);
    /* Add the last iteration end token at the end of the vector. */
    prev = _mm_blendv_epi8(curr, prev, mask_last);
    /* Right shift elements (not bits) by 1. */
    prev = _mm_alignr_epi8(prev, prev, 15);
    /* Equivalent to: `!bool(curr & prev & CanMerge)`. */
    const __m128i can_merge = _mm_set1_epi8(uint8_t(CharClass::CanMerge));
    const __m128i combined = _mm_and_si128(_mm_and_si128(curr, prev), can_merge);
    const __m128i emit = _mm_cmpeq_epi8(combined, _mm_setzero_si128());

    /* Stream compaction of data based on the emit mask (0xFF == emit, 0x00 == skip).
     * Stores `data` compacted inside `data_out` starting from `data_out + cursor` and advance
     * `cursor` by the number of element compacted. */
    {
      uint32_t mask = _mm_movemask_epi8(emit);
      /* Process in two 8-byte chunks to match your existing 8-byte shuffle tables. */
      uint8_t mask_lo = uint8_t(mask);
      uint8_t mask_hi = uint8_t(mask >> 8);

      auto process_chunk = [&](uint8_t m, __m128i chunk_data, uint32_t current_offset) {
        /* SSE shuffle requires a 16-byte register, but we only use the low 8. */
        __m128i shuffle_vec = _mm_loadl_epi64((const __m128i *)shuffle_table_8[m]);
        __m128i compacted = _mm_shuffle_epi8(chunk_data, shuffle_vec);
        /* Write types (8 bytes). */
        _mm_storel_epi64((__m128i *)((uint8_t *)types_ + cursor), compacted);
        /* Promote 8-bit shuffles to 32-bit offsets */
        __m128i shuffle32_lo = _mm_cvtepu8_epi32(shuffle_vec);
        __m128i shuffle32_hi = _mm_cvtepu8_epi32(_mm_srli_si128(shuffle_vec, 4));

        __m128i base_off = _mm_set1_epi32(current_offset);
        _mm_storeu_si128((__m128i *)(offsets_ + cursor), _mm_add_epi32(shuffle32_lo, base_off));
        _mm_storeu_si128((__m128i *)(offsets_ + cursor + 4),
                         _mm_add_epi32(shuffle32_hi, base_off));

        cursor += count_bits_i(m);
      };

      /* Low 8 bytes. */
      process_chunk(mask_lo, type, offset);
      /* High 8 bytes (shift type right by 8 bytes). */
      process_chunk(mask_hi, _mm_srli_si128(type, 8), offset + 8);
    }

    prev = curr;
  }
  /* Finish tail using scalar loop. */
  CharClass last_type = (CharClass)_mm_extract_epi8(prev, 15);

#elif defined(USE_NEON)
  uint8x16x4_t map_v[2];
  map_v[0] = vld1q_u8_x4((const uint8_t *)char_class_table);
  map_v[1] = vld1q_u8_x4((const uint8_t *)char_class_table + sizeof(uint8x16x4_t));

  const uint8x16_t mask_last = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF};

  uint8x16_t prev = {uint8_t(CharClass::None)};
  for (; offset + 16 <= str_len_; offset += 16) {
    const uint8x16_t c = vld1q_u8(str_ + offset);
    const uint8x16_t curr = simd_transform16_ascii(map_v, c);
    /* (curr > ClassToTypeThreshold) ? TokenType(curr) : TokenType(c) */
    const uint8x16_t mask_t = vcgtq_s8(curr, vdupq_n_u8(uint8_t(CharClass::ClassToTypeThreshold)));
    /* Type to store. */
    const uint8x16_t type = vbslq_u8(mask_t, curr, c);
    /* Add the last iteration end token at the end of the vector. */
    prev = vbslq_u8(mask_last, prev, curr);
    /* Right shift elements (not bits) by 1. */
    prev = vextq_u8(prev, prev, 15);
    /* Equivalent to: `!bool(curr & prev & CanMerge)`. */
    const uint8x16_t can_merge = vdupq_n_u8(uint8_t(CharClass::CanMerge));
    const uint8x16_t emit = vceqq_u8(vandq_u8(vandq_u8(curr, prev), can_merge), vdupq_n_u8(0));

    /* Stream compaction of data based on the emit mask (0xFF == emit, 0x00 == skip).
     * Stores `data` compacted inside `data_out` starting from `data_out + cursor` and advance
     * `cursor` by the number of element compacted. */
    {
      /* Make it 1 bit valid element flag. */
      const uint8x16_t mask_comp = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
      uint8x16_t mask_vec = vandq_u8(emit, mask_comp);

      uint8x8_t data_lo = vget_low_u8(type);
      uint8x8_t data_hi = vget_high_u8(type);

      int32_t mask_lo = vaddv_u8(vget_low_u8(mask_vec));
      int32_t mask_hi = vaddv_u8(vget_high_u8(mask_vec));
      /* Lookup the shuffle vector. */
      uint8x8_t shuffle_lo = vld1_u8(shuffle_table_8[mask_lo]);
      uint8x8_t shuffle_hi = vld1_u8(shuffle_table_8[mask_hi]);
      /* Table lookup. */
      data_lo = vtbl1_u8(data_lo, shuffle_lo);
      data_hi = vtbl1_u8(data_hi, shuffle_hi);

      /* Write 8 types. */
      vst1_u8((uint8_t *)types_ + cursor, data_lo);
      /* Write 8 offsets. */
      uint32x4_t offset_vec_lo = vdupq_n_u32(offset);
      /* The offsets are contained inside the 8 bit shuffle vector.
       * We need to promote it to 32 bit before adding the base offset. */
      uint16x8_t shuffle_lo16 = vmovl_u8(shuffle_lo);
      uint32x4_t shuffle_lo32_lo = vmovl_u16(vget_low_u16(shuffle_lo16));
      uint32x4_t shuffle_lo32_hi = vmovl_u16(vget_high_u16(shuffle_lo16));
      uint32x4_t offset_lo_lo = vaddq_u32(shuffle_lo32_lo, offset_vec_lo);
      uint32x4_t offset_lo_hi = vaddq_u32(shuffle_lo32_hi, offset_vec_lo);
      vst1q_u32(offsets_ + cursor + 0, offset_lo_lo);
      vst1q_u32(offsets_ + cursor + 4, offset_lo_hi);

      cursor += count_bits_i(mask_lo);

      /* Write 8 types. */
      vst1_u8((uint8_t *)types_ + cursor, data_hi);
      /* Write 8 offsets. */
      uint32x4_t offset_vec_hi = vdupq_n_u32(offset + 8);
      /* The offsets are contained inside the 8 bit shuffle vector.
       * We need to promote it to 32 bit before adding the base offset. */
      uint16x8_t shuffle_hi16 = vmovl_u8(shuffle_hi);
      uint32x4_t shuffle_hi32_lo = vmovl_u16(vget_low_u16(shuffle_hi16));
      uint32x4_t shuffle_hi32_hi = vmovl_u16(vget_high_u16(shuffle_hi16));
      uint32x4_t offset_hi_lo = vaddq_u32(shuffle_hi32_lo, offset_vec_hi);
      uint32x4_t offset_hi_hi = vaddq_u32(shuffle_hi32_hi, offset_vec_hi);
      vst1q_u32(offsets_ + cursor + 0, offset_hi_lo);
      vst1q_u32(offsets_ + cursor + 4, offset_hi_hi);

      cursor += count_bits_i(mask_hi);
    }

    prev = curr;
  }
  /* Finish tail using scalar loop. */
  CharClass last_type = CharClass(vgetq_lane_u8(prev, 15));
#else

  /* Scalar only implementation. */
  CharClass last_type = CharClass::None;
#endif

  {
    CharClass prev = last_type;
    for (; offset < str_len_; offset += 1) {
      const char c = str_[offset];
      const CharClass curr = char_class_table[c];
      /* Its faster to overwrite the previous value with the same value
       * than having a condition. */
      types_[cursor] = (curr > CharClass::ClassToTypeThreshold) ? TokenType(curr) : TokenType(c);
      offsets_[cursor] = offset;
      /* Split if no class in common. */
      cursor += (uint8_t(curr) & uint8_t(prev) & uint8_t(CharClass::CanMerge)) == 0;
      prev = curr;
    }
  }

  /* Set end of last token. */
  offsets_[cursor] = str_len_;
  /* Set end of file token. */
  types_[cursor] = EndOfFile;

  size_ = cursor;
}

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

static void lex_number(const uint8_t *c_str,
                       const TokenType *types,
                       const uint32_t *offsets,
                       uint32_t &cursor)
{
  const TokenType *type = types + cursor;
  const uint32_t *offset = offsets + cursor;
  while (true) {
    cursor++;
    type++;
    offset++;
    /* Check if previous char was an exponent "e" char. */
    if ((*type == '+' || *type == '-') && c_str[*offset - 1] != 'e') {
      break;
    }
    if (!(*type == Word || *type == Number || *type == '.' || *type == '+' || *type == '-')) {
      break;
    }
  }
  /* We need to evaluate the token we broke on. */
  cursor--;
}

void TokenBuffer::merge_complex_literals()
{
  const TokenType *in_types = types_;
  TokenType *out_type = types_;
  const uint32_t *in_offsets = offsets_;
  uint32_t *out_offset = offsets_;

  for (uint32_t i = 0; i < size_; i++, out_type++, out_offset++) {
    const TokenType type = in_types[i];
    const uint32_t offset = in_offsets[i];
#ifndef NDEBUG
    std::string_view tok_str{(const char *)str_ + offset, in_offsets[i + 1] - offset};
#endif
    *out_type = type;
    *out_offset = offset;

    switch (type) {
      case String:
        lex_string(in_types, i);
        break;
      case Number:
        lex_number(str_, in_types, in_offsets, i);
        break;
      default:
        break;
    }
  }

  assert(in_types < out_type);
  assert(out_type - in_types < 0xFFFFFFFFu);
  size_ = out_type - in_types;
  types_[size_] = EndOfFile;
  offsets_[size_] = str_len_;
}

void TokenBuffer::merge_whitespaces()
{
  assert(original_offsets_ != nullptr);
  assert(original_offsets_ != offsets_);

  const TokenType *in_types = types_;
  TokenType *out_type = types_;
  const uint32_t *in_offsets = offsets_;
  uint32_t *out_offset = offsets_;
  uint32_t *out_original_offset = original_offsets_;

  *out_original_offset = 0;
  out_original_offset++;

  if (size_ > 0) {
    /* Iter 0. */
    *out_type = in_types[0];
    *out_offset = in_offsets[0];
    *out_original_offset = in_offsets[1];
    out_type++, out_offset++, out_original_offset++;
  }

  for (uint32_t i = 1; i < size_; i++, out_type++, out_offset++, out_original_offset++) {
    const TokenType type = in_types[i];
    const uint32_t offset = in_offsets[i];
#ifndef NDEBUG
    std::string_view tok_str{(const char *)str_ + offset, in_offsets[i + 1] - offset};
#endif
    *out_type = type;
    *out_offset = offset;
    *out_original_offset = in_offsets[i + 1];

    switch (type) {
      case NewLine:
      case Space:
        break;
      default:
        continue;
    }
    /* Make next token overwrite this one. Effectively merging the token with the one before. */
    out_type--, out_offset--, out_original_offset--;
  }

  assert(in_types < out_type);
  assert(out_type - in_types < 0xFFFFFFFFu);
  size_ = out_type - in_types;
  types_[size_] = EndOfFile;
  offsets_[size_] = str_len_;
  original_offsets_[size_] = str_len_;
}

}  // namespace lexit
