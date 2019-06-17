/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#ifndef __BKE_SUBDIV_FOREACH_H__
#define __BKE_SUBDIV_FOREACH_H__

#include "BLI_sys_types.h"

struct Mesh;
struct Subdiv;
struct SubdivForeachContext;
struct SubdivToMeshSettings;

typedef bool (*SubdivForeachTopologyInformationCb)(const struct SubdivForeachContext *context,
                                                   const int num_vertices,
                                                   const int num_edges,
                                                   const int num_loops,
                                                   const int num_polygons);

typedef void (*SubdivForeachVertexFromCornerCb)(const struct SubdivForeachContext *context,
                                                void *tls,
                                                const int ptex_face_index,
                                                const float u,
                                                const float v,
                                                const int coarse_vertex_index,
                                                const int coarse_poly_index,
                                                const int coarse_corner,
                                                const int subdiv_vertex_index);

typedef void (*SubdivForeachVertexFromEdgeCb)(const struct SubdivForeachContext *context,
                                              void *tls,
                                              const int ptex_face_index,
                                              const float u,
                                              const float v,
                                              const int coarse_edge_index,
                                              const int coarse_poly_index,
                                              const int coarse_corner,
                                              const int subdiv_vertex_index);

typedef void (*SubdivForeachVertexInnerCb)(const struct SubdivForeachContext *context,
                                           void *tls,
                                           const int ptex_face_index,
                                           const float u,
                                           const float v,
                                           const int coarse_poly_index,
                                           const int coarse_corner,
                                           const int subdiv_vertex_index);

typedef void (*SubdivForeachEdgeCb)(const struct SubdivForeachContext *context,
                                    void *tls,
                                    const int coarse_edge_index,
                                    const int subdiv_edge_index,
                                    const int subdiv_v1,
                                    const int subdiv_v2);

typedef void (*SubdivForeachLoopCb)(const struct SubdivForeachContext *context,
                                    void *tls,
                                    const int ptex_face_index,
                                    const float u,
                                    const float v,
                                    const int coarse_loop_index,
                                    const int coarse_poly_index,
                                    const int coarse_corner,
                                    const int subdiv_loop_index,
                                    const int subdiv_vertex_index,
                                    const int subdiv_edge_index);

typedef void (*SubdivForeachPolygonCb)(const struct SubdivForeachContext *context,
                                       void *tls,
                                       const int coarse_poly_index,
                                       const int subdiv_poly_index,
                                       const int start_loop_index,
                                       const int num_loops);

typedef void (*SubdivForeachLooseCb)(const struct SubdivForeachContext *context,
                                     void *tls,
                                     const int coarse_vertex_index,
                                     const int subdiv_vertex_index);

typedef void (*SubdivForeachVertexOfLooseEdgeCb)(const struct SubdivForeachContext *context,
                                                 void *tls,
                                                 const int coarse_edge_index,
                                                 const float u,
                                                 const int subdiv_vertex_index);

typedef struct SubdivForeachContext {
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
  /* Called once for each loose vertex. One loose coarse vertexcorresponds
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
   * traversal is happening for,
   */
  void *user_data;

  /* Initial value of TLS data. */
  void *user_data_tls;
  /* Size of TLS data. */
  size_t user_data_tls_size;
  /* Function to free TLS storage. */
  void (*user_data_tls_free)(void *tls);
} SubdivForeachContext;

/* Invokes callbacks in the order and with values which corresponds to creation
 * of final subdivided mesh.
 *
 * Returns truth if the whole topology was traversed, without any early exits.
 *
 * TODO(sergey): Need to either get rid of subdiv or of coarse_mesh.
 * The main point here is to be able to get base level topology, which can be
 * done with either of those. Having both of them is kind of redundant.
 */
bool BKE_subdiv_foreach_subdiv_geometry(struct Subdiv *subdiv,
                                        const struct SubdivForeachContext *context,
                                        const struct SubdivToMeshSettings *mesh_settings,
                                        const struct Mesh *coarse_mesh);

#endif /* __BKE_SUBDIV_FOREACH_H__ */
