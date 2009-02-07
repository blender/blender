/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"

#include "PIL_time.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "uvedit_intern.h"
#include "uvedit_parametrizer.h"

static void ED_uvedit_create_uvs(EditMesh *em)
{
#if 0
	if (em && em->faces.first)
		EM_add_data_layer(&em->fdata, CD_MTFACE);
	
	if (!ED_uvedit_test(obedit))
		return;
	
	if (G.sima && G.sima->image) /* this is a bit of a kludge, but assume they want the image on their mesh when UVs are added */
		image_changed(G.sima, G.sima->image);
	
	/* select new UV's */
	if ((G.sima==0 || G.sima->flag & SI_SYNC_UVSEL)==0) {
		EditFace *efa;
		MTFace *tf;
		for(efa=em->faces.first; efa; efa=efa->next) {
			tf= (MTFace *)CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			simaFaceSel_Set(efa, tf);
		}
	}
#endif
}

/****************** Parametrizer Conversion ***************/

ParamHandle *construct_param_handle(Scene *scene, EditMesh *em, short implicit, short fill, short sel)
{
	ParamHandle *handle;
	EditFace *efa;
	EditEdge *eed;
	EditVert *ev;
	MTFace *tf;
	int a;
	
	handle = param_construct_begin();

	if((scene->toolsettings->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT)==0) {
		efa = EM_get_actFace(em, 1);

		if (efa) {
			float aspx = 1.0f, aspy= 1.0f;
			// XXX MTFace *tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			// XXX image_final_aspect(tf->tpage, &aspx, &aspy);
			// XXX get_space_image_aspect(sima, &aspx, &aspy);
		
			if (aspx!=aspy)
				param_aspect_ratio(handle, aspx, aspy);
		}
	}
	
	/* we need the vert indicies */
	for (ev= em->verts.first, a=0; ev; ev= ev->next, a++)
		ev->tmp.l = a;
	
	for (efa= em->faces.first; efa; efa= efa->next) {
		ParamKey key, vkeys[4];
		ParamBool pin[4], select[4];
		float *co[4];
		float *uv[4];
		int nverts;
		
		if ((efa->h) || (sel && (efa->f & SELECT)==0)) 
			continue;

		tf= (MTFace *)CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		
		if (implicit &&
			!(	uvedit_uv_selected(scene, efa, tf, 0) ||
				uvedit_uv_selected(scene, efa, tf, 1) ||
				uvedit_uv_selected(scene, efa, tf, 2) ||
				(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3)) )
		) {
			continue;
		}

		key = (ParamKey)efa;
		vkeys[0] = (ParamKey)efa->v1->tmp.l;
		vkeys[1] = (ParamKey)efa->v2->tmp.l;
		vkeys[2] = (ParamKey)efa->v3->tmp.l;

		co[0] = efa->v1->co;
		co[1] = efa->v2->co;
		co[2] = efa->v3->co;

		uv[0] = tf->uv[0];
		uv[1] = tf->uv[1];
		uv[2] = tf->uv[2];

		pin[0] = ((tf->unwrap & TF_PIN1) != 0);
		pin[1] = ((tf->unwrap & TF_PIN2) != 0);
		pin[2] = ((tf->unwrap & TF_PIN3) != 0);

		select[0] = ((uvedit_uv_selected(scene, efa, tf, 0)) != 0);
		select[1] = ((uvedit_uv_selected(scene, efa, tf, 1)) != 0);
		select[2] = ((uvedit_uv_selected(scene, efa, tf, 2)) != 0);

		if (efa->v4) {
			vkeys[3] = (ParamKey)efa->v4->tmp.l;
			co[3] = efa->v4->co;
			uv[3] = tf->uv[3];
			pin[3] = ((tf->unwrap & TF_PIN4) != 0);
			select[3] = (uvedit_uv_selected(scene, efa, tf, 3) != 0);
			nverts = 4;
		}
		else
			nverts = 3;

		param_face_add(handle, key, nverts, vkeys, co, uv, pin, select);
	}

	if (!implicit) {
		for (eed= em->edges.first; eed; eed= eed->next) {
			if(eed->seam) {
				ParamKey vkeys[2];
				vkeys[0] = (ParamKey)eed->v1->tmp.l;
				vkeys[1] = (ParamKey)eed->v2->tmp.l;
				param_edge_set_seam(handle, vkeys);
			}
		}
	}

	param_construct_end(handle, fill, implicit);

	return handle;
}

/* ******************** unwrap operator **************** */

static int unwrap_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	ParamHandle *handle;
	int method = RNA_enum_get(op->ptr, "method");
	int fill_holes = RNA_boolean_get(op->ptr, "fill_holes");
	
	/* add uvs if they don't exist yet */
	if(!ED_uvedit_test(obedit))
		ED_uvedit_create_uvs(em);

	handle= construct_param_handle(scene, em, 0, fill_holes, 0);

	param_lscm_begin(handle, PARAM_FALSE, method == 0);
	param_lscm_solve(handle);
	param_lscm_end(handle);
	
	param_pack(handle);

	param_flush(handle);

	param_delete(handle);

	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_DATA, obedit);

	return OPERATOR_FINISHED;
}

void UV_OT_unwrap(wmOperatorType *ot)
{
	static EnumPropertyItem method_items[] = {
		{0, "ANGLE_BASED", "Angle Based", ""},
		{1, "CONFORMAL", "Conformal", ""},
		{0, NULL, NULL, NULL}};

	/* identifiers */
	ot->name= "Unwrap";
	ot->idname= "UV_OT_unwrap";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= unwrap_exec;
	ot->poll= ED_operator_uvmap;

	/* properties */
	RNA_def_enum(ot->srna, "method", method_items, 0, "Method", "Unwrapping method. Angle Based usually gives better results than Conformal, while being somewhat slower.");
	RNA_def_boolean(ot->srna, "fill_holes", 1, "Fill Holes", "Virtual fill holes in mesh before unwrapping, to better avoid overlaps and preserve symmetry.");
}

