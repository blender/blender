/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_hash.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)

vec2 calc_brick_texture(vec3 p,
                        float mortar_size,
                        float mortar_smooth,
                        float bias,
                        float brick_width,
                        float row_height,
                        float offset_amount,
                        int offset_frequency,
                        float squash_amount,
                        int squash_frequency)
{
  int bricknum, rownum;
  float offset = 0.0;
  float x, y;

  rownum = floor_to_int(p.y / row_height);

  if (offset_frequency != 0 && squash_frequency != 0) {
    brick_width *= (rownum % squash_frequency != 0) ? 1.0 : squash_amount;           /* squash */
    offset = (rownum % offset_frequency != 0) ? 0.0 : (brick_width * offset_amount); /* offset */
  }

  bricknum = floor_to_int((p.x + offset) / brick_width);

  x = (p.x + offset) - brick_width * bricknum;
  y = p.y - row_height * rownum;

  float tint = clamp((integer_noise((rownum << 16) + (bricknum & 0xFFFF)) + bias), 0.0, 1.0);

  float min_dist = min(min(x, y), min(brick_width - x, row_height - y));
  if (min_dist >= mortar_size) {
    return vec2(tint, 0.0);
  }
  else if (mortar_smooth == 0.0) {
    return vec2(tint, 1.0);
  }
  else {
    min_dist = 1.0 - min_dist / mortar_size;
    return vec2(tint, smoothstep(0.0, mortar_smooth, min_dist));
  }
}

void node_tex_brick(vec3 co,
                    vec4 color1,
                    vec4 color2,
                    vec4 mortar,
                    float scale,
                    float mortar_size,
                    float mortar_smooth,
                    float bias,
                    float brick_width,
                    float row_height,
                    float offset_amount,
                    float offset_frequency,
                    float squash_amount,
                    float squash_frequency,
                    out vec4 color,
                    out float fac)
{
  vec2 f2 = calc_brick_texture(co * scale,
                               mortar_size,
                               mortar_smooth,
                               bias,
                               brick_width,
                               row_height,
                               offset_amount,
                               int(offset_frequency),
                               squash_amount,
                               int(squash_frequency));
  float tint = f2.x;
  float f = f2.y;
  if (f != 1.0) {
    float facm = 1.0 - tint;
    color1 = facm * color1 + tint * color2;
  }
  color = mix(color1, mortar, f);
  fac = f;
}
