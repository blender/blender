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
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"
#include "BLI_array.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "bmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_screen.h"

#include "UI_interface.h"

#include "editbmesh_bvh.h"
#include "mesh_intern.h"

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
	
	if (!em->emcopy)
		em->emcopy = BMEdit_Copy(em);
	em->emcopyusers++;

	va_end(list);

	return 1;
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
		em->emcopyusers = 0;
		em->emcopy = NULL;
		return 0;
	} else {
		em->emcopyusers--;
		if (em->emcopyusers < 0) {
			printf("warning: em->emcopyusers was less then zero.\n");
		}

		if (em->emcopyusers <= 0) {
			BMEdit_Free(em->emcopy);
			MEM_freeN(em->emcopy);
			em->emcopy = NULL;
		}
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

	if (!em->emcopy)
		em->emcopy = BMEdit_Copy(em);
	em->emcopyusers++;

	BMO_Exec_Op(bm, &bmop);

	va_end(list);
	return EDBM_FinishOp(em, &bmop, op, 1);
}

int EDBM_CallAndSelectOpf(BMEditMesh *em, wmOperator *op, char *selectslot, char *fmt, ...)
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

	if (!em->emcopy)
		em->emcopy = BMEdit_Copy(em);
	em->emcopyusers++;

	BMO_Exec_Op(bm, &bmop);
	BMO_HeaderFlag_Buffer(em->bm, &bmop, selectslot, BM_SELECT, BM_ALL);

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

	if (!em->emcopy)
		em->emcopy = BMEdit_Copy(em);
	em->emcopyusers++;

	BMO_Exec_Op(bm, &bmop);

	va_end(list);
	return EDBM_FinishOp(em, &bmop, NULL, 0);
}

void EDBM_selectmode_to_scene(Scene *scene, Object *obedit)
{
	BMEditMesh *em = ((Mesh*)obedit->data)->edit_btmesh;

	if (!em)
		return;

	scene->toolsettings->selectmode = em->selectmode;
}

void EDBM_MakeEditBMesh(ToolSettings *ts, Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	EditMesh *em;
	BMesh *bm;

	if (!me->mpoly && me->totface) {
		printf("yeek!! bmesh conversion issue! may lose lots of geometry!\n");
		
		/*BMESH_TODO need to write smarter code here*/
		bm = BKE_mesh_to_bmesh(me, ob);
	} else {
		bm = BKE_mesh_to_bmesh(me, ob);
	}

	me->edit_btmesh = BMEdit_Create(bm);
	me->edit_btmesh->selectmode = ts->selectmode;
	me->edit_btmesh->me = me;
	me->edit_btmesh->ob = ob;
}

void EDBM_LoadEditBMesh(Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	BMesh *bm = me->edit_btmesh->bm;

	BMO_CallOpf(bm, "object_load_bmesh scene=%p object=%p", scene, ob);
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
		
		ese = em->bm->selected.last;
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

void EDBM_select_flush(BMEditMesh *em, int selectmode)
{
	em->bm->selectmode = selectmode;
	BM_SelectMode_Flush(em->bm);
	em->bm->selectmode = em->selectmode;
}

/*BMESH_TODO*/
void EDBM_deselect_flush(BMEditMesh *em)
{
}


void EDBM_selectmode_flush(BMEditMesh *em)
{
	em->bm->selectmode = em->selectmode;
	BM_SelectMode_Flush(em->bm);
}

/*EDBM_select_[more/less] are api functions, I think the uv editor
  uses them? though the select more/less ops themselves do not.*/
void EDBM_select_more(BMEditMesh *em)
{
	BMOperator bmop;
	int usefaces = em->selectmode > SCE_SELECT_EDGE;

	BMO_InitOpf(em->bm, &bmop, 
	            "regionextend geom=%hvef constrict=%d usefaces=%d",
	            BM_SELECT, 0, usefaces);
	BMO_Exec_Op(em->bm, &bmop);
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "geomout", BM_SELECT, BM_ALL);
	BMO_Finish_Op(em->bm, &bmop);

	EDBM_selectmode_flush(em);
}

void EDBM_select_less(BMEditMesh *em)
{
	BMOperator bmop;
	int usefaces = em->selectmode > SCE_SELECT_EDGE;

	BMO_InitOpf(em->bm, &bmop, 
	            "regionextend geom=%hvef constrict=%d usefaces=%d",
	            BM_SELECT, 0, usefaces);
	BMO_Exec_Op(em->bm, &bmop);
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "geomout", BM_SELECT, BM_ALL);
	BMO_Finish_Op(em->bm, &bmop);

	EDBM_selectmode_flush(em);
}

