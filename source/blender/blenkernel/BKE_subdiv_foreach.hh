/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"

struct Mesh;

namespace blender::bke::subdiv {

struct ToMeshSettings;
struct ForeachContext;
struct Subdiv;

using ForeachTopologyInformationCb = bool (*)(const ForeachContext *context,
                                              int num_vertices,
                                              int num_edges,
                                              int num_loops,
                                              int num_faces,
                                              const int *subdiv_face_offset);

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
                               bool is_loose,
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

using ForeachPolygonCb = void (*)(const ForeachContext *context,
                                  void *tls,
                                  int coarse_face_index,
                                  int subdiv_face_index,
                                  int start_loop_index,
                                  int num_loops);

using ForeachLooseCb = void (*)(const ForeachContext *context,
                                void *tls,
                                int coarse_vert_index,
                                int subdiv_vert_index);

using ForeachVertOfLooseEdgeCb = void (*)(const ForeachContext *context,
                                          void *tls,
                                          int coarse_edge_index,
                                          float u,
                                          int subdiv_vert_index);

struct ForeachContext {
  /**
   * Is called when topology information becomes available.
   * Is only called once.
   *
   * \note If this callback returns false, the foreach loop is aborted.
   */
  ForeachTopologyInformationCb topology_info = nullptr;
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
   * Called once for each loose vertex. One loose coarse vertex corresponds
   * to a single subdivision vertex.
   */
  ForeachLooseCb vert_loose = nullptr;
  /** Called once per vertex created for loose edge. */
  ForeachVertOfLooseEdgeCb vert_of_loose_edge = nullptr;
  /**
   * \note If subdivided edge does not come from coarse edge, ORIGINDEX_NONE
   * will be passed as coarse_edge_index.
   */
  ForeachEdgeCb edge = nullptr;
  /**
   * \note If subdivided loop does not come from coarse loop, ORIGINDEX_NONE
   * will be passed as coarse_loop_index.
   */
  ForeachLoopCb loop = nullptr;
  ForeachPolygonCb poly = nullptr;

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
 * Returns true if the whole topology was traversed, without any early exits.
 *
 * TODO(sergey): Need to either get rid of subdiv or of coarse_mesh.
 * The main point here is to be able to get base level topology, which can be
 * done with either of those. Having both of them is kind of redundant.

 */
bool foreach_subdiv_geometry(Subdiv *subdiv,
                             const ForeachContext *context,
                             const ToMeshSettings *mesh_settings,
                             const Mesh *coarse_mesh);

}  // namespace blender::bke::subdiv