/* ******************** minimize stretch operator **************** */

typedef struct MinStretch {
	Scene *scene;
	Object *obedit;
	EditMesh *em;
	ParamHandle *handle;
	float blend;
	double lasttime;
	int i, iterations;
	wmTimer *timer;
} MinStretch;

static void minimize_stretch_init(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	MinStretch *ms;
	int fill_holes= RNA_boolean_get(op->ptr, "fill_holes");

	ms= MEM_callocN(sizeof(MinStretch), "MinStretch");
	ms->scene= scene;
	ms->obedit= obedit;
	ms->em= em;
	ms->blend= RNA_float_get(op->ptr, "blend");
	ms->iterations= RNA_int_get(op->ptr, "iterations");
	ms->handle= construct_param_handle(scene, em, 1, fill_holes, 1);
	ms->lasttime= PIL_check_seconds_timer();

	param_stretch_begin(ms->handle);
	if(ms->blend != 0.0f)
		param_stretch_blend(ms->handle, ms->blend);

	op->customdata= ms;
}

static void minimize_stretch_iteration(bContext *C, wmOperator *op, int interactive)
{
	MinStretch *ms= op->customdata;
	ScrArea *sa= CTX_wm_area(C);

	param_stretch_blend(ms->handle, ms->blend);
	param_stretch_iter(ms->handle);

	if(interactive && (PIL_check_seconds_timer() - ms->lasttime > 0.5)) {
		char str[100];

		param_flush(ms->handle);

		if(sa) {
			sprintf(str, "Minimize Stretch. Blend %.2f.", ms->blend);
			ED_area_headerprint(sa, str);
		}

		ms->lasttime = PIL_check_seconds_timer();

		DAG_object_flush_update(ms->scene, ms->obedit, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_DATA, ms->obedit);
	}
}

static void minimize_stretch_exit(bContext *C, wmOperator *op, int cancel)
{
	MinStretch *ms= op->customdata;
	ScrArea *sa= CTX_wm_area(C);

	if(sa)
		ED_area_headerprint(sa, NULL);
	if(ms->timer)
		WM_event_remove_window_timer(CTX_wm_window(C), ms->timer);

	if(cancel)
		param_flush_restore(ms->handle);
	else
		param_flush(ms->handle);

	param_stretch_end(ms->handle);
	param_delete(ms->handle);

	DAG_object_flush_update(ms->scene, ms->obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_DATA, ms->obedit);

	MEM_freeN(ms);
	op->customdata= NULL;
}

static int minimize_stretch_exec(bContext *C, wmOperator *op)
{
	int i, iterations;

	minimize_stretch_init(C, op);

	iterations= RNA_int_get(op->ptr, "iterations");
	for(i=0; i<iterations; i++)
		minimize_stretch_iteration(C, op, 0);
	minimize_stretch_exit(C, op, 0);

	return OPERATOR_FINISHED;
}

static int minimize_stretch_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	MinStretch *ms;

	minimize_stretch_init(C, op);
	minimize_stretch_iteration(C, op, 1);

	ms= op->customdata;
	WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);
	ms->timer= WM_event_add_window_timer(CTX_wm_window(C), TIMER, 0.01f);

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
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= minimize_stretch_exec;
	ot->invoke= minimize_stretch_invoke;
	ot->modal= minimize_stretch_modal;
	ot->cancel= minimize_stretch_cancel;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "fill_holes", 1, "Fill Holes", "Virtual fill holes in mesh before unwrapping, to better avoid overlaps and preserve symmetry.");
	RNA_def_float(ot->srna, "blend", 0.0f, 0.0f, 1.0f, "Blend", "Blend factor between stretch minimized and original.", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "iterations", 0, 0, INT_MAX, "Iterations", "Number of iterations to run, 0 is unlimited when run interactively.", 0, 100);
}

/* ******************** pack islands operator **************** */

static int pack_islands_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	ParamHandle *handle;

	handle = construct_param_handle(scene, em, 1, 0, 1);
	param_pack(handle);
	param_flush(handle);
	param_delete(handle);
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_DATA, obedit);

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
}

/* ******************** average islands scale operator **************** */

static int average_islands_scale_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	ParamHandle *handle;

	handle= construct_param_handle(scene, em, 1, 0, 1);
	param_average(handle);
	param_flush(handle);
	param_delete(handle);
	
	DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_DATA, obedit);

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
	EditMesh *em= ((Mesh*)obedit->data)->edit_mesh;
	short abf = scene->toolsettings->unwrapper == 1;
	short fillholes = scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;

	if(!ED_uvedit_test(obedit)) return;

	liveHandle = construct_param_handle(scene, em, 0, fillholes, 1);

	param_lscm_begin(liveHandle, PARAM_TRUE, abf);
}

void ED_uvedit_live_unwrap_re_solve(void)
{
	if (liveHandle) {
		param_lscm_solve(liveHandle);
		param_flush(liveHandle);
	}
}
	
void ED_uvedit_live_unwrap_end(short cancel)
{
	if (liveHandle) {
		param_lscm_end(liveHandle);
		if (cancel)
			param_flush_restore(liveHandle);
		param_delete(liveHandle);
		liveHandle = NULL;
	}
}

