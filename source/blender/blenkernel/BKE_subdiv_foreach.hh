/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_offset_indices.hh"
#include "BLI_span.hh"
#include "BLI_sys_types.hh"

namespace blender {

struct Mesh;

namespace bke::subdiv {

struct ForeachContext;
struct Subdiv;

/**
 * Describes how elements of the subdivided mesh correspond to the elements of the base mesh.
 * The subdivided elements are stored in groups for each base element:
 * - Vertices: The mesh starts with a vertex for every base mesh vertex, with the same index. Then
 *   `resolution - 2` vertices for every base mesh edge. Then the vertices inside of every base
 *   mesh face.
 * - Edges: First `resolution - 1` edges for every base mesh edge, then the edges inside of every
 *   base mesh face.
 * - Faces: The quads created for every base mesh face are contiguous.
 */
struct MeshLayout {
  /** Number of vertices along each side of the grid that a base mesh quad is subdivided into. */
  int resolution = 0;

  int verts_num = 0;
  int edges_num = 0;
  int faces_num = 0;
  int corners_num = 0;

  /** Index of the first vertex inside of a base mesh edge. */
  int edge_verts_start = 0;
  /** Index of the first vertex inside of a base mesh face. */
  int inner_verts_start = 0;
  /** Index of the first edge inside of a base mesh face. */
  int inner_edges_start = 0;

  /** Groups vertices inside of base mesh faces by face, relative to #inner_verts_start. */
  Array<int> inner_vert_offsets;
  /** Groups edges inside of base mesh faces by face, relative to #inner_edges_start. */
  Array<int> inner_edge_offsets;
  /** Groups subdivided faces by the base mesh face they come from. */
  Array<int> face_offsets;

  /** Vertices created inside of a base mesh edge, ordered from its first to second vertex. */
  IndexRange base_edge_verts(const int base_edge) const
  {
    const int verts_num = this->resolution - 2;
    return IndexRange(this->edge_verts_start + base_edge * verts_num, verts_num);
  }
};

/**
 * Calculate how the elements of a mesh subdivided with the given resolution are indexed. See
 * #ToMeshSettings::resolution.
 */
MeshLayout build_mesh_layout(const Mesh &coarse_mesh, int resolution);

using ForeachVertFromCornerCb = void (*)(const ForeachContext *context,
                                         void *tls,
                                         int ptex_face_index,
                                         float u,
                                         float v,
                                         int coarse_vert_index,
                                         int coarse_face_index,
                                         int coarse_corner,
                                         int subdiv_vert_index);

using ForeachVertFromEdgeCb = void (*)(const ForeachContext *context,
                                       void *tls,
                                       int ptex_face_index,
                                       float u,
                                       float v,
                                       int coarse_edge_index,
                                       int coarse_face_index,
                                       int coarse_corner,
                                       int subdiv_vert_index);

using ForeachVertInnerCb = void (*)(const ForeachContext *context,
                                    void *tls,
                                    int ptex_face_index,
                                    float u,
                                    float v,
                                    int coarse_face_index,
                                    int coarse_corner,
                                    int subdiv_vert_index);

using ForeachEdgeCb = void (*)(const ForeachContext *context,
                               void *tls,
                               int coarse_edge_index,
                               int subdiv_edge_index,
                               int subdiv_v1,
                               int subdiv_v2);

using ForeachLoopCb = void (*)(const ForeachContext *context,
                               void *tls,
                               int ptex_face_index,
                               float u,
                               float v,
                               int coarse_loop_index,
                               int coarse_face_index,
                               int coarse_corner,
                               int subdiv_loop_index,
                               int subdiv_vert_index,
                               int subdiv_edge_index);

/**
 * \param subdiv_faces_by_base_face: Groups the subdivided faces by the base mesh face they came
 * from (all the subdivided faces for a single base mesh face are contiguous). For more
 * information, see #OffsetIndices.
 */
using FacesCb = void (*)(const ForeachContext *context,
                         OffsetIndices<int> subdiv_faces_by_base_face);

struct ForeachContext {
  /**
   * These callbacks are called from every ptex which shares "emitting"
   * vertex or edge.
   */
  ForeachVertFromCornerCb vert_every_corner = nullptr;
  ForeachVertFromEdgeCb vert_every_edge = nullptr;
  /**
   * Those callbacks are run once per subdivision vertex, ptex is undefined
   * as in it will be whatever first ptex face happened to be traversed in
   * the multi-threaded environment and which shares "emitting" vertex or edge.
   */
  ForeachVertFromCornerCb vert_corner = nullptr;
  ForeachVertFromEdgeCb vert_edge = nullptr;
  /** Called exactly once, always corresponds to a single ptex face. */
  ForeachVertInnerCb vert_inner = nullptr;
  /**
   * \note If subdivided edge does not come from coarse edge, ORIGINDEX_NONE
   * will be passed as coarse_edge_index.
   */
  ForeachEdgeCb edge = nullptr;
  /**
   * \note If subdivided loop does not come from coarse loop, ORIGINDEX_NONE
   * will be passed as coarse_loop_index.
   *
   * \note All the face corners for a particular face are contiguous, and the face corners have the
   * same order in the mesh as their faces.
   */
  ForeachLoopCb loop = nullptr;

  /**
   * Called once for the whole mesh, passing the #OffsetIndices encoding the groups of subdivided
   * faces corresponding to each base mesh face.
   */
  FacesCb faces = nullptr;

  /**
   * User-defined pointer, to allow callbacks know something about context the
   * traversal is happening for.
   */
  void *user_data = nullptr;

  /** Initial value of TLS data. */
  void *user_data_tls = nullptr;
  /** Size of TLS data. */
  size_t user_data_tls_size = 0;
  /** Function to free TLS storage. */
  void (*user_data_tls_free)(void *tls) = nullptr;
};

/**
 * Invokes callbacks in the order and with values which corresponds to creation
 * of final subdivided mesh.
 *
 * Main goal is to abstract all the traversal routines to give geometry element
 * indices (for vertices, edges, loops, faces) in the same way as subdivision
 * modifier will do for a dense mesh.
 *
 * \param layout: The result of #build_mesh_layout for the base mesh.
 *
 * TODO(@sergey): Need to either get rid of subdiv or of coarse_mesh.
 * The main point here is to be able to get base level topology, which can be
 * done with either of those. Having both of them is kind of redundant.
 */
void foreach_subdiv_geometry(Subdiv *subdiv,
                             const ForeachContext *context,
                             const MeshLayout &layout,
                             const Mesh *coarse_mesh);

}  // namespace bke::subdiv
}  // namespace blender
