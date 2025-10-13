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

#define VELOCITY_INVALID 512.0

enum eVelocityStep : uint32_t {
  STEP_PREVIOUS = 0,
  STEP_NEXT = 1,
  STEP_CURRENT = 2,
};

struct VelocityObjectIndex {
  /** Offset inside #VelocityObjectBuf for each time-step. Indexed using eVelocityStep. */
  packed_int3 ofs;
  /** Temporary index to copy this to the #VelocityIndexBuf. */
  uint resource_id;

#ifndef GPU_SHADER
  VelocityObjectIndex() : ofs(-1, -1, -1), resource_id(-1) {};
#endif
};
BLI_STATIC_ASSERT_ALIGN(VelocityObjectIndex, 16)

struct VelocityGeometryIndex {
  /** Offset inside #VelocityGeometryBuf for each time-step. Indexed using eVelocityStep. */
  packed_int3 ofs;
  /** If true, compute deformation motion blur. */
  bool32_t do_deform;
  /**
   * Length of data inside #VelocityGeometryBuf for each time-step.
   * Indexed using eVelocityStep.
   */
  packed_int3 len;

  int _pad0;

#ifndef GPU_SHADER
  VelocityGeometryIndex() : ofs(-1, -1, -1), do_deform(false), len(-1, -1, -1), _pad0(1) {};
#endif
};
BLI_STATIC_ASSERT_ALIGN(VelocityGeometryIndex, 16)

struct VelocityIndex {
  VelocityObjectIndex obj;
  VelocityGeometryIndex geo;
};
BLI_STATIC_ASSERT_ALIGN(VelocityGeometryIndex, 16)

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
