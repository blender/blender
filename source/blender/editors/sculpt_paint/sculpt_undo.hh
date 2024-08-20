/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include <mutex>

#include "BLI_array.hh"
#include "BLI_bit_group_vector.hh"
#include "BLI_bit_vector.hh"
#include "BLI_map.hh"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

#include "DNA_customdata_types.h"

struct BMLogEntry;
struct Depsgraph;
struct Mesh;
struct Object;
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

/* Storage of geometry for the undo node.
 * Is used as a storage for either original or modified geometry. */
struct NodeGeometry {
  /* Is used for sanity check, helping with ensuring that two and only two
   * geometry pushes happened in the undo stack. */
  bool is_initialized;

  CustomData vert_data;
  CustomData edge_data;
  CustomData corner_data;
  CustomData face_data;
  int *face_offset_indices;
  const ImplicitSharingInfo *face_offsets_sharing_info;
  int totvert;
  int totedge;
  int totloop;
  int faces_num;
};

struct StepData {
  /**
   * The type of data stored in this undo step. For historical reasons this is often set when the
   * first undo node is pushed.
   */
  Type type = Type::None;

  /** Name of the object associated with this undo data (`object.id.name`). */
  std::string object_name;

  /** Name of the object's active shape key when the undo step was created. */
  std::string active_shape_key_name;

  /* The number of vertices in the entire mesh. */
  int mesh_verts_num;
  /* The number of face corners in the entire mesh. */
  int mesh_corners_num;

  /** The number of grids in the entire mesh. */
  int mesh_grids_num;
  /** A copy of #SubdivCCG::grid_size. */
  int grid_size;

  float3 pivot_pos;
  float4 pivot_rot;

  /* Geometry modification operations.
   *
   * Original geometry is stored before some modification is run and is used to restore state of
   * the object when undoing the operation
   *
   * Modified geometry is stored after the modification and is used to redo the modification. */
  bool geometry_clear_pbvh;
  NodeGeometry geometry_original;
  NodeGeometry geometry_modified;

  /* bmesh */
  BMLogEntry *bm_entry;

  /* Geometry at the bmesh enter moment. */
  NodeGeometry geometry_bmesh_enter;

  bool applied;

  std::mutex nodes_mutex;

  /**
   * #undo::Node is stored per pbvh::Node to reduce data storage needed for changes only impacting
   * small portions of the mesh. During undo step creation and brush evaluation we often need to
   * look up the undo state for a specific node. That lookup must be protected by a lock since
   * nodes are pushed from multiple threads. This map speeds up undo node access to reduce the
   * amount of time we wait for the lock.
   *
   * This is only accessible when building the undo step, in between #push_begin and #push_end.
   *
   * \todo All nodes in a single step have the same type, so using the type as part of the map key
   * should be unnecessary. However, to remove it, first the storage of the undo type should be
   * moved to #StepData from #Node.
   */
  Map<const bke::pbvh::Node *, std::unique_ptr<Node>> undo_nodes_by_pbvh_node;

  /** Storage of per-node undo data after creation of the undo step is finished. */
  Vector<std::unique_ptr<Node>> nodes;

  size_t undo_size;
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
