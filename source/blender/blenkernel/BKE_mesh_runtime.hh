/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * This file contains access functions for the Mesh.runtime struct.
 */

#include "BKE_mesh_types.hh"

struct CustomData_MeshMasks;
struct Depsgraph;
struct KeyBlock;
struct MLoopTri;
struct MVertTri;
struct Mesh;
struct Object;
struct Scene;

/** Return the number of derived triangles (looptris). */
int BKE_mesh_runtime_looptri_len(const Mesh *mesh);

const int *BKE_mesh_runtime_looptri_faces_ensure(const Mesh *mesh);

bool BKE_mesh_runtime_ensure_edit_data(Mesh *mesh);

/**
 * Clear and free any derived caches associated with the mesh geometry data. Examples include BVH
 * caches, normals, triangulation, etc. This should be called when replacing a mesh's geometry
 * directly or making other large changes to topology. It does not need to be called on new meshes.
 *
 * For "smaller" changes to meshes like updating positions, consider calling a more specific update
 * function like #BKE_mesh_tag_positions_changed.
 *
 * Also note that some derived caches like #CD_NORMAL and #CD_TANGENT are stored directly in
 * #CustomData.
 */
void BKE_mesh_runtime_clear_geometry(Mesh *mesh);

/**
 * Similar to #BKE_mesh_runtime_clear_geometry, but subtly different in that it also clears
 * data-block level features like evaluated data-blocks and edit mode data. They will be
 * functionally the same in most cases, but prefer this function if unsure, since it clears
 * more data.
 */
void BKE_mesh_runtime_clear_cache(Mesh *mesh);

/**
 * Convert triangles encoded as face corner indices to triangles encoded as vertex indices.
 */
void BKE_mesh_runtime_verttri_from_looptri(MVertTri *r_verttri,
                                           const int *corner_verts,
                                           const MLoopTri *looptri,
                                           int looptri_num);

/** \note Only used for access in C. */
bool BKE_mesh_is_deformed_only(const Mesh *mesh);
/** \note Only used for access in C. */
eMeshWrapperType BKE_mesh_wrapper_type(const Mesh *mesh);

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

void BKE_mesh_runtime_eval_to_meshkey(Mesh *me_deformed, Mesh *me, KeyBlock *kb);

#ifndef NDEBUG
bool BKE_mesh_runtime_is_valid(Mesh *me_eval);
#endif /* NDEBUG */
