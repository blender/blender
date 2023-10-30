/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* A special value that indicates that the pixel has not be flooded yet, and consequently is not a
 * seed pixel. */
#define JUMP_FLOODING_NON_FLOODED_VALUE vec4(-1.0)

/* Returns true if the pixel whose value is given was flooded, false otherwise. */
bool is_jump_flooded(vec4 value)
{
  return all(notEqual(value, JUMP_FLOODING_NON_FLOODED_VALUE));
}

/* Given the position of the closest seed, the distance to it, and whether the pixel is flooded,
 * encode that information in a vec4 in a format expected by the algorithm and return it */
vec4 encode_jump_flooding_value(vec2 position_of_closest_seed, float dist, bool is_flooded)
{
  if (is_flooded) {
    return vec4(position_of_closest_seed, dist, 0.0);
  }
  return JUMP_FLOODING_NON_FLOODED_VALUE;
}

/* Initialize the pixel at the given texel location for the algorithm as being seed or background.
 * This essentially calls encode_jump_flooding_value with the texel location, because the pixel is
 * the closest seed to itself, and a distance of zero, because that's the distance to itself. */
vec4 initialize_jump_flooding_value(ivec2 texel, bool is_seed)
{
  return encode_jump_flooding_value(vec2(texel), 0.0, is_seed);
}

/* Extracts the texel location of the closest seed to the pixel of the given value. */
ivec2 extract_jump_flooding_closest_seed_texel(vec4 value)
{
  return ivec2(value.xy);
}

/* Extracts the distance to the closest seed to the pixel of the given value. */
float extract_jump_flooding_distance_to_closest_seed(vec4 value)
{
  return value.z;
}
