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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_utils.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_DerivedMesh.h"
#include "BKE_bmesh.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_util.h"

#include "bmesh.h"

void EDBM_RecalcNormals(BMEditMesh *em)
{
	BM_mesh_normals_update(em->bm, TRUE);
}

void EDBM_ClearMesh(BMEditMesh *em)
{
	/* clear bmesh */
	BM_mesh_clear(em->bm);
	
	/* free derived meshes */
	if (em->derivedCage) {
		em->derivedCage->needsFree = 1;
		em->derivedCage->release(em->derivedCage);
	}
	if (em->derivedFinal && em->derivedFinal != em->derivedCage) {
		em->derivedFinal->needsFree = 1;
		em->derivedFinal->release(em->derivedFinal);
	}
	
	em->derivedCage = em->derivedFinal = NULL;
	
	/* free tessellation data */
	em->tottri = 0;
	if (em->looptris) 
		MEM_freeN(em->looptris);
}

void EDBM_stats_update(BMEditMesh *em)
{
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};

	BMIter iter;
	BMElem *ele;
	int *tots[3];
	int i;

	tots[0] = &em->bm->totvertsel;
	tots[1] = &em->bm->totedgesel;
	tots[2] = &em->bm->totfacesel;
	
	em->bm->totvertsel = em->bm->totedgesel = em->bm->totfacesel = 0;

	for (i = 0; i < 3; i++) {
		ele = BM_iter_new(&iter, em->bm, iter_types[i], NULL);
		for ( ; ele; ele = BM_iter_step(&iter)) {
			if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
				(*tots[i])++;
			}
		}
	}
}

int EDBM_InitOpf(BMEditMesh *em, BMOperator *bmop, wmOperator *op, const char *fmt, ...)
{
	BMesh *bm = em->bm;
	va_list list;

	va_start(list, fmt);

	if (!BMO_op_vinitf(bm, bmop, fmt, list)) {
		BKE_reportf(op->reports, RPT_ERROR, "Parse error in %s", __func__);
		va_end(list);
		return 0;
	}
	
	if (!em->emcopy)
		em->emcopy = BMEdit_Copy(em);
	em->emcopyusers++;

	va_end(list);

	return 1;
}


/* returns 0 on error, 1 on success.  executes and finishes a bmesh operator */
int EDBM_FinishOp(BMEditMesh *em, BMOperator *bmop, wmOperator *op, const int report)
{
	const char *errmsg;
	
	BMO_op_finish(em->bm, bmop);

	if (BMO_error_get(em->bm, &errmsg, NULL)) {
		BMEditMesh *emcopy = em->emcopy;

		if (report) BKE_report(op->reports, RPT_ERROR, errmsg);

		BMEdit_Free(em);
		*em = *emcopy;

		MEM_freeN(emcopy);
		em->emcopyusers = 0;
		em->emcopy = NULL;
		return 0;
	}
	else {
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

int EDBM_CallOpf(BMEditMesh *em, wmOperator *op, const char *fmt, ...)
{
	BMesh *bm = em->bm;
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_op_vinitf(bm, &bmop, fmt, list)) {
		BKE_reportf(op->reports, RPT_ERROR, "Parse error in %s", __func__);
		va_end(list);
		return 0;
	}

	if (!em->emcopy)
		em->emcopy = BMEdit_Copy(em);
	em->emcopyusers++;

	BMO_op_exec(bm, &bmop);

	va_end(list);
	return EDBM_FinishOp(em, &bmop, op, TRUE);
}

int EDBM_CallAndSelectOpf(BMEditMesh *em, wmOperator *op, const char *selectslot, const char *fmt, ...)
{
	BMesh *bm = em->bm;
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_op_vinitf(bm, &bmop, fmt, list)) {
		BKE_reportf(op->reports, RPT_ERROR, "Parse error in %s", __func__);
		va_end(list);
		return 0;
	}

	if (!em->emcopy)
		em->emcopy = BMEdit_Copy(em);
	em->emcopyusers++;

	BMO_op_exec(bm, &bmop);

	BM_mesh_elem_flag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT);

	BMO_slot_buffer_hflag_enable(em->bm, &bmop, selectslot, BM_ALL, BM_ELEM_SELECT, TRUE);

	va_end(list);
	return EDBM_FinishOp(em, &bmop, op, TRUE);
}

