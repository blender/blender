/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#pragma once

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"

struct Brush;
struct Scene;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::expand {

enum class FalloffType {
  Geodesic,
  Topology,
  TopologyNormals,
  Normals,
  Sphere,
  BoundaryTopology,
  BoundaryFaceSet,
  ActiveFaceSet,
};

enum class TargetType {
  Mask,
  FaceSets,
  Colors,
};

enum class RecursionType {
  Topology,
  Geodesic,
};

#define EXPAND_SYMM_AREAS 8

struct Cache {
  /* Target data elements that the expand operation will affect. */
  TargetType target;

  /* Falloff data. */
  FalloffType falloff_type;

  /* Indexed by vertex index, precalculated falloff value of that vertex (without any falloff
   * editing modification applied). */
  Array<float> vert_falloff;
  /* Max falloff value in *vert_falloff. */
  float max_vert_falloff;

  /* Indexed by base mesh face index, precalculated falloff value of that face. These values are
   * calculated from the per vertex falloff (*vert_falloff) when needed. */
  Array<float> face_falloff;
  float max_face_falloff;

  /* Falloff value of the active element (vertex or base mesh face) that Expand will expand to. */
  float active_falloff;

  /* When set to true, expand skips all falloff computations and considers all elements as enabled.
   */
  bool all_enabled;

  /* Initial mouse and cursor data from where the current falloff started. This data can be changed
   * during the execution of Expand by moving the origin. */
  float2 initial_mouse_move;
  float2 initial_mouse;
  int initial_active_vert;
  int initial_active_face_set;

  /* Maximum number of vertices allowed in the SculptSession for previewing the falloff using
   * geodesic distances. */
  int max_geodesic_move_preview;

  /* Original falloff type before starting the move operation. */
  FalloffType move_original_falloff_type;
  /* Falloff type using when moving the origin for preview. */
  FalloffType move_preview_falloff_type;

  /* Face set ID that is going to be used when creating a new Face Set. */
  int next_face_set;

  /* Face Set ID of the Face set selected for editing. */
  int update_face_set;

  /* Mouse position since the last time the origin was moved. Used for reference when moving the
   * initial position of Expand. */
  float2 original_mouse_move;

  /* Active island checks. */
  /* Indexed by symmetry pass index, contains the connected island ID for that
   * symmetry pass. Other connected island IDs not found in this
   * array will be ignored by Expand. */
  int active_connected_islands[EXPAND_SYMM_AREAS];

  /* Snapping. */
  /* Set containing all Face Sets IDs that Expand will use to snap the new data. */
  std::unique_ptr<Set<int>> snap_enabled_face_sets;

  /* Texture distortion data. */
  const Brush *brush;
  Scene *scene;
  // struct MTex *mtex;

  /* Controls how much texture distortion will be applied to the current falloff */
  float texture_distortion_strength;

  /* Cached pbvh::Tree nodes. This allows to skip gathering all nodes from the pbvh::Tree each time
   * expand needs to update the state of the elements. */
  IndexMaskMemory node_mask_memory;
  IndexMask node_mask;

  /* Expand state options. */

  /* Number of loops (times that the falloff is going to be repeated). */
  int loop_count;

  /* Invert the falloff result. */
  bool invert;

  /* When set to true, preserves the previous state of the data and adds the new one on top. */
  bool preserve;

  /* When set to true, the mask or colors will be applied as a gradient. */
  bool falloff_gradient;

  /* When set to true, Expand will use the Brush falloff curve data to shape the gradient. */
  bool brush_gradient;

  /* When set to true, Expand will move the origin (initial active vertex and cursor position)
   * instead of updating the active vertex and active falloff. */
  bool move;

  /* When set to true, Expand will snap the new data to the Face Sets IDs found in
   * *original_face_sets. */
  bool snap;

  /* When set to true, Expand will use the current Face Set ID to modify an existing Face Set
   * instead of creating a new one. */
  bool modify_active_face_set;

  /* When set to true, Expand will reposition the sculpt pivot to the boundary of the expand result
   * after finishing the operation. */
  bool reposition_pivot;

  /* If nothing is masked set mask of every vertex to 0. */
  bool auto_mask;

  /* Color target data type related data. */
  float fill_color[4];
  short blend_mode;

  /* Face Sets at the first step of the expand operation, before starting modifying the active
   * vertex and active falloff. These are not the original Face Sets of the sculpt before starting
   * the operator as they could have been modified by Expand when initializing the operator and
   * before starting changing the active vertex. These Face Sets are used for restoring and
   * checking the Face Sets state while the Expand operation modal runs. */
  Array<int> initial_face_sets;

  /* Original data of the sculpt as it was before running the Expand operator. */
  Array<float> original_mask;
  Array<int> original_face_sets;
  Array<float4> original_colors;

  bool check_islands;
  int normal_falloff_blur_steps;
};

}  // namespace blender::ed::sculpt_paint::expand
