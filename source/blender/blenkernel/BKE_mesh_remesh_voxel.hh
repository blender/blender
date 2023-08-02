/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct Mesh;

struct Mesh *BKE_mesh_remesh_voxel_fix_poles(const struct Mesh *mesh);
struct Mesh *BKE_mesh_remesh_voxel(const struct Mesh *mesh,
                                   float voxel_size,
                                   float adaptivity,
                                   float isovalue);
struct Mesh *BKE_mesh_remesh_quadriflow(const struct Mesh *mesh,
                                        int target_faces,
                                        int seed,
                                        bool preserve_sharp,
                                        bool preserve_boundary,
                                        bool adaptive_scale,
                                        void (*update_cb)(void *, float progress, int *cancel),
                                        void *update_cb_data);

/* Data reprojection functions */
void BKE_mesh_remesh_reproject_paint_mask(struct Mesh *target, const struct Mesh *source);
void BKE_remesh_reproject_vertex_paint(struct Mesh *target, const struct Mesh *source);
void BKE_remesh_reproject_sculpt_face_sets(struct Mesh *target, const struct Mesh *source);
