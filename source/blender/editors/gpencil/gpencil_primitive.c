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
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Operators for creating new Grease Pencil primitives (boxes, circles, ...)
 */

/** \file blender/editors/gpencil/gpencil_primitive.c
 *  \ingroup edgpencil
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_main.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_gpencil.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_space_api.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

#define MIN_EDGES 2
#define MAX_EDGES 100

#define IDLE 0
#define IN_PROGRESS 1

/* ************************************************ */
/* Core/Shared Utilities */

/* Poll callback for primitive operators */
static bool gpencil_primitive_add_poll(bContext *C)
{
	/* only 3D view */
	ScrArea *sa = CTX_wm_area(C);
	if (sa && sa->spacetype != SPACE_VIEW3D) {
		return 0;
	}

	/* need data to create primitive */
	bGPdata *gpd = CTX_data_gpencil_data(C);
	if (gpd == NULL) {
		return 0;
	}

	/* only in edit and paint modes
	 * - paint as it's the "drawing/creation mode"
	 * - edit as this is more of an atomic editing operation
	 *   (similar to copy/paste), and also for consistency
	 */
	if ((gpd->flag & (GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE)) == 0) {
		CTX_wm_operator_poll_msg_set(C, "Primitives can only be added in Draw or Edit modes");
		return 0;
	}

	/* don't allow operator to function if the active layer is locked/hidden
	 * (BUT, if there isn't an active layer, we are free to add new layer when the time comes)
	 */
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	if ((gpl) && (gpl->flag & (GP_LAYER_LOCKED | GP_LAYER_HIDE))) {
		CTX_wm_operator_poll_msg_set(C, "Primitives cannot be added as active layer is locked or hidden");
		return 0;
	}

	return 1;
}


/* ****************** Primitive Interactive *********************** */

/* Helper: Create internal strokes primitives data */
static void gp_primitive_set_initdata(bContext *C, tGPDprimitive *tgpi)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	int cfra_eval = (int)DEG_get_ctime(depsgraph);

	bGPDlayer *gpl = CTX_data_active_gpencil_layer(C);
	Brush *brush;

	/* if brush doesn't exist, create a new one */
	Paint *paint = BKE_brush_get_gpencil_paint(ts);
	/* if not exist, create a new one */
	if (paint->brush == NULL) {
		/* create new brushes */
		BKE_brush_gpencil_presets(C);
		brush = BKE_brush_getactive_gpencil(ts);
	}
	else {
		/* Use the current */
		brush = BKE_brush_getactive_gpencil(ts);
	}
	tgpi->brush = brush;

	/* if layer doesn't exist, create a new one */
	if (gpl == NULL) {
		gpl = BKE_gpencil_layer_addnew(tgpi->gpd, DATA_("Primitives"), true);
	}
	tgpi->gpl = gpl;

	/* create a new temporary frame */
	tgpi->gpf = MEM_callocN(sizeof(bGPDframe), "Temp bGPDframe");
	tgpi->gpf->framenum = tgpi->cframe = cfra_eval;

	/* create new temp stroke */
	bGPDstroke *gps = MEM_callocN(sizeof(bGPDstroke), "Temp bGPDstroke");
	gps->thickness = 2.0f;
	gps->inittime = 0.0f;

	/* enable recalculation flag by default */
	gps->flag |= GP_STROKE_RECALC_CACHES;
	/* the polygon must be closed, so enabled cyclic */
	gps->flag |= GP_STROKE_CYCLIC;
	gps->flag |= GP_STROKE_3DSPACE;

	gps->mat_nr = BKE_gpencil_get_material_index(tgpi->ob, tgpi->mat) - 1;

	/* allocate memory for storage points, but keep empty */
	gps->totpoints = 0;
	gps->points = MEM_callocN(sizeof(bGPDspoint), "gp_stroke_points");
	gps->dvert = MEM_callocN(sizeof(MDeformVert), "gp_stroke_weights");
	/* initialize triangle memory to dummy data */
	gps->tot_triangles = 0;
	gps->triangles = NULL;
	gps->flag |= GP_STROKE_RECALC_CACHES;

	/* add to strokes */
	BLI_addtail(&tgpi->gpf->strokes, gps);
}

