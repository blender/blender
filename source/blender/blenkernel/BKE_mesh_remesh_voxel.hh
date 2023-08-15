/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct Mesh;

Mesh *BKE_mesh_remesh_voxel_fix_poles(const Mesh *mesh);
Mesh *BKE_mesh_remesh_voxel(const Mesh *mesh, float voxel_size, float adaptivity, float isovalue);
Mesh *BKE_mesh_remesh_quadriflow(const Mesh *mesh,
                                 int target_faces,
                                 int seed,
                                 bool preserve_sharp,
                                 bool preserve_boundary,
                                 bool adaptive_scale,
                                 void (*update_cb)(void *, float progress, int *cancel),
                                 void *update_cb_data);

/* Data reprojection functions */
void BKE_mesh_remesh_reproject_paint_mask(Mesh *target, const Mesh *source);
void BKE_remesh_reproject_vertex_paint(Mesh *target, const Mesh *source);
void BKE_remesh_reproject_sculpt_face_sets(Mesh *target, const Mesh *source);
