/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"
#include "infos/eevee_sampling_infos.hh"
#include "infos/eevee_velocity_infos.hh"

COMPUTE_SHADER_CREATE_INFO(draw_view)
COMPUTE_SHADER_CREATE_INFO(eevee_velocity_camera)
COMPUTE_SHADER_CREATE_INFO(eevee_sampling_data)

#include "draw_math_geom_lib.glsl"
#include "eevee_motion_blur_shared.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "eevee_sampling_lib.glsl"
#include "eevee_velocity.bsl.hh"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee::motion_blur {

/* -------------------------------------------------------------------- */
/** \name Tile Indirection Buffer
 * \{ */

#define MotionTilePayload uint

enum MotionStep : uchar {
  MOTION_PREV = 0u,
  MOTION_NEXT = 1u,
};

/* Tile indirection buffer. */
struct TileBuf {
  [[storage(0, read_write)]] uint (&tile_indirection_buf)[];

  void store(MotionStep motion_step, uint2 tile, MotionTilePayload payload)
  {
    uint index = indirection_index(uint(motion_step), tile);
    atomicMax(tile_indirection_buf[index], payload);
  }

  int2 load(MotionStep motion_step, uint2 tile) const
  {
    uint index = indirection_index(uint(motion_step), tile);
    return unpack_payload(tile_indirection_buf[index]);
  }

  static uint indirection_index(uint motion_step, uint2 tile)
  {
    uint index = tile.x;
    index += tile.y * MOTION_BLUR_MAX_TILE;
    index += motion_step * MOTION_BLUR_MAX_TILE * MOTION_BLUR_MAX_TILE;
    return index;
  }

  /* Store velocity magnitude in the MSB to be able to use it with atomicMax operations. */
  static MotionTilePayload pack_payload(float2 motion, uint2 payload)
  {
    /* NOTE: Clamp to 16383 pixel velocity. After that, it is tile position that determine the tile
     * to dilate over. */
    uint velocity = min(uint(ceil(length(motion))), 0x3FFFu);
    /* Designed for 512x512 tiles max. */
    return (velocity << 18u) | ((payload.x & 0x1FFu) << 9u) | (payload.y & 0x1FFu);
  }

  /* Return thread index. */
  static int2 unpack_payload(uint data)
  {
    return int2((data >> 9u) & 0x1FFu, data & 0x1FFu);
  }
};

/** \} */

namespace flatten {

template<enum TextureWriteFormat velocity_format> struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_velocity_camera;

  [[uniform(0)]] const MotionBlurData &motion_blur_buf;
  [[sampler(0)]] sampler2DDepth depth_tx;
  [[image(0, read_write, velocity_format)]] image2D velocity_img;
  [[image(1, write, SFLOAT_16_16_16_16)]] image2D out_tiles_img;

  [[shared]] uint payload_prev;
  [[shared]] uint payload_next;
  [[shared]] float2 max_motion_prev;
  [[shared]] float2 max_motion_next;
};

template struct Resources<SFLOAT_16_16>;
template struct Resources<SFLOAT_16_16_16_16>;

/* Store velocity magnitude in the MSB and thread id in the LSB. */
uint pack_payload(float2 motion, uint2 thread_id)
{
  /* NOTE: We clamp max velocity to 16k pixels. */
  return (min(uint(ceil(length(motion))), 0xFFFFu) << 16u) | (thread_id.y << 8) | thread_id.x;
}

/* Return thread index from the payload. */
uint2 unpack_payload(uint payload)
{
  return uint2(payload & 0xFFu, (payload >> 8) & 0xFFu);
}

/**
 * Shaders that down-sample velocity buffer into squared tile of MB_TILE_DIVISOR pixels wide.
 * Outputs the largest motion vector in the tile area.
 * Also perform velocity resolve to speedup the convolution pass.
 *
 * Based on:
 * A Fast and Stable Feature-Aware Motion Blur Filter
 * by Jean-Philippe Guertin, Morgan McGuire, Derek Nowrouzezahrai
 *
 * Adapted from G3D Innovation Engine implementation.
 */
