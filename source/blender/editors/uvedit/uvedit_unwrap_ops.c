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

#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_uvproject.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_mesh.h"

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
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	Image *ima;
	bScreen *sc;
	ScrArea *sa;
	SpaceLink *slink;
	SpaceImage *sima;

	if(ED_uvedit_test(obedit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return 1;
	}

	if(em && em->faces.first)
		EM_add_data_layer(em, &em->fdata, CD_MTFACE, NULL);
	
	if(!ED_uvedit_test(obedit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
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
	for(efa=em->faces.first; efa; efa=efa->next) {
		tf= (MTFace *)CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		uvedit_face_select(scene, efa, tf);
	}

	BKE_mesh_end_editmesh(obedit->data, em);
	return 1;
}

/****************** Parametrizer Conversion ***************/

static int uvedit_have_selection(Scene *scene, EditMesh *em, short implicit)
{
	EditFace *efa;
	MTFace *tf;

	/* verify if we have any selected uv's before unwrapping,
	   so we can cancel the operator early */
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(scene->toolsettings->uv_flag & UV_SYNC_SELECTION) {
			if(efa->h)
				continue;
		}
		else if((efa->h) || ((efa->f & SELECT)==0))
			continue;

		tf= (MTFace *)CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(!tf)
			return 1; /* default selected if doesn't exists */
		
		if(implicit &&
			!(	uvedit_uv_selected(scene, efa, tf, 0) ||
				uvedit_uv_selected(scene, efa, tf, 1) ||
				uvedit_uv_selected(scene, efa, tf, 2) ||
				(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3)) )
		) {
			continue;
		}

		return 1;
	}

	return 0;
}

