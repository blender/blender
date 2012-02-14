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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/uvedit/uvedit_unwrap_ops.c
 *  \ingroup eduv
 */


#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_modifier_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_uvproject.h"
#include "BLI_utildefines.h"
#include "BLI_rand.h"
#include "BLI_string.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_subsurf.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_scanfill.h"
#include "BLI_array.h"
#include "BLI_uvproject.h"

#include "PIL_time.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"


#include "WM_api.h"
#include "WM_types.h"

#include "uvedit_intern.h"
#include "uvedit_parametrizer.h"

static int ED_uvedit_ensure_uvs(bContext *C, Scene *scene, Object *obedit)
{
	Main *bmain= CTX_data_main(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMIter iter;
	Image *ima;
	bScreen *sc;
	ScrArea *sa;
	SpaceLink *slink;
	SpaceImage *sima;

	if(ED_uvedit_test(obedit)) {
		return 1;
	}

	if(em && em->bm->totface && !CustomData_has_layer(&em->bm->pdata, CD_MTEXPOLY)) {
		BM_data_layer_add(em->bm, &em->bm->pdata, CD_MTEXPOLY);
		BM_data_layer_add(em->bm, &em->bm->ldata, CD_MLOOPUV);
	}

	if(!ED_uvedit_test(obedit)) {
		return 0;
	}

	ima= CTX_data_edit_image(C);

	if(!ima) {
		/* no image in context in the 3d view, we find first image window .. */
		sc= CTX_wm_screen(C);

		for(sa=sc->areabase.first; sa; sa=sa->next) {
			slink= sa->spacedata.first;
			if(slink->spacetype == SPACE_IMAGE) {
				sima= (SpaceImage*)slink;

				ima= sima->image;
				if(ima) {
					if(ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE)
						ima= NULL;
					else
						break;
				}
			}
		}
	}
	
	if(ima)
		ED_uvedit_assign_image(bmain, scene, obedit, ima, NULL);
	
	/* select new UV's */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		uvedit_face_select(scene, em, efa);
	}

	return 1;
}

/****************** Parametrizer Conversion ***************/

static int uvedit_have_selection(Scene *scene, BMEditMesh *em, short implicit)
{
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	
	/* verify if we have any selected uv's before unwrapping,
	   so we can cancel the operator early */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
			if(BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
				continue;
		}
		else if(BM_elem_flag_test(efa, BM_ELEM_HIDDEN) || !BM_elem_flag_test(efa, BM_ELEM_SELECT))
			continue;
	
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			if (!luv)
				return 1;
			
			if (uvedit_uv_selected(em, scene, l))
				break;
		}
		
		if (implicit && !l)
			continue;
		
		return 1;
	}

	return 0;
}

static ParamHandle *construct_param_handle(Scene *scene, BMEditMesh *em, 
				    short implicit, short fill, short sel, 
				    short correct_aspect)
{
	ParamHandle *handle;
	BMFace *efa;
	BMLoop *l;
	BMEdge *eed;
	BMIter iter, liter;
	MTexPoly *tf;
	
	handle = param_construct_begin();

	if(correct_aspect) {
		efa = BM_active_face_get(em->bm, TRUE);

		if(efa) {
			float aspx, aspy;
			tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			ED_image_uv_aspect(tf->tpage, &aspx, &aspy);
		
			if(aspx!=aspy)
				param_aspect_ratio(handle, aspx, aspy);
		}
	}
	
	/* we need the vert indices */
	BM_mesh_elem_index_ensure(em->bm, BM_VERT);
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		EditVert *v, *lastv, *firstv;
		EditFace *sefa;
		ParamKey key, vkeys[4];
		ParamBool pin[4], select[4];
		BMLoop *ls[3];
		MLoopUV *luvs[3];
		float *co[4];
		float *uv[4];
		int lsel;

		if((BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) || (sel && BM_elem_flag_test(efa, BM_ELEM_SELECT)==0))
			continue;

		/* tf= (MTexPoly *)CustomData_em_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY); */ /* UNUSED */
		lsel = 0;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			if (uvedit_uv_selected(em, scene, l)) {
				lsel = 1;
				break;
			}
		}

		if (implicit && !lsel)
			continue;

		key = (ParamKey)efa;

		/*scanfill time!*/
		BLI_begin_edgefill();
		
		firstv = lastv = NULL;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			int i;
			
			v = BLI_addfillvert(l->v->co);
			
			/*add small random offset*/
			for (i=0; i<3; i++) {
				v->co[i] += (BLI_drand()-0.5f)*FLT_EPSILON*50;
			}
			
			v->tmp.p = l;

			if (lastv) {
				BLI_addfilledge(lastv, v);
			}

			lastv = v;
			if (!firstv) 
				firstv = v;
		}

		BLI_addfilledge(firstv, v);
		
		/*mode 2 enables faster handling of tri/quads*/
		BLI_edgefill(2);
		for (sefa = fillfacebase.first; sefa; sefa=sefa->next) {
			ls[0] = sefa->v1->tmp.p;
			ls[1] = sefa->v2->tmp.p;
			ls[2] = sefa->v3->tmp.p;
			
			luvs[0] = CustomData_bmesh_get(&em->bm->ldata, ls[0]->head.data, CD_MLOOPUV);
			luvs[1] = CustomData_bmesh_get(&em->bm->ldata, ls[1]->head.data, CD_MLOOPUV);
			luvs[2] = CustomData_bmesh_get(&em->bm->ldata, ls[2]->head.data, CD_MLOOPUV);

			vkeys[0] = (ParamKey)BM_elem_index_get(ls[0]->v);
			vkeys[1] = (ParamKey)BM_elem_index_get(ls[1]->v);
			vkeys[2] = (ParamKey)BM_elem_index_get(ls[2]->v);

			co[0] = ls[0]->v->co;
			co[1] = ls[1]->v->co;
			co[2] = ls[2]->v->co;

			uv[0] = luvs[0]->uv;
			uv[1] = luvs[1]->uv;
			uv[2] = luvs[2]->uv;

			pin[0] = (luvs[0]->flag & MLOOPUV_PINNED) != 0;
			pin[1] = (luvs[1]->flag & MLOOPUV_PINNED) != 0;
			pin[2] = (luvs[2]->flag & MLOOPUV_PINNED) != 0;

			select[0] = uvedit_uv_selected(em, scene, ls[0]) != 0;
			select[1] = uvedit_uv_selected(em, scene, ls[1]) != 0;
			select[2] = uvedit_uv_selected(em, scene, ls[2]) != 0;

			if (!p_face_exists(handle,vkeys,0,1,2))
					param_face_add(handle, key, 3, vkeys, co, uv, pin, select);
		}

		BLI_end_edgefill();
	}

	if(!implicit) {
		BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
			if(BM_elem_flag_test(eed, BM_ELEM_SEAM)) {
				ParamKey vkeys[2];
				vkeys[0] = (ParamKey)BM_elem_index_get(eed->v1);
				vkeys[1] = (ParamKey)BM_elem_index_get(eed->v2);
				param_edge_set_seam(handle, vkeys);
			}
		}
	}

	param_construct_end(handle, fill, implicit);

	return handle;
}


