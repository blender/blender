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
 */

#pragma once

/** \file
 * \ingroup bke
 *
 * The \link edmesh EDBM module \endlink is for editmode bmesh stuff.
 * In contrast, this module is for code shared with blenkernel that's
 * only concerned with low level operations on the #BMEditMesh structure.
 */

#include "DNA_customdata_types.h"
#include "bmesh.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMLoop;
struct BMPartialUpdate;
struct BMesh;
struct BMeshCalcTessellation_Params;
struct BoundBox;
struct Depsgraph;
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
 * The entire modifier system works with this structure, and not #BMesh.
 * #Mesh.edit_bmesh stores a pointer to this structure. */
typedef struct BMEditMesh {
  struct BMesh *bm;

  /**
   * Face triangulation (tessellation) is stored as triplets of three loops,
   * which each define a triangle.
   *
   * \see #MLoopTri as the documentation gives useful hints that apply to this data too.
   */
  struct BMLoop *(*looptris)[3];
  int tottri;

  /** Selection mode (#SCE_SELECT_VERTEX, #SCE_SELECT_EDGE & #SCE_SELECT_FACE). */
  short selectmode;
  /** The active material (assigned to newly created faces). */
  short mat_nr;

  /** Temp variables for x-mirror editing (-1 when the layer does not exist). */
  int mirror_cdlayer;

  /**
   * Enable for evaluated copies, causes the edit-mesh to free the memory, not it's contents.
   */
  char is_shallow_copy;

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;

} BMEditMesh;

/* editmesh.c */
void BKE_editmesh_looptri_calc_ex(BMEditMesh *em,
                                  const struct BMeshCalcTessellation_Params *params);
void BKE_editmesh_looptri_calc(BMEditMesh *em);
void BKE_editmesh_looptri_calc_with_partial_ex(BMEditMesh *em,
                                               struct BMPartialUpdate *bmpinfo,
                                               const struct BMeshCalcTessellation_Params *params);
void BKE_editmesh_looptri_calc_with_partial(BMEditMesh *em, struct BMPartialUpdate *bmpinfo);
void BKE_editmesh_looptri_and_normals_calc_with_partial(BMEditMesh *em,
                                                        struct BMPartialUpdate *bmpinfo);

/**
 * Performing the face normal calculation at the same time as tessellation
 * gives a reasonable performance boost (approx ~20% faster).
 */
void BKE_editmesh_looptri_and_normals_calc(BMEditMesh *em);

/**
 * \note The caller is responsible for ensuring triangulation data,
 * typically by calling #BKE_editmesh_looptri_calc.
 */
BMEditMesh *BKE_editmesh_create(BMesh *bm);
BMEditMesh *BKE_editmesh_copy(BMEditMesh *em);
/**
 * \brief Return the #BMEditMesh for a given object
 *
 * \note this function assumes this is a mesh object,
 * don't add NULL data check here. caller must do that
 */
BMEditMesh *BKE_editmesh_from_object(struct Object *ob);
/**
 * \note Does not free the #BMEditMesh struct itself.
 */
void BKE_editmesh_free_data(BMEditMesh *em);

float (*BKE_editmesh_vert_coords_alloc(struct Depsgraph *depsgraph,
                                       struct BMEditMesh *em,
                                       struct Scene *scene,
                                       struct Object *ob,
                                       int *r_vert_len))[3];
float (*BKE_editmesh_vert_coords_alloc_orco(BMEditMesh *em, int *r_vert_len))[3];
const float (*BKE_editmesh_vert_coords_when_deformed(struct Depsgraph *depsgraph,
                                                     struct BMEditMesh *em,
                                                     struct Scene *scene,
                                                     struct Object *obedit,
                                                     int *r_vert_len,
                                                     bool *r_is_alloc))[3];

void BKE_editmesh_lnorspace_update(BMEditMesh *em, struct Mesh *me);
/**
 * If auto-smooth not already set, set it.
 */
void BKE_editmesh_ensure_autosmooth(BMEditMesh *em, struct Mesh *me);
struct BoundBox *BKE_editmesh_cage_boundbox_get(struct Object *object, BMEditMesh *em);

#ifdef __cplusplus
}
#endif
