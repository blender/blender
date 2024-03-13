/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * This file contains access functions for the Mesh.runtime struct.
 */

struct CustomData_MeshMasks;
struct Depsgraph;
struct KeyBlock;
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
 *
 * Also note that some derived caches like #CD_TANGENT are stored directly in #CustomData.
 */
void BKE_mesh_runtime_clear_geometry(Mesh *mesh);

/**
 * Similar to #BKE_mesh_runtime_clear_geometry, but subtly different in that it also clears
 * data-block level features like evaluated data-blocks and edit mode data. They will be
 * functionally the same in most cases, but prefer this function if unsure, since it clears
 * more data.
 */
void BKE_mesh_runtime_clear_cache(Mesh *mesh);

/* NOTE: the functions below are defined in DerivedMesh.cc, and are intended to be moved
 * to a more suitable location when that file is removed.
 * They should also be renamed to use conventions from BKE, not old DerivedMesh.cc.
 * For now keep the names similar to avoid confusion. */

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

void BKE_mesh_runtime_eval_to_meshkey(Mesh *me_deformed, Mesh *mesh, KeyBlock *kb);

#ifndef NDEBUG
bool BKE_mesh_runtime_is_valid(Mesh *mesh_eval);
#endif /* !NDEBUG */