static void texface_from_original_index(BMFace *efa, int index, float **uv, ParamBool *pin, ParamBool *select, Scene *scene, BMEditMesh *em)
{
	BMLoop *l;
	BMIter liter;
	MLoopUV *luv;

	*uv = NULL;
	*pin = 0;
	*select = 1;

	if(index == ORIGINDEX_NONE)
		return;

	BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
		if(BM_elem_index_get(l->v) == index) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			*uv = luv->uv;
			*pin = (luv->flag & MLOOPUV_PINNED)? 1 : 0;
			*select = (uvedit_uv_selected(em, scene, l) != 0);
		}
	}
}

/* unwrap handle initialisation for subsurf aware-unwrapper. The many modifications required to make the original function(see above)
 * work justified the existence of a new function. */
static ParamHandle *construct_param_handle_subsurfed(Scene *scene, BMEditMesh *em, short fill, short sel, short correct_aspect)
{
	ParamHandle *handle;
	/* index pointers */
	MFace *face;
	MEdge *edge;
	BMFace *editFace;
	int i;

	/* modifier initialisation data, will  control what type of subdivision will happen*/
	SubsurfModifierData smd = {{0}};
	/* Used to hold subsurfed Mesh */
	DerivedMesh *derivedMesh, *initialDerived;
	/* holds original indices for subsurfed mesh */
	int *origVertIndices, *origFaceIndices, *origEdgeIndices;
	/* Holds vertices of subsurfed mesh */
	MVert *subsurfedVerts;
	MEdge *subsurfedEdges;
	MFace *subsurfedFaces;
	/* number of vertices and faces for subsurfed mesh*/
	int numOfEdges, numOfFaces;

	/* holds a map to editfaces for every subsurfed MFace. These will be used to get hidden/ selected flags etc. */
	BMFace **faceMap;
	/* similar to the above, we need a way to map edges to their original ones */
	BMEdge **edgeMap;

	handle = param_construct_begin();

	if(correct_aspect) {
		editFace = BM_active_face_get(em->bm, TRUE);

		if(editFace) {
			MTexPoly *tf;
			float aspx, aspy;
			tf= CustomData_bmesh_get(&em->bm->pdata, editFace->head.data, CD_MTEXPOLY);

			ED_image_uv_aspect(tf->tpage, &aspx, &aspy);

			if(aspx!=aspy)
				param_aspect_ratio(handle, aspx, aspy);
		}
	}

	/* number of subdivisions to perform */
	smd.levels = scene->toolsettings->uv_subsurf_level;
	smd.subdivType = ME_CC_SUBSURF;
		
	initialDerived = CDDM_from_BMEditMesh(em, NULL, 0, 0);
	derivedMesh = subsurf_make_derived_from_derived(initialDerived, &smd,
		0, NULL, 0, 0, 1);

	initialDerived->release(initialDerived);

	/* get the derived data */
	subsurfedVerts = derivedMesh->getVertArray(derivedMesh);
	subsurfedEdges = derivedMesh->getEdgeArray(derivedMesh);
	subsurfedFaces = derivedMesh->getTessFaceArray(derivedMesh);

	origVertIndices = derivedMesh->getVertDataArray(derivedMesh, CD_ORIGINDEX);
	origEdgeIndices = derivedMesh->getEdgeDataArray(derivedMesh, CD_ORIGINDEX);
	origFaceIndices = derivedMesh->getTessFaceDataArray(derivedMesh, CD_ORIGINDEX);

	numOfEdges = derivedMesh->getNumEdges(derivedMesh);
	numOfFaces = derivedMesh->getNumTessFaces(derivedMesh);

	faceMap = MEM_mallocN(numOfFaces*sizeof(EditFace *), "unwrap_edit_face_map");

	BM_mesh_elem_index_ensure(em->bm, BM_VERT);
	EDBM_init_index_arrays(em, 0, 1, 1);

	/* map subsurfed faces to original editFaces */
	for(i = 0; i < numOfFaces; i++)
		faceMap[i] = EDBM_get_face_for_index(em, origFaceIndices[i]);

	edgeMap = MEM_mallocN(numOfEdges*sizeof(EditEdge *), "unwrap_edit_edge_map");

	/* map subsurfed edges to original editEdges */
	for(i = 0; i < numOfEdges; i++) {
		/* not all edges correspond to an old edge */
		edgeMap[i] = (origEdgeIndices[i] != -1)?
			EDBM_get_edge_for_index(em, origEdgeIndices[i]) : NULL;
	}

	EDBM_free_index_arrays(em);

	/* Prepare and feed faces to the solver */
	for(i = 0; i < numOfFaces; i++) {
		ParamKey key, vkeys[4];
		ParamBool pin[4], select[4];
		float *co[4];
		float *uv[4];
		BMFace *origFace = faceMap[i];
		
		face = subsurfedFaces+i;

		if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
			if(BM_elem_flag_test(origFace, BM_ELEM_HIDDEN))
				continue;
		}
		else {
			if(BM_elem_flag_test(origFace, BM_ELEM_HIDDEN) || (sel && !BM_elem_flag_test(origFace, BM_ELEM_SELECT)))
				continue;
		}

		/* We will not check for v4 here. Subsurfed mfaces always have 4 vertices. */
		key = (ParamKey)face;
		vkeys[0] = (ParamKey)face->v1;
		vkeys[1] = (ParamKey)face->v2;
		vkeys[2] = (ParamKey)face->v3;
		vkeys[3] = (ParamKey)face->v4;

		co[0] = subsurfedVerts[face->v1].co;
		co[1] = subsurfedVerts[face->v2].co;
		co[2] = subsurfedVerts[face->v3].co;
		co[3] = subsurfedVerts[face->v4].co;
		
		/* This is where all the magic is done. If the vertex exists in the, we pass the original uv pointer to the solver, thus
		 * flushing the solution to the edit mesh. */
		texface_from_original_index(origFace, origVertIndices[face->v1], &uv[0], &pin[0], &select[0], scene, em);
		texface_from_original_index(origFace, origVertIndices[face->v2], &uv[1], &pin[1], &select[1], scene, em);
		texface_from_original_index(origFace, origVertIndices[face->v3], &uv[2], &pin[2], &select[2], scene, em);
		texface_from_original_index(origFace, origVertIndices[face->v4], &uv[3], &pin[3], &select[3], scene, em);

		param_face_add(handle, key, 4, vkeys, co, uv, pin, select);
	}

	/* these are calculated from original mesh too */
	for(edge = subsurfedEdges, i = 0; i < numOfEdges; i++, edge++) {
		if((edgeMap[i] != NULL) && BM_elem_flag_test(edgeMap[i], BM_ELEM_SEAM)) {
			ParamKey vkeys[2];
			vkeys[0] = (ParamKey)edge->v1;
			vkeys[1] = (ParamKey)edge->v2;
			param_edge_set_seam(handle, vkeys);
		}
	}

	param_construct_end(handle, fill, 0);

	/* cleanup */
	MEM_freeN(faceMap);
	MEM_freeN(edgeMap);
	derivedMesh->release(derivedMesh);

	return handle;
}