/* ----------------------- */
/* Drawing Callbacks */

/* Drawing callback for modal operator in 3d mode */
static void gpencil_primitive_draw_3d(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
	tGPDprimitive *tgpi = (tGPDprimitive *)arg;
	ED_gp_draw_primitives(C, tgpi, REGION_DRAW_POST_VIEW);
}

/* ----------------------- */

/* Helper: Draw status message while the user is running the operator */
static void gpencil_primitive_status_indicators(bContext *C, tGPDprimitive *tgpi)
{
	Scene *scene = tgpi->scene;
	char status_str[UI_MAX_DRAW_STR];
	char msg_str[UI_MAX_DRAW_STR];

	if (tgpi->type == GP_STROKE_BOX) {
		BLI_strncpy(msg_str, IFACE_("Rectangle: ESC/RMB to cancel, LMB set origin, Enter/LMB to confirm, Shift to square"), UI_MAX_DRAW_STR);
	}
	else if (tgpi->type == GP_STROKE_LINE) {
		BLI_strncpy(msg_str, IFACE_("Line: ESC/RMB to cancel, LMB set origin, Enter/LMB to confirm"), UI_MAX_DRAW_STR);
	}
	else {
		BLI_strncpy(msg_str, IFACE_("Circle: ESC/RMB to cancel, Enter/LMB to confirm, WHEEL to adjust edge number, Shift to square"), UI_MAX_DRAW_STR);
	}

	if (tgpi->type == GP_STROKE_CIRCLE) {
		if (hasNumInput(&tgpi->num)) {
			char str_offs[NUM_STR_REP_LEN];

			outputNumInput(&tgpi->num, str_offs, &scene->unit);
			BLI_snprintf(status_str, sizeof(status_str), "%s: %s", msg_str, str_offs);
		}
		else {
			if (tgpi->flag == IN_PROGRESS) {
				BLI_snprintf(
				        status_str, sizeof(status_str), "%s: %d (%d, %d) (%d, %d)", msg_str, (int)tgpi->tot_edges,
				        tgpi->top[0], tgpi->top[1], tgpi->bottom[0], tgpi->bottom[1]);
			}
			else {
				BLI_snprintf(
				        status_str, sizeof(status_str), "%s: %d (%d, %d)", msg_str, (int)tgpi->tot_edges,
				        tgpi->bottom[0], tgpi->bottom[1]);
			}
		}
	}
	else {
		if (tgpi->flag == IN_PROGRESS) {
			BLI_snprintf(
			        status_str, sizeof(status_str), "%s: (%d, %d) (%d, %d)", msg_str,
			        tgpi->top[0], tgpi->top[1], tgpi->bottom[0], tgpi->bottom[1]);
		}
		else {
			BLI_snprintf(
			        status_str, sizeof(status_str), "%s: (%d, %d)", msg_str,
			        tgpi->bottom[0], tgpi->bottom[1]);
		}
	}
	ED_workspace_status_text(C, status_str);
}

/* ----------------------- */

/* create a rectangle */
static void gp_primitive_rectangle(tGPDprimitive *tgpi, tGPspoint *points2D)
{
	BLI_assert(tgpi->tot_edges == 4);

	points2D[0].x = tgpi->top[0];
	points2D[0].y = tgpi->top[1];

	points2D[1].x = tgpi->bottom[0];
	points2D[1].y = tgpi->top[1];

	points2D[2].x = tgpi->bottom[0];
	points2D[2].y = tgpi->bottom[1];

	points2D[3].x = tgpi->top[0];
	points2D[3].y = tgpi->bottom[1];
}