static ParamHandle *construct_param_handle(Scene *scene, EditMesh *em, short implicit,
                                           short fill, short sel, short correct_aspect)
{
	ParamHandle *handle;
	EditFace *efa;
	EditEdge *eed;
	EditVert *ev;
	MTFace *tf;
	int a;
	
	handle = param_construct_begin();

	if(correct_aspect) {
		efa = EM_get_actFace(em, 1);

		if(efa) {
			float aspx, aspy;
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			ED_image_uv_aspect(tf->tpage, &aspx, &aspy);
		
			if(aspx!=aspy)
				param_aspect_ratio(handle, aspx, aspy);
		}
	}
	
	/* we need the vert indices */
	for(ev= em->verts.first, a=0; ev; ev= ev->next, a++)
		ev->tmp.l = a;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		ParamKey key, vkeys[4];
		ParamBool pin[4], select[4];
		float *co[4];
		float *uv[4];
		int nverts;
		
		if((efa->h) || (sel && (efa->f & SELECT)==0))
			continue;

		tf= (MTFace *)CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		
		if(implicit &&
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

		if(efa->v4) {
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

	if(!implicit) {
		for(eed= em->edges.first; eed; eed= eed->next) {
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

/* ******************** Minimize Stretch operator **************** */

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

static int minimize_stretch_init(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	MinStretch *ms;
	int fill_holes= RNA_boolean_get(op->ptr, "fill_holes");
	short implicit= 1;

	if(!uvedit_have_selection(scene, em, implicit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
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
			sprintf(str, "Minimize Stretch. Blend %.2f", ms->blend);
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
	RNA_def_boolean(ot->srna, "fill_holes", 1, "Fill Holes",
	                "Virtual fill holes in mesh before unwrapping, to better avoid overlaps and preserve symmetry");
	RNA_def_float_factor(ot->srna, "blend", 0.0f, 0.0f, 1.0f, "Blend",
	                     "Blend factor between stretch minimized and original", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "iterations", 0, 0, INT_MAX, "Iterations",
	            "Number of iterations to run, 0 is unlimited when run interactively", 0, 100);
}

/* ******************** Pack Islands operator **************** */

static int pack_islands_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ParamHandle *handle;
	short implicit= 1;

	if(!uvedit_have_selection(scene, em, implicit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	if(RNA_property_is_set(op->ptr, "margin")) {
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

	BKE_mesh_end_editmesh(obedit->data, em);
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
	RNA_def_float_factor(ot->srna, "margin", 0.0f, 0.0f, 1.0f, "Margin",
	                     "Space between islands", 0.0f, 1.0f);
}

/* ******************** Average Islands Scale operator **************** */

static int average_islands_scale_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ParamHandle *handle;
	short implicit= 1;

	if(!uvedit_have_selection(scene, em, implicit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	handle= construct_param_handle(scene, em, implicit, 0, 1, 1);
	param_average(handle);
	param_flush(handle);
	param_delete(handle);
	
	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
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
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	short abf = scene->toolsettings->unwrapper == 0;
	short fillholes = scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;

	if(!ED_uvedit_test(obedit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return;
	}

	liveHandle = construct_param_handle(scene, em, 0, fillholes, 0, 1);

	param_lscm_begin(liveHandle, PARAM_TRUE, abf);
	BKE_mesh_end_editmesh(obedit->data, em);
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

static void uv_map_transform_center(Scene *scene, View3D *v3d, float *result, Object *ob, EditMesh *em)
{
	EditFace *efa;
	float min[3], max[3], *cursx;
	int around= (v3d)? v3d->around: V3D_CENTER;

	/* only operates on the edit object - this is all that's needed now */

	switch(around)  {
		case V3D_CENTER: /* bounding box center */
			min[0]= min[1]= min[2]= 1e20f;
			max[0]= max[1]= max[2]= -1e20f; 

			for(efa= em->faces.first; efa; efa= efa->next) {
				if(efa->f & SELECT) {
					DO_MINMAX(efa->v1->co, min, max);
					DO_MINMAX(efa->v2->co, min, max);
					DO_MINMAX(efa->v3->co, min, max);
					if(efa->v4) DO_MINMAX(efa->v4->co, min, max);
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
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
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

	BKE_mesh_end_editmesh(obedit->data, em);
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

static void correct_uv_aspect(EditMesh *em)
{
	EditFace *efa= EM_get_actFace(em, 1);
	MTFace *tf;
	float scale, aspx= 1.0f, aspy=1.0f;
	
	if(efa) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		ED_image_uv_aspect(tf->tpage, &aspx, &aspy);
	}
	
	if(aspx == aspy)
		return;
		
	if(aspx > aspy) {
		scale= aspy/aspx;

		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				tf->uv[0][0]= ((tf->uv[0][0]-0.5f)*scale)+0.5f;
				tf->uv[1][0]= ((tf->uv[1][0]-0.5f)*scale)+0.5f;
				tf->uv[2][0]= ((tf->uv[2][0]-0.5f)*scale)+0.5f;
				if(efa->v4)
					tf->uv[3][0]= ((tf->uv[3][0]-0.5f)*scale)+0.5f;
			}
		}
	}
	else {
		scale= aspx/aspy;

		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				tf->uv[0][1]= ((tf->uv[0][1]-0.5f)*scale)+0.5f;
				tf->uv[1][1]= ((tf->uv[1][1]-0.5f)*scale)+0.5f;
				tf->uv[2][1]= ((tf->uv[2][1]-0.5f)*scale)+0.5f;
				if(efa->v4)
					tf->uv[3][1]= ((tf->uv[3][1]-0.5f)*scale)+0.5f;
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

static void uv_map_clip_correct(EditMesh *em, wmOperator *op)
{
	EditFace *efa;
	MTFace *tf;
	float dx, dy, min[2], max[2];
	int b, nverts;
	int correct_aspect= RNA_boolean_get(op->ptr, "correct_aspect");
	int clip_to_bounds= RNA_boolean_get(op->ptr, "clip_to_bounds");
	int scale_to_bounds= RNA_boolean_get(op->ptr, "scale_to_bounds");

	/* correct for image aspect ratio */
	if(correct_aspect)
		correct_uv_aspect(em);

	if(scale_to_bounds) {
		INIT_MINMAX2(min, max);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				DO_MINMAX2(tf->uv[0], min, max);
				DO_MINMAX2(tf->uv[1], min, max);
				DO_MINMAX2(tf->uv[2], min, max);

				if(efa->v4)
					DO_MINMAX2(tf->uv[3], min, max);
			}
		}
		
		/* rescale UV to be in 1/1 */
		dx= (max[0]-min[0]);
		dy= (max[1]-min[1]);

		if(dx > 0.0f)
			dx= 1.0f/dx;
		if(dy > 0.0f)
			dy= 1.0f/dy;

		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				nverts= (efa->v4)? 4: 3;

				for(b=0; b<nverts; b++) {
					tf->uv[b][0]= (tf->uv[b][0]-min[0])*dx;
					tf->uv[b][1]= (tf->uv[b][1]-min[1])*dy;
				}
			}
		}
	}
	else if(clip_to_bounds) {
		/* clipping and wrapping */
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			
				nverts= (efa->v4)? 4: 3;

				for(b=0; b<nverts; b++) {
					CLAMP(tf->uv[b][0], 0.0f, 1.0f);
					CLAMP(tf->uv[b][1], 0.0f, 1.0f);
				}
			}
		}
	}
}

/* ******************** Unwrap operator **************** */

/* assumes UV Map is checked, doesn't run update funcs */
void ED_unwrap_lscm(Scene *scene, Object *obedit, const short sel)
{
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ParamHandle *handle;

	const short fill_holes= scene->toolsettings->uvcalc_flag & UVCALC_FILLHOLES;
	const short correct_aspect= !(scene->toolsettings->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT);
	short implicit= 0;

	handle= construct_param_handle(scene, em, implicit, fill_holes, sel, correct_aspect);

	param_lscm_begin(handle, PARAM_FALSE, scene->toolsettings->unwrapper == 0);
	param_lscm_solve(handle);
	param_lscm_end(handle);

	param_pack(handle, scene->toolsettings->uvcalc_margin);

	param_flush(handle);

	param_delete(handle);

	BKE_mesh_end_editmesh(obedit->data, em);
}

static int unwrap_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	int method = RNA_enum_get(op->ptr, "method");
	int fill_holes = RNA_boolean_get(op->ptr, "fill_holes");
	int correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect");
	short implicit= 0;

	if(!uvedit_have_selection(scene, em, implicit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return 0;
	}

	BKE_mesh_end_editmesh(obedit->data, em);
	
	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		return OPERATOR_CANCELLED;
	}

	/* remember last method for live unwrap */
	scene->toolsettings->unwrapper = method;

	if(fill_holes)		scene->toolsettings->uvcalc_flag |=  UVCALC_FILLHOLES;
	else				scene->toolsettings->uvcalc_flag &= ~UVCALC_FILLHOLES;

	if(correct_aspect)	scene->toolsettings->uvcalc_flag &= ~UVCALC_NO_ASPECT_CORRECT;
	else				scene->toolsettings->uvcalc_flag |=  UVCALC_NO_ASPECT_CORRECT;

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
}

/**************** Project From View operator **************/
static int uv_from_view_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Camera *camera= NULL;
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ARegion *ar= CTX_wm_region(C);
	View3D *v3d= CTX_wm_view3d(C);
	RegionView3D *rv3d= ar->regiondata;
	EditFace *efa;
	MTFace *tf;
	float rotmat[4][4];

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	/* establish the camera object, so we can default to view mapping if anything is wrong with it */
	if ((rv3d->persp==RV3D_CAMOB) && (v3d->camera) && (v3d->camera->type==OB_CAMERA)) {
		camera= v3d->camera->data;
	}

	if(RNA_boolean_get(op->ptr, "orthographic")) {
		uv_map_rotation_matrix(rotmat, ar->regiondata, obedit, 90.0f, 0.0f, 1.0f);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				project_from_view_ortho(tf->uv[0], efa->v1->co, rotmat);
				project_from_view_ortho(tf->uv[1], efa->v2->co, rotmat);
				project_from_view_ortho(tf->uv[2], efa->v3->co, rotmat);
				if(efa->v4)
					project_from_view_ortho(tf->uv[3], efa->v4->co, rotmat);
			}
		}
	}
	else if (camera) {
		struct UvCameraInfo *uci= project_camera_info(v3d->camera, obedit->obmat, scene->r.xsch, scene->r.ysch);
		
		if(uci) {
			for(efa= em->faces.first; efa; efa= efa->next) {
				if(efa->f & SELECT) {
					tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

					project_from_camera(tf->uv[0], efa->v1->co, uci);
					project_from_camera(tf->uv[1], efa->v2->co, uci);
					project_from_camera(tf->uv[2], efa->v3->co, uci);
					if(efa->v4)
						project_from_camera(tf->uv[3], efa->v4->co, uci);
				}
			}
			
			MEM_freeN(uci);
		}
	}
	else {
		copy_m4_m4(rotmat, obedit->obmat);

		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				project_from_view(tf->uv[0], efa->v1->co, rv3d->persmat, rotmat, ar->winx, ar->winy);
				project_from_view(tf->uv[1], efa->v2->co, rv3d->persmat, rotmat, ar->winx, ar->winy);
				project_from_view(tf->uv[2], efa->v3->co, rv3d->persmat, rotmat, ar->winx, ar->winy);
				if(efa->v4)
					project_from_view(tf->uv[3], efa->v4->co, rv3d->persmat, rotmat, ar->winx, ar->winy);
			}
		}
	}

	uv_map_clip_correct(em, op);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
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
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->f & SELECT) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			tf->uv[0][0]= 0.0f;
			tf->uv[0][1]= 0.0f;
			
			tf->uv[1][0]= 1.0f;
			tf->uv[1][1]= 0.0f;
			
			tf->uv[2][0]= 1.0f;
			tf->uv[2][1]= 1.0f;
			
			tf->uv[3][0]= 0.0f;
			tf->uv[3][1]= 1.0f;
		}
	}

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
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

static void uv_map_mirror(EditFace *efa, MTFace *tf)
{
	float dx;
	int nverts, i, mi;

	nverts= (efa->v4)? 4: 3;

	mi = 0;
	for(i=1; i<nverts; i++)
		if(tf->uv[i][0] > tf->uv[mi][0])
			mi = i;

	for(i=0; i<nverts; i++) {
		if(i != mi) {
			dx = tf->uv[mi][0] - tf->uv[i][0];
			if(dx > 0.5f) tf->uv[i][0] += 1.0f;
		} 
	} 
}

static int sphere_project_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	float center[3], rotmat[4][4];

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	uv_map_transform(C, op, center, rotmat);

	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->f & SELECT) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			uv_sphere_project(tf->uv[0], efa->v1->co, center, rotmat);
			uv_sphere_project(tf->uv[1], efa->v2->co, center, rotmat);
			uv_sphere_project(tf->uv[2], efa->v3->co, center, rotmat);
			if(efa->v4)
				uv_sphere_project(tf->uv[3], efa->v4->co, center, rotmat);

			uv_map_mirror(efa, tf);
		}
	}

	uv_map_clip_correct(em, op);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
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
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	float center[3], rotmat[4][4];

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	uv_map_transform(C, op, center, rotmat);

	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->f & SELECT) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			uv_cylinder_project(tf->uv[0], efa->v1->co, center, rotmat);
			uv_cylinder_project(tf->uv[1], efa->v2->co, center, rotmat);
			uv_cylinder_project(tf->uv[2], efa->v3->co, center, rotmat);
			if(efa->v4)
				uv_cylinder_project(tf->uv[3], efa->v4->co, center, rotmat);

			uv_map_mirror(efa, tf);
		}
	}

	uv_map_clip_correct(em, op);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
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
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	float no[3], cube_size, *loc, dx, dy;
	int cox, coy;

	/* add uvs if they don't exist yet */
	if(!ED_uvedit_ensure_uvs(C, scene, obedit)) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	loc= obedit->obmat[3];
	cube_size= RNA_float_get(op->ptr, "cube_size");

	/* choose x,y,z axis for projection depending on the largest normal
	 * component, but clusters all together around the center of map. */

	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->f & SELECT) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			normal_tri_v3( no,efa->v1->co, efa->v2->co, efa->v3->co);

			axis_dominant_v3(&cox, &coy, no);

			tf->uv[0][0]= 0.5f+0.5f*cube_size*(loc[cox] + efa->v1->co[cox]);
			tf->uv[0][1]= 0.5f+0.5f*cube_size*(loc[coy] + efa->v1->co[coy]);
			dx = floor(tf->uv[0][0]);
			dy = floor(tf->uv[0][1]);
			tf->uv[0][0] -= dx;
			tf->uv[0][1] -= dy;
			tf->uv[1][0]= 0.5f+0.5f*cube_size*(loc[cox] + efa->v2->co[cox]);
			tf->uv[1][1]= 0.5f+0.5f*cube_size*(loc[coy] + efa->v2->co[coy]);
			tf->uv[1][0] -= dx;
			tf->uv[1][1] -= dy;
			tf->uv[2][0]= 0.5f+0.5f*cube_size*(loc[cox] + efa->v3->co[cox]);
			tf->uv[2][1]= 0.5f+0.5f*cube_size*(loc[coy] + efa->v3->co[coy]);
			tf->uv[2][0] -= dx;
			tf->uv[2][1] -= dy;

			if(efa->v4) {
				tf->uv[3][0]= 0.5f+0.5f*cube_size*(loc[cox] + efa->v4->co[cox]);
				tf->uv[3][1]= 0.5f+0.5f*cube_size*(loc[coy] + efa->v4->co[coy]);
				tf->uv[3][0] -= dx;
				tf->uv[3][1] -= dy;
			}
		}
	}

	uv_map_clip_correct(em, op);

	DAG_id_tag_update(obedit->data, 0);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
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
	RNA_def_float(ot->srna, "cube_size", 1.0f, 0.0f, FLT_MAX, "Cube Size",
	              "Size of the cube to project on", 0.001f, 100.0f);
	uv_map_clip_correct_properties(ot);
}