/* ******************** Minimize Stretch operator **************** */

typedef struct MinStretch {
	Scene *scene;
	Object *obedit;
	BMEditMesh *em;
	ParamHandle *handle;
	float blend;
	double lasttime;
	int i, iterations;
	wmTimer *timer;
} MinStretch;

static int minimize_stretch_init(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	MinStretch *ms;
	int fill_holes= RNA_boolean_get(op->ptr, "fill_holes");
	short implicit= 1;

	if(!uvedit_have_selection(scene, em, implicit)) {
		return 0;
	}

	ms= MEM_callocN(sizeof(MinStretch), "MinStretch");
	ms->scene= scene;
	ms->obedit= obedit;
	ms->em= em;
	ms->blend= RNA_float_get(op->ptr, "blend");
	ms->iterations= RNA_int_get(op->ptr, "iterations");
	ms->i= 0;
	ms->handle= construct_param_handle(scene, em, implicit, fill_holes, 1, 1);
	ms->lasttime= PIL_check_seconds_timer();

	param_stretch_begin(ms->handle);
	if(ms->blend != 0.0f)
		param_stretch_blend(ms->handle, ms->blend);

	op->customdata= ms;

	return 1;
}

static void minimize_stretch_iteration(bContext *C, wmOperator *op, int interactive)
{
	MinStretch *ms= op->customdata;
	ScrArea *sa= CTX_wm_area(C);

	param_stretch_blend(ms->handle, ms->blend);
	param_stretch_iter(ms->handle);

	ms->i++;
	RNA_int_set(op->ptr, "iterations", ms->i);

	if(interactive && (PIL_check_seconds_timer() - ms->lasttime > 0.5)) {
		char str[100];

		param_flush(ms->handle);

		if(sa) {
			BLI_snprintf(str, sizeof(str), "Minimize Stretch. Blend %.2f", ms->blend);
			ED_area_headerprint(sa, str);
		}

		ms->lasttime = PIL_check_seconds_timer();

		DAG_id_tag_update(ms->obedit->data, 0);
		WM_event_add_notifier(C, NC_GEOM|ND_DATA, ms->obedit->data);
	}
}