/* create a line */
static void gp_primitive_line(tGPDprimitive *tgpi, tGPspoint *points2D)
{
	BLI_assert(tgpi->tot_edges == 2);

	points2D[0].x = tgpi->top[0];
	points2D[0].y = tgpi->top[1];

	points2D[1].x = tgpi->bottom[0];
	points2D[1].y = tgpi->bottom[1];
}

/* create a circle */
static void gp_primitive_circle(tGPDprimitive *tgpi, tGPspoint *points2D)
{
	const int totpoints = tgpi->tot_edges;
	const float step = (2.0f * M_PI) / (float)(totpoints);
	float center[2];
	float radius[2];
	float a = 0.0f;

	/* TODO: Use math-lib functions for these? */
	center[0] = tgpi->top[0] + ((tgpi->bottom[0] - tgpi->top[0]) / 2.0f);
	center[1] = tgpi->top[1] + ((tgpi->bottom[1] - tgpi->top[1]) / 2.0f);
	radius[0] = fabsf(((tgpi->bottom[0] - tgpi->top[0]) / 2.0f));
	radius[1] = fabsf(((tgpi->bottom[1] - tgpi->top[1]) / 2.0f));

	for (int i = 0; i < totpoints; i++) {
		tGPspoint *p2d = &points2D[i];

		p2d->x = (int)(center[0] + cosf(a) * radius[0]);
		p2d->y = (int)(center[1] + sinf(a) * radius[1]);
		a += step;
	}
}

/* Helper: Update shape of the stroke */
static void gp_primitive_update_strokes(bContext *C, tGPDprimitive *tgpi)
{
	ToolSettings *ts = tgpi->scene->toolsettings;
	bGPdata *gpd = tgpi->gpd;
	bGPDstroke *gps = tgpi->gpf->strokes.first;

	/* realloc points to new size */
	/* TODO: only do this if the size has changed? */
	gps->points = MEM_reallocN(gps->points, sizeof(bGPDspoint) * tgpi->tot_edges);
	gps->dvert = MEM_reallocN(gps->dvert, sizeof(MDeformVert) * tgpi->tot_edges);
	gps->totpoints = tgpi->tot_edges;

	/* compute screen-space coordinates for points */
	tGPspoint *points2D = MEM_callocN(sizeof(tGPspoint) * tgpi->tot_edges, "gp primitive points2D");
	switch (tgpi->type) {
		case GP_STROKE_BOX:
			gp_primitive_rectangle(tgpi, points2D);
			break;
		case GP_STROKE_LINE:
			gp_primitive_line(tgpi, points2D);
			break;
		case GP_STROKE_CIRCLE:
			gp_primitive_circle(tgpi, points2D);
			break;
		default:
			break;
	}

	/* convert screen-coordinates to 3D coordinates */
	for (int i = 0; i < gps->totpoints; i++) {
		bGPDspoint *pt = &gps->points[i];
		MDeformVert *dvert = &gps->dvert[i];
		tGPspoint *p2d = &points2D[i];


		/* convert screen-coordinates to 3D coordinates */
		gp_stroke_convertcoords_tpoint(tgpi->scene, tgpi->ar, tgpi->v3d, tgpi->ob, tgpi->gpl, p2d, NULL, &pt->x);

		pt->pressure = 1.0f;
		pt->strength = tgpi->brush->gpencil_settings->draw_strength;
		pt->time = 0.0f;

		dvert->totweight = 0;
		dvert->dw = NULL;
	}

	/* if axis locked, reproject to plane locked */
	if (tgpi->lock_axis > GP_LOCKAXIS_NONE) {
		bGPDspoint *tpt = gps->points;
		float origin[3];
		ED_gp_get_drawing_reference(tgpi->v3d, tgpi->scene, tgpi->ob, tgpi->gpl,
		                            ts->gpencil_v3d_align, origin);

		for (int i = 0; i < gps->totpoints; i++, tpt++) {
			ED_gp_project_point_to_plane(tgpi->ob, tgpi->rv3d, origin,
			                             ts->gp_sculpt.lock_axis - 1,
			                             tpt);
		}
	}

	/* if parented change position relative to parent object */
	for (int i = 0; i < gps->totpoints; i++) {
		bGPDspoint *pt = &gps->points[i];
		gp_apply_parent_point(tgpi->depsgraph, tgpi->ob, tgpi->gpd, tgpi->gpl, pt);
	}

	/* force fill recalc */
	gps->flag |= GP_STROKE_RECALC_CACHES;

	/* free temp data */
	MEM_SAFE_FREE(points2D);

	DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
}

