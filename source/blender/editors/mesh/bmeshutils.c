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

#include "BMF_Api.h"

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

int EDBM_CallOpf(EditMesh *em, wmOperator *op, char *fmt, ...)
{
	BMesh *bm = editmesh_to_bmesh(em);
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_VInitOpf(bm, &bmop, fmt, list)) {
		BKE_report(op->reports, RPT_ERROR,
			   "Parse error in EDBM_CallOpf");
		va_end(list);
		return 0;
	}

	BMO_Exec_Op(bm, &bmop);
	BMO_Finish_Op(bm, &bmop);

	va_end(list);

	return EDBM_Finish(bm, em, op, 1);
}

int EDBM_CallOpfSilent(EditMesh *em, char *fmt, ...)
{
	BMesh *bm = editmesh_to_bmesh(em);
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_VInitOpf(bm, &bmop, fmt, list)) {
		va_end(list);
		return 0;
	}

	BMO_Exec_Op(bm, &bmop);
	BMO_Finish_Op(bm, &bmop);

	va_end(list);

	return EDBM_Finish(bm, em, NULL, 0);
}

/*returns 0 on error, 1 on success*/
int EDBM_Finish(BMesh *bm, EditMesh *em, wmOperator *op, int report) {
	EditMesh *em2;
	char *errmsg;

	if (BMO_GetError(bm, &errmsg, NULL)) {
		if (report) BKE_report(op->reports, RPT_ERROR, errmsg);
		BM_Free_Mesh(bm);
		return 0;
	}

	em2 = bmesh_to_editmesh(bm);
	set_editMesh(em, em2);
	MEM_freeN(em2);
	BM_Free_Mesh(bm);

	return 1;
}

void EDBM_MakeEditBMesh(Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	EditMesh *em;
	BMesh *bm;

	em = make_editMesh(scene, ob);
	bm = editmesh_to_bmesh(em);

	me->edit_btmesh = TM_Create(bm);
	me->edit_btmesh->selectmode = scene->selectmode;

	free_editMesh(em);
}

void EDBM_LoadEditBMesh(Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	EditMesh *em = bmesh_to_editmesh(me->edit_btmesh->bm);
	
	load_editMesh(scene, ob, em);
	free_editMesh(em);
}

void EDBM_FreeEditBMesh(BMEditMesh *tm)
{
	BM_Free_Mesh(tm->bm);
	TM_Free(tm);
}

void EDBM_init_index_arrays(BMEditMesh *tm, int forvert, int foredge, int forface)
{
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
	em->act_face = efa;
}