static void minimize_stretch_exit(bContext *C, wmOperator *op, int cancel)
{
	MinStretch *ms= op->customdata;
	ScrArea *sa= CTX_wm_area(C);

	if(sa)
		ED_area_headerprint(sa, NULL);
	if(ms->timer)
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), ms->timer);

	if(cancel)
		param_flush_restore(ms->handle);
	else
		param_flush(ms->handle);

	param_stretch_end(ms->handle);
	param_delete(ms->handle);

	DAG_id_tag_update(ms->obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, ms->obedit->data);

	MEM_freeN(ms);
	op->customdata= NULL;
}

static int minimize_stretch_exec(bContext *C, wmOperator *op)
{
	int i, iterations;

	if(!minimize_stretch_init(C, op))
		return OPERATOR_CANCELLED;

	iterations= RNA_int_get(op->ptr, "iterations");
	for(i=0; i<iterations; i++)
		minimize_stretch_iteration(C, op, 0);
	minimize_stretch_exit(C, op, 0);

	return OPERATOR_FINISHED;
}

static int minimize_stretch_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	MinStretch *ms;

	if(!minimize_stretch_init(C, op))
		return OPERATOR_CANCELLED;

	minimize_stretch_iteration(C, op, 1);

	ms= op->customdata;
	WM_event_add_modal_handler(C, op);
	ms->timer= WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);

	return OPERATOR_RUNNING_MODAL;
}

static int minimize_stretch_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	MinStretch *ms= op->customdata;

	switch(event->type) {
		case ESCKEY:
		case RIGHTMOUSE:
			minimize_stretch_exit(C, op, 1);
			return OPERATOR_CANCELLED;
		case RETKEY:
		case PADENTER:
		case LEFTMOUSE:
			minimize_stretch_exit(C, op, 0);
			return OPERATOR_FINISHED;
		case PADPLUSKEY:
		case WHEELUPMOUSE:
			if(ms->blend < 0.95f) {
				ms->blend += 0.1f;
				ms->lasttime= 0.0f;
				RNA_float_set(op->ptr, "blend", ms->blend);
				minimize_stretch_iteration(C, op, 1);
			}
			break;
		case PADMINUS:
		case WHEELDOWNMOUSE:
			if(ms->blend > 0.05f) {
				ms->blend -= 0.1f;
				ms->lasttime= 0.0f;
				RNA_float_set(op->ptr, "blend", ms->blend);
				minimize_stretch_iteration(C, op, 1);
			}
			break;
		case TIMER:
			if(ms->timer == event->customdata) {
				double start= PIL_check_seconds_timer();

				do {
					minimize_stretch_iteration(C, op, 1);
				} while(PIL_check_seconds_timer() - start < 0.01);
			}
			break;
	}

	if(ms->iterations && ms->i >= ms->iterations) {
		minimize_stretch_exit(C, op, 0);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int minimize_stretch_cancel(bContext *C, wmOperator *op)
{
	minimize_stretch_exit(C, op, 1);

	return OPERATOR_CANCELLED;
}

void UV_OT_minimize_stretch(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Minimize Stretch";
	ot->idname= "UV_OT_minimize_stretch";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_GRAB_POINTER|OPTYPE_BLOCKING;
	ot->description="Reduce UV stretching by relaxing angles";
	
	/* api callbacks */
	ot->exec= minimize_stretch_exec;
	ot->invoke= minimize_stretch_invoke;
	ot->modal= minimize_stretch_modal;
	ot->cancel= minimize_stretch_cancel;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "fill_holes", 1, "Fill Holes", "Virtual fill holes in mesh before unwrapping, to better avoid overlaps and preserve symmetry");
	RNA_def_float_factor(ot->srna, "blend", 0.0f, 0.0f, 1.0f, "Blend", "Blend factor between stretch minimized and original", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "iterations", 0, 0, INT_MAX, "Iterations", "Number of iterations to run, 0 is unlimited when run interactively", 0, 100);
}

/* ******************** Pack Islands operator **************** */

static int pack_islands_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	ParamHandle *handle;
	short implicit= 1;

	if(!uvedit_have_selection(scene, em, implicit)) {
		return OPERATOR_CANCELLED;
	}

	if(RNA_struct_property_is_set(op->ptr, "margin")) {
		scene->toolsettings->uvcalc_margin= RNA_float_get(op->ptr, "margin");
	}
	else {
		RNA_float_set(op->ptr, "margin", scene->toolsettings->uvcalc_margin);
	}

	handle = construct_param_handle(scene, em, implicit, 0, 1, 1);
	param_pack(handle, scene->toolsettings->uvcalc_margin);
	param_flush(handle);
	param_delete(handle);
	
	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_pack_islands(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Pack Islands";
	ot->idname= "UV_OT_pack_islands";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= pack_islands_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_float_factor(ot->srna, "margin", 0.0f, 0.0f, 1.0f, "Margin", "Space between islands", 0.0f, 1.0f);
}

/* ******************** Average Islands Scale operator **************** */