template<enum TextureWriteFormat velocity_format>
[[compute, local_size(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)]]
void flatten_comp([[resource_table]] Resources<velocity_format> &srt,
                  [[work_group_id]] const uint3 group_id,
                  [[global_invocation_id]] const uint3 global_id,
                  [[local_invocation_id]] const uint3 local_id,
                  [[local_invocation_index]] const uint local_index)
{
  if (local_index == 0u) {
    srt.payload_prev = 0u;
    srt.payload_next = 0u;
  }
  barrier();

  uint local_payload_prev = 0u;
  uint local_payload_next = 0u;
  float2 local_max_motion_prev;
  float2 local_max_motion_next;

  int2 texel = min(int2(global_id.xy), imageSize(srt.velocity_img) - 1);

  float2 render_size = float2(imageSize(srt.velocity_img).xy);
  float2 uv = (float2(texel) + 0.5f) / render_size;
  float depth = reverse_z::read(texelFetch(srt.depth_tx, texel, 0).r);
  float4 motion = velocity::resolve(imageLoad(srt.velocity_img, texel), uv, depth);
#ifdef FLATTEN_RG
  /* imageLoad does not perform the swizzling like sampler does. Do it manually. */
  motion = motion.xyxy;
#endif

  /* Store resolved velocity to speedup the gather pass. Out of bounds writes are ignored.
   * Unfortunately, we cannot convert to pixel space here since it is also used by TAA and the
   * motion blur needs to remain optional. */
  imageStore(srt.velocity_img, int2(global_id.xy), velocity::pack(motion));
  /* Clip velocity to viewport bounds (in NDC space). */
  float2 line_clip;
  line_clip.x = line_unit_square_intersect_dist_safe(uv * 2.0f - 1.0f, motion.xy * 2.0f);
  line_clip.y = line_unit_square_intersect_dist_safe(uv * 2.0f - 1.0f, -motion.zw * 2.0f);
  motion *= min(line_clip, float2(1.0f)).xxyy;
  /* Convert to pixel space. Note this is only for velocity tiles. */
  motion *= render_size.xyxy;
  /* Rescale to shutter relative motion for viewport. */
  motion *= srt.motion_blur_buf.motion_scale.xxyy;

  uint sample_payload_prev = pack_payload(motion.xy, local_id.xy);
  if (local_payload_prev < sample_payload_prev) {
    local_payload_prev = sample_payload_prev;
    local_max_motion_prev = motion.xy;
  }

  uint sample_payload_next = pack_payload(motion.zw, local_id.xy);
  if (local_payload_next < sample_payload_next) {
    local_payload_next = sample_payload_next;
    local_max_motion_next = motion.zw;
  }

  /* Compare the local payload with the other threads. */
  atomicMax(srt.payload_prev, local_payload_prev);
  atomicMax(srt.payload_next, local_payload_next);
  barrier();

  /* Need to broadcast the result to another thread in order to issue a unique write. */
  if (all(equal(unpack_payload(srt.payload_prev), local_id.xy))) {
    srt.max_motion_prev = local_max_motion_prev;
  }
  if (all(equal(unpack_payload(srt.payload_next), local_id.xy))) {
    srt.max_motion_next = local_max_motion_next;
  }
  barrier();

  if (local_index == 0u) {
    int2 tile_co = int2(group_id.xy);
    imageStore(srt.out_tiles_img, tile_co, float4(srt.max_motion_prev, srt.max_motion_next));
  }
}

template void flatten_comp<SFLOAT_16_16>(
    Resources<SFLOAT_16_16> &, const uint3, const uint3, const uint3, const uint);
template void flatten_comp<SFLOAT_16_16_16_16>(
    Resources<SFLOAT_16_16_16_16> &, const uint3, const uint3, const uint3, const uint);

}  // namespace flatten

namespace dilate {

#define DEBUG_BYPASS_DILATION 0

struct MotionRect {
  int2 bottom_left;
  int2 extent;
};

struct MotionLine {
  /** Origin of the line. */
  float2 origin;
  /** Normal to the line direction. */
  float2 normal;

  bool is_inside_motion_line(int2 tile) const
  {
#if DEBUG_BYPASS_DILATION
    return true;
#endif
    /* NOTE: Everything in is tile unit. */
    float dist = point_line_projection_dist(float2(tile), origin, normal);
    /* In order to be conservative and for simplicity, we use the tiles bounding circles.
     * Consider that both the tile and the line have bounding radius of M_SQRT1_2. */
    return abs(dist) < M_SQRT2;
  }
};

struct Resources {
  [[image(1, read, SFLOAT_16_16_16_16)]] const image2D in_tiles_img;