/* Update screen and stroke */
static void gpencil_primitive_update(bContext *C, wmOperator *op, tGPDprimitive *tgpi)
{
	/* update indicator in header */
	gpencil_primitive_status_indicators(C, tgpi);
	/* apply... */
	tgpi->type = RNA_enum_get(op->ptr, "type");
	tgpi->tot_edges = RNA_int_get(op->ptr, "edges");
	/* update points position */
	gp_primitive_update_strokes(C, tgpi);
}

/* ----------------------- */

/* Exit and free memory */
static void gpencil_primitive_exit(bContext *C, wmOperator *op)
{
	tGPDprimitive *tgpi = op->customdata;
	bGPdata *gpd = tgpi->gpd;

	/* don't assume that operator data exists at all */
	if (tgpi) {
		/* remove drawing handler */
		if (tgpi->draw_handle_3d) {
			ED_region_draw_cb_exit(tgpi->ar->type, tgpi->draw_handle_3d);
		}

		/* clear status message area */
		ED_workspace_status_text(C, NULL);

		/* finally, free memory used by temp data */
		BKE_gpencil_free_strokes(tgpi->gpf);
		MEM_freeN(tgpi->gpf);
		MEM_freeN(tgpi);
	}
	DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

	/* clear pointer */
	op->customdata = NULL;
}

/* Init new temporary primitive data */
static void gpencil_primitive_init(bContext *C, wmOperator *op)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	bGPdata *gpd = CTX_data_gpencil_data(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	int cfra_eval = (int)DEG_get_ctime(depsgraph);

	/* create temporary operator data */
	tGPDprimitive *tgpi = MEM_callocN(sizeof(tGPDprimitive), "GPencil Primitive Data");
	op->customdata = tgpi;

	/* set current scene and window info */
	tgpi->scene = scene;
	tgpi->ob = CTX_data_active_object(C);
	tgpi->sa = CTX_wm_area(C);
	tgpi->ar = CTX_wm_region(C);
	tgpi->rv3d = tgpi->ar->regiondata;
	tgpi->v3d = tgpi->sa->spacedata.first;
	tgpi->depsgraph = CTX_data_depsgraph(C);
	tgpi->win = CTX_wm_window(C);

	/* set current frame number */
	tgpi->cframe = cfra_eval;

	/* set GP datablock */
	tgpi->gpd = gpd;

	/* getcolor info */
	tgpi->mat = BKE_gpencil_material_ensure(bmain, tgpi->ob);

	/* set parameters */
	tgpi->type = RNA_enum_get(op->ptr, "type");

	/* if circle set default to 32 */
	if (tgpi->type == GP_STROKE_CIRCLE) {
		RNA_int_set(op->ptr, "edges", 32);
	}
	else if (tgpi->type == GP_STROKE_BOX) {
		RNA_int_set(op->ptr, "edges", 4);
	}
	else { /* LINE */
		RNA_int_set(op->ptr, "edges", 2);
	}

	tgpi->tot_edges = RNA_int_get(op->ptr, "edges");
	tgpi->flag = IDLE;

	tgpi->lock_axis = ts->gp_sculpt.lock_axis;

	/* set temp layer, frame and stroke */
	gp_primitive_set_initdata(C, tgpi);
}

