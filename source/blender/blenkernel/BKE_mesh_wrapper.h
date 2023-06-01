/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

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
    const float (*vert_coords)[3],
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

struct Mesh *BKE_mesh_wrapper_ensure_subdivision(struct Mesh *me);

#ifdef __cplusplus
}
#endif