int EDBM_get_actSelection(BMEditMesh *em, BMEditSelection *ese)
{
	BMEditSelection *ese_last = em->bm->selected.last;
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

	if (flag & BM_SELECT)
		BM_clear_selection_history(em->bm);

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
		
		BM_ITER(ele, &iter, em->bm, type, NULL) {
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

typedef struct undomesh {
	Mesh me;
	int selectmode;
} undomesh;

/*undo simply makes copies of a bmesh*/
static void *editbtMesh_to_undoMesh(void *emv, void *obdata)
{
	BMEditMesh *em = emv;
	Mesh *obme = obdata;
	undomesh *me = MEM_callocN(sizeof(undomesh), "undo Mesh");
	
	/*make sure shape keys work*/
	me->me.key = obme->key ? copy_key_nolib(obme->key) : NULL;

	/*we recalc the tesselation here, to avoid seeding calls to
	  BMEdit_RecalcTesselation throughout the code.*/
	BMEdit_RecalcTesselation(em);

	BMO_CallOpf(em->bm, "bmesh_to_mesh mesh=%p notesselation=%i", me, 1);
	me->selectmode = em->selectmode;

	return me;
}

static void undoMesh_to_editbtMesh(void *umv, void *emv, void *obdata)
{
	BMEditMesh *em = emv, *em2;
	Object ob = {0,};
	undomesh *me = umv;
	BMesh *bm;
	int allocsize[4] = {512, 512, 2048, 512};

	ob.data = me;
	ob.type = OB_MESH;
	ob.shapenr = em->bm->shapenr;

	BMEdit_Free(em);

	bm = BM_Make_Mesh(allocsize);
	BMO_CallOpf(bm, "mesh_to_bmesh mesh=%p object=%p", me, &ob);

	em2 = BMEdit_Create(bm);
	*em = *em2;
	
	em->selectmode = me->selectmode;

	MEM_freeN(em2);
}


static void free_undo(void *umv)
{
	free_mesh(umv, 0);
	MEM_freeN(umv);
}

/* and this is all the undo system needs to know */
void undo_push_mesh(bContext *C, char *name)
{
	undo_editmode_push(C, name, getEditMesh, free_undo, undoMesh_to_editbtMesh, editbtMesh_to_undoMesh, NULL);
}

/*write comment here*/
UvVertMap *EDBM_make_uv_vert_map(BMEditMesh *em, int selected, int do_face_idx_array, float *limit)
{
	BMVert *ev;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	/* vars from original func */
	UvVertMap *vmap;
	UvMapVert *buf;
	MTexPoly *tf;
	MLoopUV *luv;
	unsigned int a;
	int totverts, i, totuv;
	
	if (do_face_idx_array)
		EDBM_init_index_arrays(em, 0, 0, 1);
	
	/* we need the vert */
	totverts=0;
	BM_ITER(ev, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(ev, totverts);
		totverts++;
	}
	
	totuv = 0;

	/* generate UvMapVert array */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if(!selected || ((!BM_TestHFlag(efa, BM_HIDDEN)) && BM_TestHFlag(efa, BM_SELECT)))
			totuv += efa->len;
	}

	if(totuv==0) {
		if (do_face_idx_array)
			EDBM_free_index_arrays(em);
		return NULL;
	}
	vmap= (UvVertMap*)MEM_callocN(sizeof(*vmap), "UvVertMap");
	if (!vmap) {
		if (do_face_idx_array)
			EDBM_free_index_arrays(em);
		return NULL;
	}

	vmap->vert= (UvMapVert**)MEM_callocN(sizeof(*vmap->vert)*totverts, "UvMapVert*");
	buf= vmap->buf= (UvMapVert*)MEM_callocN(sizeof(*vmap->buf)*totuv, "UvMapVert");

	if (!vmap->vert || !vmap->buf) {
		free_uv_vert_map(vmap);
		if (do_face_idx_array)
			EDBM_free_index_arrays(em);
		return NULL;
	}
	
	a = 0;
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if(!selected || ((!BM_TestHFlag(efa, BM_HIDDEN)) && BM_TestHFlag(efa, BM_SELECT))) {
			i = 0;
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				buf->tfindex= i;
				buf->f= a;
				buf->separate = 0;
				
				buf->next= vmap->vert[BMINDEX_GET(l->v)];
				vmap->vert[BMINDEX_GET(l->v)]= buf;
				
				buf++;
				i++;
			}
		}

		a++;
	}
	
	/* sort individual uvs for each vert */
	a = 0;
	BM_ITER(ev, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		UvMapVert *newvlist= NULL, *vlist=vmap->vert[a];
		UvMapVert *iterv, *v, *lastv, *next;
		float *uv, *uv2, uvdiff[2];

		while(vlist) {
			v= vlist;
			vlist= vlist->next;
			v->next= newvlist;
			newvlist= v;

			efa = EDBM_get_face_for_index(em, v->f);
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			
			l = BMIter_AtIndex(em->bm, BM_LOOPS_OF_FACE, efa, v->tfindex);
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			uv = luv->uv; 
			
			lastv= NULL;
			iterv= vlist;

			while(iterv) {
				next= iterv->next;
				efa = EDBM_get_face_for_index(em, iterv->f);
				tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
				
				l = BMIter_AtIndex(em->bm, BM_LOOPS_OF_FACE, efa, iterv->tfindex);
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				uv2 = luv->uv; 
				
				sub_v2_v2v2(uvdiff, uv2, uv);

				if(fabs(uv[0]-uv2[0]) < limit[0] && fabs(uv[1]-uv2[1]) < limit[1]) {
					if(lastv) lastv->next= next;
					else vlist= next;
					iterv->next= newvlist;
					newvlist= iterv;
				}
				else
					lastv=iterv;

				iterv= next;
			}

			newvlist->separate = 1;
		}

		vmap->vert[a]= newvlist;
		a++;
	}
	
	if (do_face_idx_array)
		EDBM_free_index_arrays(em);
	
	return vmap;
}


