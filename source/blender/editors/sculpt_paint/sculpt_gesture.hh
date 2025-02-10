/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_scene_enums.h"
#include "DNA_vec_types.h"

#include "ED_view3d.hh"

struct SculptSession;
struct wmOperatorType;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::gesture {
enum ShapeType {
  Box = 0,

  /* In the context of a sculpt gesture, both lasso and polyline modal
   * operators are handled as the same general shape. */
  Lasso = 1,
  Line = 2,
};

enum class SelectionType {
  Inside = 0,
  Outside = 1,
};

/* Common data structure for both lasso and polyline. */
struct LassoData {
  float4x4 projviewobjmat;

  rcti boundbox;
  int width;

  /* 2D bitmap to test if a vertex is affected by the surrounding shape. */
  blender::BitVector<> mask_px;
};

struct LineData {
  /* Plane aligned to the gesture line. */
  float4 true_plane;
  float4 plane;

  /* Planes to limit the action to the length of the gesture segment at both sides of the affected
   * area. */
  std::array<float4, 2> side_plane;
  std::array<float4, 2> true_side_plane;
  bool use_side_planes;

  bool flip;
};

struct Operation;

/* Common data used for executing a gesture operation. */
struct GestureData {
  SculptSession *ss;
  ViewContext vc;

  /* Enabled and currently active symmetry. */
  ePaintSymmetryFlags symm;
  ePaintSymmetryFlags symmpass;

  /* Operation parameters. */
  ShapeType shape_type;
  bool front_faces_only;
  SelectionType selection_type;

  Operation *operation;

  /* Gesture data. */
  /* Screen space points that represent the gesture shape. */
  Array<float2> gesture_points;

  /* View parameters. */
  float3 true_view_normal;
  float3 view_normal;

  float3 true_view_origin;
  float3 view_origin;

  float true_clip_planes[4][4];
  float clip_planes[4][4];

  /* These store the view origin and normal in world space, which is used in some gestures to
   * generate geometry aligned from the view directly in world space. */
  /* World space view origin and normal are not affected by object symmetry when doing symmetry
   * passes, so there is no separate variables with the `true_` prefix to store their original
   * values without symmetry modifications. */
  float3 world_space_view_origin;
  float3 world_space_view_normal;

  /* Lasso & Polyline Gesture. */
  LassoData lasso;

  /* Line Gesture. */
  LineData line;

  /* Task Callback Data. */
  IndexMaskMemory node_mask_memory;
  IndexMask node_mask;

  ~GestureData();
};

/* Common abstraction structure for gesture operations. */
struct Operation {
  /* Initial setup (data updates, special undo push...). */
  void (*begin)(bContext &, wmOperator &, GestureData &);

  /* Apply the gesture action for each symmetry pass. */
  void (*apply_for_symmetry_pass)(bContext &, GestureData &);

  /* Remaining actions after finishing the symmetry passes iterations
   * (updating data-layers, tagging bke::pbvh::Tree updates...). */
  void (*end)(bContext &, GestureData &);
};

/* Determines whether or not a gesture action should be applied. */
bool is_affected(const GestureData &gesture_data, const float3 &position, const float3 &normal);
void filter_factors(const GestureData &gesture_data,
                    Span<float3> positions,
                    Span<float3> normals,
                    MutableSpan<float> factors);

/* Initialization functions. */
std::unique_ptr<GestureData> init_from_box(bContext *C, wmOperator *op);
std::unique_ptr<GestureData> init_from_lasso(bContext *C, wmOperator *op);
std::unique_ptr<GestureData> init_from_polyline(bContext *C, wmOperator *op);
std::unique_ptr<GestureData> init_from_line(bContext *C, const wmOperator *op);

/* Common gesture operator properties. */
void operator_properties(wmOperatorType *ot, ShapeType shapeType);

/* Apply the gesture action to the selected nodes. */
void apply(bContext &C, GestureData &gesture_data, wmOperator &op);

}  // namespace blender::ed::sculpt_paint::gesture
