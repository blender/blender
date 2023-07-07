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

struct Mesh *BKE_mesh_wrapper_from_editmesh(struct BMEditMesh *em,
                                            const struct CustomData_MeshMasks *cd_mask_extra,
                                            const struct Mesh *me_settings);
void BKE_mesh_wrapper_ensure_mdata(struct Mesh *me);
bool BKE_mesh_wrapper_minmax(const struct Mesh *me, float min[3], float max[3]);

int BKE_mesh_wrapper_vert_len(const struct Mesh *me);
int BKE_mesh_wrapper_edge_len(const struct Mesh *me);
int BKE_mesh_wrapper_loop_len(const struct Mesh *me);
int BKE_mesh_wrapper_poly_len(const struct Mesh *me);

/**
 * Return a contiguous array of vertex position values, if available.
 * Otherwise, vertex positions are stored in BMesh vertices.
 */
const float (*BKE_mesh_wrapper_vert_coords(const struct Mesh *mesh))[3];

/**
 * Return a contiguous array of face normal values, if available.
 * Otherwise, normals are stored in BMesh faces.
 */
const float (*BKE_mesh_wrapper_poly_normals(struct Mesh *mesh))[3];

void BKE_mesh_wrapper_tag_positions_changed(struct Mesh *mesh);

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
