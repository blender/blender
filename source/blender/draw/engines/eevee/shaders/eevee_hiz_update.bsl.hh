/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shader that down-sample depth buffer, creating a Hierarchical-Z buffer.
 * Saves max value of each 2x2 texel in the mipmap above the one we are
 * rendering to. Adapted from
 * http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
 *
 * Major simplification has been made since we pad the buffer to always be
 * bigger than input to avoid mipmapping misalignment.
 *
 * Start by copying the base level by quad loading the depth.
 * Then each thread compute it's local depth for level 1.
 * After that we use shared variables to do inter thread communication and
 * downsample to max level.
 */

#pragma once

#include "eevee_defines.hh"
#include "eevee_reverse_z_lib.bsl.hh"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"

namespace eevee::hiz {

struct Update {
  [[specialization_constant(true)]] bool update_mip_0;

  [[compilation_constant]] bool use_layer;

  [[sampler(0), condition(!use_layer)]] sampler2DDepth depth_tx;
  [[sampler(0), condition(use_layer)]] sampler2DArrayDepth depth_layered_tx;
  [[push_constant, condition(use_layer)]] int layer_id;

  [[storage(0, read_write)]] uint &finished_tile_counter;

  [[image(0, write, SFLOAT_32)]] image2D out_mip_0;
  [[image(1, write, SFLOAT_32)]] image2D out_mip_1;
  [[image(2, write, SFLOAT_32)]] image2D out_mip_2;
  [[image(3, write, SFLOAT_32)]] image2D out_mip_3;
  [[image(4, write, SFLOAT_32)]] image2D out_mip_4;
  [[image(5, read_write, SFLOAT_32)]] image2D out_mip_5;
  [[image(6, write, SFLOAT_32)]] image2D out_mip_6;

  [[shared]] float local_depths[HIZ_GROUP_SIZE][HIZ_GROUP_SIZE];

  /* Load values from the previous lod level. */
  float4 load_local_depths(int2 pixel)
  {
    pixel *= 2;
    return float4(local_depths[pixel.y + 1][pixel.x + 0],
                  local_depths[pixel.y + 1][pixel.x + 1],
                  local_depths[pixel.y + 0][pixel.x + 1],
                  local_depths[pixel.y + 0][pixel.x + 0]);
  }

