 /* $Id: bmeshutils.c
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_key_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_types.h"
#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_screen.h"
#include "BIF_transform.h"

#include "UI_interface.h"

#include "mesh_intern.h"
#include "bmesh.h"

void EDBM_RecalcNormals(BMEditMesh *em)
{
	BM_Compute_Normals(em->bm);
}

void EDBM_stats_update(BMEditMesh *em)
{
	BMIter iter;
	BMHeader *ele;
	int types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};
	int *tots[3];
	int i;

	tots[0] = &em->bm->totvertsel;
	tots[1] = &em->bm->totedgesel;
	tots[2] = &em->bm->totfacesel;
	
	em->bm->totvertsel = em->bm->totedgesel = em->bm->totfacesel = 0;

	for (i=0; i<3; i++) {
		ele = BMIter_New(&iter, em->bm, types[i], NULL);
		for ( ; ele; ele=BMIter_Step(&iter)) {
			if (BM_TestHFlag(ele, BM_SELECT)) {
				*tots[i]++;
			}
		}
	}
}

/*this function is defunct, dead*/
void EDBM_Tesselate(EditMesh *em)
{
	EditMesh *em2;
	EditFace *efa;
	BMesh *bm;
	int found=0;
	
	for (efa=em->faces.first; efa; efa=efa->next) {
		if ((efa->e1->h & EM_FGON) || (efa->e2->h & EM_FGON) ||
		    (efa->e3->h & EM_FGON) || (efa->e4&&(efa->e4->h&EM_FGON)))
		{
			found = 1;
			break;
		}
	}

	if (found) {
		bm = editmesh_to_bmesh(em);
		em2 = bmesh_to_editmesh(bm);
		set_editMesh(em, em2);

		MEM_freeN(em2);
		BM_Free_Mesh(bm);
	}
}

int EDBM_InitOpf(BMEditMesh *em, BMOperator *bmop, wmOperator *op, char *fmt, ...)
{
	BMesh *bm = em->bm;
	va_list list;

	va_start(list, fmt);

	if (!BMO_VInitOpf(bm, bmop, fmt, list)) {
		BKE_report(op->reports, RPT_ERROR,
			   "Parse error in EDBM_CallOpf");
		va_end(list);
		return 0;
	}

	em->emcopy = BMEdit_Copy(em);

	va_end(list);
}


/*returns 0 on error, 1 on success.  executes and finishes a bmesh operator*/
int EDBM_FinishOp(BMEditMesh *em, BMOperator *bmop, wmOperator *op, int report) {
	char *errmsg;
	
	BMO_Finish_Op(em->bm, bmop);

	if (BMO_GetError(em->bm, &errmsg, NULL)) {
		BMEditMesh *emcopy = em->emcopy;

		if (report) BKE_report(op->reports, RPT_ERROR, errmsg);

		BMEdit_Free(em);
		*em = *emcopy;

		MEM_freeN(emcopy);
		return 0;
	} else {
		BMEdit_Free(em->emcopy);
		MEM_freeN(em->emcopy);
		em->emcopy = NULL;
	}

	return 1;
}

int EDBM_CallOpf(BMEditMesh *em, wmOperator *op, char *fmt, ...)
{
	BMesh *bm = em->bm;
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_VInitOpf(bm, &bmop, fmt, list)) {
		BKE_report(op->reports, RPT_ERROR,
			   "Parse error in EDBM_CallOpf");
		va_end(list);
		return 0;
	}

	em->emcopy = BMEdit_Copy(em);

	BMO_Exec_Op(bm, &bmop);

	va_end(list);
	return EDBM_FinishOp(em, &bmop, op, 1);
}

int EDBM_CallOpfSilent(BMEditMesh *em, char *fmt, ...)
{
	BMesh *bm = em->bm;
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_VInitOpf(bm, &bmop, fmt, list)) {
		va_end(list);
		return 0;
	}

	em->emcopy = BMEdit_Copy(em);

	BMO_Exec_Op(bm, &bmop);

	va_end(list);
	return EDBM_FinishOp(em, &bmop, NULL, 0);
}

