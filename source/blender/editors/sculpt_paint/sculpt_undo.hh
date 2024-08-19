/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_bit_group_vector.hh"
#include "BLI_bit_vector.hh"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

struct BMLogEntry;
struct Depsgraph;
struct Mesh;
struct Object;
struct StepData;
struct wmOperator;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::undo {

enum class Type : int8_t {
  None,
  Position,
  HideVert,
  HideFace,
  Mask,
  DyntopoBegin,
  DyntopoEnd,
  DyntopoSymmetrize,
  Geometry,
  FaceSet,
  Color,
};

struct Node {
  Array<float3> position;
  Array<float3> orig_position;
  Array<float3> normal;
  Array<float4> col;
  Array<float> mask;

  Array<float4> loop_col;
  Array<float4> orig_loop_col;

  /* Mesh. */

  Array<int> vert_indices;
  int unique_verts_num;

  Array<int> corner_indices;

  BitVector<> vert_hidden;
  BitVector<> face_hidden;

  /* Multires. */

  /** Indices of grids in the pbvh::Tree node. */
  Array<int> grids;
  BitGroupVector<> grid_hidden;

  /* Sculpt Face Sets */
  Array<int> face_sets;

  Vector<int> face_indices;
};

/**
 * Store undo data of the given type for a pbvh::Tree node. This function can be called by multiple
 * threads concurrently, as long as they don't pass the same pbvh::Tree node.
 *
 * This is only possible when building an undo step, in between #push_begin and #push_end.
 */
void push_node(const Depsgraph &depsgraph,
               const Object &object,
               const bke::pbvh::Node *node,
               undo::Type type);
void push_nodes(const Depsgraph &depsgraph,
                Object &object,
                Span<const bke::pbvh::Node *> nodes,
                undo::Type type);

/**
 * Retrieve the undo data of a given type for the active undo step. For example, this is used to
 * access "original" data from before the current stroke.
 *
 * This is only possible when building an undo step, in between #push_begin and #push_end.
 */
const undo::Node *get_node(const bke::pbvh::Node *node, undo::Type type);

/**
 * Pushes an undo step using the operator name. This is necessary for
 * redo panels to work; operators that do not support that may use
 * #push_begin_ex instead if so desired.
 */
void push_begin(Object &ob, const wmOperator *op);

/**
 * NOTE: #push_begin is preferred since `name`
 * must match operator name for redo panels to work.
 */
void push_begin_ex(Object &ob, const char *name);
void push_end(Object &ob);
void push_end_ex(Object &ob, bool use_nested_undo);

void restore_from_bmesh_enter_geometry(const StepData &step_data, Mesh &mesh);
BMLogEntry *get_bmesh_log_entry();

void restore_position_from_undo_step(const Depsgraph &depsgraph, Object &object);
}  // namespace blender::ed::sculpt_paint::undo
