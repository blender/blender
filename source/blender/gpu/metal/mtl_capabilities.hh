/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

namespace blender::gpu {

/*** Derived from: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf ***/
/** Upper Bound/Fixed Limits **/

#define MTL_MAX_IMAGE_SLOTS GPU_MAX_IMAGE /* Must match becaus of StateManager::image_formats. */
#define MTL_MAX_SAMPLER_SLOTS 64
/* Theoretical limit is 128 no target hardware. */
#define MTL_MAX_TEXTURE_SLOTS (MTL_MAX_SAMPLER_SLOTS + MTL_MAX_IMAGE_SLOTS)
/* Max limit without using bind-less for samplers. */
#define MTL_MAX_DEFAULT_SAMPLERS 16
/* Total maximum buffers which can be bound to an encoder, for use within a shader.
 * Uniform buffers and storage buffers share the set of available bind buffers.
 * The total number of buffer bindings must be <= MTL_MAX_BUFFER_BINDINGS
 * We also require an additional 2 core buffers for:
 * - Argument buffer for bindless resources (e.g. samplers)
 * - Default push constant block
 * Along with up to 6+1 buffers for vertex data, and index data. */
#define MTL_MAX_BUFFER_BINDINGS 31
#define MTL_MAX_VERTEX_INPUT_ATTRIBUTES 31
#define MTL_MAX_UNIFORMS_PER_BLOCK 64

#define MTL_MAX_SET_BYTES_SIZE 4096

enum AppleGPUType { APPLE_GPU_UNKNOWN = 0, APPLE_GPU_M1 = 1, APPLE_GPU_M2 = 2, APPLE_GPU_M3 = 3 };

/* Context-specific limits -- populated in 'MTLBackend::platform_init' */
struct MTLCapabilities {

  /* Variable Limits & feature sets. */
  int max_color_render_targets = 4;          /* Minimum = 4 */
  int buffer_alignment_for_textures = 256;   /* Upper bound = 256 bytes */
  int minimum_buffer_offset_alignment = 256; /* Upper bound = 256 bytes */

  /* Capabilities */
  bool supports_vertex_amplification = false;
  bool supports_texture_swizzle = true;
  bool supports_cubemaps = true;
  bool supports_layered_rendering = true;
  bool supports_memory_barriers = false;
  bool supports_sampler_border_color = false;
  bool supports_argument_buffers_tier2 = false;
  bool supports_texture_gather = false;
  bool supports_texture_atomics = false;
  bool supports_native_tile_inputs = false;

  /* GPU Family */
  bool supports_family_mac1 = false;
  bool supports_family_mac2 = false;
  bool supports_family_mac_catalyst1 = false;
  bool supports_family_mac_catalyst2 = false;
  AppleGPUType gpu = APPLE_GPU_UNKNOWN;

  /* CPU Info */
  int num_performance_cores = -1;
  int num_efficiency_cores = -1;
};

}  // namespace blender::gpu