int EDBM_CallOpfSilent(BMEditMesh *em, const char *fmt, ...)
{
	BMesh *bm = em->bm;
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_op_vinitf(bm, &bmop, fmt, list)) {
		va_end(list);
		return 0;
	}

	if (!em->emcopy)
		em->emcopy = BMEdit_Copy(em);
	em->emcopyusers++;

	BMO_op_exec(bm, &bmop);

	va_end(list);
	return EDBM_FinishOp(em, &bmop, NULL, FALSE);
}

void EDBM_selectmode_to_scene(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);

	if (!em)
		return;

	scene->toolsettings->selectmode = em->selectmode;

	/* Request redraw of header buttons (to show new select mode) */
	WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, scene);
}

void EDBM_MakeEditBMesh(ToolSettings *ts, Scene *UNUSED(scene), Object *ob)
{
	Mesh *me = ob->data;
	BMesh *bm;

	if (!me->mpoly && me->totface) {
		fprintf(stderr, "%s: bmesh conversion issue! may lose lots of geometry! (bmesh internal error)\n", __func__);
		
		/* BMESH_TODO need to write smarter code here */
		bm = BKE_mesh_to_bmesh(me, ob);
	}
	else {
		bm = BKE_mesh_to_bmesh(me, ob);
	}

	if (me->edit_btmesh) {
		/* this happens when switching shape keys */
		BMEdit_Free(me->edit_btmesh);
		MEM_freeN(me->edit_btmesh);
	}

	/* currently executing operators re-tessellates, so we can avoid doing here
	 * but at some point it may need to be added back. */
#if 0
	me->edit_btmesh = BMEdit_Create(bm, TRUE);
#else
	me->edit_btmesh = BMEdit_Create(bm, FALSE);
#endif

	me->edit_btmesh->selectmode = me->edit_btmesh->bm->selectmode = ts->selectmode;
	me->edit_btmesh->me = me;
	me->edit_btmesh->ob = ob;
}