static int average_islands_scale_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	ParamHandle *handle;
	short implicit= 1;

	if(!uvedit_have_selection(scene, em, implicit)) {
		return OPERATOR_CANCELLED;
	}

	handle= construct_param_handle(scene, em, implicit, 0, 1, 1);
	param_average(handle);
	param_flush(handle);
	param_delete(handle);
	
	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_average_islands_scale(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Average Islands Scale";
	ot->idname= "UV_OT_average_islands_scale";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= average_islands_scale_exec;
	ot->poll= ED_operator_uvedit;
}

/**************** Live Unwrap *****************/

static ParamHandle *liveHandle = NULL;

void ED_uvedit_live_unwrap_begin(Scene *scene, Object *obedit)
{
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	short abf = scene->toolsettings->unwrapper == 0;
	short fillholes = scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;
	short use_subsurf = scene->toolsettings->uvcalc_flag & UVCALC_USESUBSURF;

	if(!ED_uvedit_test(obedit)) {
		return;
	}

	if(use_subsurf)
		liveHandle = construct_param_handle_subsurfed(scene, em, fillholes, 0, 1);
	else
		liveHandle = construct_param_handle(scene, em, 0, fillholes, 0, 1);

	param_lscm_begin(liveHandle, PARAM_TRUE, abf);
}

void ED_uvedit_live_unwrap_re_solve(void)
{
	if(liveHandle) {
		param_lscm_solve(liveHandle);
		param_flush(liveHandle);
	}
}
	
void ED_uvedit_live_unwrap_end(short cancel)
{
	if(liveHandle) {
		param_lscm_end(liveHandle);
		if(cancel)
			param_flush_restore(liveHandle);
		param_delete(liveHandle);
		liveHandle = NULL;
	}
}

/*************** UV Map Common Transforms *****************/

#define VIEW_ON_EQUATOR 0
#define VIEW_ON_POLES	1
#define ALIGN_TO_OBJECT	2

#define POLAR_ZX	0
#define POLAR_ZY	1

static void uv_map_transform_center(Scene *scene, View3D *v3d, float *result, 
				    Object *ob, BMEditMesh *em)
{
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	float min[3], max[3], *cursx;
	int around= (v3d)? v3d->around: V3D_CENTER;

	/* only operates on the edit object - this is all that's needed now */

	switch(around)  {
		case V3D_CENTER: /* bounding box center */
			min[0]= min[1]= min[2]= 1e20f;
			max[0]= max[1]= max[2]= -1e20f; 
			
			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL)  {
				if(BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
					BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
						DO_MINMAX(l->v->co, min, max);
					}
				}
			}
			mid_v3_v3v3(result, min, max);
			break;

		case V3D_CURSOR: /*cursor center*/ 
			cursx= give_cursor(scene, v3d);
			/* shift to objects world */
			result[0]= cursx[0]-ob->obmat[3][0];
			result[1]= cursx[1]-ob->obmat[3][1];
			result[2]= cursx[2]-ob->obmat[3][2];
			break;

		case V3D_LOCAL: /*object center*/
		case V3D_CENTROID: /* multiple objects centers, only one object here*/
		default:
			result[0]= result[1]= result[2]= 0.0;
			break;
	}
}

static void uv_map_rotation_matrix(float result[][4], RegionView3D *rv3d, Object *ob,
                                   float upangledeg, float sideangledeg, float radius)
{
	float rotup[4][4], rotside[4][4], viewmatrix[4][4], rotobj[4][4];
	float sideangle= 0.0f, upangle= 0.0f;
	int k;

	/* get rotation of the current view matrix */
	if(rv3d)
		copy_m4_m4(viewmatrix, rv3d->viewmat);
	else
		unit_m4(viewmatrix);

	/* but shifting */
	for(k=0; k<4; k++)
		viewmatrix[3][k] =0.0f;

	/* get rotation of the current object matrix */
	copy_m4_m4(rotobj,ob->obmat);

	/* but shifting */
	for(k=0; k<4; k++)
		rotobj[3][k] =0.0f;

	zero_m4(rotup);
	zero_m4(rotside);

	/* compensate front/side.. against opengl x,y,z world definition */
	/* this is "kanonen gegen spatzen", a few plus minus 1 will do here */
	/* i wanted to keep the reason here, so we're rotating*/
	sideangle= (float)M_PI*(sideangledeg + 180.0f)/180.0f;
	rotside[0][0]= (float)cos(sideangle);
	rotside[0][1]= -(float)sin(sideangle);
	rotside[1][0]= (float)sin(sideangle);
	rotside[1][1]= (float)cos(sideangle);
	rotside[2][2]= 1.0f;

	upangle= (float)M_PI*upangledeg/180.0f;
	rotup[1][1]= (float)cos(upangle)/radius;
	rotup[1][2]= -(float)sin(upangle)/radius;
	rotup[2][1]= (float)sin(upangle)/radius;
	rotup[2][2]= (float)cos(upangle)/radius;
	rotup[0][0]= (float)1.0f/radius;

	/* calculate transforms*/
	mul_serie_m4(result, rotup, rotside, viewmatrix, rotobj, NULL, NULL, NULL, NULL);
}

