/* SPDX-FileCopyrightText: 2018 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_sys_types.h"

struct Mesh;
struct Subdiv;
struct SubdivForeachContext;
struct SubdivToMeshSettings;

using SubdivForeachTopologyInformationCb = bool (*)(const SubdivForeachContext *context,
                                                    int num_vertices,
                                                    int num_edges,
                                                    int num_loops,
                                                    int num_polygons,
                                                    const int *subdiv_polygon_offset);

using SubdivForeachVertexFromCornerCb = void (*)(const SubdivForeachContext *context,
                                                 void *tls,
                                                 int ptex_face_index,
                                                 float u,
                                                 float v,
                                                 int coarse_vertex_index,
                                                 int coarse_poly_index,
                                                 int coarse_corner,
                                                 int subdiv_vertex_index);

using SubdivForeachVertexFromEdgeCb = void (*)(const SubdivForeachContext *context,
                                               void *tls,
                                               int ptex_face_index,
                                               float u,
                                               float v,
                                               int coarse_edge_index,
                                               int coarse_poly_index,
                                               int coarse_corner,
                                               int subdiv_vertex_index);

using SubdivForeachVertexInnerCb = void (*)(const SubdivForeachContext *context,
                                            void *tls,
                                            int ptex_face_index,
                                            float u,
                                            float v,
                                            int coarse_poly_index,
                                            int coarse_corner,
                                            int subdiv_vertex_index);

using SubdivForeachEdgeCb = void (*)(const SubdivForeachContext *context,
                                     void *tls,
                                     int coarse_edge_index,
                                     int subdiv_edge_index,
                                     bool is_loose,
                                     int subdiv_v1,
                                     int subdiv_v2);

using SubdivForeachLoopCb = void (*)(const SubdivForeachContext *context,
                                     void *tls,
                                     int ptex_face_index,
                                     float u,
                                     float v,
                                     int coarse_loop_index,
                                     int coarse_poly_index,
                                     int coarse_corner,
                                     int subdiv_loop_index,
                                     int subdiv_vertex_index,
                                     int subdiv_edge_index);

using SubdivForeachPolygonCb = void (*)(const SubdivForeachContext *context,
                                        void *tls,
                                        int coarse_poly_index,
                                        int subdiv_poly_index,
                                        int start_loop_index,
                                        int num_loops);

using SubdivForeachLooseCb = void (*)(const SubdivForeachContext *context,
                                      void *tls,
                                      int coarse_vertex_index,
                                      int subdiv_vertex_index);

using SubdivForeachVertexOfLooseEdgeCb = void (*)(const SubdivForeachContext *context,
                                                  void *tls,
                                                  int coarse_edge_index,
                                                  float u,
                                                  int subdiv_vertex_index);

struct SubdivForeachContext {
  /* Is called when topology information becomes available.
   * Is only called once.
   *
   * NOTE: If this callback returns false, the foreach loop is aborted.
   */
  SubdivForeachTopologyInformationCb topology_info;
  /* These callbacks are called from every ptex which shares "emitting"
   * vertex or edge.
   */
  SubdivForeachVertexFromCornerCb vertex_every_corner;
  SubdivForeachVertexFromEdgeCb vertex_every_edge;
  /* Those callbacks are run once per subdivision vertex, ptex is undefined
   * as in it will be whatever first ptex face happened to be traversed in
   * the multi-threaded environment and which shares "emitting" vertex or
   * edge.
   */
  SubdivForeachVertexFromCornerCb vertex_corner;
  SubdivForeachVertexFromEdgeCb vertex_edge;
  /* Called exactly once, always corresponds to a single ptex face. */
  SubdivForeachVertexInnerCb vertex_inner;
  /* Called once for each loose vertex. One loose coarse vertex corresponds
   * to a single subdivision vertex.
   */
  SubdivForeachLooseCb vertex_loose;
  /* Called once per vertex created for loose edge. */
  SubdivForeachVertexOfLooseEdgeCb vertex_of_loose_edge;
  /* NOTE: If subdivided edge does not come from coarse edge, ORIGINDEX_NONE
   * will be passed as coarse_edge_index.
   */
  SubdivForeachEdgeCb edge;
  /* NOTE: If subdivided loop does not come from coarse loop, ORIGINDEX_NONE
   * will be passed as coarse_loop_index.
   */
  SubdivForeachLoopCb loop;
  SubdivForeachPolygonCb poly;

  /* User-defined pointer, to allow callbacks know something about context the
   * traversal is happening for.
   */
  void *user_data;

  /* Initial value of TLS data. */
  void *user_data_tls;
  /* Size of TLS data. */
  size_t user_data_tls_size;
  /* Function to free TLS storage. */
  void (*user_data_tls_free)(void *tls);
};

/* Invokes callbacks in the order and with values which corresponds to creation
 * of final subdivided mesh.
 *
 * Main goal is to abstract all the traversal routines to give geometry element
 * indices (for vertices, edges, loops, polygons) in the same way as subdivision
 * modifier will do for a dense mesh.
 *
 * Returns true if the whole topology was traversed, without any early exits.
 *
 * TODO(sergey): Need to either get rid of subdiv or of coarse_mesh.
 * The main point here is to be able to get base level topology, which can be
 * done with either of those. Having both of them is kind of redundant.
 */
bool BKE_subdiv_foreach_subdiv_geometry(Subdiv *subdiv,
                                        const SubdivForeachContext *context,
                                        const SubdivToMeshSettings *mesh_settings,
                                        const Mesh *coarse_mesh);
