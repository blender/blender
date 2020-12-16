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
 * The \link edmesh EDBM module\endlink is for editmode bmesh stuff.
 * In contrast, this module is for code shared with blenkernel that's
 * only concerned with low level operations on the #BMEditMesh structure.
 */

#include "BKE_customdata.h"
#include "bmesh.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMLoop;
struct BMesh;
struct BoundBox;
struct Depsgraph;
struct Mesh;
struct Object;
struct Scene;

/**
 * This structure is used for mesh edit-mode.
 *
 * through this, you get access to both the edit #BMesh,
 * its tessellation, and various stuff that doesn't belong in the BMesh
 * struct itself.
 *
 * the entire derivedmesh and modifier system works with this structure,
 * and not BMesh.  Mesh->edit_bmesh stores a pointer to this structure. */
typedef struct BMEditMesh {
  struct BMesh *bm;

  /*this is for undoing failed operations*/
  struct BMEditMesh *emcopy;
  int emcopyusers;

  /* we store tessellations as triplets of three loops,
   * which each define a triangle.*/
  struct BMLoop *(*looptris)[3];
  int tottri;

  struct Mesh *mesh_eval_final, *mesh_eval_cage;

  /** Cached cage bounding box for selection. */
  struct BoundBox *bb_cage;

  /*derivedmesh stuff*/
  CustomData_MeshMasks lastDataMask;

  /*selection mode*/
  short selectmode;
  short mat_nr;

  /*temp variables for x-mirror editing*/
  int mirror_cdlayer; /* -1 is invalid */

  /**
   * ID data is older than edit-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;

} BMEditMesh;

/* editmesh.c */
void BKE_editmesh_looptri_calc(BMEditMesh *em);
BMEditMesh *BKE_editmesh_create(BMesh *bm, const bool do_tessellate);
BMEditMesh *BKE_editmesh_copy(BMEditMesh *em);
BMEditMesh *BKE_editmesh_from_object(struct Object *ob);
void BKE_editmesh_free_derivedmesh(BMEditMesh *em);
void BKE_editmesh_free(BMEditMesh *em);

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
void BKE_editmesh_ensure_autosmooth(BMEditMesh *em, struct Mesh *me);
struct BoundBox *BKE_editmesh_cage_boundbox_get(BMEditMesh *em);

#ifdef __cplusplus
}
#endif