static void uv_map_transform(bContext *C, wmOperator *op, float center[3], float rotmat[4][4])
{
	/* context checks are messy here, making it work in both 3d view and uv editor */
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	View3D *v3d= CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	/* common operator properties */
	int align= RNA_enum_get(op->ptr, "align");
	int direction= RNA_enum_get(op->ptr, "direction");
	float radius= RNA_struct_find_property(op->ptr, "radius")? RNA_float_get(op->ptr, "radius"): 1.0f;
	float upangledeg, sideangledeg;

	uv_map_transform_center(scene, v3d, center, obedit, em);

	if(direction == VIEW_ON_EQUATOR) {
		upangledeg= 90.0f;
		sideangledeg= 0.0f;
	}
	else {
		upangledeg= 0.0f;
		if(align == POLAR_ZY) sideangledeg= 0.0f;
		else sideangledeg= 90.0f;
	}

	/* be compatible to the "old" sphere/cylinder mode */
	if(direction == ALIGN_TO_OBJECT)
		unit_m4(rotmat);
	else 
		uv_map_rotation_matrix(rotmat, rv3d, obedit, upangledeg, sideangledeg, radius);

}

static void uv_transform_properties(wmOperatorType *ot, int radius)
{
	static EnumPropertyItem direction_items[]= {
		{VIEW_ON_EQUATOR, "VIEW_ON_EQUATOR", 0, "View on Equator", "3D view is on the equator"},
		{VIEW_ON_POLES, "VIEW_ON_POLES", 0, "View on Poles", "3D view is on the poles"},
		{ALIGN_TO_OBJECT, "ALIGN_TO_OBJECT", 0, "Align to Object", "Align according to object transform"},
		{0, NULL, 0, NULL, NULL}
	};
	static EnumPropertyItem align_items[]= {
		{POLAR_ZX, "POLAR_ZX", 0, "Polar ZX", "Polar 0 is X"},
		{POLAR_ZY, "POLAR_ZY", 0, "Polar ZY", "Polar 0 is Y"},
		{0, NULL, 0, NULL, NULL}
	};

	RNA_def_enum(ot->srna, "direction", direction_items, VIEW_ON_EQUATOR, "Direction",
	             "Direction of the sphere or cylinder");
	RNA_def_enum(ot->srna, "align", align_items, VIEW_ON_EQUATOR, "Align",
	             "How to determine rotation around the pole");
	if(radius)
		RNA_def_float(ot->srna, "radius", 1.0f, 0.0f, FLT_MAX, "Radius",
		              "Radius of the sphere or cylinder", 0.0001f, 100.0f);
}

static void correct_uv_aspect(BMEditMesh *em)
{
	BMFace *efa= BM_active_face_get(em->bm, TRUE);
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float scale, aspx= 1.0f, aspy=1.0f;
	
	if(efa) {
		tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		ED_image_uv_aspect(tf->tpage, &aspx, &aspy);
	}
	
	if(aspx == aspy)
		return;
		
	if(aspx > aspy) {
		scale= aspy/aspx;

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT) || BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
				continue;
			
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				luv->uv[0] = ((luv->uv[0]-0.5)*scale)+0.5;
			}
		}
	}
	else {
		scale= aspx/aspy;

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)||BM_elem_flag_test(efa, BM_ELEM_HIDDEN))
				continue;
			
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				luv->uv[1] = ((luv->uv[1]-0.5)*scale)+0.5;
			}
		}
	}
}

/******************** Map Clip & Correct ******************/

static void uv_map_clip_correct_properties(wmOperatorType *ot)
{
	RNA_def_boolean(ot->srna, "correct_aspect", 1, "Correct Aspect",
	                "Map UVs taking image aspect ratio into account");
	RNA_def_boolean(ot->srna, "clip_to_bounds", 0, "Clip to Bounds",
	                "Clip UV coordinates to bounds after unwrapping");
	RNA_def_boolean(ot->srna, "scale_to_bounds", 0, "Scale to Bounds",
	                "Scale UV coordinates to bounds after unwrapping");
}

static void uv_map_clip_correct(BMEditMesh *em, wmOperator *op)
{
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	float dx, dy, min[2], max[2];
	int correct_aspect= RNA_boolean_get(op->ptr, "correct_aspect");
	int clip_to_bounds= RNA_boolean_get(op->ptr, "clip_to_bounds");
	int scale_to_bounds= RNA_boolean_get(op->ptr, "scale_to_bounds");

	/* correct for image aspect ratio */
	if(correct_aspect)
		correct_uv_aspect(em);

	if(scale_to_bounds) {
		INIT_MINMAX2(min, max);
		
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				DO_MINMAX2(luv->uv, min, max);
			}
		}
		
		/* rescale UV to be in 1/1 */
		dx= (max[0]-min[0]);
		dy= (max[1]-min[1]);

		if(dx > 0.0f)
			dx= 1.0f/dx;
		if(dy > 0.0f)
			dy= 1.0f/dy;

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				
				luv->uv[0] = (luv->uv[0]-min[0])*dx;
				luv->uv[1] = (luv->uv[1]-min[1])*dy;
			}
		}
	}
	else if(clip_to_bounds) {
		/* clipping and wrapping */
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				CLAMP(luv->uv[0], 0.0f, 1.0f);
				CLAMP(luv->uv[1], 0.0f, 1.0f);
			}
		}
	}
}

/* ******************** Unwrap operator **************** */

