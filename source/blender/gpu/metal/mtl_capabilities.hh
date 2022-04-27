/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

namespace blender {
namespace gpu {

/*** Derived from: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf ***/
/** Upper Bound/Fixed Limits **/

#define MTL_MAX_TEXTURE_SLOTS 128
#define MTL_MAX_SAMPLER_SLOTS MTL_MAX_TEXTURE_SLOTS
#define MTL_MAX_UNIFORM_BUFFER_BINDINGS 31
#define MTL_MAX_VERTEX_INPUT_ATTRIBUTES 31
#define MTL_MAX_UNIFORMS_PER_BLOCK 64

/* Context-specific limits -- populated in 'MTLBackend::platform_init' */
typedef struct MTLCapabilities {

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

  /* GPU Family */
  bool supports_family_mac1 = false;
  bool supports_family_mac2 = false;
  bool supports_family_mac_catalyst1 = false;
  bool supports_family_mac_catalyst2 = false;

} MTLCapabilities;

}  // namespace gpu
}  // namespace blender