BMFace *EDBM_get_actFace(BMEditMesh *em, int sloppy)
{
	if (em->act_face) {
		return em->act_face;
	} else if (sloppy) {
		BMFace *efa= NULL;
		BMEditSelection *ese;
		
		ese = em->selected.last;
		for (; ese; ese=ese->prev){
			if(ese->type == EDITFACE) {
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
}


int EDBM_get_actSelection(BMEditMesh *em, BMEditSelection *ese)
{
	ese->data = NULL;
	return 0;
#if 0
	EditSelection *ese_last = em->selected.last;
	EditFace *efa = EM_get_actFace(em, 0);

	ese->next = ese->prev = NULL;
	
	if (ese_last) {
		if (ese_last->type == EDITFACE) { /* if there is an active face, use it over the last selected face */
			if (efa) {
				ese->data = (void *)efa;
			} else {
				ese->data = ese_last->data;
			}
			ese->type = EDITFACE;
		} else {
			ese->data = ese_last->data;
			ese->type = ese_last->type;
		}
	} else if (efa) { /* no */
		ese->data = (void *)efa;
		ese->type = EDITFACE;
	} else {
		ese->data = NULL;
		return 0;
	}
	return 1;
#endif
}

/* ********* Selection History ************ */
static int EDBM_check_selection(BMEditMesh *em, void *data)
{
#if 0
	EditSelection *ese;
	
	for(ese = em->selected.first; ese; ese = ese->next){
		if(ese->data == data) return 1;
	}
	
	return 0;
#endif
}

void EDBM_remove_selection(BMEditMesh *em, void *data)
{
#if 0
	EditSelection *ese;
	for(ese=em->selected.first; ese; ese = ese->next){
		if(ese->data == data){
			BLI_freelinkN(&(em->selected),ese);
			break;
		}
	}
#endif
}

void EDBM_store_selection(BMEditMesh *em, void *data)
{
#if 0
	EditSelection *ese;
	if(!EM_check_selection(em, data)){
		ese = (EditSelection*) MEM_callocN( sizeof(EditSelection), "Edit Selection");
		ese->type = type;
		ese->data = data;
		BLI_addtail(&(em->selected),ese);
	}
#endif
}

void EDBM_validate_selections(BMEditMesh *em)
{
#if 0
	EditSelection *ese, *nextese;

	ese = em->selected.first;

	while(ese){
		nextese = ese->next;
		if(ese->type == EDITVERT && !(((EditVert*)ese->data)->f & SELECT)) BLI_freelinkN(&(em->selected), ese);
		else if(ese->type == EDITEDGE && !(((EditEdge*)ese->data)->f & SELECT)) BLI_freelinkN(&(em->selected), ese);
		else if(ese->type == EDITFACE && !(((EditFace*)ese->data)->f & SELECT)) BLI_freelinkN(&(em->selected), ese);
		ese = nextese;
	}
#endif
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
			BM_ClearHFlag(ele, flag);
		}
	}
}

static void EDBM_strip_selections(BMEditMesh *em)
{
#if 0
	EditSelection *ese, *nextese;
	if(!(em->selectmode & SCE_SELECT_VERTEX)){
		ese = em->selected.first;
		while(ese){
			nextese = ese->next; 
			if(ese->type == EDITVERT) BLI_freelinkN(&(em->selected),ese);
			ese = nextese;
		}
	}
	if(!(em->selectmode & SCE_SELECT_EDGE)){
		ese=em->selected.first;
		while(ese){
			nextese = ese->next;
			if(ese->type == EDITEDGE) BLI_freelinkN(&(em->selected), ese);
			ese = nextese;
		}
	}
	if(!(em->selectmode & SCE_SELECT_FACE)){
		ese=em->selected.first;
		while(ese){
			nextese = ese->next;
			if(ese->type == EDITFACE) BLI_freelinkN(&(em->selected), ese);
			ese = nextese;
		}
	}
#endif
}

/* generic way to get data from an EditSelection type 
These functions were written to be used by the Modifier widget when in Rotate about active mode,
but can be used anywhere.
EM_editselection_center
EM_editselection_normal
EM_editselection_plane
*/
void EDBM_editselection_center(BMEditMesh *em, float *center, BMEditSelection *ese)
{
	if (ese->type==EDITVERT) {
		BMVert *eve= ese->data;
		VecCopyf(center, eve->co);
	} else if (ese->type==EDITEDGE) {
		BMEdge *eed= ese->data;
		VecAddf(center, eed->v1->co, eed->v2->co);
		VecMulf(center, 0.5);
	} else if (ese->type==EDITFACE) {
		BMFace *efa= ese->data;
		BM_Compute_Face_Center(em->bm, efa, center);
	}
}

void EDBM_editselection_normal(float *normal, BMEditSelection *ese)
{
	if (ese->type==EDITVERT) {
		BMVert *eve= ese->data;
		VecCopyf(normal, eve->no);
	} else if (ese->type==EDITEDGE) {
		BMEdge *eed= ese->data;
		float plane[3]; /* need a plane to correct the normal */
		float vec[3]; /* temp vec storage */
		
		VecAddf(normal, eed->v1->no, eed->v2->no);
		VecSubf(plane, eed->v2->co, eed->v1->co);
		
		/* the 2 vertex normals will be close but not at rightangles to the edge
		for rotate about edge we want them to be at right angles, so we need to
		do some extra colculation to correct the vert normals,
		we need the plane for this */
		Crossf(vec, normal, plane);
		Crossf(normal, plane, vec); 
		Normalize(normal);
		
	} else if (ese->type==EDITFACE) {
		BMFace *efa= ese->data;
		VecCopyf(normal, efa->no);
	}
}

/* Calculate a plane that is rightangles to the edge/vert/faces normal
also make the plane run allong an axis that is related to the geometry,
because this is used for the manipulators Y axis.*/
void EDBM_editselection_plane(BMEditMesh *em, float *plane, BMEditSelection *ese)
{
	if (ese->type==EDITVERT) {
		BMVert *eve= ese->data;
		float vec[3]={0,0,0};
		
		if (ese->prev) { /*use previously selected data to make a usefull vertex plane */
			EDBM_editselection_center(em, vec, ese->prev);
			VecSubf(plane, vec, eve->co);
		} else {
			/* make a fake  plane thats at rightangles to the normal
			we cant make a crossvec from a vec thats the same as the vec
			unlikely but possible, so make sure if the normal is (0,0,1)
			that vec isnt the same or in the same direction even.*/
			if (eve->no[0]<0.5)		vec[0]=1;
			else if (eve->no[1]<0.5)	vec[1]=1;
			else				vec[2]=1;
			Crossf(plane, eve->no, vec);
		}
	} else if (ese->type==EDITEDGE) {
		BMEdge *eed= ese->data;

		/*the plane is simple, it runs allong the edge
		however selecting different edges can swap the direction of the y axis.
		this makes it less likely for the y axis of the manipulator
		(running along the edge).. to flip less often.
		at least its more pradictable */
		if (eed->v2->co[1] > eed->v1->co[1]) /*check which to do first */
			VecSubf(plane, eed->v2->co, eed->v1->co);
		else
			VecSubf(plane, eed->v1->co, eed->v2->co);
		
	} else if (ese->type==EDITFACE) {
		BMFace *efa= ese->data;
		float vec[3] = {0.0f, 0.0f, 0.0f};
		
		/*for now, use face normal*/

		/* make a fake  plane thats at rightangles to the normal
		we cant make a crossvec from a vec thats the same as the vec
		unlikely but possible, so make sure if the normal is (0,0,1)
		that vec isnt the same or in the same direction even.*/
		if (efa->no[0]<0.5)		vec[0]=1.0f;
		else if (efa->no[1]<0.5)	vec[1]=1.0f;
		else				vec[2]=1.0f;
		Crossf(plane, efa->no, vec);
#if 0

		if (efa->v4) { /*if its a quad- set the plane along the 2 longest edges.*/
			float vecA[3], vecB[3];
			VecSubf(vecA, efa->v4->co, efa->v3->co);
			VecSubf(vecB, efa->v1->co, efa->v2->co);
			VecAddf(plane, vecA, vecB);
			
			VecSubf(vecA, efa->v1->co, efa->v4->co);
			VecSubf(vecB, efa->v2->co, efa->v3->co);
			VecAddf(vec, vecA, vecB);						
			/*use the biggest edge length*/
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
		} else {
			/*start with v1-2 */
			VecSubf(plane, efa->v1->co, efa->v2->co);
			
			/*test the edge between v2-3, use if longer */
			VecSubf(vec, efa->v2->co, efa->v3->co);
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
			
			/*test the edge between v1-3, use if longer */
			VecSubf(vec, efa->v3->co, efa->v1->co);
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
		}
#endif
	}
	Normalize(plane);
}
