/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

struct BMEditMesh;
struct CustomData_MeshMasks;
struct Mesh;

Mesh *BKE_mesh_wrapper_from_editmesh(BMEditMesh *em,
                                     const CustomData_MeshMasks *cd_mask_extra,
                                     const Mesh *me_settings);
void BKE_mesh_wrapper_ensure_mdata(Mesh *mesh);

int BKE_mesh_wrapper_vert_len(const Mesh *mesh);
int BKE_mesh_wrapper_edge_len(const Mesh *mesh);
int BKE_mesh_wrapper_loop_len(const Mesh *mesh);
int BKE_mesh_wrapper_face_len(const Mesh *mesh);

/**
 * Return a contiguous array of vertex position values, if available.
 * Otherwise, vertex positions are stored in BMesh vertices and this returns null.
 */
blender::Span<blender::float3> BKE_mesh_wrapper_vert_coords(const Mesh *mesh);

/**
 * Return a contiguous array of face normal values, if available.
 * Otherwise, normals are stored in BMesh faces and this returns null.
 */
blender::Span<blender::float3> BKE_mesh_wrapper_face_normals(Mesh *mesh);

void BKE_mesh_wrapper_tag_positions_changed(Mesh *mesh);

void BKE_mesh_wrapper_vert_coords_copy(const Mesh *mesh,
                                       blender::MutableSpan<blender::float3> positions);
void BKE_mesh_wrapper_vert_coords_copy_with_mat4(const Mesh *mesh,
                                                 float (*vert_coords)[3],
                                                 int vert_coords_len,
                                                 const float mat[4][4]);

Mesh *BKE_mesh_wrapper_ensure_subdivision(Mesh *mesh);