/* assumes UV Map is checked, doesn't run update funcs */
void ED_unwrap_lscm(Scene *scene, Object *obedit, const short sel)
{
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	ParamHandle *handle;

	const short fill_holes= scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;
	const short correct_aspect= !(scene->toolsettings->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT);
	const short use_subsurf = scene->toolsettings->uvcalc_flag & UVCALC_USESUBSURF;

	if(use_subsurf)
		handle = construct_param_handle_subsurfed(scene, em, fill_holes, sel, correct_aspect);
	else
		handle= construct_param_handle(scene, em, 0, fill_holes, sel, correct_aspect);

	param_lscm_begin(handle, PARAM_FALSE, scene->toolsettings->unwrapper == 0);
	param_lscm_solve(handle);
	param_lscm_end(handle);

	param_pack(handle, scene->toolsettings->uvcalc_margin);

	param_flush(handle);

	param_delete(handle);
}

static int unwrap_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	int method = RNA_enum_get(op->ptr, "method");
	int fill_holes = RNA_boolean_get(op->ptr, "fill_holes");
	int correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect");
	int use_subsurf = RNA_boolean_get(op->ptr, "use_subsurf_data");
	int subsurf_level = RNA_int_get(op->ptr, "uv_subsurf_level");
	float obsize[3], unitsize[3] = {1.0f, 1.0f, 1.0f};
	short implicit= 0;

	if(!uvedit_have_selection(scene, em, implicit)) {
		return OPERATOR_CANCELLED;
	}
	
	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		return OPERATOR_CANCELLED;
	}

	mat4_to_size(obsize, obedit->obmat);
	if(!compare_v3v3(obsize, unitsize, 1e-4f))
		BKE_report(op->reports, RPT_INFO, "Object scale is not 1.0. Unwrap will operate on a non-scaled version of the mesh.");

	/* remember last method for live unwrap */
	scene->toolsettings->unwrapper = method;
	
	scene->toolsettings->uv_subsurf_level = subsurf_level;

	if(fill_holes)		scene->toolsettings->uvcalc_flag |=  UVCALC_FILLHOLES;
	else				scene->toolsettings->uvcalc_flag &= ~UVCALC_FILLHOLES;

	if(correct_aspect)	scene->toolsettings->uvcalc_flag &= ~UVCALC_NO_ASPECT_CORRECT;
	else				scene->toolsettings->uvcalc_flag |=  UVCALC_NO_ASPECT_CORRECT;

	if(use_subsurf)		scene->toolsettings->uvcalc_flag |= UVCALC_USESUBSURF;
	else				scene->toolsettings->uvcalc_flag &= ~UVCALC_USESUBSURF;

	/* execute unwrap */
	ED_unwrap_lscm(scene, obedit, TRUE);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_unwrap(wmOperatorType *ot)
{
	static EnumPropertyItem method_items[] = {
		{0, "ANGLE_BASED", 0, "Angle Based", ""},
		{1, "CONFORMAL", 0, "Conformal", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Unwrap";
	ot->description= "Unwrap the mesh of the object being edited";
	ot->idname= "UV_OT_unwrap";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= unwrap_exec;
	ot->poll= ED_operator_uvmap;

	/* properties */
	RNA_def_enum(ot->srna, "method", method_items, 0, "Method",
	             "Unwrapping method (Angle Based usually gives better results than Conformal, while being somewhat slower)");
	RNA_def_boolean(ot->srna, "fill_holes", 1, "Fill Holes",
	                "Virtual fill holes in mesh before unwrapping, to better avoid overlaps and preserve symmetry");
	RNA_def_boolean(ot->srna, "correct_aspect", 1, "Correct Aspect",
	                "Map UVs taking image aspect ratio into account");
	RNA_def_boolean(ot->srna, "use_subsurf_data", 0, "Use Subsurf Data",
	                          "Map UVs taking vertex position after subsurf into account");
	RNA_def_int(ot->srna, "uv_subsurf_level", 1, 1, 6, "SubSurf Target",
	                      "Number of times to subdivide before calculating UVs", 1, 6);
}

/**************** Project From View operator **************/
static int uv_from_view_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Camera *camera= NULL;
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d= CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	float rotmat[4][4];

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		return OPERATOR_CANCELLED;
	}

	/* establish the camera object, so we can default to view mapping if anything is wrong with it */
	if ((rv3d->persp==RV3D_CAMOB) && (v3d->camera) && (v3d->camera->type==OB_CAMERA)) {
		camera= v3d->camera->data;
	}

	if(RNA_boolean_get(op->ptr, "orthographic")) {
		uv_map_rotation_matrix(rotmat, rv3d, obedit, 90.0f, 0.0f, 1.0f);
		
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				project_from_view_ortho(luv->uv, l->v->co, rotmat);
			}
		}
	}
	else if (camera) {
		struct UvCameraInfo *uci= project_camera_info(v3d->camera, obedit->obmat, scene->r.xsch, scene->r.ysch);
		
		if(uci) {
			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
					continue;

				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					project_from_camera(luv->uv, l->v->co, uci);
				}
			}
			
			MEM_freeN(uci);
		}
	}
	else {
		copy_m4_m4(rotmat, obedit->obmat);

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				project_from_view(luv->uv, l->v->co, rv3d->persmat, rotmat, ar->winx, ar->winy);
			}
		}
	}

	uv_map_clip_correct(em, op);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

static int uv_from_view_poll(bContext *C)
{
	RegionView3D *rv3d= CTX_wm_region_view3d(C);

	if(!ED_operator_uvmap(C))
		return 0;

	return (rv3d != NULL);
}

void UV_OT_from_view(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Project From View";
	ot->idname= "UV_OT_project_from_view";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= uv_from_view_exec;
	ot->poll= uv_from_view_poll;

	/* properties */
	RNA_def_boolean(ot->srna, "orthographic", 0, "Orthographic", "Use orthographic projection");
	uv_map_clip_correct_properties(ot);
}

