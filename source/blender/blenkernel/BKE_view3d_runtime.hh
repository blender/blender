/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <cstdint>

#include "BLI_enum_flags.hh"
#include "BLI_utility_mixins.hh"

namespace blender {
struct RegionView3D;
struct SmoothView3DStore;
struct ViewRender;
struct wmTimer;
enum eRegionView3D_ViewLock : char;
}  // namespace blender

namespace blender::bke {

enum eRegionView3DTags : uint8_t {
  RV3D_TAG_NAVIGATING = 1 << 0,
  RV3D_TAG_GPULIGHT_UPDATE = 1 << 1,
  RV3D_TAG_PAINTING = 1 << 2,
};
ENUM_OPERATORS(eRegionView3DTags)

class RegionView3DRuntime : NonCopyable, NonMovable {
 public:
  /** Offset/scale for camera GLSL texture-coordinates. */
  float viewcamtexcofac[4] = {};

  /** viewmat/persmat multiplied with object matrix, while drawing and selection. */
  float viewmatob[4][4] = {};
  float persmatob[4][4] = {};

  /**
   * Clip in object space,
   * means we can test for clipping in edit-mode without first going into world-space.
   */
  float clip_local[6][4] = {};

  /** Transform gizmo matrix. */
  float twmat[4][4] = {};
  /** min/max dot product on `twmat` XYZ axis. */
  float tw_axis_min[3] = {}, tw_axis_max[3] = {};
  float tw_axis_matrix[3][3] = {};

  /** XXX can easily get rid of this (Julian). */
  short twdrawflag = 0;

  /* Should usually be accessed with bke::region_view3d_lock_flags_get()! */
  eRegionView3D_ViewLock viewlock = {};

  /** Active rotation from NDOF (run-time only). */
  float ndof_rot_angle = 0;
  float ndof_rot_axis[3] = {};

  ViewRender *view_render = nullptr;

  /** Animated smooth view. */
  SmoothView3DStore *sms = nullptr;
  wmTimer *smooth_timer = nullptr;

  eRegionView3DTags tags = {};
};

/**
 * Get both the lock flags of the region view 3D and its runtime.
 */
eRegionView3D_ViewLock region_view3d_lock_flags_get(const RegionView3D *rv3d);

}  // namespace blender::bke
