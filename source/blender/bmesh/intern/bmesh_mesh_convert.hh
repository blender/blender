/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "BLI_string_ref.hh"

#include "bmesh.hh"

/**
 * \return Whether attributes with the given name are stored in special flags or fields in BMesh
 * rather than in the regular custom data blocks.
 */
bool BM_attribute_stored_in_bmesh_builtin(const blender::StringRef name);

struct CustomData_MeshMasks;
struct Main;
struct Mesh;

struct BMeshFromMeshParams {
  bool calc_face_normal;
  bool calc_vert_normal;
  /* add a vertex CD_SHAPE_KEYINDEX layer */
  bool add_key_index;
  /* set vertex coordinates from the shapekey */
  bool use_shapekey;
  /* define the active shape key (index + 1) */
  int active_shapekey;
  struct CustomData_MeshMasks cd_mask_extra;
};
/**
 * \brief Mesh -> BMesh
 * \param bm: The mesh to write into, while this is typically a newly created BMesh,
 * merging into existing data is supported.
 * Note the custom-data layout isn't used.
 * If more comprehensive merging is needed we should move this into a separate function
 * since this should be kept fast for edit-mode switching and storing undo steps.
 *
 * \warning This function doesn't calculate face normals.
 */
void BM_mesh_bm_from_me(BMesh *bm, const Mesh *mesh, const BMeshFromMeshParams *params)
    ATTR_NONNULL(1, 3);

struct BMeshToMeshParams {
  /** Update object hook indices & vertex parents. */
  bool calc_object_remap;
  /**
   * This re-assigns shape-key indices. Only do if the BMesh will have continued use
   * to update the mesh & shape key in the future.
   * In the case the BMesh is freed immediately, this can be left false.
   *
   * This is needed when flushing changes from edit-mode into object mode,
   * so a second flush or edit-mode exit doesn't run with indices
   * that have become invalid from updating the shape-key, see #71865.
   */
  bool update_shapekey_indices;
  /**
   * Instead of copying the basis shape-key into the position array,
   * copy the #BMVert.co directly to the #Mesh position (used for reading undo data).
   */
  bool active_shapekey_to_mvert;
  struct CustomData_MeshMasks cd_mask_extra;
};

/**
 * \param bmain: May be NULL in case \a calc_object_remap parameter option is not set.
 */
void BM_mesh_bm_to_me(struct Main *bmain, BMesh *bm, Mesh *mesh, const BMeshToMeshParams *params)
    ATTR_NONNULL(2, 3, 4);

/**
 * A version of #BM_mesh_bm_to_me intended for getting the mesh
 * to pass to the modifier stack for evaluation,
 * instead of mode switching (where we make sure all data is kept
 * and do expensive lookups to maintain shape keys).
 *
 * Key differences:
 *
 * - Don't support merging with existing mesh.
 * - Ignore shape-keys.
 * - Ignore vertex-parents.
 * - Ignore selection history.
 * - Uses #CD_MASK_DERIVEDMESH instead of #CD_MASK_MESH.
 *
 * \note Was `cddm_from_bmesh_ex` in 2.7x, removed `MFace` support.
 */
void BM_mesh_bm_to_me_for_eval(BMesh &bm, Mesh &mesh, const CustomData_MeshMasks *cd_mask_extra);

/**
 * A version of #BM_mesh_bm_to_me_for_eval but copying data layers and Mesh attributes is optional.
 * It also allows shape-keys but don't re-assigns shape-key indices.
 *
 * \param mask                Custom data masks to control which layers are copied.
 *                            If nullptr, no layer data is copied.
 * \param add_mesh_attributes If true, adds mesh attributes during the conversion.
 */
void BM_mesh_bm_to_me_compact(BMesh &bm,
                              Mesh &mesh,
                              const CustomData_MeshMasks *mask,
                              bool add_mesh_attributes);
