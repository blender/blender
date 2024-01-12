/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* This is identical to the EEVEE implementation in eevee_motion_blur_gather_comp.glsl with the
 * necessary adjustments to make it work for the compositor:
 *
 *   - depth_compare() uses an inverted sign since the depth texture stores linear depth.
 *   - The next velocities are inverted since the velocity textures stores the previous and next
 *     velocities in the same direction.
 *   - The samples count is a variable uniform and not fixed to 8 samples.
 *   - The depth scale is constant and set to 100.
 *   - The motion scale is defined by the shutter_speed. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_motion_blur_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

const float g_depth_scale = 100.0;

/* Interleaved gradient noise by Jorge Jimenez
 * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare. */
float interleaved_gradient_noise(ivec2 p)
{
  return fract(52.9829189 * fract(0.06711056 * p.x + 0.00583715 * p.y));
}

vec2 spread_compare(float center_motion_length, float sample_motion_length, float offset_length)
{
  return clamp(vec2(center_motion_length, sample_motion_length) - offset_length + 1.0, 0.0, 1.0);
}

vec2 depth_compare(float center_depth, float sample_depth)
{
  vec2 depth_scale = vec2(g_depth_scale, -g_depth_scale);
  return clamp(0.5 + depth_scale * (sample_depth - center_depth), 0.0, 1.0);
}

/* Kill contribution if not going the same direction. */
float dir_compare(vec2 offset, vec2 sample_motion, float sample_motion_length)
{
  if (sample_motion_length < 0.5) {
    return 1.0;
  }
  return (dot(offset, sample_motion) > 0.0) ? 1.0 : 0.0;
}

/* Return background (x) and foreground (y) weights. */
vec2 sample_weights(float center_depth,
                    float sample_depth,
                    float center_motion_length,
                    float sample_motion_length,
                    float offset_length)
{
  /* Classify foreground/background. */
  vec2 depth_weight = depth_compare(center_depth, sample_depth);
  /* Weight if sample is overlapping or under the center pixel. */
  vec2 spread_weight = spread_compare(center_motion_length, sample_motion_length, offset_length);
  return depth_weight * spread_weight;
}

struct Accumulator {
  vec4 fg;
  vec4 bg;
  /** x: Background, y: Foreground, z: dir. */
  vec3 weight;
};

void gather_sample(vec2 screen_uv,
                   float center_depth,
                   float center_motion_len,
                   vec2 offset,
                   float offset_len,
                   const bool next,
                   inout Accumulator accum)
{
  vec2 sample_uv = screen_uv - offset / vec2(texture_size(input_tx));
  vec4 sample_vectors = texture(velocity_tx, sample_uv) *
                        vec4(vec2(shutter_speed), vec2(-shutter_speed));
  vec2 sample_motion = (next) ? sample_vectors.zw : sample_vectors.xy;
  float sample_motion_len = length(sample_motion);
  float sample_depth = texture(depth_tx, sample_uv).r;
  vec4 sample_color = texture(input_tx, sample_uv);

  vec3 weights;
  weights.xy = sample_weights(
      center_depth, sample_depth, center_motion_len, sample_motion_len, offset_len);
  weights.z = dir_compare(offset, sample_motion, sample_motion_len);
  weights.xy *= weights.z;

  accum.fg += sample_color * weights.y;
  accum.bg += sample_color * weights.x;
  accum.weight += weights;
}

void gather_blur(vec2 screen_uv,
                 vec2 center_motion,
                 float center_depth,
                 vec2 max_motion,
                 float ofs,
                 const bool next,
                 inout Accumulator accum)
{
  float center_motion_len = length(center_motion);
  float max_motion_len = length(max_motion);

  /* Tile boundaries randomization can fetch a tile where there is less motion than this pixel.
   * Fix this by overriding the max_motion. */
  if (max_motion_len < center_motion_len) {
    max_motion_len = center_motion_len;
    max_motion = center_motion;
  }

  if (max_motion_len < 0.5) {
    return;
  }

  int i;
  float t, inc = 1.0 / float(samples_count);
  for (i = 0, t = ofs * inc; i < samples_count; i++, t += inc) {
    gather_sample(screen_uv,
                  center_depth,
                  center_motion_len,
                  max_motion * t,
                  max_motion_len * t,
                  next,
                  accum);
  }

  if (center_motion_len < 0.5) {
    return;
  }

  for (i = 0, t = ofs * inc; i < samples_count; i++, t += inc) {
    /* Also sample in center motion direction.
     * Allow recovering motion where there is conflicting
     * motion between foreground and background. */
    gather_sample(screen_uv,
                  center_depth,
                  center_motion_len,
                  center_motion * t,
                  center_motion_len * t,
                  next,
                  accum);
  }
}

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec2 uv = (vec2(texel) + 0.5) / vec2(texture_size(input_tx));

  /* Data of the center pixel of the gather (target). */
  float center_depth = texture_load(depth_tx, texel).x;
  vec4 center_motion = texture(velocity_tx, uv) * vec4(vec2(shutter_speed), vec2(-shutter_speed));
  vec4 center_color = textureLod(input_tx, uv, 0.0);

  /* Randomize tile boundary to avoid ugly discontinuities. Randomize 1/4th of the tile.
   * Note this randomize only in one direction but in practice it's enough. */
  float rand = interleaved_gradient_noise(texel);
  ivec2 tile = (texel + ivec2(rand * 2.0 - 1.0 * float(MOTION_BLUR_TILE_SIZE) * 0.25)) /
               MOTION_BLUR_TILE_SIZE;

  vec4 max_motion;
  /* Load dilation result from the indirection table. */
  ivec2 tile_prev;
  motion_blur_tile_indirection_load(tile_indirection_buf, MOTION_PREV, uvec2(tile), tile_prev);
  max_motion.xy = texture_load(max_velocity_tx, tile_prev).xy;
  ivec2 tile_next;
  motion_blur_tile_indirection_load(tile_indirection_buf, MOTION_NEXT, uvec2(tile), tile_next);
  max_motion.zw = texture_load(max_velocity_tx, tile_next).zw;

  max_motion *= vec4(vec2(shutter_speed), vec2(-shutter_speed));

  Accumulator accum;
  accum.weight = vec3(0.0, 0.0, 1.0);
  accum.bg = vec4(0.0);
  accum.fg = vec4(0.0);
  /* First linear gather. time = [T - delta, T] */
  gather_blur(uv, center_motion.xy, center_depth, max_motion.xy, rand, false, accum);
  /* Second linear gather. time = [T, T + delta] */
  gather_blur(uv, center_motion.zw, center_depth, max_motion.zw, rand, true, accum);

#if 1 /* Own addition. Not present in reference implementation. */
  /* Avoid division by 0.0. */
  float w = 1.0 / (50.0 * float(samples_count) * 4.0);
  accum.bg += center_color * w;
  accum.weight.x += w;
  /* NOTE: In Jimenez's presentation, they used center sample.
   * We use background color as it contains more information for foreground
   * elements that have not enough weights.
   * Yield better blur in complex motion. */
  center_color = accum.bg / accum.weight.x;
#endif
  /* Merge background. */
  accum.fg += accum.bg;
  accum.weight.y += accum.weight.x;
  /* Balance accumulation for failed samples.
   * We replace the missing foreground by the background. */
  float blend_fac = clamp(1.0 - accum.weight.y / accum.weight.z, 0.0, 1.0);
  vec4 out_color = (accum.fg / accum.weight.z) + center_color * blend_fac;

  imageStore(output_img, texel, out_color);
}