UvMapVert *EDBM_get_uv_map_vert(UvVertMap *vmap, unsigned int v)
{
	return vmap->vert[v];
}

void EDBM_free_uv_vert_map(UvVertMap *vmap)
{
	if (vmap) {
		if (vmap->vert) MEM_freeN(vmap->vert);
		if (vmap->buf) MEM_freeN(vmap->buf);
		MEM_freeN(vmap);
	}
}


/* last_sel, use em->act_face otherwise get the last selected face in the editselections
 * at the moment, last_sel is mainly useful for gaking sure the space image dosnt flicker */
MTexPoly *EDBM_get_active_mtexpoly(BMEditMesh *em, BMFace **act_efa, int sloppy)
{
	BMFace *efa = NULL;
	
	if(!EDBM_texFaceCheck(em))
		return NULL;
	
	efa = EDBM_get_actFace(em, sloppy);
	
	if (efa) {
		if (act_efa) *act_efa = efa; 
		return CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
	}

	if (act_efa) *act_efa= NULL;
	return NULL;
}

/* can we edit UV's for this mesh?*/
int EDBM_texFaceCheck(BMEditMesh *em)
{
	/* some of these checks could be a touch overkill */
	return em && em->bm->totface && CustomData_has_layer(&em->bm->pdata, CD_MTEXPOLY) &&
		   CustomData_has_layer(&em->bm->ldata, CD_MLOOPCOL);
}

int EDBM_vertColorCheck(BMEditMesh *em)
{
	/* some of these checks could be a touch overkill */
	return em && em->bm->totface && CustomData_has_layer(&em->bm->ldata, CD_MLOOPCOL);
}


void EDBM_CacheMirrorVerts(BMEditMesh *em)
{
	BMBVHTree *tree = BMBVH_NewBVH(em);
	BMIter iter;
	BMVert *v;
	float invmat[4][4];
	int li, i;

	if (!em->vert_index) {
		EDBM_init_index_arrays(em, 1, 0, 0);
		em->mirr_free_arrays = 1;
	}

	if (!CustomData_get_layer_named(&em->bm->vdata, CD_PROP_INT, "__mirror_index")) {
		BM_add_data_layer_named(em->bm, &em->bm->vdata, CD_PROP_INT, "__mirror_index");
	}

	li = CustomData_get_named_layer_index(&em->bm->vdata, CD_PROP_INT, "__mirror_index");
	em->bm->vdata.layers[li].flag |= CD_FLAG_TEMPORARY;

	/*multiply verts by object matrix, temporarily*/

	i = 0;
	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(v, i);
		i++;

		if (em->ob) 
			mul_m4_v3(em->ob->obmat, v->co);
	}

	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		BMVert *mirr;
		int *idx = CustomData_bmesh_get_layer_n(&em->bm->vdata, v->head.data, li);
		float co[3] = {-v->co[0], v->co[1], v->co[2]};

		//temporary for testing, check for selection
		if (!BM_TestHFlag(v, BM_SELECT))
			continue;
		
		mirr = BMBVH_FindClosestVertTopo(tree, co, BM_SEARCH_MAXDIST, v);
		if (mirr && mirr != v) {
			*idx = BMINDEX_GET(mirr);
			idx = CustomData_bmesh_get_layer_n(&em->bm->vdata,mirr->head.data, li);
			*idx = BMINDEX_GET(v);
		} else *idx = -1;
	}

	/*unmultiply by object matrix*/
	if (em->ob) {
		i = 0;
		invert_m4_m4(invmat, em->ob->obmat);
		BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			BMINDEX_SET(v, i);
			i++;

			mul_m4_v3(invmat, v->co);
		}

		BMBVH_FreeBVH(tree);
	}
}

BMVert *EDBM_GetMirrorVert(BMEditMesh *em, BMVert *v)
{
	int *mirr = CustomData_bmesh_get_layer_n(&em->bm->vdata, v->head.data, em->mirror_cdlayer);
	
	if (mirr && *mirr >=0 && *mirr < em->bm->totvert) {
		if (!em->vert_index) {
			printf("err: should only be called between "
				"EDBM_CacheMirrorVerts and EDBM_EndMirrorCache");
			return NULL;
		}

		return em->vert_index[*mirr];
	}

	return NULL;
}

void EDBM_EndMirrorCache(BMEditMesh *em)
{
	if (em->mirr_free_arrays) {
		MEM_freeN(em->vert_index);
		em->vert_index = NULL;
	}
}