void EDBM_LoadEditBMesh(Scene *scene, Object *ob)
{
	Mesh *me = ob->data;
	BMesh *bm = me->edit_btmesh->bm;

	BMO_op_callf(bm, "object_load_bmesh scene=%p object=%p", scene, ob);

#ifdef USE_TESSFACE_DEFAULT
	BKE_mesh_tessface_calc(me);
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
		int i = 0;
		
		tm->vert_index = MEM_mallocN(sizeof(void **) * tm->bm->totvert, "tm->vert_index");

		ele = BM_iter_new(&iter, tm->bm, BM_VERTS_OF_MESH, NULL);
		for ( ; ele; ele = BM_iter_step(&iter)) {
			tm->vert_index[i++] = ele;
		}
	}

	if (foredge) {
		BMIter iter;
		BMEdge *ele;
		int i = 0;
		
		tm->edge_index = MEM_mallocN(sizeof(void **) * tm->bm->totedge, "tm->edge_index");

		ele = BM_iter_new(&iter, tm->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; ele; ele = BM_iter_step(&iter)) {
			tm->edge_index[i++] = ele;
		}
	}

	if (forface) {
		BMIter iter;
		BMFace *ele;
		int i = 0;
		
		tm->face_index = MEM_mallocN(sizeof(void **) * tm->bm->totface, "tm->face_index");

		ele = BM_iter_new(&iter, tm->bm, BM_FACES_OF_MESH, NULL);
		for ( ; ele; ele = BM_iter_step(&iter)) {
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
	return tm->vert_index && index < tm->bm->totvert ? tm->vert_index[index] : NULL;
}

BMEdge *EDBM_get_edge_for_index(BMEditMesh *tm, int index)
{
	return tm->edge_index && index < tm->bm->totedge ? tm->edge_index[index] : NULL;
}

BMFace *EDBM_get_face_for_index(BMEditMesh *tm, int index)
{
	return (tm->face_index && index < tm->bm->totface && index >= 0) ? tm->face_index[index] : NULL;
}

void EDBM_selectmode_flush_ex(BMEditMesh *em, int selectmode)
{
	em->bm->selectmode = selectmode;
	BM_mesh_select_mode_flush(em->bm);
	em->bm->selectmode = em->selectmode;
}

void EDBM_selectmode_flush(BMEditMesh *em)
{
	EDBM_selectmode_flush_ex(em, em->selectmode);
}

void EDBM_deselect_flush(BMEditMesh *em)
{
	/* function below doesnt use. just do this to keep the values in sync */
	em->bm->selectmode = em->selectmode;
	BM_mesh_deselect_flush(em->bm);
}


void EDBM_select_flush(BMEditMesh *em)
{
	/* function below doesnt use. just do this to keep the values in sync */
	em->bm->selectmode = em->selectmode;
	BM_mesh_select_flush(em->bm);
}

void EDBM_select_more(BMEditMesh *em)
{
	BMOperator bmop;
	int use_faces = em->selectmode > SCE_SELECT_EDGE;

	BMO_op_initf(em->bm, &bmop,
	             "regionextend geom=%hvef constrict=%b use_faces=%b",
	             BM_ELEM_SELECT, FALSE, use_faces);
	BMO_op_exec(em->bm, &bmop);
	/* don't flush selection in edge/vertex mode  */
	BMO_slot_buffer_hflag_enable(em->bm, &bmop, "geomout", BM_ALL, BM_ELEM_SELECT, use_faces ? TRUE : FALSE);
	BMO_op_finish(em->bm, &bmop);

	EDBM_select_flush(em);
}

void EDBM_select_less(BMEditMesh *em)
{
	BMOperator bmop;
	int use_faces = em->selectmode > SCE_SELECT_EDGE;

	BMO_op_initf(em->bm, &bmop,
	             "regionextend geom=%hvef constrict=%b use_faces=%b",
	             BM_ELEM_SELECT, TRUE, use_faces);
	BMO_op_exec(em->bm, &bmop);
	/* don't flush selection in edge/vertex mode  */
	BMO_slot_buffer_hflag_disable(em->bm, &bmop, "geomout", BM_ALL, BM_ELEM_SELECT, use_faces ? TRUE : FALSE);
	BMO_op_finish(em->bm, &bmop);

	EDBM_selectmode_flush(em);
}

int EDBM_get_actSelection(BMEditMesh *em, BMEditSelection *ese)
{
	BMEditSelection *ese_last = em->bm->selected.last;
	BMFace *efa = BM_active_face_get(em->bm, FALSE);

	ese->next = ese->prev = NULL;
	
	if (ese_last) {
		if (ese_last->htype == BM_FACE) { /* if there is an active face, use it over the last selected face */
			if (efa) {
				ese->ele = (BMElem *)efa;
			}
			else {
				ese->ele = ese_last->ele;
			}
			ese->htype = BM_FACE;
		}
		else {
			ese->ele =   ese_last->ele;
			ese->htype = ese_last->htype;
		}
	}
	else if (efa) { /* no */
		ese->ele   = (BMElem *)efa;
		ese->htype = BM_FACE;
	}
	else {
		ese->ele = NULL;
		return 0;
	}
	return 1;
}

void EDBM_flag_disable_all(BMEditMesh *em, const char hflag)
{
	BM_mesh_elem_flag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, hflag);
}

void EDBM_flag_enable_all(BMEditMesh *em, const char hflag)
{
	BM_mesh_elem_flag_enable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, hflag);
}

/**************-------------- Undo ------------*****************/

/* for callbacks */

static void *getEditMesh(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_MESH) {
		Mesh *me = obedit->data;
		return me->edit_btmesh;
	}
	return NULL;
}

typedef struct undomesh {
	Mesh me;
	int selectmode;
	char obname[MAX_ID_NAME - 2];
} undomesh;

/* undo simply makes copies of a bmesh */
static void *editbtMesh_to_undoMesh(void *emv, void *obdata)
{
	BMEditMesh *em = emv;
	Mesh *obme = obdata;
	
	undomesh *um = MEM_callocN(sizeof(undomesh), "undo Mesh");
	BLI_strncpy(um->obname, em->ob->id.name + 2, sizeof(um->obname));
	
	/* make sure shape keys work */
	um->me.key = obme->key ? copy_key_nolib(obme->key) : NULL;

	/* BM_mesh_validate(em->bm); */ /* for troubleshooting */

	BMO_op_callf(em->bm, "bmesh_to_mesh mesh=%p notessellation=%b", &um->me, TRUE);
	um->selectmode = em->selectmode;

	return um;
}

