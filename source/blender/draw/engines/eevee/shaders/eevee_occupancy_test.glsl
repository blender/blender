/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#line 9

#include "eevee_occupancy_lib.glsl"
#include "gpu_shader_test_lib.glsl"

#define TEST(a, b) if (true)

void main()
{
  TEST(eevee_occupancy, Occupancy)
  {
    OccupancyBits occup;

    /* occupancy_from_depth */
    occup = occupancy_from_depth(0.1, 1);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0xFFFFFFFFu, ~0u, ~0u, ~0u));

    occup = occupancy_from_depth(0.6, 1);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0xFFFFFFFEu, ~0u, ~0u, ~0u));

    occup = occupancy_from_depth(0.5, 32);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0xFFFF0000u, ~0u, ~0u, ~0u));

    occup = occupancy_from_depth(0.5, 64);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0u, ~0u, ~0u, ~0u));

    occup = occupancy_from_depth(0.5, 128);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0u, 0u, ~0u, ~0u));

    occup = occupancy_from_depth(33.0 / 64.0, 64);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0u, 0xFFFFFFFEu, ~0u, ~0u));

    /* occupancy_bit_from_depth */
    occup = occupancy_bit_from_depth(0.1, 1);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0x00000001u, 0u, 0u, 0u));

    occup = occupancy_bit_from_depth(0.6, 1);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0x00000002u, 0u, 0u, 0u));

    occup = occupancy_bit_from_depth(0.5, 32);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0x00010000u, 0u, 0u, 0u));

    occup = occupancy_bit_from_depth(0.5, 64);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0x00000000u, 0x00000001u, 0u, 0u));

    occup = occupancy_bit_from_depth(0.5, 128);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0x00000000u, 0x00000000u, 0x00000001u, 0u));

    occup = occupancy_bit_from_depth(33.0 / 64.0, 64);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0x00000000u, 0x00000002u, 0u, 0u));

    /* Test composing occupancy an the expected result. */
    /* Start empty. */
    OccupancyBits entry = occupancy_new();
    OccupancyBits exit = occupancy_new();
    entry = occupancy_or(entry, occupancy_bit_from_depth(1.0 / 32.0, 32));
    /* Second entry at the same depth. Should not change anything. */
    entry = occupancy_or(entry, occupancy_bit_from_depth(1.1 / 32.0, 32));
    /* Exit 2 bits later. */
    exit = occupancy_or(exit, occupancy_bit_from_depth(3.0 / 32.0, 32));
    /* Second exit. Should not change anything. */
    exit = occupancy_or(exit, occupancy_bit_from_depth(5.0 / 32.0, 32));
    /* Third entry is valid. */
    entry = occupancy_or(entry, occupancy_bit_from_depth(7.0 / 32.0, 32));
    /* Third exit is valid. */
    exit = occupancy_or(exit, occupancy_bit_from_depth(9.0 / 32.0, 32));
    /* Fourth entry is valid. */
    entry = occupancy_or(entry, occupancy_bit_from_depth(11.0 / 32.0, 32));
    /* Fourth exit on the same depth. Cancels the occupancy. */
    exit = occupancy_or(exit, occupancy_bit_from_depth(11.0 / 32.0, 32));
    EXPECT_EQ(entry.bits[0], 2178u); /* 1000 1000 0010 */
    EXPECT_EQ(exit.bits[0], 2600u);  /* 1010 0010 1000 */

    occup = occupancy_resolve(entry, exit, 32);
    EXPECT_EQ(occup.bits[0], 390u); /* 0001 1000 0110 */

    /* Start empty. */
    entry = occupancy_new();
    exit = occupancy_new();
    /* First exit. Anything prior should be considered in volume. */
    exit = occupancy_or(exit, occupancy_bit_from_depth(33.0 / 44.0, 44));
    /* First entry. */
    entry = occupancy_or(entry, occupancy_bit_from_depth(36.0 / 44.0, 44));
    /* Second exit. Should not change anything. */
    exit = occupancy_or(exit, occupancy_bit_from_depth(40.0 / 44.0, 44));
    /* 0000 0001 0000   0000 0000 0000 0000  0000 0000 0000 0000 */
    EXPECT_EQ(occupancy_to_uint4(entry), uvec4(0x00000000u, 0x010u, 0u, 0u));
    /* 0001 0000 0010   0000 0000 0000 0000  0000 0000 0000 0000 */
    EXPECT_EQ(occupancy_to_uint4(exit), uvec4(0x00000000u, 0x102u, 0u, 0u));

    EXPECT_EQ(occupancy_find_lsb(entry), 36);
    EXPECT_EQ(occupancy_find_lsb(exit), 33);

    occup = occupancy_resolve(entry, exit, 44);
    /* 0000 1111 0001   1111 1111 1111 1111  1111 1111 1111 1111 */
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0xFFFFFFFFu, 0x0F1u, 0u, 0u));

    occup = occupancy_new();
    occup = occupancy_set_bits_high(occup, 16, 32);
    occup = occupancy_set_bits_high(occup, 80, 16);
    EXPECT_EQ(occupancy_to_uint4(occup), uvec4(0xFFFF0000u, 0x0000FFFFu, 0xFFFF0000u, 0u));
  }
}
