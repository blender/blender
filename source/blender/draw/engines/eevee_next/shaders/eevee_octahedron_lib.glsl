/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Convert from a cube-map vector to an octahedron UV coordinate.
 */
vec2 octahedral_uv_from_direction(vec3 co)
{
  /* Projection onto octahedron. */
  co /= dot(vec3(1.0), abs(co));

  /* Out-folding of the downward faces. */
  if (co.z < 0.0) {
    vec2 sign = step(0.0, co.xy) * 2.0 - 1.0;
    co.xy = (1.0 - abs(co.yx)) * sign;
  }

  /* Mapping to [0;1]^2 texture space. */
  vec2 uvs = co.xy * (0.5) + 0.5;

  return uvs;
}

vec3 octahedral_uv_to_direction(vec2 co)
{
  /* Change range to between [-1..1] */
  co = co * 2.0 - 1.0;

  vec2 abs_co = abs(co);
  vec3 v = vec3(co, 1.0 - (abs_co.x + abs_co.y));

  if (abs_co.x + abs_co.y > 1.0) {
    v.xy = (abs(co.yx) - 1.0) * -sign(co.xy);
  }

  return v;
}

/**
 * Return the octahedral uv coordinates for the given texture uv coordinate on the packed
 * octahedral texture layer for the given probe.
 *
 * It also applies wrapping in the additional space near borders.
 * NOTE: Doesn't apply the translation part of the packing.
 */
vec2 octahedral_uv_from_layer_texture_coords(vec2 uv, vec2 texel_size)
{
  /* Apply border region. */
  vec2 shrinked_uv = (uv - REFLECTION_PROBE_BORDER_SIZE * texel_size) /
                     (1.0 - 2.0 * REFLECTION_PROBE_BORDER_SIZE * texel_size);
  /* Use ping/pong to extend the octahedral coordinates. */
  vec2 translated_pos = clamp(-sign(shrinked_uv), vec2(0.0), vec2(1.0)) * vec2(2.0) + shrinked_uv;
  ivec2 checker_pos = ivec2(translated_pos);
  bool is_even = ((checker_pos.x + checker_pos.y) & 1) == 0;
  return is_even ? fract(shrinked_uv) : vec2(1.0) - fract(shrinked_uv);
}
