/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * This file contains access functions for the Mesh.runtime struct.
 */

#include "BKE_mesh_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CustomData_MeshMasks;
struct Depsgraph;
struct KeyBlock;
struct MLoopTri;
struct MVertTri;
struct Mesh;
struct Object;
struct Scene;

/** Return the number of derived triangles (looptris). */
int BKE_mesh_runtime_looptri_len(const struct Mesh *mesh);

/**
 * Return mesh triangulation data, calculated lazily when necessary.
 * See #MLoopTri for further description of mesh triangulation.
 *
 * \note Prefer #Mesh::looptris() in C++ code.
 */
const struct MLoopTri *BKE_mesh_runtime_looptri_ensure(const struct Mesh *mesh);
const int *BKE_mesh_runtime_looptri_faces_ensure(const struct Mesh *mesh);

bool BKE_mesh_runtime_ensure_edit_data(struct Mesh *mesh);
void BKE_mesh_runtime_reset_edit_data(struct Mesh *mesh);

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
void BKE_mesh_runtime_clear_geometry(struct Mesh *mesh);

/**
 * Similar to #BKE_mesh_runtime_clear_geometry, but subtly different in that it also clears
 * data-block level features like evaluated data-blocks and edit mode data. They will be
 * functionally the same in most cases, but prefer this function if unsure, since it clears
 * more data.
 */
void BKE_mesh_runtime_clear_cache(struct Mesh *mesh);

/**
 * Convert triangles encoded as face corner indices to triangles encoded as vertex indices.
 */
void BKE_mesh_runtime_verttri_from_looptri(struct MVertTri *r_verttri,
                                           const int *corner_verts,
                                           const struct MLoopTri *looptri,
                                           int looptri_num);

/** \note Only used for access in C. */
bool BKE_mesh_is_deformed_only(const struct Mesh *mesh);
/** \note Only used for access in C. */
eMeshWrapperType BKE_mesh_wrapper_type(const struct Mesh *mesh);

/* NOTE: the functions below are defined in DerivedMesh.cc, and are intended to be moved
 * to a more suitable location when that file is removed.
 * They should also be renamed to use conventions from BKE, not old DerivedMesh.cc.
 * For now keep the names similar to avoid confusion. */

struct Mesh *mesh_get_eval_deform(struct Depsgraph *depsgraph,
                                  const struct Scene *scene,
                                  struct Object *ob,
                                  const struct CustomData_MeshMasks *dataMask);

struct Mesh *mesh_create_eval_final(struct Depsgraph *depsgraph,
                                    const struct Scene *scene,
                                    struct Object *ob,
                                    const struct CustomData_MeshMasks *dataMask);

struct Mesh *mesh_create_eval_no_deform(struct Depsgraph *depsgraph,
                                        const struct Scene *scene,
                                        struct Object *ob,
                                        const struct CustomData_MeshMasks *dataMask);
struct Mesh *mesh_create_eval_no_deform_render(struct Depsgraph *depsgraph,
                                               const struct Scene *scene,
                                               struct Object *ob,
                                               const struct CustomData_MeshMasks *dataMask);

void BKE_mesh_runtime_eval_to_meshkey(struct Mesh *me_deformed,
                                      struct Mesh *me,
                                      struct KeyBlock *kb);

#ifndef NDEBUG
bool BKE_mesh_runtime_is_valid(struct Mesh *me_eval);
#endif /* NDEBUG */

#ifdef __cplusplus
}
#endif
