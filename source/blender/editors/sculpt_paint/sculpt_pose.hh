/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_math_vector.hh"
#include "BLI_span.hh"

struct Brush;
struct Depsgraph;
struct Object;
struct Sculpt;
struct SculptPoseIKChainPreview;
struct SculptSession;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::pose {

/** Pose Brush IK Chain. */
struct IKChainSegment {
  float3 orig;
  float3 head;

  float3 initial_orig;
  float3 initial_head;
  float len;
  float3 scale;
  float rot[4];
  Array<float> weights;

  /* Store a 4x4 transform matrix for each of the possible combinations of enabled XYZ symmetry
   * axis. */
  std::array<float4x4, PAINT_SYMM_AREAS> trans_mat;
  std::array<float4x4, PAINT_SYMM_AREAS> pivot_mat;
  std::array<float4x4, PAINT_SYMM_AREAS> pivot_mat_inv;
};

struct IKChain {
  Array<IKChainSegment> segments;
  float3 grab_delta_offset;
};

/**
 * Main Brush Function.
 */
void do_pose_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &ob,
                   const IndexMask &node_mask);
/**
 * Calculate the pose origin and (Optionally the pose factor)
 * that is used when using the pose brush.
 *
 * \param r_pose_origin: Must be a valid pointer.
 * \param r_pose_factor: Optional, when set to NULL it won't be calculated.
 */
void pose_brush_init(const Depsgraph &depsgraph,
                     Object &ob,
                     SculptSession &ss,
                     const Brush &brush);
std::unique_ptr<SculptPoseIKChainPreview> preview_ik_chain_init(const Depsgraph &depsgraph,
                                                                Object &ob,
                                                                SculptSession &ss,
                                                                const Brush &brush,
                                                                const float3 &initial_location,
                                                                float radius);

}  // namespace blender::ed::sculpt_paint::pose
