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
#ifndef __BKE_MESH_WRAPPER_H__
#define __BKE_MESH_WRAPPER_H__

/** \file
 * \ingroup bke
 */

struct BMEditMesh;
struct CustomData_MeshMasks;
struct Mesh;

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh *BKE_mesh_wrapper_from_editmesh_with_coords(
    struct BMEditMesh *em,
    const struct CustomData_MeshMasks *cd_mask_extra,
    float (*vertexCos)[3],
    const struct Mesh *me_settings);
struct Mesh *BKE_mesh_wrapper_from_editmesh(struct BMEditMesh *em,
                                            const struct CustomData_MeshMasks *cd_mask_extra,
                                            const struct Mesh *me_settings);
void BKE_mesh_wrapper_ensure_mdata(struct Mesh *me);
bool BKE_mesh_wrapper_minmax(const struct Mesh *me, float min[3], float max[3]);

int BKE_mesh_wrapper_vert_len(const struct Mesh *me);
int BKE_mesh_wrapper_edge_len(const struct Mesh *me);
int BKE_mesh_wrapper_loop_len(const struct Mesh *me);
int BKE_mesh_wrapper_poly_len(const struct Mesh *me);

void BKE_mesh_wrapper_vert_coords_copy(const struct Mesh *me,
                                       float (*vert_coords)[3],
                                       int vert_coords_len);
void BKE_mesh_wrapper_vert_coords_copy_with_mat4(const struct Mesh *me,
                                                 float (*vert_coords)[3],
                                                 int vert_coords_len,
                                                 const float mat[4][4]);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_MESH_WRAPPER_H__ */