  MotionLine compute_motion_line(int2 tile, float2 motion)
  {
    float2 dir = safe_normalize(motion);
    return {.origin = float2(tile),
            /* Rotate 90 degrees counter-clockwise. */
            .normal = float2(-dir.y, dir.x)};
  }

  MotionRect compute_motion_rect(int2 tile, float2 motion)
  {
#if DEBUG_BYPASS_DILATION
    return {tile, int2(1)};
#endif
    /* `ceil()` to number of tile touched. */
    int2 point1 = tile + int2(sign(motion) * ceil(abs(motion) / float(MOTION_BLUR_TILE_SIZE)));
    int2 point2 = tile;

    int2 max_point = max(point1, point2);
    int2 min_point = min(point1, point2);
    /* Clamp to bounds. */
    max_point = min(max_point, imageSize(in_tiles_img) - 1);
    min_point = max(min_point, int2(0));

    return {.bottom_left = min_point, .extent = 1 + max_point - min_point};
  }
};

/**
 * Dilate motion vector tiles until we covered maximum velocity.
 * Outputs the largest intersecting motion vector in the neighborhood.
 */
[[compute, local_size(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)]]
void dilate_comp([[resource_table]] Resources &srt,
                 [[resource_table]] TileBuf &tiles,
                 [[global_invocation_id]] const uint3 global_id)
{
  int2 src_tile = int2(global_id.xy);
  if (any(greaterThanEqual(src_tile, imageSize(srt.in_tiles_img)))) {
    return;
  }

  float4 max_motion = imageLoad(srt.in_tiles_img, src_tile);

  MotionTilePayload payload_prev = TileBuf::pack_payload(max_motion.xy, uint2(src_tile));
  MotionTilePayload payload_next = TileBuf::pack_payload(max_motion.zw, uint2(src_tile));
  if (true) {
    /* Rectangular area (in tiles) where the motion vector spreads. */
    MotionRect motion_rect = srt.compute_motion_rect(src_tile, max_motion.xy);
    MotionLine motion_line = srt.compute_motion_line(src_tile, max_motion.xy);
    /* Do a conservative rasterization of the line of the motion vector line. */
    for (int x = 0; x < motion_rect.extent.x; x++) {
      for (int y = 0; y < motion_rect.extent.y; y++) {
        int2 tile = motion_rect.bottom_left + int2(x, y);
        if (motion_line.is_inside_motion_line(tile)) {
          tiles.store(MOTION_PREV, uint2(tile), payload_prev);
          /* FIXME: This is a bit weird, but for some reason, we need the store the same vector in
           * the motion next so that weighting in gather pass is better. */
          tiles.store(MOTION_NEXT, uint2(tile), payload_next);
        }
      }
    }
  }

  if (true) {
    /* Rectangular area (in tiles) where the motion vector spreads. */
    MotionRect motion_rect = srt.compute_motion_rect(src_tile, max_motion.zw);
    MotionLine motion_line = srt.compute_motion_line(src_tile, max_motion.zw);
    /* Do a conservative rasterization of the line of the motion vector line. */
    for (int x = 0; x < motion_rect.extent.x; x++) {
      for (int y = 0; y < motion_rect.extent.y; y++) {
        int2 tile = motion_rect.bottom_left + int2(x, y);
        if (motion_line.is_inside_motion_line(tile)) {
          tiles.store(MOTION_NEXT, uint2(tile), payload_next);
          /* FIXME: This is a bit weird, but for some reason, we need the store the same vector in
           * the motion next so that weighting in gather pass is better. */
          tiles.store(MOTION_PREV, uint2(tile), payload_prev);
        }
      }
    }
  }
}

}  // namespace dilate

namespace gather {

#define gather_sample_count 8

struct Accumulator {
  float4 fg;
  float4 bg;
  /** x: Background, y: Foreground, z: dir. */
  float3 weight;
};

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;
  [[legacy_info]] ShaderCreateInfo eevee_sampling_data;

  [[uniform(0)]] const MotionBlurData &motion_blur_buf;
  [[sampler(0)]] sampler2DDepth depth_tx;
  [[sampler(1)]] sampler2D velocity_tx;
  [[sampler(2)]] sampler2D in_color_tx;
  [[image(0, read, SFLOAT_16_16_16_16)]] const image2D in_tiles_img;
  [[image(1, write, SFLOAT_16_16_16_16)]] image2D out_color_img;

