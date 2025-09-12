/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#include "GPU_shader_shared_utils.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

/** These are just to make more sense of G.debug_value's values. Reserved range is 1-30. */
enum eDebugMode : uint32_t {
  DEBUG_NONE = 0u,
  /**
   * Gradient showing light evaluation hot-spots.
   */
  DEBUG_LIGHT_CULLING = 1u,
  /**
   * Show incorrectly down-sample tiles in red.
   */
  DEBUG_HIZ_VALIDATION = 2u,
  /**
   * Display IrradianceCache surfels.
   */
  DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL = 3u,
  DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE = 4u,
  DEBUG_IRRADIANCE_CACHE_SURFELS_VISIBILITY = 5u,
  DEBUG_IRRADIANCE_CACHE_SURFELS_CLUSTER = 6u,
  /**
   * Display IrradianceCache virtual offset.
   */
  DEBUG_IRRADIANCE_CACHE_VIRTUAL_OFFSET = 7u,
  DEBUG_IRRADIANCE_CACHE_VALIDITY = 8u,
  /**
   * Show tiles depending on their status.
   */
  DEBUG_SHADOW_TILEMAPS = 10u,
  /**
   * Show content of shadow map. Used to verify projection code.
   */
  DEBUG_SHADOW_VALUES = 11u,
  /**
   * Show random color for each tile. Verify allocation and LOD assignment.
   */
  DEBUG_SHADOW_TILE_RANDOM_COLOR = 12u,
  /**
   * Show random color for each tile. Verify distribution and LOD transitions.
   */
  DEBUG_SHADOW_TILEMAP_RANDOM_COLOR = 13u,
  /**
   * Show storage cost of each pixel in the gbuffer.
   */
  DEBUG_GBUFFER_STORAGE = 14u,
  /**
   * Show evaluation cost of each pixel.
   */
  DEBUG_GBUFFER_EVALUATION = 15u,
  /**
   * Color different buffers of the depth of field.
   */
  DEBUG_DOF_PLANES = 16u,
};

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