/* ----------------------- */

/* Invoke handler: Initialize the operator */
static int gpencil_primitive_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	wmWindow *win = CTX_wm_window(C);
	bGPdata *gpd = CTX_data_gpencil_data(C);
	tGPDprimitive *tgpi = NULL;

	/* initialize operator runtime data */
	gpencil_primitive_init(C, op);
	tgpi = op->customdata;

	/* if in tools region, wait till we get to the main (3d-space)
	 * region before allowing drawing to take place.
	 */
	op->flag |= OP_IS_MODAL_CURSOR_REGION;

	/* Enable custom drawing handlers */
	tgpi->draw_handle_3d = ED_region_draw_cb_activate(tgpi->ar->type, gpencil_primitive_draw_3d, tgpi, REGION_DRAW_POST_VIEW);

	/* set cursor to indicate modal */
	WM_cursor_modal_set(win, BC_CROSSCURSOR);

	/* update sindicator in header */
	gpencil_primitive_status_indicators(C, tgpi);
	DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

	/* add a modal handler for this operator */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* Helper to complete a primitive */
static void gpencil_primitive_done(bContext *C, wmOperator *op, wmWindow *win, tGPDprimitive *tgpi)
{
	bGPDframe *gpf;
	bGPDstroke *gps;

	/* return to normal cursor and header status */
	ED_workspace_status_text(C, NULL);
	WM_cursor_modal_restore(win);

	/* insert keyframes as required... */
	gpf = BKE_gpencil_layer_getframe(tgpi->gpl, tgpi->cframe, GP_GETFRAME_ADD_NEW);

	/* prepare stroke to get transfered */
	gps = tgpi->gpf->strokes.first;
	if (gps) {
		gps->thickness = tgpi->brush->size;
		gps->flag |= GP_STROKE_RECALC_CACHES;
	}

	/* transfer stroke from temporary buffer to the actual frame */
	BLI_movelisttolist(&gpf->strokes, &tgpi->gpf->strokes);
	BLI_assert(BLI_listbase_is_empty(&tgpi->gpf->strokes));

	/* clean up temp data */
	gpencil_primitive_exit(C, op);
}

