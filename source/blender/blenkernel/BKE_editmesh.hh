/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * The \link edmesh EDBM module \endlink is for editmode bmesh stuff.
 * In contrast, this module is for code shared with blenkernel that's
 * only concerned with low level operations on the #EditMesh structure.
 */

#include <array>

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "bmesh.hh"

namespace blender {

struct BMLoop;
struct BMPartialUpdate;
struct BMesh;
struct BMeshCalcTessellation_Params;
struct Depsgraph;
struct Main;
struct Mesh;
struct Object;
struct Scene;

/**
 * This structure is used for mesh edit-mode.
 *
 * Through this, you get access to both the edit #BMesh, its tessellation,
 * and various data that doesn't belong in the #BMesh struct itself
 * (mostly related to mesh evaluation).
 *
 * #Mesh.runtime.edit_mesh stores a pointer to this structure.
 */
struct EditMesh {
  /**
   * Always owned by an original mesh in edit mode. Access via #BKE_editmesh_bmesh_get_for_write or
   * BKE_editmesh_bmesh_get.
   */
  BMesh *bm;

  /**
   * Face triangulation (tessellation) is stored as triplets of three loops,
   * which each define a triangle.
   *
   * \see #Mesh::corner_tris() as the documentation gives useful hints that apply to this data too.
   */
  Array<std::array<BMLoop *, 3>> looptris;

  /** Selection mode (#SCE_SELECT_VERTEX, #SCE_SELECT_EDGE & #SCE_SELECT_FACE). */
  short selectmode;
  /** The active material (zero-based, assigned to newly created faces). */
  short mat_nr;

  /** Temp variables for x-mirror editing (-1 when the layer does not exist). */
  int mirror_cdlayer;

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;
};

/* editmesh.cc */

void BKE_editmesh_looptris_calc_ex(EditMesh *em,
                                   BMesh *bm,
                                   const BMeshCalcTessellation_Params *params);
void BKE_editmesh_looptris_calc(EditMesh *em, BMesh *bm);
void BKE_editmesh_looptris_calc_with_partial_ex(EditMesh *em,
                                                BMesh *bm,
                                                BMPartialUpdate *bmpinfo,
                                                const BMeshCalcTessellation_Params *params);
void BKE_editmesh_looptris_calc_with_partial(EditMesh *em, BMesh *bm, BMPartialUpdate *bmpinfo);
void BKE_editmesh_looptris_and_normals_calc_with_partial(EditMesh *em,
                                                         BMesh *bm,
                                                         BMPartialUpdate *bmpinfo);

/**
 * Performing the face normal calculation at the same time as tessellation
 * gives a reasonable performance boost (approx ~20% faster).
 */
void BKE_editmesh_looptris_and_normals_calc(EditMesh *em, BMesh *bm);

/**
 * \note The caller is responsible for ensuring triangulation data,
 * typically by calling #BKE_editmesh_looptris_calc.
 */
EditMesh *BKE_editmesh_create(BMesh *bm);
EditMesh *BKE_editmesh_copy(EditMesh *em);
/**
 * \brief Return the #EditMesh for a given object
 *
 * \note this function assumes this is a mesh object,
 * don't add NULL data check here. caller must do that.
 *
 * \note Even in edit-mode, evaluated objects may return null since the
 * evaluated mesh may have had its edit-mesh cleared.
 *
 * In this case the caller must either:
 * - Use the original mesh to guarantee #EditMesh is returned.
 * - Null check the result if we want to respect the evaluated result, see: #154739.
 */
EditMesh *BKE_editmesh_from_object(Object *ob);

/**
 * Get the edit-mode #BMesh if it holds the mesh's data, otherwise null. In some cases during and
 * after evaluation, the mesh may carry a reference to the original mesh's #EditMesh, but that is
 * only the source of truth for mesh data if the mesh is a #ME_WRAPPER_TYPE_BMESH wrapper.
 *
 * The #Object overload is only for original objects. After evaluation, the data of an evaluated
 * object is its final mesh, so to ask whether the original is in edit mode, pass
 * #DEG_get_original or #BKE_object_get_pre_modified_mesh instead.
 */
const BMesh *BKE_editmesh_bmesh_get(const Object *ob);
const BMesh *BKE_editmesh_bmesh_get(const Mesh *mesh);

/**
 * Same as #BKE_editmesh_bmesh_get, but for callers that change the #BMesh, which is only allowed
 * on original meshes.
 */
BMesh *BKE_editmesh_bmesh_get_for_write(Mesh *mesh);
BMesh *BKE_editmesh_bmesh_get_for_write(Object *ob);

/**
 * Get the edit-mode #BMesh for code that needs it to exist and a #Mesh alternative is not
 * implemented. Only valid for original meshes in edit mode.
 *
 * \note For now this is the same as #BKE_editmesh_bmesh_get_for_write. Once the #BMesh is created
 * lazily from the #Mesh data, this will build it when necessary, see #163678.
 */
BMesh *BKE_editmesh_bmesh_ensure(Mesh *mesh);
BMesh *BKE_editmesh_bmesh_ensure(Object *object);

namespace bke {

/** Find objects that refer to vertices of the mesh with hook modifiers or vertex parenting. */
Vector<Object *> mesh_find_objects_with_vert_references(Main &bmain, const Mesh &mesh);

/**
 * Update the vertex indices of hook modifiers and vertex parents to match topology changes.
 *
 * \param objects: Objects found with #mesh_find_objects_with_vert_references.
 * \param old_to_new_verts: The new index of every vertex before the change, or -1 when it was
 * removed. Indices past its end are treated as removed too.
 */
void mesh_remap_object_vert_indices(Span<Object *> objects,
                                    const Mesh &mesh,
                                    Span<int> old_to_new_verts);

}  // namespace bke

/**
 * Return whether the evaluated mesh is a "descendant" of the original mesh: whether it is a
 * version of the original mesh propagated during evaluation. This will be false if the mesh was
 * taken from an different object during evaluation, with the object info node for example.
 */
bool BKE_editmesh_eval_orig_map_available(const Mesh &mesh_eval, const Mesh *mesh_orig);

/**
 * \note Does not free the #EditMesh itself.
 */
void BKE_editmesh_free_data(EditMesh *em);

Array<float3> BKE_editmesh_vert_coords_alloc(Depsgraph *depsgraph,
                                             const BMesh *bm,
                                             Scene *scene,
                                             Object *ob);
Array<float3> BKE_editmesh_vert_coords_alloc_orco(const BMesh *bm);
Span<float3> BKE_editmesh_vert_coords_when_deformed(Depsgraph *depsgraph,
                                                    Scene *scene,
                                                    Object *obedit,
                                                    Array<float3> &r_alloc);

void BKE_editmesh_lnorspace_update(BMesh *bm);

}  // namespace blender