  /* Converts uv velocity into pixel space. Assumes velocity_tx is the same resolution as the
   * target post-FX frame-buffer. */
  float4 motion_blur_sample_velocity(float2 uv)
  {
    /* We can load velocity without velocity_resolve() since we resolved during the flatten pass.
     */
    float4 velocity = velocity::unpack(texture(velocity_tx, uv));
    return velocity * float2(textureSize(velocity_tx, 0)).xyxy * motion_blur_buf.motion_scale.xxyy;
  }

  float2 spread_compare(float center_motion_length,
                        float sample_motion_length,
                        float offset_length)
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
    float2 spread_weight = spread_compare(
        center_motion_length, sample_motion_length, offset_length);
    return depth_weight * spread_weight;
  }

  void gather_sample(float2 screen_uv,
                     float center_depth,
                     float center_motion_len,
                     float2 offset,
                     float offset_len,
                     const bool next,
                     Accumulator &accum)
  {
    float2 sample_uv = screen_uv - offset * motion_blur_buf.target_size_inv;
    float4 sample_vectors = motion_blur_sample_velocity(sample_uv);
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
                   Accumulator &accum)
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
};

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
[[compute, local_size(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)]]
void gather_comp([[resource_table]] Resources &srt,
                 [[resource_table]] const TileBuf &tiles,
                 [[global_invocation_id]] const uint3 global_id)
{
  int2 texel = int2(global_id.xy);
  float2 uv = (float2(texel) + 0.5f) / float2(textureSize(srt.depth_tx, 0).xy);

  if (!in_texture_range(texel, srt.depth_tx)) {
    return;
  }

  /* Data of the center pixel of the gather (target). */
  float center_depth = reverse_z::read(texelFetch(srt.depth_tx, texel, 0).r);
  center_depth = drw_depth_screen_to_view(center_depth);
  float4 center_motion = srt.motion_blur_sample_velocity(uv);

  float4 center_color = textureLod(srt.in_color_tx, uv, 0.0f);

  float noise_offset = sampling_rng_1D_get(SAMPLING_TIME);
  /** TODO(fclem) Blue noise. */
  float2 rand = float2(interleaved_gradient_noise(float2(global_id.xy), 0, noise_offset),
                       interleaved_gradient_noise(float2(global_id.xy), 1, noise_offset));

  /* Randomize tile boundary to avoid ugly discontinuities. Randomize 1/4th of the tile.
   * Note this randomize only in one direction but in practice it's enough. */
  rand.x = rand.x * 2.0f - 1.0f;
  int2 tile = (texel + int2(int(rand.x * float(MOTION_BLUR_TILE_SIZE) * 0.25f))) /
              MOTION_BLUR_TILE_SIZE;
  tile = clamp(tile, int2(0), imageSize(srt.in_tiles_img) - 1);
  /* NOTE: Tile velocity is already in pixel space and with correct zw sign. */
  float4 max_motion;
  /* Load dilation result from the indirection table. */
  int2 tile_prev = tiles.load(MOTION_PREV, uint2(tile));
  max_motion.xy = imageLoad(srt.in_tiles_img, tile_prev).xy;
  int2 tile_next = tiles.load(MOTION_NEXT, uint2(tile));
  max_motion.zw = imageLoad(srt.in_tiles_img, tile_next).zw;

  Accumulator accum;
  accum.weight = float3(0.0f, 0.0f, 1.0f);
  accum.bg = float4(0.0f);
  accum.fg = float4(0.0f);
  /* First linear gather. time = [T - delta, T] */
  srt.gather_blur(uv, center_motion.xy, center_depth, max_motion.xy, rand.y, false, accum);
  /* Second linear gather. time = [T, T + delta] */
  srt.gather_blur(uv, center_motion.zw, center_depth, max_motion.zw, rand.y, true, accum);

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

  imageStoreFast(srt.out_color_img, texel, out_color);
}

}  // namespace gather

}  // namespace eevee::motion_blur

PipelineCompute eevee_motion_blur_tiles_dilate(eevee::motion_blur::dilate::dilate_comp);
PipelineCompute eevee_motion_blur_tiles_flatten_rg(
    eevee::motion_blur::flatten::flatten_comp<SFLOAT_16_16>);
PipelineCompute eevee_motion_blur_tiles_flatten_rgba(
    eevee::motion_blur::flatten::flatten_comp<SFLOAT_16_16_16_16>);
PipelineCompute eevee_motion_blur_gather(eevee::motion_blur::gather::gather_comp);