void EDBM_MakeEditBMesh(Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	EditMesh *em;
	BMesh *bm;

	if (!me->mpoly && me->totface) {
		em = make_editMesh(scene, ob);
		bm = editmesh_to_bmesh(em);
	
		free_editMesh(em);
	} else {
		bm = BKE_mesh_to_bmesh(me);
	}

	me->edit_btmesh = BMEdit_Create(bm);
	me->edit_btmesh->selectmode = scene->selectmode;
}

void EDBM_LoadEditBMesh(Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	BMesh *bm = me->edit_btmesh->bm;

	BMO_CallOpf(bm, "object_load_bmesh scene=%p object=%p", scene, ob);

#if 0
	EditMesh *em = bmesh_to_editmesh(me->edit_btmesh->bm);
	
	load_editMesh(scene, ob, em);
	free_editMesh(em);
	MEM_freeN(em);
#endif
}

void EDBM_FreeEditBMesh(BMEditMesh *tm)
{
	BMEdit_Free(tm);
}

void EDBM_init_index_arrays(BMEditMesh *tm, int forvert, int foredge, int forface)
{
	EDBM_free_index_arrays(tm);

	if (forvert) {
		BMIter iter;
		BMVert *ele;
		int i=0;
		
		tm->vert_index = MEM_mallocN(sizeof(void**)*tm->bm->totvert, "tm->vert_index");

		ele = BMIter_New(&iter, tm->bm, BM_VERTS_OF_MESH, NULL);
		for ( ; ele; ele=BMIter_Step(&iter)) {
			tm->vert_index[i++] = ele;
		}
	}

	if (foredge) {
		BMIter iter;
		BMEdge *ele;
		int i=0;
		
		tm->edge_index = MEM_mallocN(sizeof(void**)*tm->bm->totedge, "tm->edge_index");

		ele = BMIter_New(&iter, tm->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; ele; ele=BMIter_Step(&iter)) {
			tm->edge_index[i++] = ele;
		}
	}

	if (forface) {
		BMIter iter;
		BMFace *ele;
		int i=0;
		
		tm->face_index = MEM_mallocN(sizeof(void**)*tm->bm->totface, "tm->face_index");

		ele = BMIter_New(&iter, tm->bm, BM_FACES_OF_MESH, NULL);
		for ( ; ele; ele=BMIter_Step(&iter)) {
			tm->face_index[i++] = ele;
		}
	}
}

void EDBM_free_index_arrays(BMEditMesh *tm)
{
	if (tm->vert_index) {
		MEM_freeN(tm->vert_index);
		tm->vert_index = NULL;
	}

	if (tm->edge_index) {
		MEM_freeN(tm->edge_index);
		tm->edge_index = NULL;
	}

	if (tm->face_index) {
		MEM_freeN(tm->face_index);
		tm->face_index = NULL;
	}
}

BMVert *EDBM_get_vert_for_index(BMEditMesh *tm, int index)
{
	return tm->vert_index?tm->vert_index[index]:NULL;
}

BMEdge *EDBM_get_edge_for_index(BMEditMesh *tm, int index)
{
	return tm->edge_index?tm->edge_index[index]:NULL;
}

BMFace *EDBM_get_face_for_index(BMEditMesh *tm, int index)
{
	return tm->face_index?tm->face_index[index]:NULL;
}

/* this replaces the active flag used in uv/face mode */
void EDBM_set_actFace(BMEditMesh *em, BMFace *efa)
{
	em->bm->act_face = efa;
}

BMFace *EDBM_get_actFace(BMEditMesh *em, int sloppy)
{
	if (em->bm->act_face) {
		return em->bm->act_face;
	} else if (sloppy) {
		BMFace *efa= NULL;
		BMEditSelection *ese;
		
		ese = em->selected.last;
		for (; ese; ese=ese->prev){
			if(ese->type == BM_FACE) {
				efa = (BMFace *)ese->data;
				
				if (BM_TestHFlag(efa, BM_HIDDEN)) efa= NULL;
				else break;
			}
		}
		if (efa==NULL) {
			BMIter iter;
			efa = BMIter_New(&iter, em->bm, BM_FACES_OF_MESH, NULL);
			for ( ; efa; efa=BMIter_Step(&iter)) {
				if (BM_TestHFlag(efa, BM_SELECT))
					break;
			}
		}
		return efa; /* can still be null */
	}
	return NULL;

}

