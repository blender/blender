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
 * The Original Code is Copyright (C) 2019 by Blender Foundation
 * All rights reserved.
 */

#ifndef __BKE_MESH_REMESH_VOXEL_H__
#define __BKE_MESH_REMESH_VOXEL_H__

/** \file
 * \ingroup bke
 */

#ifdef WITH_OPENVDB
#  include "openvdb_capi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;

/* OpenVDB Voxel Remesher */
#ifdef WITH_OPENVDB
struct OpenVDBLevelSet *BKE_mesh_remesh_voxel_ovdb_mesh_to_level_set_create(
    struct Mesh *mesh, struct OpenVDBTransform *transform);
struct Mesh *BKE_mesh_remesh_voxel_ovdb_volume_to_mesh_nomain(struct OpenVDBLevelSet *level_set,
                                                              double isovalue,
                                                              double adaptivity,
                                                              bool relax_disoriented_triangles);
#endif

struct Mesh *BKE_mesh_remesh_voxel_fix_poles(struct Mesh *mesh);
struct Mesh *BKE_mesh_remesh_voxel_to_mesh_nomain(struct Mesh *mesh,
                                                  float voxel_size,
                                                  float adaptivity,
                                                  float isovalue);
struct Mesh *BKE_mesh_remesh_quadriflow_to_mesh_nomain(struct Mesh *mesh,
                                                       int target_faces,
                                                       int seed,
                                                       bool preserve_sharp,
                                                       bool preserve_boundary,
                                                       bool adaptive_scale,
                                                       void *update_cb,
                                                       void *update_cb_data);

/* Data reprojection functions */
void BKE_mesh_remesh_reproject_paint_mask(struct Mesh *target, struct Mesh *source);
void BKE_remesh_reproject_vertex_paint(struct Mesh *target, struct Mesh *source);
void BKE_remesh_reproject_sculpt_face_sets(struct Mesh *target, struct Mesh *source);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_MESH_REMESH_VOXEL_H__ */
