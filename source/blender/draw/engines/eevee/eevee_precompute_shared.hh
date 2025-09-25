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

/* Look-Up Table Generation. */
enum PrecomputeType : uint32_t {
  LUT_GGX_BRDF_SPLIT_SUM = 0u,
  LUT_GGX_BTDF_IOR_GT_ONE = 1u,
  LUT_GGX_BSDF_SPLIT_SUM = 2u,
  LUT_BURLEY_SSS_PROFILE = 3u,
  LUT_RANDOM_WALK_SSS_PROFILE = 4u,
};

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