/* Modal handler: Events handling during interactive part */
static int gpencil_primitive_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	tGPDprimitive *tgpi = op->customdata;
	wmWindow *win = CTX_wm_window(C);
	const bool has_numinput = hasNumInput(&tgpi->num);

	switch (event->type) {
		case LEFTMOUSE:
			if ((event->val == KM_PRESS) && (tgpi->flag == IDLE)) {
				/* start drawing primitive */
				/* TODO: Ignore if not in main region yet */
				tgpi->flag = IN_PROGRESS;

				tgpi->top[0] = event->mval[0];
				tgpi->top[1] = event->mval[1];

				tgpi->bottom[0] = event->mval[0];
				tgpi->bottom[1] = event->mval[1];
			}
			else if ((event->val == KM_RELEASE) && (tgpi->flag == IN_PROGRESS)) {
				/* stop drawing primitive */
				tgpi->flag = IDLE;
				gpencil_primitive_done(C, op, win, tgpi);
				/* done! */
				return OPERATOR_FINISHED;
			}
			else {
				if (G.debug & G_DEBUG) {
					printf("GP Add Primitive Modal: LEFTMOUSE %d, Status = %d\n", event->val, tgpi->flag);
				}
			}
			break;
		case RETKEY:  /* confirm */
		{
			tgpi->flag = IDLE;
			gpencil_primitive_done(C, op, win, tgpi);
			/* done! */
			return OPERATOR_FINISHED;
		}

		case ESCKEY:    /* cancel */
		case RIGHTMOUSE:
		{
			/* return to normal cursor and header status */
			ED_workspace_status_text(C, NULL);
			WM_cursor_modal_restore(win);

			/* clean up temp data */
			gpencil_primitive_exit(C, op);

			/* canceled! */
			return OPERATOR_CANCELLED;
		}

		case WHEELUPMOUSE:
		{
			if (tgpi->type == GP_STROKE_CIRCLE) {
				tgpi->tot_edges = tgpi->tot_edges + 1;
				CLAMP(tgpi->tot_edges, MIN_EDGES, MAX_EDGES);
				RNA_int_set(op->ptr, "edges", tgpi->tot_edges);

				/* update screen */
				gpencil_primitive_update(C, op, tgpi);
			}
			break;
		}
		case WHEELDOWNMOUSE:
		{
			if (tgpi->type == GP_STROKE_CIRCLE) {
				tgpi->tot_edges = tgpi->tot_edges - 1;
				CLAMP(tgpi->tot_edges, MIN_EDGES, MAX_EDGES);
				RNA_int_set(op->ptr, "edges", tgpi->tot_edges);

				/* update screen */
				gpencil_primitive_update(C, op, tgpi);
			}
			break;
		}
		case MOUSEMOVE: /* calculate new position */
		{
			/* only handle mousemove if not doing numinput */
			if (has_numinput == false) {
				/* update position of mouse */
				tgpi->bottom[0] = event->mval[0];
				tgpi->bottom[1] = event->mval[1];
				if (tgpi->flag == IDLE) {
					tgpi->top[0] = event->mval[0];
					tgpi->top[1] = event->mval[1];
				}
				/* Keep square if shift key */
				if (event->shift) {
					tgpi->bottom[1] = tgpi->top[1] - (tgpi->bottom[0] - tgpi->top[0]);
				}
				/* update screen */
				gpencil_primitive_update(C, op, tgpi);
			}
			break;
		}
		default:
		{
			if ((event->val == KM_PRESS) && handleNumInput(C, &tgpi->num, event)) {
				float value;

				/* Grab data from numeric input, and store this new value (the user see an int) */
				value = tgpi->tot_edges;
				applyNumInput(&tgpi->num, &value);
				tgpi->tot_edges = value;

				CLAMP(tgpi->tot_edges, MIN_EDGES, MAX_EDGES);
				RNA_int_set(op->ptr, "edges", tgpi->tot_edges);

				/* update screen */
				gpencil_primitive_update(C, op, tgpi);

				break;
			}
			else {
				/* unhandled event - allow to pass through */
				return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
			}
		}
	}

	/* still running... */
	return OPERATOR_RUNNING_MODAL;
}

/* Cancel handler */
static void gpencil_primitive_cancel(bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	gpencil_primitive_exit(C, op);
}

void GPENCIL_OT_primitive(wmOperatorType *ot)
{
	static EnumPropertyItem primitive_type[] = {
		{ GP_STROKE_BOX, "BOX", 0, "Box", "" },
		{ GP_STROKE_LINE, "LINE", 0, "Line", "" },
		{ GP_STROKE_CIRCLE, "CIRCLE", 0, "Circle", "" },
		{ 0, NULL, 0, NULL, NULL }
	};

	/* identifiers */
	ot->name = "Grease Pencil Shapes";
	ot->idname = "GPENCIL_OT_primitive";
	ot->description = "Create predefined grease pencil stroke shapes";

	/* callbacks */
	ot->invoke = gpencil_primitive_invoke;
	ot->modal = gpencil_primitive_modal;
	ot->cancel = gpencil_primitive_cancel;
	ot->poll = gpencil_primitive_add_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* properties */
	RNA_def_int(ot->srna, "edges", 4, MIN_EDGES, MAX_EDGES, "Edges", "Number of polygon edges", MIN_EDGES, MAX_EDGES);
	RNA_def_enum(ot->srna, "type", primitive_type, GP_STROKE_BOX, "Type", "Type of shape");
}

/* *************************************************************** */
