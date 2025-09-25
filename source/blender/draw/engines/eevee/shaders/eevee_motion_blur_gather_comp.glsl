/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Perform two gather blur in the 2 motion blur directions
 * Based on:
 * A Fast and Stable Feature-Aware Motion Blur Filter
 * by Jean-Philippe Guertin, Morgan McGuire, Derek Nowrouzezahrai
 *
 * With modification from the presentation:
 * Next Generation Post Processing in Call of Duty Advanced Warfare
 * by Jorge Jimenez
 */

#include "infos/eevee_motion_blur_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_motion_blur_gather)

#include "draw_view_lib.glsl"
#include "eevee_motion_blur_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_velocity_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#define gather_sample_count 8

/* Converts uv velocity into pixel space. Assumes velocity_tx is the same resolution as the
 * target post-FX frame-buffer. */
float4 motion_blur_sample_velocity(sampler2D velocity_tx, float2 uv)
{
  /* We can load velocity without velocity_resolve() since we resolved during the flatten pass. */
  float4 velocity = velocity_unpack(texture(velocity_tx, uv));
  return velocity * float2(textureSize(velocity_tx, 0)).xyxy * motion_blur_buf.motion_scale.xxyy;
}

float2 spread_compare(float center_motion_length, float sample_motion_length, float offset_length)
{
  return saturate(float2(center_motion_length, sample_motion_length) - offset_length + 1.0f);
}

float2 depth_compare(float center_depth, float sample_depth)
{
  float2 depth_scale = float2(-motion_blur_buf.depth_scale, motion_blur_buf.depth_scale);
  return saturate(0.5f + depth_scale * (sample_depth - center_depth));
}

/* Kill contribution if not going the same direction. */
float dir_compare(float2 offset, float2 sample_motion, float sample_motion_length)
{
  if (sample_motion_length < 0.5f) {
    return 1.0f;
  }
  return (dot(offset, sample_motion) > 0.0f) ? 1.0f : 0.0f;
}

/* Return background (x) and foreground (y) weights. */
float2 sample_weights(float center_depth,
                      float sample_depth,
                      float center_motion_length,
                      float sample_motion_length,
                      float offset_length)
{
  /* Classify foreground/background. */
  float2 depth_weight = depth_compare(center_depth, sample_depth);
  /* Weight if sample is overlapping or under the center pixel. */
  float2 spread_weight = spread_compare(center_motion_length, sample_motion_length, offset_length);
  return depth_weight * spread_weight;
}

struct Accumulator {
  float4 fg;
  float4 bg;
  /** x: Background, y: Foreground, z: dir. */
  float3 weight;
};

void gather_sample(float2 screen_uv,
                   float center_depth,
                   float center_motion_len,
                   float2 offset,
                   float offset_len,
                   const bool next,
                   inout Accumulator accum)
{
  float2 sample_uv = screen_uv - offset * motion_blur_buf.target_size_inv;
  float4 sample_vectors = motion_blur_sample_velocity(velocity_tx, sample_uv);
  float2 sample_motion = (next) ? sample_vectors.zw : sample_vectors.xy;
  float sample_motion_len = length(sample_motion);
  float sample_depth = reverse_z::read(textureLod(depth_tx, sample_uv, 0.0f).r);
  float4 sample_color = textureLod(in_color_tx, sample_uv, 0.0f);

  sample_depth = drw_depth_screen_to_view(sample_depth);

  float3 weights;
  weights.xy = sample_weights(
      center_depth, sample_depth, center_motion_len, sample_motion_len, offset_len);
  weights.z = dir_compare(offset, sample_motion, sample_motion_len);
  weights.xy *= weights.z;

  accum.fg += sample_color * weights.y;
  accum.bg += sample_color * weights.x;
  accum.weight += weights;
}

void gather_blur(float2 screen_uv,
                 float2 center_motion,
                 float center_depth,
                 float2 max_motion,
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

  if (max_motion_len < 0.5f) {
    return;
  }

  int i;
  float t, inc = 1.0f / float(gather_sample_count);
  for (i = 0, t = ofs * inc; i < gather_sample_count; i++, t += inc) {
    gather_sample(screen_uv,
                  center_depth,
                  center_motion_len,
                  max_motion * t,
                  max_motion_len * t,
                  next,
                  accum);
  }

  if (center_motion_len < 0.5f) {
    return;
  }

  for (i = 0, t = ofs * inc; i < gather_sample_count; i++, t += inc) {
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
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float2 uv = (float2(texel) + 0.5f) / float2(textureSize(depth_tx, 0).xy);

  if (!in_texture_range(texel, depth_tx)) {
    return;
  }

  /* Data of the center pixel of the gather (target). */
  float center_depth = reverse_z::read(texelFetch(depth_tx, texel, 0).r);
  center_depth = drw_depth_screen_to_view(center_depth);
  float4 center_motion = motion_blur_sample_velocity(velocity_tx, uv);

  float4 center_color = textureLod(in_color_tx, uv, 0.0f);

  float noise_offset = sampling_rng_1D_get(SAMPLING_TIME);
  /** TODO(fclem) Blue noise. */
  float2 rand = float2(
      interleaved_gradient_noise(float2(gl_GlobalInvocationID.xy), 0, noise_offset),
      interleaved_gradient_noise(float2(gl_GlobalInvocationID.xy), 1, noise_offset));

  /* Randomize tile boundary to avoid ugly discontinuities. Randomize 1/4th of the tile.
   * Note this randomize only in one direction but in practice it's enough. */
  rand.x = rand.x * 2.0f - 1.0f;
  int2 tile = (texel + int2(rand.x * float(MOTION_BLUR_TILE_SIZE) * 0.25f)) /
              MOTION_BLUR_TILE_SIZE;
  tile = clamp(tile, int2(0), imageSize(in_tiles_img) - 1);
  /* NOTE: Tile velocity is already in pixel space and with correct zw sign. */
  float4 max_motion;
  /* Load dilation result from the indirection table. */
  int2 tile_prev;
  motion_blur_tile_indirection_load(tile_indirection_buf, MOTION_PREV, uint2(tile), tile_prev);
  max_motion.xy = imageLoad(in_tiles_img, tile_prev).xy;
  int2 tile_next;
  motion_blur_tile_indirection_load(tile_indirection_buf, MOTION_NEXT, uint2(tile), tile_next);
  max_motion.zw = imageLoad(in_tiles_img, tile_next).zw;

  Accumulator accum;
  accum.weight = float3(0.0f, 0.0f, 1.0f);
  accum.bg = float4(0.0f);
  accum.fg = float4(0.0f);
  /* First linear gather. time = [T - delta, T] */
  gather_blur(uv, center_motion.xy, center_depth, max_motion.xy, rand.y, false, accum);
  /* Second linear gather. time = [T, T + delta] */
  gather_blur(uv, center_motion.zw, center_depth, max_motion.zw, rand.y, true, accum);

#if 1 /* Own addition. Not present in reference implementation. */
  /* Avoid division by 0.0. */
  float w = 1.0f / (50.0f * float(gather_sample_count) * 4.0f);
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
  float blend_fac = saturate(1.0f - accum.weight.y / accum.weight.z);
  float4 out_color = (accum.fg / accum.weight.z) + center_color * blend_fac;

#if 0 /* For debugging. */
  out_color.rgb = out_color.ggg;
  out_color.rg += max_motion.xy;
#endif

  imageStoreFast(out_color_img, texel, out_color);
}
