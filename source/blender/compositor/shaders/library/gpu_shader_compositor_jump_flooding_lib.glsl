/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* A special value that indicates that the pixel has not be flooded yet, and consequently is not a
 * seed pixel. */
#define JUMP_FLOODING_NON_FLOODED_VALUE ivec2(-1)

/* Given the texel location of the closest seed pixel and whether the pixel is flooded, encode that
 * information in an ivec2. */
ivec2 encode_jump_flooding_value(ivec2 closest_seed_texel, bool is_flooded)
{
  return is_flooded ? closest_seed_texel : JUMP_FLOODING_NON_FLOODED_VALUE;
}

/* Initialize the pixel at the given texel location for the algorithm as being seed or background.
 * This essentially calls encode_jump_flooding_value with the texel location, because the pixel is
 * the closest seed to itself. */
ivec2 initialize_jump_flooding_value(ivec2 texel, bool is_seed)
{
  return encode_jump_flooding_value(texel, is_seed);
}
