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

#ifndef __BKE_TESSMESH_H__
#define __BKE_TESSMESH_H__

#include "bmesh.h"

struct BMesh;
struct BMLoop;
struct BMFace;
struct Mesh;
struct DerivedMesh;

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
	int lastDataMask;

	/* index tables, to map indices to elements via
	 * EDBM_init_index_arrays and associated functions.  don't
	 * touch this or read it directly.*/
	struct BMVert **vert_index;
	struct BMEdge **edge_index;
	struct BMFace **face_index;

	/*selection mode*/
	short selectmode;
	short mat_nr;

	/*Mesh structure this editmesh came from, if it came from one*/
	struct Mesh *me;
	struct Object *ob;

	/*temp variables for x-mirror editing*/
	int mirror_cdlayer; /* -1 is invalid */
	int mirr_free_arrays;
} BMEditMesh;

void BMEdit_RecalcTessellation(BMEditMesh *tm);
BMEditMesh *BMEdit_Create(BMesh *bm, int do_tessellate);
BMEditMesh *BMEdit_Copy(BMEditMesh *tm);
BMEditMesh *BMEdit_FromObject(struct Object *ob);
void BMEdit_Free(BMEditMesh *em);
void BMEdit_UpdateLinkedCustomData(BMEditMesh *em);

#endif /* __BKE_TESSMESH_H__ */