static void undoMesh_to_editbtMesh(void *umv, void *emv, void *UNUSED(obdata))
{
	BMEditMesh *em = emv, *em2;
	Object *ob;
	undomesh *um = umv;
	BMesh *bm;

	/* BMESH_TODO - its possible the name wont be found right?, should fallback */
	ob = (Object *)find_id("OB", um->obname);
	ob->shapenr = em->bm->shapenr;

	BMEdit_Free(em);

	bm = BM_mesh_create(&bm_mesh_allocsize_default);
	BMO_op_callf(bm, "mesh_to_bmesh mesh=%p object=%p set_shapekey=%b", &um->me, ob, FALSE);

	em2 = BMEdit_Create(bm, TRUE);
	*em = *em2;
	
	em->selectmode = um->selectmode;

	MEM_freeN(em2);
}


static void free_undo(void *umv)
{
	if (((Mesh *)umv)->key) {
		free_key(((Mesh *)umv)->key);
		MEM_freeN(((Mesh *)umv)->key);
	}
	
	free_mesh(umv, 0);
	MEM_freeN(umv);
}

/* and this is all the undo system needs to know */
void undo_push_mesh(bContext *C, const char *name)
{
	/* em->ob gets out of date and crashes on mesh undo,
	 * this is an easy way to ensure its OK
	 * though we could investigate the matter further. */
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	em->ob = obedit;

	undo_editmode_push(C, name, getEditMesh, free_undo, undoMesh_to_editbtMesh, editbtMesh_to_undoMesh, NULL);
}