/********************** Reset operator ********************/

static int reset_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Mesh *me = (Mesh*)obedit->data;

	/* add uvs if they don't exist yet */
	if (!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		return OPERATOR_CANCELLED;
	}

	if (!ED_mesh_uv_loop_reset(C, me)) {
		return OPERATOR_CANCELLED;
	}

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);
	
	return OPERATOR_FINISHED;
}

void UV_OT_reset(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reset";
	ot->idname= "UV_OT_reset";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= reset_exec;
	ot->poll= ED_operator_uvmap;
}

/****************** Sphere Project operator ***************/

static void uv_sphere_project(float target[2], float source[3], float center[3], float rotmat[4][4])
{
	float pv[3];

	sub_v3_v3v3(pv, source, center);
	mul_m4_v3(rotmat, pv);

	map_to_sphere( &target[0], &target[1],pv[0], pv[1], pv[2]);

	/* split line is always zero */
	if(target[0] >= 1.0f)
		target[0] -= 1.0f;  
}

static void uv_map_mirror(BMEditMesh *em, BMFace *efa, MTexPoly *UNUSED(tf))
{
	BMLoop *l;
	BMIter liter;
	MLoopUV *luv;
	float **uvs = NULL;
	BLI_array_fixedstack_declare(uvs, BM_NGON_STACK_SIZE, efa->len, __func__);
	float dx;
	int i, mi;

	i = 0;
	BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
		luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		uvs[i] = luv->uv;
		i++;
	}

	mi = 0;
	for(i=1; i<efa->len; i++)
		if(uvs[i][0] > uvs[mi][0])
			mi = i;

	for(i=0; i<efa->len; i++) {
		if(i != mi) {
			dx = uvs[mi][0] - uvs[i][0];
			if(dx > 0.5f) uvs[i][0] += 1.0f;
		} 
	} 

	BLI_array_fixedstack_free(uvs);
}

static int sphere_project_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float center[3], rotmat[4][4];

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		return OPERATOR_CANCELLED;
	}

	uv_map_transform(C, op, center, rotmat);

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
			continue;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			uv_sphere_project(luv->uv, l->v->co, center, rotmat);
		}

		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		uv_map_mirror(em, efa, tf);
	}

	uv_map_clip_correct(em, op);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_sphere_project(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Sphere Projection";
	ot->idname= "UV_OT_sphere_project";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= sphere_project_exec;
	ot->poll= ED_operator_uvmap;

	/* properties */
	uv_transform_properties(ot, 0);
	uv_map_clip_correct_properties(ot);
}

/***************** Cylinder Project operator **************/

static void uv_cylinder_project(float target[2], float source[3], float center[3], float rotmat[4][4])
{
	float pv[3];

	sub_v3_v3v3(pv, source, center);
	mul_m4_v3(rotmat, pv);

	map_to_tube( &target[0], &target[1],pv[0], pv[1], pv[2]);

	/* split line is always zero */
	if(target[0] >= 1.0f)
		target[0] -= 1.0f;  
}

static int cylinder_project_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float center[3], rotmat[4][4];

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		return OPERATOR_CANCELLED;
	}

	uv_map_transform(C, op, center, rotmat);

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
			continue;
		
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			uv_cylinder_project(luv->uv, l->v->co, center, rotmat);
		}

		uv_map_mirror(em, efa, tf);
	}

	uv_map_clip_correct(em, op);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_cylinder_project(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Cylinder Projection";
	ot->idname= "UV_OT_cylinder_project";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= cylinder_project_exec;
	ot->poll= ED_operator_uvmap;

	/* properties */
	uv_transform_properties(ot, 1);
	uv_map_clip_correct_properties(ot);
}

/******************* Cube Project operator ****************/

static int cube_project_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	/* MTexPoly *tf; */ /* UNUSED */
	MLoopUV *luv;
	float cube_size, *loc, dx, dy;
	int cox, coy;

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		return OPERATOR_CANCELLED;
	}

	loc= obedit->obmat[3];
	cube_size= RNA_float_get(op->ptr, "cube_size");

	/* choose x,y,z axis for projection depending on the largest normal
	 * component, but clusters all together around the center of map. */

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		int first=1;

		/* tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY); */ /* UNUSED */
		if (!BM_elem_flag_test(efa, BM_ELEM_SELECT))
			continue;

		axis_dominant_v3(&cox, &coy, efa->no);

		dx = dy = 0;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			luv->uv[0] = 0.5f+0.5f*cube_size*(loc[cox] + l->v->co[cox]);
			luv->uv[1] = 0.5f+0.5f*cube_size*(loc[coy] + l->v->co[coy]);
			
			if (first) {
				dx = floor(luv->uv[0]);
				dy = floor(luv->uv[1]);
				first = 0;
			}
			

			luv->uv[0] -= dx;
			luv->uv[1] -= dy;
		}
	}

	uv_map_clip_correct(em, op);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_cube_project(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Cube Projection";
	ot->idname= "UV_OT_cube_project";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= cube_project_exec;
	ot->poll= ED_operator_uvmap;

	/* properties */
	RNA_def_float(ot->srna, "cube_size", 1.0f, 0.0f, FLT_MAX, "Cube Size", "Size of the cube to project on", 0.001f, 100.0f);
	uv_map_clip_correct_properties(ot);
}
