/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_EDITMESH_H__
#define __BKE_EDITMESH_H__

/** \file BKE_editmesh.h
 *  \ingroup bke
 */

#include "BKE_customdata.h"
#include "bmesh.h"

struct BMesh;
struct BMLoop;
struct BMFace;
struct Mesh;
struct Scene;
struct DerivedMesh;
struct MeshStatVis;

/* ok: the EDBM module is for editmode bmesh stuff.  in contrast, the 
 *     BMEdit module is for code shared with blenkernel that concerns
 *     the BMEditMesh structure.
 */

/* this structure replaces EditMesh.
 *
 * through this, you get access to both the edit bmesh,
 * it's tessellation, and various stuff that doesn't belong in the BMesh
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

	/*derivedmesh stuff*/
	struct DerivedMesh *derivedFinal, *derivedCage;
	CustomDataMask lastDataMask;
	unsigned char (*derivedVertColor)[4];
	int derivedVertColorLen;
	unsigned char (*derivedFaceColor)[4];
	int derivedFaceColorLen;

	/*selection mode*/
	short selectmode;
	short mat_nr;

	/* Object this editmesh came from (if it came from one) */
	struct Object *ob;

	/*temp variables for x-mirror editing*/
	int mirror_cdlayer; /* -1 is invalid */
} BMEditMesh;

/* editmesh.c */
void        BKE_editmesh_tessface_calc(BMEditMesh *em);
BMEditMesh *BKE_editmesh_create(BMesh *bm, const bool do_tessellate);
BMEditMesh *BKE_editmesh_copy(BMEditMesh *em);
BMEditMesh *BKE_editmesh_from_object(struct Object *ob);
void        BKE_editmesh_free(BMEditMesh *em);
void        BKE_editmesh_update_linked_customdata(BMEditMesh *em);

void        BKE_editmesh_color_free(BMEditMesh *em);
void        BKE_editmesh_color_ensure(BMEditMesh *em, const char htype);

/* editderivedmesh.c */
/* should really be defined in editmesh.c, but they use 'EditDerivedBMesh' */
void        BKE_editmesh_statvis_calc(BMEditMesh *em, struct DerivedMesh *dm,
                                      struct MeshStatVis *statvis);

float (*BKE_editmesh_vertexCos_get(struct BMEditMesh *em, struct Scene *scene, int *r_numVerts))[3];

#endif /* __BKE_EDITMESH_H__ */
