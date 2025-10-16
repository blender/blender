/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * This file contains access functions for the Mesh.runtime struct.
 */

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

struct BMEditMesh;
struct CustomData_MeshMasks;
struct Depsgraph;
struct KeyBlock;
struct ModifierData;
struct Mesh;
struct Object;
struct Scene;

/** Return the number of derived triangles (corner_tris). */
int BKE_mesh_runtime_corner_tris_len(const Mesh *mesh);

void BKE_mesh_runtime_ensure_edit_data(Mesh *mesh);

/**
 * Clear and free any derived caches associated with the mesh geometry data. Examples include BVH
 * caches, normals, triangulation, etc. This should be called when replacing a mesh's geometry
 * directly or making other large changes to topology. It does not need to be called on new meshes.
 *
 * For "smaller" changes to meshes like updating positions, consider calling a more specific update
 * function like #Mesh::tag_positions_changed().
 */
void BKE_mesh_runtime_clear_geometry(Mesh *mesh);

/**
 * Similar to #BKE_mesh_runtime_clear_geometry, but subtly different in that it also clears
 * data-block level features like evaluated data-blocks and edit mode data. They will be
 * functionally the same in most cases, but prefer this function if unsure, since it clears
 * more data.
 */
void BKE_mesh_runtime_clear_cache(Mesh *mesh);

namespace blender::bke {

void mesh_get_mapped_verts_coords(Mesh *mesh_eval, MutableSpan<float3> r_cos);

Mesh *editbmesh_get_eval_cage(Depsgraph *depsgraph,
                              const Scene *scene,
                              Object *obedit,
                              BMEditMesh *em,
                              const CustomData_MeshMasks *dataMask);
Mesh *editbmesh_get_eval_cage_from_orig(Depsgraph *depsgraph,
                                        const Scene *scene,
                                        Object *obedit,
                                        const CustomData_MeshMasks *dataMask);

bool editbmesh_modifier_is_enabled(const Scene *scene,
                                   const Object *ob,
                                   ModifierData *md,
                                   bool has_prev_mesh);

Mesh *mesh_get_eval_deform(Depsgraph *depsgraph,
                           const Scene *scene,
                           Object *ob,
                           const CustomData_MeshMasks *dataMask);

Mesh *mesh_create_eval_final(Depsgraph *depsgraph,
                             const Scene *scene,
                             Object *ob,
                             const CustomData_MeshMasks *dataMask);

Mesh *mesh_create_eval_no_deform(Depsgraph *depsgraph,
                                 const Scene *scene,
                                 Object *ob,
                                 const CustomData_MeshMasks *dataMask);
Mesh *mesh_create_eval_no_deform_render(Depsgraph *depsgraph,
                                        const Scene *scene,
                                        Object *ob,
                                        const CustomData_MeshMasks *dataMask);

}  // namespace blender::bke