/* write comment here */
UvVertMap *EDBM_make_uv_vert_map(BMEditMesh *em, int selected, int do_face_idx_array, float *limit)
{
	BMVert *ev;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	/* vars from original func */
	UvVertMap *vmap;
	UvMapVert *buf;
	/* MTexPoly *tf; */ /* UNUSED */
	MLoopUV *luv;
	unsigned int a;
	int totverts, i, totuv;
	
	if (do_face_idx_array)
		EDBM_init_index_arrays(em, 0, 0, 1);

	BM_mesh_elem_index_ensure(em->bm, BM_VERT);
	
	totverts = em->bm->totvert;
	totuv = 0;

	/* generate UvMapVert array */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (!selected || ((!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) && BM_elem_flag_test(efa, BM_ELEM_SELECT)))
			totuv += efa->len;
	}

	if (totuv == 0) {
		if (do_face_idx_array)
			EDBM_free_index_arrays(em);
		return NULL;
	}
	vmap = (UvVertMap *)MEM_callocN(sizeof(*vmap), "UvVertMap");
	if (!vmap) {
		if (do_face_idx_array)
			EDBM_free_index_arrays(em);
		return NULL;
	}

	vmap->vert = (UvMapVert **)MEM_callocN(sizeof(*vmap->vert) * totverts, "UvMapVert_pt");
	buf = vmap->buf = (UvMapVert *)MEM_callocN(sizeof(*vmap->buf) * totuv, "UvMapVert");

	if (!vmap->vert || !vmap->buf) {
		free_uv_vert_map(vmap);
		if (do_face_idx_array)
			EDBM_free_index_arrays(em);
		return NULL;
	}
	
	a = 0;
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (!selected || ((!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) && BM_elem_flag_test(efa, BM_ELEM_SELECT))) {
			i = 0;
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				buf->tfindex = i;
				buf->f = a;
				buf->separate = 0;
				
				buf->next = vmap->vert[BM_elem_index_get(l->v)];
				vmap->vert[BM_elem_index_get(l->v)] = buf;
				
				buf++;
				i++;
			}
		}

		a++;
	}
	
	/* sort individual uvs for each vert */
	a = 0;
	BM_ITER(ev, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		UvMapVert *newvlist = NULL, *vlist = vmap->vert[a];
		UvMapVert *iterv, *v, *lastv, *next;
		float *uv, *uv2, uvdiff[2];

		while (vlist) {
			v = vlist;
			vlist = vlist->next;
			v->next = newvlist;
			newvlist = v;

			efa = EDBM_get_face_for_index(em, v->f);
			/* tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY); */ /* UNUSED */
			
			l = BM_iter_at_index(em->bm, BM_LOOPS_OF_FACE, efa, v->tfindex);
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			uv = luv->uv;
			
			lastv = NULL;
			iterv = vlist;

			while (iterv) {
				next = iterv->next;
				efa = EDBM_get_face_for_index(em, iterv->f);
				/* tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY); */ /* UNUSED */
				
				l = BM_iter_at_index(em->bm, BM_LOOPS_OF_FACE, efa, iterv->tfindex);
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				uv2 = luv->uv;
				
				sub_v2_v2v2(uvdiff, uv2, uv);

				if (fabs(uvdiff[0]) < limit[0] && fabs(uvdiff[1]) < limit[1]) {
					if (lastv) lastv->next = next;
					else vlist = next;
					iterv->next = newvlist;
					newvlist = iterv;
				}
				else {
					lastv = iterv;
				}

				iterv = next;
			}

			newvlist->separate = 1;
		}

		vmap->vert[a] = newvlist;
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

/* from editmesh_lib.c in trunk */


/* A specialized vert map used by stitch operator */
UvElementMap *EDBM_make_uv_element_map(BMEditMesh *em, int selected, int do_islands)
{
	BMVert *ev;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	/* vars from original func */
	UvElementMap *element_map;
	UvElement *buf;
	UvElement *islandbuf;
	/* island number for faces */
	int *island_number;

	MLoopUV *luv;
	int totverts, i, totuv, j, nislands = 0, islandbufsize = 0;

	unsigned int *map;
	BMFace **stack;
	int stacksize = 0;

	BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_FACE);

	totverts = em->bm->totvert;
	totuv = 0;

	island_number = MEM_mallocN(sizeof(*stack) * em->bm->totface, "uv_island_number_face");
	if (!island_number) {
		return NULL;
	}

	/* generate UvElement array */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (!selected || ((!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) && BM_elem_flag_test(efa, BM_ELEM_SELECT)))
			totuv += efa->len;
	}

	if (totuv == 0) {
		MEM_freeN(island_number);
		return NULL;
	}
	element_map = (UvElementMap *)MEM_callocN(sizeof(*element_map), "UvElementMap");
	if (!element_map) {
		MEM_freeN(island_number);
		return NULL;
	}
	element_map->totalUVs = totuv;
	element_map->vert = (UvElement **)MEM_callocN(sizeof(*element_map->vert) * totverts, "UvElementVerts");
	buf = element_map->buf = (UvElement *)MEM_callocN(sizeof(*element_map->buf) * totuv, "UvElement");

	if (!element_map->vert || !element_map->buf) {
		EDBM_free_uv_element_map(element_map);
		MEM_freeN(island_number);
		return NULL;
	}

	j = 0;
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		island_number[j++] = INVALID_ISLAND;
		if (!selected || ((!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) && BM_elem_flag_test(efa, BM_ELEM_SELECT))) {
			BM_ITER_INDEX(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa, i) {
				buf->l = l;
				buf->face = efa;
				buf->separate = 0;
				buf->island = INVALID_ISLAND;
				buf->tfindex = i;

				buf->next = element_map->vert[BM_elem_index_get(l->v)];
				element_map->vert[BM_elem_index_get(l->v)] = buf;

				buf++;
			}
		}
	}

	/* sort individual uvs for each vert */
	i = 0;
	BM_ITER(ev, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		UvElement *newvlist = NULL, *vlist = element_map->vert[i];
		UvElement *iterv, *v, *lastv, *next;
		float *uv, *uv2, uvdiff[2];

		while (vlist) {
			v = vlist;
			vlist = vlist->next;
			v->next = newvlist;
			newvlist = v;

			l = v->l;
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			uv = luv->uv;

			lastv = NULL;
			iterv = vlist;

			while (iterv) {
				next = iterv->next;

				l = iterv->l;
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				uv2 = luv->uv;

				sub_v2_v2v2(uvdiff, uv2, uv);

				if (fabsf(uvdiff[0]) < STD_UV_CONNECT_LIMIT && fabsf(uvdiff[1]) < STD_UV_CONNECT_LIMIT) {
					if (lastv) lastv->next = next;
					else vlist = next;
					iterv->next = newvlist;
					newvlist = iterv;
				}
				else {
					lastv = iterv;
				}

				iterv = next;
			}

			newvlist->separate = 1;
		}

		element_map->vert[i] = newvlist;
		i++;
	}

	if (do_islands) {
		/* map holds the map from current vmap->buf to the new, sorted map */
		map = MEM_mallocN(sizeof(*map) * totuv, "uvelement_remap");
		stack = MEM_mallocN(sizeof(*stack) * em->bm->totface, "uv_island_face_stack");
		islandbuf = MEM_callocN(sizeof(*islandbuf) * totuv, "uvelement_island_buffer");

		/* at this point, every UvElement in vert points to a UvElement sharing the same vertex. Now we should sort uv's in islands. */
		for (i = 0; i < totuv; i++) {
			if (element_map->buf[i].island == INVALID_ISLAND) {
				element_map->buf[i].island = nislands;
				stack[0] = element_map->buf[i].face;
				island_number[BM_elem_index_get(stack[0])] = nislands;
				stacksize = 1;

				while (stacksize > 0) {
					efa = stack[--stacksize];

					BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
						UvElement *element, *initelement = element_map->vert[BM_elem_index_get(l->v)];

						for (element = initelement; element; element = element->next) {
							if (element->separate)
								initelement = element;

							if (element->face == efa) {
								/* found the uv corresponding to our face and vertex. Now fill it to the buffer */
								element->island = nislands;
								map[element - element_map->buf] = islandbufsize;
								islandbuf[islandbufsize].l = element->l;
								islandbuf[islandbufsize].face = element->face;
								islandbuf[islandbufsize].separate = element->separate;
								islandbuf[islandbufsize].tfindex = element->tfindex;
								islandbuf[islandbufsize].island =  nislands;
								islandbufsize++;

								for (element = initelement; element; element = element->next) {
									if (element->separate && element != initelement)
										break;

									if (island_number[BM_elem_index_get(element->face)] == INVALID_ISLAND) {
										stack[stacksize++] = element->face;
										island_number[BM_elem_index_get(element->face)] = nislands;
									}
								}
								break;
							}
						}
					}
				}

				nislands++;
			}
		}

		/* remap */
		for (i = 0; i < em->bm->totvert; i++) {
			/* important since we may do selection only. Some of these may be NULL */
			if (element_map->vert[i])
				element_map->vert[i] = &islandbuf[map[element_map->vert[i] - element_map->buf]];
		}

		element_map->islandIndices = MEM_callocN(sizeof(*element_map->islandIndices) * nislands, "UvElementMap_island_indices");
		if (!element_map->islandIndices) {
			MEM_freeN(islandbuf);
			MEM_freeN(stack);
			MEM_freeN(map);
			EDBM_free_uv_element_map(element_map);
			MEM_freeN(island_number);
		}

		j = 0;
		for (i = 0; i < totuv; i++) {
			UvElement *element = element_map->buf[i].next;
			if (element == NULL)
				islandbuf[map[i]].next = NULL;
			else
				islandbuf[map[i]].next = &islandbuf[map[element - element_map->buf]];

			if (islandbuf[i].island != j) {
				j++;
				element_map->islandIndices[j] = i;
			}
		}

		MEM_freeN(element_map->buf);

		element_map->buf = islandbuf;
		element_map->totalIslands = nislands;
		MEM_freeN(stack);
		MEM_freeN(map);
	}
	MEM_freeN(island_number);

	return element_map;
}


