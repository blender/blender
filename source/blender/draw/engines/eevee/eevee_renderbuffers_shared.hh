/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client code-bases.
 */

#pragma once

#include "eevee_camera_shared.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

/* Theoretical max is 256 across color and value AOVS with texture array restrictions.
 * However, the `output_aov()` function performs a linear search inside all the hashes.
 * If we can find a way to avoid this we can bump this number up. */
#define AOV_MAX 128

struct AOVsInfoData {
  /* Pack 4 hashes per uint4, using std140 packing rules.
   * Color AOV hashes are placed before value AOV hashes. */
  uint4 hash[AOV_MAX / 4];
  /* Number of AOVs stored. */
  int color_len;
  int value_len;
  /** Id of the AOV to be displayed (from the start of the AOV array). -1 for combined. */
  int display_id;
  /** True if the AOV to be displayed is from the value accumulation buffer. */
  bool32_t display_is_value;
};
BLI_STATIC_ASSERT_ALIGN(AOVsInfoData, 16)

struct RenderBuffersInfoData {
  AOVsInfoData aovs;
  /* Color. */
  int color_len;
  int normal_id;
  int position_id;
  int diffuse_light_id;
  int diffuse_color_id;
  int specular_light_id;
  int specular_color_id;
  int volume_light_id;
  int emission_id;
  int environment_id;
  int transparent_id;
  /* Value */
  int value_len;
  int shadow_id;
  int ambient_occlusion_id;
  int _pad0, _pad1;
};
BLI_STATIC_ASSERT_ALIGN(RenderBuffersInfoData, 16)

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