  void store_local_depth(int2 pixel, float depth)
  {
    local_depths[pixel.y][pixel.x] = depth;
  }
};

[[compute, local_size(HIZ_GROUP_SIZE, HIZ_GROUP_SIZE)]]
void update_main([[resource_table]] Update &srt,
                 [[local_invocation_id]] const uint3 local_id,
                 [[work_group_id]] const uint3 group_id)
{
  int2 local_px = int2(local_id.xy);
  /* Bottom left corner of the kernel. */
  int2 kernel_origin = int2(HIZ_GROUP_SIZE * group_id.xy);

  /* Copy level 0. */
  int2 src_px = int2(kernel_origin + local_px) * 2;
  float4 samp;

  if (srt.use_layer) [[static_branch]] {
    float2 samp_co = float2(src_px + 1) / float2(textureSize(srt.depth_layered_tx, 0).xy);
    samp = textureGather(srt.depth_layered_tx, float3(samp_co, float(srt.layer_id)));
  }
  else {
    float2 samp_co = float2(src_px + 1) / float2(textureSize(srt.depth_tx, 0));
    samp = textureGather(srt.depth_tx, samp_co);
  }

  samp = reverse_z::read(samp);

  if (srt.update_mip_0) {
    imageStoreFast(srt.out_mip_0, src_px + int2(0, 1), samp.xxxx);
    imageStoreFast(srt.out_mip_0, src_px + int2(1, 1), samp.yyyy);
    imageStoreFast(srt.out_mip_0, src_px + int2(1, 0), samp.zzzz);
    imageStoreFast(srt.out_mip_0, src_px + int2(0, 0), samp.wwww);
  }

  /* Level 1. (No load) */
  float max_depth = reduce_max(samp);
  int2 dst_px = int2(kernel_origin + local_px);
  imageStoreFast(srt.out_mip_1, dst_px, float4(max_depth));
  srt.store_local_depth(local_px, max_depth);

  /* Level 2. */
  {
    barrier(); /* Wait for previous writes to finish. */
    bool active_thread = all(lessThan(uint2(local_px), uint2(HIZ_GROUP_SIZE) >> uint(1)));
    if (active_thread) {
      max_depth = reduce_max(srt.load_local_depths(local_px));
      dst_px = int2((kernel_origin >> 1) + local_px);
      imageStoreFast(srt.out_mip_2, dst_px, float4(max_depth));
    }
    barrier(); /* Wait for previous reads to finish. */
    if (active_thread) {
      srt.store_local_depth(local_px, max_depth);
    }
  }
  /* Level 3. */
  {
    barrier(); /* Wait for previous writes to finish. */
    bool active_thread = all(lessThan(uint2(local_px), uint2(HIZ_GROUP_SIZE) >> uint(2)));
    if (active_thread) {
      max_depth = reduce_max(srt.load_local_depths(local_px));
      dst_px = int2((kernel_origin >> 2) + local_px);
      imageStoreFast(srt.out_mip_3, dst_px, float4(max_depth));
    }
    barrier(); /* Wait for previous reads to finish. */
    if (active_thread) {
      srt.store_local_depth(local_px, max_depth);
    }
  }
  /* Level 4. */
  {
    barrier(); /* Wait for previous writes to finish. */
    bool active_thread = all(lessThan(uint2(local_px), uint2(HIZ_GROUP_SIZE) >> uint(3)));
    if (active_thread) {
      max_depth = reduce_max(srt.load_local_depths(local_px));
      dst_px = int2((kernel_origin >> 3) + local_px);
      imageStoreFast(srt.out_mip_4, dst_px, float4(max_depth));
    }
    barrier(); /* Wait for previous reads to finish. */
    if (active_thread) {
      srt.store_local_depth(local_px, max_depth);
    }
  }
  /* Level 5. */
  {
    barrier(); /* Wait for previous writes to finish. */
    bool active_thread = all(lessThan(uint2(local_px), uint2(HIZ_GROUP_SIZE) >> uint(4)));
    if (active_thread) {
      max_depth = reduce_max(srt.load_local_depths(local_px));
      dst_px = int2((kernel_origin >> 4) + local_px);
      imageStoreFast(srt.out_mip_5, dst_px, float4(max_depth));
    }
    barrier(); /* Wait for previous reads to finish. */
    if (active_thread) {
      srt.store_local_depth(local_px, max_depth);
    }
  }

  /* Since we pad the destination texture, the mip size is equal to the dispatch size. */
  uint2 mip5_size = uint2(imageSize(srt.out_mip_5).xy);
  uint tile_count = mip5_size.x * mip5_size.y;
  /* Let the last tile handle the remaining LOD. */
  bool last_tile = atomicAdd(srt.finished_tile_counter, 1u) + 1u < tile_count;
  if (last_tile == false) {
    return;
  }
  /* Reset for next dispatch. */
  srt.finished_tile_counter = 0u;

  int2 iter = divide_ceil(imageSize(srt.out_mip_5), int2(HIZ_GROUP_SIZE * 2u));
  int2 image_border = imageSize(srt.out_mip_5) - 1;
  for (int y = 0; y < iter.y; y++) {
    for (int x = 0; x < iter.x; x++) {
      /* Load result of the other work groups. */
      kernel_origin = int2(HIZ_GROUP_SIZE) * int2(x, y);
      src_px = int2(kernel_origin + local_px) * 2;
      float4 samp;
      samp.x = imageLoadFast(srt.out_mip_5, min(src_px + int2(0, 1), image_border)).x;
      samp.y = imageLoadFast(srt.out_mip_5, min(src_px + int2(1, 1), image_border)).x;
      samp.z = imageLoadFast(srt.out_mip_5, min(src_px + int2(1, 0), image_border)).x;
      samp.w = imageLoadFast(srt.out_mip_5, min(src_px + int2(0, 0), image_border)).x;
      /* Level 6. */
      float max_depth = reduce_max(samp);
      int2 dst_px = int2(kernel_origin + local_px);
      imageStoreFast(srt.out_mip_6, dst_px, float4(max_depth));
      srt.store_local_depth(local_px, max_depth);

      /* Level 7 requires barriers inside a non-uniform control flow. */
      // downsample_level(out_mip_7, 1);

      /* Limited by OpenGL maximum of 8 image slot. */
      // downsample_level(out_mip_8, 2);
      // downsample_level(out_mip_9, 3);
      // downsample_level(out_mip_10, 4);
    }
  }
}

}  // namespace eevee::hiz

#ifndef GLSL_CPP_STUBS
PipelineCompute eevee_hiz_update(eevee::hiz::update_main, eevee::hiz::Update{.use_layer = false});
PipelineCompute eevee_hiz_update_layer(eevee::hiz::update_main,
                                       eevee::hiz::Update{.use_layer = true});
#endif