UvMapVert *EM_get_uv_map_vert(UvVertMap *vmap, unsigned int v)
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

void EDBM_free_uv_element_map(UvElementMap *element_map)
{
	if (element_map) {
		if (element_map->vert) MEM_freeN(element_map->vert);
		if (element_map->buf) MEM_freeN(element_map->buf);
		if (element_map->islandIndices) MEM_freeN(element_map->islandIndices);
		MEM_freeN(element_map);
	}
}

/* last_sel, use em->act_face otherwise get the last selected face in the editselections
 * at the moment, last_sel is mainly useful for making sure the space image dosnt flicker */
MTexPoly *EDBM_get_active_mtexpoly(BMEditMesh *em, BMFace **r_act_efa, int sloppy)
{
	BMFace *efa = NULL;
	
	if (!EDBM_texFaceCheck(em))
		return NULL;
	
	efa = BM_active_face_get(em->bm, sloppy);
	
	if (efa) {
		if (r_act_efa) *r_act_efa = efa;
		return CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
	}

	if (r_act_efa) *r_act_efa = NULL;
	return NULL;
}

/* can we edit UV's for this mesh?*/
int EDBM_texFaceCheck(BMEditMesh *em)
{
	/* some of these checks could be a touch overkill */
	return em && em->bm->totface && CustomData_has_layer(&em->bm->pdata, CD_MTEXPOLY) &&
	       CustomData_has_layer(&em->bm->ldata, CD_MLOOPUV);
}

int EDBM_vertColorCheck(BMEditMesh *em)
{
	/* some of these checks could be a touch overkill */
	return em && em->bm->totface && CustomData_has_layer(&em->bm->ldata, CD_MLOOPCOL);
}

