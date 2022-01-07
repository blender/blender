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
 */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;

typedef enum MeshForeachFlag {
  MESH_FOREACH_NOP = 0,
  /* foreachMappedVert, foreachMappedLoop, foreachMappedFaceCenter */
  MESH_FOREACH_USE_NORMAL = (1 << 0),
} MeshForeachFlag;

void BKE_mesh_foreach_mapped_vert(struct Mesh *mesh,
                                  void (*func)(void *userData,
                                               int index,
                                               const float co[3],
                                               const float no_f[3],
                                               const short no_s[3]),
                                  void *userData,
                                  MeshForeachFlag flag);
/**
 * Copied from #cdDM_foreachMappedEdge.
 * \param tot_edges: Number of original edges. Used to avoid calling the callback with invalid
 * edge indices.
 */
void BKE_mesh_foreach_mapped_edge(
    struct Mesh *mesh,
    int tot_edges,
    void (*func)(void *userData, int index, const float v0co[3], const float v1co[3]),
    void *userData);
void BKE_mesh_foreach_mapped_loop(struct Mesh *mesh,
                                  void (*func)(void *userData,
                                               int vertex_index,
                                               int face_index,
                                               const float co[3],
                                               const float no[3]),
                                  void *userData,
                                  MeshForeachFlag flag);
void BKE_mesh_foreach_mapped_face_center(
    struct Mesh *mesh,
    void (*func)(void *userData, int index, const float cent[3], const float no[3]),
    void *userData,
    MeshForeachFlag flag);
void BKE_mesh_foreach_mapped_subdiv_face_center(
    struct Mesh *mesh,
    void (*func)(void *userData, int index, const float cent[3], const float no[3]),
    void *userData,
    MeshForeachFlag flag);

void BKE_mesh_foreach_mapped_vert_coords_get(struct Mesh *me_eval, float (*r_cos)[3], int totcos);

#ifdef __cplusplus
}
#endif