void EDBM_selectmode_flush(BMEditMesh *em)
{
	em->bm->selectmode = em->selectmode;
	BM_SelectMode_Flush(em->bm);
}


int EDBM_get_actSelection(BMEditMesh *em, BMEditSelection *ese)
{
	BMEditSelection *ese_last = em->selected.last;
	BMFace *efa = EDBM_get_actFace(em, 0);

	ese->next = ese->prev = NULL;
	
	if (ese_last) {
		if (ese_last->type == BM_FACE) { /* if there is an active face, use it over the last selected face */
			if (efa) {
				ese->data = (void *)efa;
			} else {
				ese->data = ese_last->data;
			}
			ese->type = BM_FACE;
		} else {
			ese->data = ese_last->data;
			ese->type = ese_last->type;
		}
	} else if (efa) { /* no */
		ese->data = (void *)efa;
		ese->type = BM_FACE;
	} else {
		ese->data = NULL;
		return 0;
	}
	return 1;
}

void EDBM_clear_flag_all(BMEditMesh *em, int flag)
{
	BMIter iter;
	BMHeader *ele;
	int i, type;

	for (i=0; i<3; i++) {
		switch (i) {
			case 0:
				type = BM_VERTS_OF_MESH;
				break;
			case 1:
				type = BM_EDGES_OF_MESH;
				break;
			case 2:
				type = BM_FACES_OF_MESH;
				break;
		}
		ele = BMIter_New(&iter, em->bm, type, NULL);
		for ( ; ele; ele=BMIter_Step(&iter)) {
			if (flag & BM_SELECT) BM_Select(em->bm, ele, 0);
			BM_ClearHFlag(ele, flag);
		}
	}
}


void EDBM_set_flag_all(BMEditMesh *em, int flag)
{
	BMIter iter;
	BMHeader *ele;
	int i, type;

	for (i=0; i<3; i++) {
		switch (i) {
			case 0:
				type = BM_VERTS_OF_MESH;
				break;
			case 1:
				type = BM_EDGES_OF_MESH;
				break;
			case 2:
				type = BM_FACES_OF_MESH;
				break;
		}
		ele = BMIter_New(&iter, em->bm, type, NULL);
		for ( ; ele; ele=BMIter_Step(&iter)) {
			if (flag & BM_SELECT) BM_Select(em->bm, ele, 1);
			BM_SetHFlag(ele, flag);
		}
	}
}

/**************-------------- Undo ------------*****************/

/* for callbacks */

static void *getEditMesh(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_MESH) {
		Mesh *me= obedit->data;
		return me->edit_btmesh;
	}
	return NULL;
}

/*undo simply makes copies of a bmesh*/
static void *editbtMesh_to_undoMesh(void *emv)
{
	/*we recalc the tesselation here, to avoid seeding calls to
	  BMEdit_RecalcTesselation throughout the code.*/
	BMEdit_RecalcTesselation(emv);

	return BMEdit_Copy(emv);
}

static void undoMesh_to_editbtMesh(void *umv, void *emv)
{
	BMEditMesh *bm1 = umv, *bm2 = emv;

	BMEdit_Free(bm2);

	*bm2 = *BMEdit_Copy(bm1);
}


static void free_undo(void *umv)
{
	BMEditMesh *em = umv;

	BMEdit_Free(em);
}

/* and this is all the undo system needs to know */
void undo_push_mesh(bContext *C, char *name)
{
	undo_editmode_push(C, name, getEditMesh, free_undo, undoMesh_to_editbtMesh, editbtMesh_to_undoMesh, NULL);
}