static BMVert *cache_mirr_intptr_as_bmvert(intptr_t *index_lookup, int index)
{
	intptr_t eve_i = index_lookup[index];
	return (eve_i == -1) ? NULL : (BMVert *)eve_i;
}

/* BM_SEARCH_MAXDIST is too big, copied from 2.6x MOC_THRESH, should become a
 * preference */
#define BM_SEARCH_MAXDIST_MIRR 0.00002f
#define BM_CD_LAYER_ID "__mirror_index"
void EDBM_CacheMirrorVerts(BMEditMesh *em, const short use_select)
{
	Mesh *me = em->me;
	BMesh *bm = em->bm;
	BMIter iter;
	BMVert *v;
	int li, topo = 0;

	/* one or the other is used depending if topo is enabled */
	BMBVHTree *tree = NULL;
	MirrTopoStore_t mesh_topo_store = {NULL, -1, -1, -1};

	if (me && (me->editflag & ME_EDIT_MIRROR_TOPO)) {
		topo = 1;
	}

	if (!em->vert_index) {
		EDBM_init_index_arrays(em, 1, 0, 0);
		em->mirr_free_arrays = 1;
	}

	if (!CustomData_get_layer_named(&bm->vdata, CD_PROP_INT, BM_CD_LAYER_ID)) {
		BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_INT, BM_CD_LAYER_ID);
	}

	li = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_INT, BM_CD_LAYER_ID);

	bm->vdata.layers[li].flag |= CD_FLAG_TEMPORARY;

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	if (topo) {
		ED_mesh_mirrtopo_init(me, -1, &mesh_topo_store, TRUE);
	}
	else {
		tree = BMBVH_NewBVH(em, 0, NULL, NULL);
	}

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {

		/* temporary for testing, check for selection */
		if (use_select && !BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			/* do nothing */
		}
		else {
			BMVert *mirr;
			int *idx = CustomData_bmesh_get_layer_n(&bm->vdata, v->head.data, li);

			if (topo) {
				mirr = cache_mirr_intptr_as_bmvert(mesh_topo_store.index_lookup, BM_elem_index_get(v));
			}
			else {
				float co[3] = {-v->co[0], v->co[1], v->co[2]};
				mirr = BMBVH_FindClosestVert(tree, co, BM_SEARCH_MAXDIST_MIRR);
			}

			if (mirr && mirr != v) {
				*idx = BM_elem_index_get(mirr);
				idx = CustomData_bmesh_get_layer_n(&bm->vdata, mirr->head.data, li);
				*idx = BM_elem_index_get(v);
			}
			else {
				*idx = -1;
			}
		}

	}


	if (topo) {
		ED_mesh_mirrtopo_free(&mesh_topo_store);
	}
	else {
		BMBVH_FreeBVH(tree);
	}

	em->mirror_cdlayer = li;
}

BMVert *EDBM_GetMirrorVert(BMEditMesh *em, BMVert *v)
{
	int *mirr = CustomData_bmesh_get_layer_n(&em->bm->vdata, v->head.data, em->mirror_cdlayer);

	BLI_assert(em->mirror_cdlayer != -1); /* invalid use */

	if (mirr && *mirr >= 0 && *mirr < em->bm->totvert) {
		if (!em->vert_index) {
			printf("err: should only be called between "
			       "EDBM_CacheMirrorVerts and EDBM_EndMirrorCache");
			return NULL;
		}

		return em->vert_index[*mirr];
	}

	return NULL;
}

void EDBM_ClearMirrorVert(BMEditMesh *em, BMVert *v)
{
	int *mirr = CustomData_bmesh_get_layer_n(&em->bm->vdata, v->head.data, em->mirror_cdlayer);

	BLI_assert(em->mirror_cdlayer != -1); /* invalid use */

	if (mirr) {
		*mirr = -1;
	}
}

void EDBM_EndMirrorCache(BMEditMesh *em)
{
	if (em->mirr_free_arrays) {
		MEM_freeN(em->vert_index);
		em->vert_index = NULL;
	}

	em->mirror_cdlayer = -1;
}

void EDBM_ApplyMirrorCache(BMEditMesh *em, const int sel_from, const int sel_to)
{
	BMIter iter;
	BMVert *v;

	BLI_assert(em->vert_index != NULL);

	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_elem_flag_test(v, BM_ELEM_SELECT) == sel_from) {
			BMVert *mirr = EDBM_GetMirrorVert(em, v);
			if (mirr) {
				if (BM_elem_flag_test(mirr, BM_ELEM_SELECT) == sel_to) {
					copy_v3_v3(mirr->co, v->co);
					mirr->co[0] *= -1.0f;
				}
			}
		}
	}
}


/* swap is 0 or 1, if 1 it hides not selected */
void EDBM_hide_mesh(BMEditMesh *em, int swap)
{
	BMIter iter;
	BMElem *ele;
	int itermode;

	if (em == NULL) return;

	if (em->selectmode & SCE_SELECT_VERTEX)
		itermode = BM_VERTS_OF_MESH;
	else if (em->selectmode & SCE_SELECT_EDGE)
		itermode = BM_EDGES_OF_MESH;
	else
		itermode = BM_FACES_OF_MESH;

	BM_ITER(ele, &iter, em->bm, itermode, NULL) {
		if (BM_elem_flag_test(ele, BM_ELEM_SELECT) ^ swap)
			BM_elem_hide_set(em->bm, ele, TRUE);
	}

	EDBM_selectmode_flush(em);

	/* original hide flushing comment (OUTDATED):
	 * hide happens on least dominant select mode, and flushes up, not down! (helps preventing errors in subsurf) */
	/* - vertex hidden, always means edge is hidden too
	 * - edge hidden, always means face is hidden too
	 * - face hidden, only set face hide
	 * - then only flush back down what's absolute hidden
	 */
}


void EDBM_reveal_mesh(BMEditMesh *em)
{
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};

	int sels[3] = {(em->selectmode & SCE_SELECT_VERTEX),
	               (em->selectmode & SCE_SELECT_EDGE),
	               (em->selectmode & SCE_SELECT_FACE), };

	BMIter iter;
	BMElem *ele;
	int i;

	/* Use tag flag to remember what was hidden before all is revealed.
	 * BM_ELEM_HIDDEN --> BM_ELEM_TAG */
	for (i = 0; i < 3; i++) {
		BM_ITER(ele, &iter, em->bm, iter_types[i], NULL) {
			BM_elem_flag_set(ele, BM_ELEM_TAG, BM_elem_flag_test(ele, BM_ELEM_HIDDEN));
		}
	}

	/* Reveal everything */
	EDBM_flag_disable_all(em, BM_ELEM_HIDDEN);

	/* Select relevant just-revealed elements */
	for (i = 0; i < 3; i++) {
		if (!sels[i]) {
			continue;
		}

		BM_ITER(ele, &iter, em->bm, iter_types[i], NULL) {
			if (BM_elem_flag_test(ele, BM_ELEM_TAG)) {
				BM_elem_select_set(em->bm, ele, TRUE);
			}
		}
	}

	EDBM_selectmode_flush(em);
}

/* so many tools call these that we better make it a generic function.
 */
void EDBM_update_generic(bContext *C, BMEditMesh *em, const short do_tessface)
{
	Object *ob = em->ob;
	/* order of calling isn't important */
	DAG_id_tag_update(ob->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	if (do_tessface) {
		BMEdit_RecalcTessellation(em);
	}
}

/* * Selection History ***************************************************** */
/* these wrap equivalent bmesh functions.  I'm in two minds of it we should
 * just use the bm functions directly; on the one hand, there's no real
 * need (at the moment) to wrap them, but on the other hand having these
 * wrapped avoids a confusing mess of mixing BM_ and EDBM_ namespaces. */

void EDBM_editselection_center(BMEditMesh *em, float *center, BMEditSelection *ese)
{
	BM_editselection_center(em->bm, center, ese);
}

void EDBM_editselection_normal(float *normal, BMEditSelection *ese)
{
	BM_editselection_normal(normal, ese);
}

/* Calculate a plane that is rightangles to the edge/vert/faces normal
 * also make the plane run along an axis that is related to the geometry,
 * because this is used for the manipulators Y axis. */
void EDBM_editselection_plane(BMEditMesh *em, float *plane, BMEditSelection *ese)
{
	BM_editselection_plane(em->bm, plane, ese);
}

void EDBM_remove_selection(BMEditMesh *em, BMElem *ele)
{
	BM_select_history_remove(em->bm, ele);
}

void EDBM_store_selection(BMEditMesh *em, BMElem *ele)
{
	BM_select_history_store(em->bm, ele);
}

void EDBM_validate_selections(BMEditMesh *em)
{
	BM_select_history_validate(em->bm);
}
/* end select history */
