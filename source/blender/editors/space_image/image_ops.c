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
 * Contributor(s): Blender Foundation, 2002-2009, Xavier Thomas
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_image/image_ops.c
 *  \ingroup spimage
 */


#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_image.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_uvedit.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "image_intern.h"

/******************** view navigation utilities *********************/

static void sima_zoom_set(SpaceImage *sima, ARegion *ar, float zoom, float location[2])
{
	float oldzoom= sima->zoom;
	int width, height;

	sima->zoom= zoom;

	if (sima->zoom < 0.1f || sima->zoom > 4.0f) {
		/* check zoom limits */
		ED_space_image_size(sima, &width, &height);

		width *= sima->zoom;
		height *= sima->zoom;

		if((width < 4) && (height < 4))
			sima->zoom= oldzoom;
		else if((ar->winrct.xmax - ar->winrct.xmin) <= sima->zoom)
			sima->zoom= oldzoom;
		else if((ar->winrct.ymax - ar->winrct.ymin) <= sima->zoom)
			sima->zoom= oldzoom;
	}

	if((U.uiflag & USER_ZOOM_TO_MOUSEPOS) && location) {
		float aspx, aspy, w, h;

		ED_space_image_size(sima, &width, &height);
		ED_space_image_aspect(sima, &aspx, &aspy);

		w= width*aspx;
		h= height*aspy;

		sima->xof+= ((location[0]-0.5f)*w-sima->xof)*(sima->zoom-oldzoom)/sima->zoom;
		sima->yof+= ((location[1]-0.5f)*h-sima->yof)*(sima->zoom-oldzoom)/sima->zoom;
	}
}

static void sima_zoom_set_factor(SpaceImage *sima, ARegion *ar, float zoomfac, float location[2])
{
	sima_zoom_set(sima, ar, sima->zoom*zoomfac, location);
}

#if 0 // currently unused
static int image_poll(bContext *C)
{
	return (CTX_data_edit_image(C) != NULL);
}
#endif

static int space_image_buffer_exists_poll(bContext *C)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	if(sima && sima->spacetype==SPACE_IMAGE)
		if(ED_space_image_has_buffer(sima))
			return 1;
	return 0;
}

static int space_image_file_exists_poll(bContext *C)
{
	if(space_image_buffer_exists_poll(C)) {
		Main *bmain= CTX_data_main(C);
		SpaceImage *sima= CTX_wm_space_image(C);
		ImBuf *ibuf;
		void *lock;
		int ret= FALSE;
		char name[FILE_MAX];

		ibuf= ED_space_image_acquire_buffer(sima, &lock);
		if(ibuf) {
			BLI_strncpy(name, ibuf->name, FILE_MAX);
			BLI_path_abs(name, bmain->name);

			if(BLI_exists(name) == FALSE) {
				CTX_wm_operator_poll_msg_set(C, "image file not found");
			}
			else if (BLI_file_is_writable(name) == FALSE) {
				CTX_wm_operator_poll_msg_set(C, "image path can't be written to");
			}
			else {
				ret= TRUE;
			}
		}
		ED_space_image_release_buffer(sima, lock);

		return ret;
	}
	return 0;
}

static int space_image_poll(bContext *C)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	if(sima && sima->spacetype==SPACE_IMAGE && sima->image)
		return 1;
	return 0;
}

int space_image_main_area_poll(bContext *C)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	// XXX ARegion *ar= CTX_wm_region(C);

	if(sima)
		return 1; // XXX (ar && ar->type->regionid == RGN_TYPE_WINDOW);
	
	return 0;
}

/* For IMAGE_OT_curves_point_set to avoid sampling when in uv smooth mode */
int space_image_main_area_not_uv_brush_poll(bContext *C)
{
	SpaceImage *sima= CTX_wm_space_image(C);

	ToolSettings *toolsettings = CTX_data_scene(C)->toolsettings;
	if(sima && !toolsettings->uvsculpt)
		return 1;

	return 0;
}

static int space_image_image_sample_poll(bContext *C)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Object *obedit= CTX_data_edit_object(C);
	ToolSettings *toolsettings = CTX_data_scene(C)->toolsettings;

	if(obedit){
		if(ED_space_image_show_uvedit(sima, obedit) && (toolsettings->use_uv_sculpt))
			return 0;
	}
	return space_image_main_area_poll(C);
}
/********************** view pan operator *********************/

typedef struct ViewPanData {
	float x, y;
	float xof, yof;
	int event_type;
} ViewPanData;

static void image_view_pan_init(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ViewPanData *vpd;

	op->customdata= vpd= MEM_callocN(sizeof(ViewPanData), "ImageViewPanData");
	WM_cursor_modal(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->x= event->x;
	vpd->y= event->y;
	vpd->xof= sima->xof;
	vpd->yof= sima->yof;
	vpd->event_type= event->type;

	WM_event_add_modal_handler(C, op);
}

static void image_view_pan_exit(bContext *C, wmOperator *op, int cancel)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ViewPanData *vpd= op->customdata;

	if(cancel) {
		sima->xof= vpd->xof;
		sima->yof= vpd->yof;
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	WM_cursor_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int image_view_pan_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	float offset[2];

	RNA_float_get_array(op->ptr, "offset", offset);
	sima->xof += offset[0];
	sima->yof += offset[1];

	ED_region_tag_redraw(CTX_wm_region(C));

	/* XXX notifier? */
#if 0
	if(image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif
	
	return OPERATOR_FINISHED;
}

static int image_view_pan_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (event->type == MOUSEPAN) {
		SpaceImage *sima= CTX_wm_space_image(C);
		float offset[2];
		
		offset[0]= (event->x - event->prevx)/sima->zoom;
		offset[1]= (event->y - event->prevy)/sima->zoom;
		RNA_float_set_array(op->ptr, "offset", offset);

		image_view_pan_exec(C, op);
		return OPERATOR_FINISHED;
	}
	else {
		image_view_pan_init(C, op, event);
		return OPERATOR_RUNNING_MODAL;
	}
}

static int image_view_pan_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ViewPanData *vpd= op->customdata;
	float offset[2];

	switch(event->type) {
		case MOUSEMOVE:
			sima->xof= vpd->xof;
			sima->yof= vpd->yof;
			offset[0]= (vpd->x - event->x)/sima->zoom;
			offset[1]= (vpd->y - event->y)/sima->zoom;
			RNA_float_set_array(op->ptr, "offset", offset);
			image_view_pan_exec(C, op);
			break;
		default:
			if(event->type==vpd->event_type &&  event->val==KM_RELEASE) {
				image_view_pan_exit(C, op, 0);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int image_view_pan_cancel(bContext *C, wmOperator *op)
{
	image_view_pan_exit(C, op, 1);
	return OPERATOR_CANCELLED;
}

void IMAGE_OT_view_pan(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Pan";
	ot->idname = "IMAGE_OT_view_pan";
	
	/* api callbacks */
	ot->exec = image_view_pan_exec;
	ot->invoke = image_view_pan_invoke;
	ot->modal = image_view_pan_modal;
	ot->cancel = image_view_pan_cancel;
	ot->poll = space_image_main_area_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;
	
	/* properties */
	RNA_def_float_vector(ot->srna, "offset", 2, NULL, -FLT_MAX, FLT_MAX,
		"Offset", "Offset in floating point units, 1.0 is the width and height of the image", -FLT_MAX, FLT_MAX);
}

/********************** view zoom operator *********************/

typedef struct ViewZoomData {
	float x, y;
	float zoom;
	int event_type;
	float location[2];
} ViewZoomData;

static void image_view_zoom_init(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	ViewZoomData *vpd;

	op->customdata= vpd= MEM_callocN(sizeof(ViewZoomData), "ImageViewZoomData");
	WM_cursor_modal(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->x= event->x;
	vpd->y= event->y;
	vpd->zoom= sima->zoom;
	vpd->event_type= event->type;

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &vpd->location[0], &vpd->location[1]);

	WM_event_add_modal_handler(C, op);
}

static void image_view_zoom_exit(bContext *C, wmOperator *op, int cancel)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ViewZoomData *vpd= op->customdata;

	if(cancel) {
		sima->zoom= vpd->zoom;
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	WM_cursor_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int image_view_zoom_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);

	sima_zoom_set_factor(sima, ar, RNA_float_get(op->ptr, "factor"), NULL);

	ED_region_tag_redraw(CTX_wm_region(C));

	/* XXX notifier? */
#if 0
	if(image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif
	
	return OPERATOR_FINISHED;
}

static int image_view_zoom_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (event->type == MOUSEZOOM) {
		SpaceImage *sima= CTX_wm_space_image(C);
		ARegion *ar= CTX_wm_region(C);
		float factor, location[2];

		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);

		factor= 1.0f + (event->x-event->prevx+event->y-event->prevy)/300.0f;
		RNA_float_set(op->ptr, "factor", factor);
		sima_zoom_set(sima, ar, sima->zoom*factor, location);
		ED_region_tag_redraw(CTX_wm_region(C));
		
		return OPERATOR_FINISHED;
	}
	else {
		image_view_zoom_init(C, op, event);
		return OPERATOR_RUNNING_MODAL;
	}
}

static int image_view_zoom_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	ViewZoomData *vpd= op->customdata;
	float factor;

	switch(event->type) {
		case MOUSEMOVE:
			factor= 1.0f + (vpd->x-event->x+vpd->y-event->y)/300.0f;
			RNA_float_set(op->ptr, "factor", factor);
			sima_zoom_set(sima, ar, vpd->zoom*factor, vpd->location);
			ED_region_tag_redraw(CTX_wm_region(C));
			break;
		default:
			if(event->type==vpd->event_type && event->val==KM_RELEASE) {
				image_view_zoom_exit(C, op, 0);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int image_view_zoom_cancel(bContext *C, wmOperator *op)
{
	image_view_zoom_exit(C, op, 1);
	return OPERATOR_CANCELLED;
}

void IMAGE_OT_view_zoom(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom";
	ot->idname = "IMAGE_OT_view_zoom";
	
	/* api callbacks */
	ot->exec = image_view_zoom_exec;
	ot->invoke = image_view_zoom_invoke;
	ot->modal = image_view_zoom_modal;
	ot->cancel = image_view_zoom_cancel;
	ot->poll = space_image_main_area_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;
	
	/* properties */
	RNA_def_float(ot->srna, "factor", 0.0f, 0.0f, FLT_MAX,
		"Factor", "Zoom factor, values higher than 1.0 zoom in, lower values zoom out", -FLT_MAX, FLT_MAX);
}

/********************** NDOF operator *********************/

/* Combined pan/zoom from a 3D mouse device.
 * Z zooms, XY pans
 * "view" (not "paper") control -- user moves the viewpoint, not the image being viewed
 * that explains the negative signs in the code below
 */

static int image_view_ndof_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	if (event->type != NDOF_MOTION)
		return OPERATOR_CANCELLED;
	else {
		SpaceImage *sima= CTX_wm_space_image(C);
		ARegion *ar= CTX_wm_region(C);

		wmNDOFMotionData* ndof = (wmNDOFMotionData*) event->customdata;

		float dt = ndof->dt;
		/* tune these until it feels right */
		const float zoom_sensitivity = 0.5f; // 50% per second (I think)
		const float pan_sensitivity = 300.f; // screen pixels per second

		float pan_x = pan_sensitivity * dt * ndof->tvec[0] / sima->zoom;
		float pan_y = pan_sensitivity * dt * ndof->tvec[1] / sima->zoom;

		/* "mouse zoom" factor = 1 + (dx + dy) / 300
		 * what about "ndof zoom" factor? should behave like this:
		 * at rest -> factor = 1
		 * move forward -> factor > 1
		 * move backward -> factor < 1
		 */
		float zoom_factor = 1.f + zoom_sensitivity * dt * -ndof->tvec[2];

		if (U.ndof_flag & NDOF_ZOOM_INVERT)
			zoom_factor = -zoom_factor;

		sima_zoom_set_factor(sima, ar, zoom_factor, NULL);
		sima->xof += pan_x;
		sima->yof += pan_y;

		ED_region_tag_redraw(ar);	

		return OPERATOR_FINISHED;
	}
}

void IMAGE_OT_view_ndof(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "NDOF Pan/Zoom";
	ot->idname = "IMAGE_OT_view_ndof";
	
	/* api callbacks */
	ot->invoke = image_view_ndof_invoke;
}

/********************** view all operator *********************/

/* Updates the fields of the View2D member of the SpaceImage struct.
 * Default behavior is to reset the position of the image and set the zoom to 1
 * If the image will not fit within the window rectangle, the zoom is adjusted */

static int image_view_all_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceImage *sima;
	ARegion *ar;
	float aspx, aspy, zoomx, zoomy, w, h;
	int width, height;

	/* retrieve state */
	sima= CTX_wm_space_image(C);
	ar= CTX_wm_region(C);

	ED_space_image_size(sima, &width, &height);
	ED_space_image_aspect(sima, &aspx, &aspy);

	w= width*aspx;
	h= height*aspy;
	
	/* check if the image will fit in the image with zoom==1 */
	width = ar->winrct.xmax - ar->winrct.xmin + 1;
	height = ar->winrct.ymax - ar->winrct.ymin + 1;

	if((w >= width || h >= height) && (width > 0 && height > 0)) {
		/* find the zoom value that will fit the image in the image space */
		zoomx= width/w;
		zoomy= height/h;
		sima_zoom_set(sima, ar, 1.0f/power_of_2(1/MIN2(zoomx, zoomy)), NULL);
	}
	else
		sima_zoom_set(sima, ar, 1.0f, NULL);

	sima->xof= sima->yof= 0.0f;

	ED_region_tag_redraw(CTX_wm_region(C));
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View All";
	ot->idname = "IMAGE_OT_view_all";
	
	/* api callbacks */
	ot->exec = image_view_all_exec;
	ot->poll = space_image_main_area_poll;
}

/********************** view selected operator *********************/

static int image_view_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceImage *sima;
	ARegion *ar;
	Scene *scene;
	Object *obedit;
	Image *ima;
	float size, min[2], max[2], d[2], aspx, aspy;
	int width, height;

	/* retrieve state */
	sima= CTX_wm_space_image(C);
	ar= CTX_wm_region(C);
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);

	ima= ED_space_image(sima);
	ED_space_image_size(sima, &width, &height);
	ED_image_aspect(ima, &aspx, &aspy);

	width= width*aspx;
	height= height*aspy;

	/* get bounds */
	if(!ED_uvedit_minmax(scene, ima, obedit, min, max))
		return OPERATOR_CANCELLED;

	/* adjust offset and zoom */
	sima->xof= (int)(((min[0] + max[0])*0.5f - 0.5f)*width);
	sima->yof= (int)(((min[1] + max[1])*0.5f - 0.5f)*height);

	d[0]= max[0] - min[0];
	d[1]= max[1] - min[1];
	size= 0.5f*MAX2(d[0], d[1])*MAX2(width, height)/256.0f;
	
	if(size<=0.01f) size= 0.01f;
	sima_zoom_set(sima, ar, 0.7f/size, NULL);

	ED_region_tag_redraw(CTX_wm_region(C));
	
	return OPERATOR_FINISHED;
}

static int image_view_selected_poll(bContext *C)
{
	return (space_image_main_area_poll(C) && ED_operator_uvedit(C));
}

void IMAGE_OT_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Center";
	ot->idname = "IMAGE_OT_view_selected";
	
	/* api callbacks */
	ot->exec = image_view_selected_exec;
	ot->poll = image_view_selected_poll;
}

/********************** view zoom in/out operator *********************/

static int image_view_zoom_in_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	float location[2];

	RNA_float_get_array(op->ptr, "location", location);

	sima_zoom_set_factor(sima, ar, 1.25f, location);

	ED_region_tag_redraw(CTX_wm_region(C));
	
	return OPERATOR_FINISHED;
}

static int image_view_zoom_in_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	float location[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return image_view_zoom_in_exec(C, op);
}

void IMAGE_OT_view_zoom_in(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom In";
	ot->idname = "IMAGE_OT_view_zoom_in";
	
	/* api callbacks */
	ot->invoke = image_view_zoom_in_invoke;
	ot->exec = image_view_zoom_in_exec;
	ot->poll = space_image_main_area_poll;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location", "Cursor location in screen coordinates", -10.0f, 10.0f);
}

static int image_view_zoom_out_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	float location[2];

	RNA_float_get_array(op->ptr, "location", location);

	sima_zoom_set_factor(sima, ar, 0.8f, location);

	ED_region_tag_redraw(CTX_wm_region(C));
	
	return OPERATOR_FINISHED;
}

static int image_view_zoom_out_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	float location[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return image_view_zoom_out_exec(C, op);
}

void IMAGE_OT_view_zoom_out(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom Out";
	ot->idname = "IMAGE_OT_view_zoom_out";
	
	/* api callbacks */
	ot->invoke = image_view_zoom_out_invoke;
	ot->exec = image_view_zoom_out_exec;
	ot->poll = space_image_main_area_poll;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location", "Cursor location in screen coordinates", -10.0f, 10.0f);
}

/********************** view zoom ratio operator *********************/

static int image_view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);

	sima_zoom_set(sima, ar, RNA_float_get(op->ptr, "ratio"), NULL);
	
	/* ensure pixel exact locations for draw */
	sima->xof= (int)sima->xof;
	sima->yof= (int)sima->yof;

	/* XXX notifier? */
#if 0
	if(image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif

	ED_region_tag_redraw(CTX_wm_region(C));
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_ratio(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom Ratio";
	ot->idname = "IMAGE_OT_view_zoom_ratio";
	
	/* api callbacks */
	ot->exec = image_view_zoom_ratio_exec;
	ot->poll = space_image_main_area_poll;
	
	/* properties */
	RNA_def_float(ot->srna, "ratio", 0.0f, 0.0f, FLT_MAX,
		"Ratio", "Zoom ratio, 1.0 is 1:1, higher is zoomed in, lower is zoomed out", -FLT_MAX, FLT_MAX);
}

/**************** load/replace/save callbacks ******************/
static void image_filesel(bContext *C, wmOperator *op, const char *path)
{
	RNA_string_set(op->ptr, "filepath", path);
	WM_event_add_fileselect(C, op); 
}

/******************** open image operator ********************/

static void image_open_init(bContext *C, wmOperator *op)
{
	PropertyPointerRNA *pprop;

	op->customdata= pprop= MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
	uiIDContextProperty(C, &pprop->ptr, &pprop->prop);
}

static int image_open_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;
	return OPERATOR_CANCELLED;
}

static int image_open_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C); /* XXX other space types can call */
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	ImageUser *iuser= NULL;
	PropertyPointerRNA *pprop;
	PointerRNA idptr;
	Image *ima= NULL;
	char str[FILE_MAX];

	RNA_string_get(op->ptr, "filepath", str);
	/* default to frame 1 if there's no scene in context */

	errno= 0;

	ima= BKE_add_image_file(str);

	if(!ima) {
		if(op->customdata) MEM_freeN(op->customdata);
		BKE_reportf(op->reports, RPT_ERROR, "Can't read: \"%s\", %s", str, errno ? strerror(errno) : "Unsupported image format");
		return OPERATOR_CANCELLED;
	}
	
	if(!op->customdata)
		image_open_init(C, op);

	/* hook into UI */
	pprop= op->customdata;

	if(pprop->prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		ima->id.us--;

		RNA_id_pointer_create(&ima->id, &idptr);
		RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr);
		RNA_property_update(C, &pprop->ptr, pprop->prop);
	}
	else if(sima) {
		ED_space_image_set(sima, scene, obedit, ima);
		iuser= &sima->iuser;
	}
	else {
		Tex *tex= CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
		if(tex && tex->type==TEX_IMAGE)
			iuser= &tex->iuser;
		
	}
	
	/* initialize because of new image */
	if(iuser) {
		iuser->sfra= 1;
		iuser->offset= 0;
		iuser->fie_ima= 2;
	}

	/* XXX unpackImage frees image buffers */
	ED_preview_kill_jobs(C);
	
	BKE_image_signal(ima, iuser, IMA_SIGNAL_RELOAD);
	WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, ima);
	
	MEM_freeN(op->customdata);

	return OPERATOR_FINISHED;
}

static int image_open_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceImage *sima= CTX_wm_space_image(C); /* XXX other space types can call */
	char *path=U.textudir;
	Image *ima= NULL;

	if(sima) {
		ima= sima->image;
	}

	if (ima==NULL) {
		Tex *tex= CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
		if(tex && tex->type==TEX_IMAGE)
			ima= tex->ima;
	}

	if(ima)
		path= ima->name;

	if(RNA_struct_property_is_set(op->ptr, "filepath"))
		return image_open_exec(C, op);
	
	image_open_init(C, op);

	image_filesel(C, op, path);

	return OPERATOR_RUNNING_MODAL;
}

/* called by other space types too */
void IMAGE_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Open Image";
	ot->description = "Open image";
	ot->idname = "IMAGE_OT_open";
	
	/* api callbacks */
	ot->exec = image_open_exec;
	ot->invoke = image_open_invoke;
	ot->cancel = image_open_cancel;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE|MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH|WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
}

/******************** replace image operator ********************/

static int image_replace_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	char str[FILE_MAX];

	if(!sima->image)
		return OPERATOR_CANCELLED;
	
	RNA_string_get(op->ptr, "filepath", str);

	/* we cant do much if the str is longer then FILE_MAX :/ */
	BLI_strncpy(sima->image->name, str, sizeof(sima->image->name));

	if (BLI_testextensie_array(str, imb_ext_movie))
		sima->image->source= IMA_SRC_MOVIE;
	else
		sima->image->source= IMA_SRC_FILE;

	/* XXX unpackImage frees image buffers */
	ED_preview_kill_jobs(C);
	
	BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_RELOAD);
	WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, sima->image);

	return OPERATOR_FINISHED;
}

static int image_replace_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceImage *sima= CTX_wm_space_image(C);

	if(!sima->image)
		return OPERATOR_CANCELLED;

	if(RNA_struct_property_is_set(op->ptr, "filepath"))
		return image_replace_exec(C, op);

	if(!RNA_struct_property_is_set(op->ptr, "relative_path"))
		RNA_boolean_set(op->ptr, "relative_path", (strncmp(sima->image->name, "//", 2))==0);

	image_filesel(C, op, sima->image->name);

	return OPERATOR_RUNNING_MODAL;
}

void IMAGE_OT_replace(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Replace Image";
	ot->idname = "IMAGE_OT_replace";
	
	/* api callbacks */
	ot->exec = image_replace_exec;
	ot->invoke = image_replace_invoke;
	ot->poll = space_image_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE|MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH|WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
}

/******************** save image as operator ********************/

typedef struct {
	/* matching scene->r settings */
	//short planes, imtype, subimtype, quality;
	ImageFormatData im_format;
	char filepath[FILE_MAX]; /* keep absolute */
} SaveImageOptions;

static void save_image_options_defaults(SaveImageOptions *simopts)
{
	memset(&simopts->im_format, 0, sizeof(simopts->im_format));
	simopts->im_format.planes= R_IMF_PLANES_RGB;
	simopts->im_format.imtype= R_IMF_IMTYPE_PNG;
	simopts->im_format.quality= 90;
	simopts->im_format.compress= 90;
	simopts->filepath[0]= '\0';
}

static char imtype_best_depth(ImBuf *ibuf, const char imtype)
{
	const char depth_ok= BKE_imtype_valid_depths(imtype);

	if (ibuf->rect_float) {
		if (depth_ok & R_IMF_CHAN_DEPTH_32) return R_IMF_CHAN_DEPTH_32;
		if (depth_ok & R_IMF_CHAN_DEPTH_24) return R_IMF_CHAN_DEPTH_24;
		if (depth_ok & R_IMF_CHAN_DEPTH_16) return R_IMF_CHAN_DEPTH_16;
		if (depth_ok & R_IMF_CHAN_DEPTH_12) return R_IMF_CHAN_DEPTH_12;
		return R_IMF_CHAN_DEPTH_8;
	}
	else {
		if (depth_ok & R_IMF_CHAN_DEPTH_8) return R_IMF_CHAN_DEPTH_8;
		if (depth_ok & R_IMF_CHAN_DEPTH_12) return R_IMF_CHAN_DEPTH_12;
		if (depth_ok & R_IMF_CHAN_DEPTH_16) return R_IMF_CHAN_DEPTH_16;
		if (depth_ok & R_IMF_CHAN_DEPTH_24) return R_IMF_CHAN_DEPTH_24;
		if (depth_ok & R_IMF_CHAN_DEPTH_32) return R_IMF_CHAN_DEPTH_32;
		return R_IMF_CHAN_DEPTH_8; /* fallback, should not get here */
	}
}

static int save_image_options_init(SaveImageOptions *simopts, SpaceImage *sima, Scene *scene, const short guess_path)
{
	void *lock;
	ImBuf *ibuf= ED_space_image_acquire_buffer(sima, &lock);

	if(ibuf) {
		Image *ima= sima->image;
		short is_depth_set= FALSE;

		simopts->im_format.planes= ibuf->planes;

		if(ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
			/* imtype */
			simopts->im_format= scene->r.im_format;
			is_depth_set= TRUE;
		}
		else if (ima->source == IMA_SRC_GENERATED) {
			simopts->im_format.imtype= R_IMF_IMTYPE_PNG;
		}
		else {
			simopts->im_format.imtype= BKE_ftype_to_imtype(ibuf->ftype);
		}
		//simopts->subimtype= scene->r.subimtype; /* XXX - this is lame, we need to make these available too! */
		simopts->im_format.quality= ibuf->ftype & 0xff;

		BLI_strncpy(simopts->filepath, ibuf->name, sizeof(simopts->filepath));

		/* sanitize all settings */

		/* unlikely but just in case */
		if (ELEM3(simopts->im_format.planes, R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA) == 0) {
			simopts->im_format.planes= R_IMF_PLANES_RGBA;
		}

		/* depth, account for float buffer and format support */
		if (is_depth_set == FALSE) {
			simopts->im_format.depth= imtype_best_depth(ibuf, simopts->im_format.imtype);
		}

		/* some formats don't use quality so fallback to scenes quality */
		if (simopts->im_format.quality == 0) {
			simopts->im_format.quality= scene->r.im_format.quality;
		}

		/* check for empty path */
		if(guess_path && simopts->filepath[0]==0) {
			if ( (G.ima[0] == '/') && (G.ima[1] == '/') && (G.ima[2] == '\0') ) {
				BLI_strncpy(simopts->filepath, "//untitled", FILE_MAX);
			} else {
				BLI_strncpy(simopts->filepath, G.ima, FILE_MAX);
			}
			BLI_path_abs(simopts->filepath, G.main->name);
		}
	}

	ED_space_image_release_buffer(sima, lock);

	return (ibuf != NULL);
}

static void save_image_options_from_op(SaveImageOptions *simopts, wmOperator *op)
{
	if (op->customdata) {
		simopts->im_format= *(ImageFormatData *)op->customdata;
	}

	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		RNA_string_get(op->ptr, "filepath", simopts->filepath);
		BLI_path_abs(simopts->filepath, G.main->name);
	}
}

static void save_image_options_to_op(SaveImageOptions *simopts, wmOperator *op)
{
	if (op->customdata) {
		*(ImageFormatData *)op->customdata= simopts->im_format;
	}

	RNA_string_set(op->ptr, "filepath", simopts->filepath);
}

/* assumes name is FILE_MAX */
/* ima->name and ibuf->name should end up the same */
static void save_image_doit(bContext *C, SpaceImage *sima, wmOperator *op, SaveImageOptions *simopts, int do_newpath)
{
	Image *ima= ED_space_image(sima);
	void *lock;
	ImBuf *ibuf= ED_space_image_acquire_buffer(sima, &lock);

	if (ibuf) {
		const char *relbase= ID_BLEND_PATH(CTX_data_main(C), &ima->id);
		const short relative= (RNA_struct_find_property(op->ptr, "relative_path") && RNA_boolean_get(op->ptr, "relative_path"));
		const short save_copy= (RNA_struct_find_property(op->ptr, "copy") && RNA_boolean_get(op->ptr, "copy"));
		short ok= FALSE;

		/* old global to ensure a 2nd save goes to same dir */
		BLI_strncpy(G.ima, simopts->filepath, sizeof(G.ima));

		WM_cursor_wait(1);

		if(ima->type == IMA_TYPE_R_RESULT) {
			/* enforce user setting for RGB or RGBA, but skip BW */
			if(simopts->im_format.planes==R_IMF_PLANES_RGBA) {
				ibuf->planes= R_IMF_PLANES_RGBA;
			}
			else if(simopts->im_format.planes==R_IMF_PLANES_RGB) {
				ibuf->planes= R_IMF_PLANES_RGB;
			}
		}
		else {
			/* TODO, better solution, if a 24bit image is painted onto it may contain alpha */
			if(ibuf->userflags & IB_BITMAPDIRTY) { /* it has been painted onto */
				/* checks each pixel, not ideal */
				ibuf->planes= BKE_alphatest_ibuf(ibuf) ? 32 : 24;
			}
		}
		
		if(simopts->im_format.imtype==R_IMF_IMTYPE_MULTILAYER) {
			Scene *scene= CTX_data_scene(C);
			RenderResult *rr= BKE_image_acquire_renderresult(scene, ima);
			if(rr) {
				RE_WriteRenderResult(op->reports, rr, simopts->filepath, simopts->im_format.quality);
				ok= TRUE;
			}
			else {
				BKE_report(op->reports, RPT_ERROR, "Did not write, no Multilayer Image");
			}
			BKE_image_release_renderresult(scene, ima);
		}
		else {
			if (BKE_write_ibuf_as(ibuf, simopts->filepath, &simopts->im_format, save_copy)) {
				ok= TRUE;
			}
		}

		if (ok)	{
			if(!save_copy) {
				if(do_newpath) {
					BLI_strncpy(ibuf->name, simopts->filepath, sizeof(ibuf->name));
					BLI_strncpy(ima->name, simopts->filepath, sizeof(ima->name));
				}

				ibuf->userflags &= ~IB_BITMAPDIRTY;

				/* change type? */
				if(ima->type==IMA_TYPE_R_RESULT) {
					ima->type= IMA_TYPE_IMAGE;

					/* workaround to ensure the render result buffer is no longer used
					 * by this image, otherwise can crash when a new render result is
					 * created. */
					if(ibuf->rect && !(ibuf->mall & IB_rect))
						imb_freerectImBuf(ibuf);
					if(ibuf->rect_float && !(ibuf->mall & IB_rectfloat))
						imb_freerectfloatImBuf(ibuf);
					if(ibuf->zbuf && !(ibuf->mall & IB_zbuf))
						IMB_freezbufImBuf(ibuf);
					if(ibuf->zbuf_float && !(ibuf->mall & IB_zbuffloat))
						IMB_freezbuffloatImBuf(ibuf);
				}
				if( ELEM(ima->source, IMA_SRC_GENERATED, IMA_SRC_VIEWER)) {
					ima->source= IMA_SRC_FILE;
					ima->type= IMA_TYPE_IMAGE;
				}

				/* only image path, never ibuf */
				if(relative) {
					BLI_path_rel(ima->name, relbase); /* only after saving */
				}
			}
		}
		else {
			BKE_reportf(op->reports, RPT_ERROR, "Couldn't write image: %s", simopts->filepath);
		}


		WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, sima->image);

		WM_cursor_wait(0);
	}

	ED_space_image_release_buffer(sima, lock);
}

static void image_save_as_free(wmOperator *op)
{
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata= NULL;
	}
}

static int image_save_as_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	SaveImageOptions simopts;

	save_image_options_defaults(&simopts);

	/* just in case to initialize values,
	 * these should be set on invoke or by the caller. */
	save_image_options_init(&simopts, sima, CTX_data_scene(C), 0);

	save_image_options_from_op(&simopts, op);

	save_image_doit(C, sima, op, &simopts, TRUE);

	image_save_as_free(op);
	return OPERATOR_FINISHED;
}


static int image_save_as_check(bContext *UNUSED(C), wmOperator *op)
{
	ImageFormatData *imf= op->customdata;
	char filepath[FILE_MAX];
	RNA_string_get(op->ptr, "filepath", filepath);
	if(BKE_add_image_extension(filepath, imf->imtype)) {
		RNA_string_set(op->ptr, "filepath", filepath);
		return TRUE;
	}
	return FALSE;
}

static int image_save_as_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Image *ima = ED_space_image(sima);
	Scene *scene= CTX_data_scene(C);
	SaveImageOptions simopts;

	if(RNA_struct_property_is_set(op->ptr, "filepath"))
		return image_save_as_exec(C, op);

	if (save_image_options_init(&simopts, sima, scene, TRUE) == 0)
		return OPERATOR_CANCELLED;
	save_image_options_to_op(&simopts, op);

	/* enable save_copy by default for render results */
	if(ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE) && !RNA_struct_property_is_set(op->ptr, "copy")) {
		RNA_boolean_set(op->ptr, "copy", TRUE);
	}

	op->customdata= MEM_mallocN(sizeof(simopts.im_format), __func__);
	memcpy(op->customdata, &simopts.im_format, sizeof(simopts.im_format));

	image_filesel(C, op, simopts.filepath);

	return OPERATOR_RUNNING_MODAL;
}

static int image_save_as_cancel(bContext *UNUSED(C), wmOperator *op)
{
	image_save_as_free(op);

	return OPERATOR_CANCELLED;
}

static int image_save_as_draw_check_prop(PointerRNA *ptr, PropertyRNA *prop)
{
	const char *prop_id= RNA_property_identifier(prop);

	return !(strcmp(prop_id, "filepath") == 0 ||
	         strcmp(prop_id, "directory") == 0 ||
	         strcmp(prop_id, "filename") == 0 ||
	         /* when saving a copy, relative path has no effect */
	         ((strcmp(prop_id, "relative_path") == 0) && RNA_boolean_get(ptr, "copy"))
	         );
}

static void image_save_as_draw(bContext *UNUSED(C), wmOperator *op)
{
	uiLayout *layout= op->layout;
	ImageFormatData *imf= op->customdata;
	PointerRNA ptr;

	/* image template */
	RNA_pointer_create(NULL, &RNA_ImageFormatSettings, imf, &ptr);
	uiTemplateImageSettings(layout, &ptr);

	/* main draw call */
	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	uiDefAutoButsRNA(layout, &ptr, image_save_as_draw_check_prop, '\0');
}

void IMAGE_OT_save_as(wmOperatorType *ot)
{
//	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Save As Image";
	ot->idname = "IMAGE_OT_save_as";
	
	/* api callbacks */
	ot->exec = image_save_as_exec;
	ot->check = image_save_as_check;
	ot->invoke = image_save_as_invoke;
	ot->cancel = image_save_as_cancel;
	ot->ui = image_save_as_draw;
	ot->poll = space_image_buffer_exists_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "copy", 0, "Copy", "Create a new image file without modifying the current image in blender");

	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE|MOVIEFILE, FILE_SPECIAL, FILE_SAVE, WM_FILESEL_FILEPATH|WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
}

/******************** save image operator ********************/

static int image_save_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	SaveImageOptions simopts;

	if (save_image_options_init(&simopts, sima, scene, FALSE) == 0)
		return OPERATOR_CANCELLED;
	save_image_options_from_op(&simopts, op);

	if (BLI_exists(simopts.filepath) && BLI_file_is_writable(simopts.filepath)) {
		save_image_doit(C, sima, op, &simopts, FALSE);
	}
	else {
		BKE_reportf(op->reports, RPT_ERROR, "Can not save image, path '%s' is not writable", simopts.filepath);
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

void IMAGE_OT_save(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Save Image";
	ot->idname = "IMAGE_OT_save";
	
	/* api callbacks */
	ot->exec = image_save_exec;
	ot->poll = space_image_file_exists_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************* save sequence operator ********************/

static int image_save_sequence_exec(bContext *C, wmOperator *op)
{
	Main *bmain= CTX_data_main(C);
	SpaceImage *sima= CTX_wm_space_image(C);
	ImBuf *ibuf;
	int tot= 0;
	char di[FILE_MAX], fi[FILE_MAX];
	
	if(sima->image==NULL)
		return OPERATOR_CANCELLED;

	if(sima->image->source!=IMA_SRC_SEQUENCE) {
		BKE_report(op->reports, RPT_ERROR, "Can only save sequence on image sequences");
		return OPERATOR_CANCELLED;
	}

	if(sima->image->type==IMA_TYPE_MULTILAYER) {
		BKE_report(op->reports, RPT_ERROR, "Can't save multilayer sequences");
		return OPERATOR_CANCELLED;
	}
	
	/* get total */
	for(ibuf= sima->image->ibufs.first; ibuf; ibuf= ibuf->next) 
		if(ibuf->userflags & IB_BITMAPDIRTY)
			tot++;
	
	if(tot==0) {
		BKE_report(op->reports, RPT_WARNING, "No images have been changed");
		return OPERATOR_CANCELLED;
	}

	/* get a filename for menu */
	for(ibuf= sima->image->ibufs.first; ibuf; ibuf= ibuf->next) 
		if(ibuf->userflags & IB_BITMAPDIRTY)
			break;
	
	BLI_strncpy(di, ibuf->name, FILE_MAX);
	BLI_splitdirstring(di, fi);
	
	BKE_reportf(op->reports, RPT_INFO, "%d Image(s) will be saved in %s", tot, di);

	for(ibuf= sima->image->ibufs.first; ibuf; ibuf= ibuf->next) {
		if(ibuf->userflags & IB_BITMAPDIRTY) {
			char name[FILE_MAX];
			BLI_strncpy(name, ibuf->name, sizeof(name));
			
			BLI_path_abs(name, bmain->name);

			if(0 == IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat)) {
				BKE_reportf(op->reports, RPT_ERROR, "Could not write image %s", name);
				break;
			}

			BKE_reportf(op->reports, RPT_INFO, "Saved: %s\n", ibuf->name);
			ibuf->userflags &= ~IB_BITMAPDIRTY;
		}
	}

	return OPERATOR_FINISHED;
}

void IMAGE_OT_save_sequence(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Save Sequence";
	ot->idname = "IMAGE_OT_save_sequence";
	
	/* api callbacks */
	ot->exec = image_save_sequence_exec;
	ot->poll = space_image_buffer_exists_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/******************** reload image operator ********************/

static int image_reload_exec(bContext *C, wmOperator *UNUSED(op))
{
	Image *ima= CTX_data_edit_image(C);
	SpaceImage *sima= CTX_wm_space_image(C);

	if(!ima)
		return OPERATOR_CANCELLED;

	/* XXX unpackImage frees image buffers */
	ED_preview_kill_jobs(C);
	
	// XXX other users?
	BKE_image_signal(ima, (sima)? &sima->iuser: NULL, IMA_SIGNAL_RELOAD);

	WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, ima);
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_reload(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reload Image";
	ot->idname = "IMAGE_OT_reload";
	
	/* api callbacks */
	ot->exec = image_reload_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER; /* no undo, image buffer is not handled by undo */
}

/********************** new image operator *********************/

static int image_new_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima;
	Scene *scene;
	Object *obedit;
	Image *ima;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	char name[MAX_ID_NAME-2];
	float color[4];
	int width, height, floatbuf, uvtestgrid, alpha;

	/* retrieve state */
	sima= CTX_wm_space_image(C);
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);

	RNA_string_get(op->ptr, "name", name);
	width= RNA_int_get(op->ptr, "width");
	height= RNA_int_get(op->ptr, "height");
	floatbuf= RNA_boolean_get(op->ptr, "float");
	uvtestgrid= RNA_boolean_get(op->ptr, "uv_test_grid");
	RNA_float_get_array(op->ptr, "color", color);
	alpha= RNA_boolean_get(op->ptr, "alpha");
	
	if (!floatbuf && scene->r.color_mgt_flag & R_COLOR_MANAGEMENT)
		linearrgb_to_srgb_v3_v3(color, color);

	if(!alpha)
		color[3]= 1.0f;

	ima = BKE_add_image_size(width, height, name, alpha ? 32 : 24, floatbuf, uvtestgrid, color);

	if(!ima)
		return OPERATOR_CANCELLED;

	/* hook into UI */
	uiIDContextProperty(C, &ptr, &prop);

	if(prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		ima->id.us--;

		RNA_id_pointer_create(&ima->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	else if(sima)
		ED_space_image_set(sima, scene, obedit, ima);

	// XXX other users?
	BKE_image_signal(ima, (sima)? &sima->iuser: NULL, IMA_SIGNAL_USER_NEW_IMAGE);
	
	return OPERATOR_FINISHED;
}

/* XXX, Ton is not a fan of OK buttons but using this function to avoid undo/redo bug while in mesh-editmode, - campbell */
static int image_new_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	return WM_operator_props_dialog_popup(C, op, 300, 100);

}

void IMAGE_OT_new(wmOperatorType *ot)
{
	PropertyRNA *prop;
	static float default_color[4]= {0.0f, 0.0f, 0.0f, 1.0f};
	
	/* identifiers */
	ot->name = "New Image";
	ot->description = "Create a new image";
	ot->idname = "IMAGE_OT_new";
	
	/* api callbacks */
	ot->exec = image_new_exec;
	ot->invoke = image_new_invoke;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "name", "untitled", MAX_ID_NAME-2, "Name", "Image datablock name");
	RNA_def_int(ot->srna, "width", 1024, 1, INT_MAX, "Width", "Image width", 1, 16384);
	RNA_def_int(ot->srna, "height", 1024, 1, INT_MAX, "Height", "Image height", 1, 16384);
	prop= RNA_def_float_color(ot->srna, "color", 4, NULL, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
	RNA_def_property_float_array_default(prop, default_color);
	RNA_def_boolean(ot->srna, "alpha", 1, "Alpha", "Create an image with an alpha channel");
	RNA_def_boolean(ot->srna, "uv_test_grid", 0, "UV Test Grid", "Fill the image with a grid for UV map testing");
	RNA_def_boolean(ot->srna, "float", 0, "32 bit Float", "Create image with 32 bit floating point bit depth");
}

/********************* invert operators *********************/

static int image_invert_poll(bContext *C)
{
	Image *ima= CTX_data_edit_image(C);
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	
	if( ibuf != NULL )
		return 1;
	return 0;
}

static int image_invert_exec(bContext *C, wmOperator *op)
{
	Image *ima= CTX_data_edit_image(C);
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);

	// flags indicate if this channel should be inverted
	const short r= RNA_boolean_get(op->ptr, "invert_r");
	const short g= RNA_boolean_get(op->ptr, "invert_g");
	const short b= RNA_boolean_get(op->ptr, "invert_b");
	const short a= RNA_boolean_get(op->ptr, "invert_a");

	int i;

	if( ibuf == NULL) // TODO: this should actually never happen, but does for render-results -> cleanup
		return OPERATOR_CANCELLED;

	/* TODO: make this into an IMB_invert_channels(ibuf,r,g,b,a) method!? */
	if (ibuf->rect_float) {
		
		float *fp = (float *) ibuf->rect_float;
		for( i = ibuf->x * ibuf->y; i > 0; i--, fp+=4 ) {
			if( r ) fp[0] = 1.0f - fp[0];
			if( g ) fp[1] = 1.0f - fp[1];
			if( b ) fp[2] = 1.0f - fp[2];
			if( a ) fp[3] = 1.0f - fp[3];
		}

		if(ibuf->rect) {
			IMB_rect_from_float(ibuf);
		}
	}
	else if(ibuf->rect) {
		
		char *cp = (char *) ibuf->rect;
		for( i = ibuf->x * ibuf->y; i > 0; i--, cp+=4 ) {
			if( r ) cp[0] = 255 - cp[0];
			if( g ) cp[1] = 255 - cp[1];
			if( b ) cp[2] = 255 - cp[2];
			if( a ) cp[3] = 255 - cp[3];
		}
	}
	else {
		return OPERATOR_CANCELLED;
	}

	ibuf->userflags |= IB_BITMAPDIRTY;
	if(ibuf->mipmap[0])
		ibuf->userflags |= IB_MIPMAP_INVALID;

	WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, ima);
	return OPERATOR_FINISHED;
}

void IMAGE_OT_invert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Invert Channels";
	ot->idname = "IMAGE_OT_invert";
	
	/* api callbacks */
	ot->exec = image_invert_exec;
	ot->poll = image_invert_poll;
	
	/* properties */
	RNA_def_boolean(ot->srna, "invert_r", 0, "Red", "Invert Red Channel");
	RNA_def_boolean(ot->srna, "invert_g", 0, "Green", "Invert Green Channel");
	RNA_def_boolean(ot->srna, "invert_b", 0, "Blue", "Invert Blue Channel");
	RNA_def_boolean(ot->srna, "invert_a", 0, "Alpha", "Invert Alpha Channel");
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************* pack operator *********************/

static int image_pack_test(bContext *C, wmOperator *op)
{
	Image *ima= CTX_data_edit_image(C);
	int as_png= RNA_boolean_get(op->ptr, "as_png");

	if(!ima)
		return 0;
	if(!as_png && ima->packedfile)
		return 0;

	if(ima->source==IMA_SRC_SEQUENCE || ima->source==IMA_SRC_MOVIE) {
		BKE_report(op->reports, RPT_ERROR, "Packing movies or image sequences not supported");
		return 0;
	}

	return 1;
}

static int image_pack_exec(bContext *C, wmOperator *op)
{
	struct Main *bmain= CTX_data_main(C);
	Image *ima= CTX_data_edit_image(C);
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	int as_png= RNA_boolean_get(op->ptr, "as_png");

	if(!image_pack_test(C, op))
		return OPERATOR_CANCELLED;
	
	if(!as_png && (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))) {
		BKE_report(op->reports, RPT_ERROR, "Can't pack edited image from disk, only as internal PNG");
		return OPERATOR_CANCELLED;
	}

	if(as_png)
		BKE_image_memorypack(ima);
	else
		ima->packedfile= newPackedFile(op->reports, ima->name, ID_BLEND_PATH(bmain, &ima->id));

	WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, ima);
	
	return OPERATOR_FINISHED;
}

static int image_pack_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Image *ima= CTX_data_edit_image(C);
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	uiPopupMenu *pup;
	uiLayout *layout;
	int as_png= RNA_boolean_get(op->ptr, "as_png");

	if(!image_pack_test(C, op))
		return OPERATOR_CANCELLED;
	
	if(!as_png && (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))) {
		pup= uiPupMenuBegin(C, "OK", ICON_QUESTION);
		layout= uiPupMenuLayout(pup);
		uiItemBooleanO(layout, "Can't pack edited image from disk. Pack as internal PNG?", ICON_NONE, op->idname, "as_png", 1);
		uiPupMenuEnd(C, pup);

		return OPERATOR_CANCELLED;
	}

	return image_pack_exec(C, op);
}

void IMAGE_OT_pack(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Pack Image";
	ot->description = "Pack an image as embedded data into the .blend file"; 
	ot->idname = "IMAGE_OT_pack";
	
	/* api callbacks */
	ot->exec = image_pack_exec;
	ot->invoke = image_pack_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "as_png", 0, "Pack As PNG", "Pack image as lossless PNG");
}

/********************* unpack operator *********************/

static int image_unpack_exec(bContext *C, wmOperator *op)
{
	Image *ima= CTX_data_edit_image(C);
	int method= RNA_enum_get(op->ptr, "method");

	/* find the suppplied image by name */
	if (RNA_struct_property_is_set(op->ptr, "id")) {
		char imaname[MAX_ID_NAME-2];
		RNA_string_get(op->ptr, "id", imaname);
		ima = BLI_findstring(&CTX_data_main(C)->image, imaname, offsetof(ID, name) + 2);
		if (!ima) ima = CTX_data_edit_image(C);
	}
	
	if(!ima || !ima->packedfile)
		return OPERATOR_CANCELLED;

	if(ima->source==IMA_SRC_SEQUENCE || ima->source==IMA_SRC_MOVIE) {
		BKE_report(op->reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
		return OPERATOR_CANCELLED;
	}

	if(G.fileflags & G_AUTOPACK)
		BKE_report(op->reports, RPT_WARNING, "AutoPack is enabled, so image will be packed again on file save");
	
	/* XXX unpackImage frees image buffers */
	ED_preview_kill_jobs(C);
	
	unpackImage(op->reports, ima, method);
	
	WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, ima);

	return OPERATOR_FINISHED;
}

static int image_unpack_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	Image *ima= CTX_data_edit_image(C);

	if(RNA_struct_property_is_set(op->ptr, "id"))
		return image_unpack_exec(C, op);
		
	if(!ima || !ima->packedfile)
		return OPERATOR_CANCELLED;

	if(ima->source==IMA_SRC_SEQUENCE || ima->source==IMA_SRC_MOVIE) {
		BKE_report(op->reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
		return OPERATOR_CANCELLED;
	}

	if(G.fileflags & G_AUTOPACK)
		BKE_report(op->reports, RPT_WARNING, "AutoPack is enabled, so image will be packed again on file save");

	unpack_menu(C, "IMAGE_OT_unpack", ima->id.name+2, ima->name, "textures", ima->packedfile);

	return OPERATOR_FINISHED;
}

void IMAGE_OT_unpack(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unpack Image";
	ot->description = "Save an image packed in the .blend file to disk"; 
	ot->idname = "IMAGE_OT_unpack";
	
	/* api callbacks */
	ot->exec = image_unpack_exec;
	ot->invoke = image_unpack_invoke;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_enum(ot->srna, "method", unpack_method_items, PF_USE_LOCAL, "Method", "How to unpack");
	RNA_def_string(ot->srna, "id", "", MAX_ID_NAME-2, "Image Name", "Image datablock name to unpack"); /* XXX, weark!, will fail with library, name collisions */
}

/******************** sample image operator ********************/

typedef struct ImageSampleInfo {
	ARegionType *art;
	void *draw_handle;
	int x, y;
	int channels;

	unsigned char col[4];
	float colf[4];
	int z;
	float zf;

	unsigned char *colp;
	float *colfp;
	int *zp;
	float *zfp;

	int draw;
} ImageSampleInfo;

static void image_sample_draw(const bContext *UNUSED(C), ARegion *ar, void *arg_info)
{
	ImageSampleInfo *info= arg_info;
	if(info->draw) {
		/* no color management needed for images (color_manage=0) */
		ED_image_draw_info(ar, 0, info->channels, info->x, info->y, info->colp, info->colfp, info->zp, info->zfp);
	}
}

static void image_sample_apply(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	void *lock;
	ImBuf *ibuf= ED_space_image_acquire_buffer(sima, &lock);
	ImageSampleInfo *info= op->customdata;
	float fx, fy;
	
	if(ibuf == NULL) {
		ED_space_image_release_buffer(sima, lock);
		return;
	}

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fx, &fy);

	if(fx>=0.0f && fy>=0.0f && fx<1.0f && fy<1.0f) {
		float *fp;
		unsigned char *cp;
		int x= (int)(fx*ibuf->x), y= (int)(fy*ibuf->y);

		CLAMP(x, 0, ibuf->x-1);
		CLAMP(y, 0, ibuf->y-1);

		info->x= x;
		info->y= y;
		info->draw= 1;
		info->channels= ibuf->channels;

		info->colp= NULL;
		info->colfp= NULL;
		info->zp= NULL;
		info->zfp= NULL;
		
		if(ibuf->rect) {
			cp= (unsigned char *)(ibuf->rect + y*ibuf->x + x);

			info->col[0]= cp[0];
			info->col[1]= cp[1];
			info->col[2]= cp[2];
			info->col[3]= cp[3];
			info->colp= info->col;

			info->colf[0]= (float)cp[0]/255.0f;
			info->colf[1]= (float)cp[1]/255.0f;
			info->colf[2]= (float)cp[2]/255.0f;
			info->colf[3]= (float)cp[3]/255.0f;
			info->colfp= info->colf;
		}
		if(ibuf->rect_float) {
			fp= (ibuf->rect_float + (ibuf->channels)*(y*ibuf->x + x));

			info->colf[0]= fp[0];
			info->colf[1]= fp[1];
			info->colf[2]= fp[2];
			info->colf[3]= fp[3];
			info->colfp= info->colf;
		}

		if(ibuf->zbuf) {
			info->z= ibuf->zbuf[y*ibuf->x + x];
			info->zp= &info->z;
		}
		if(ibuf->zbuf_float) {
			info->zf= ibuf->zbuf_float[y*ibuf->x + x];
			info->zfp= &info->zf;
		}
		
		if(sima->cumap && ibuf->channels==4) {
			/* we reuse this callback for set curves point operators */
			if(RNA_struct_find_property(op->ptr, "point")) {
				int point= RNA_enum_get(op->ptr, "point");

				if(point == 1) {
					curvemapping_set_black_white(sima->cumap, NULL, info->colfp);
					if(ibuf->rect_float)
						curvemapping_do_ibuf(sima->cumap, ibuf);
				}
				else if(point == 0) {
					curvemapping_set_black_white(sima->cumap, info->colfp, NULL);
					if(ibuf->rect_float)
						curvemapping_do_ibuf(sima->cumap, ibuf);
				}
			}
		}
				
		// XXX node curve integration ..
#if 0
		{
			ScrArea *sa, *cur= curarea;
			
			node_curvemap_sample(fp);	/* sends global to node editor */
			for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
				if(sa->spacetype==SPACE_NODE) {
					areawinset(sa->win);
					scrarea_do_windraw(sa);
				}
			}
			node_curvemap_sample(NULL);		/* clears global in node editor */
			curarea= cur;
		}
#endif
	}
	else
		info->draw= 0;

	ED_space_image_release_buffer(sima, lock);
	ED_area_tag_redraw(CTX_wm_area(C));
}

static void image_sample_exit(bContext *C, wmOperator *op)
{
	ImageSampleInfo *info= op->customdata;

	ED_region_draw_cb_exit(info->art, info->draw_handle);
	ED_area_tag_redraw(CTX_wm_area(C));
	MEM_freeN(info);
}

static int image_sample_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	ImageSampleInfo *info;

	if(!ED_space_image_has_buffer(sima))
		return OPERATOR_CANCELLED;
	
	info= MEM_callocN(sizeof(ImageSampleInfo), "ImageSampleInfo");
	info->art= ar->type;
	info->draw_handle = ED_region_draw_cb_activate(ar->type, image_sample_draw, info, REGION_DRAW_POST_PIXEL);
	op->customdata= info;

	image_sample_apply(C, op, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int image_sample_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	switch(event->type) {
		case LEFTMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			image_sample_exit(C, op);
			return OPERATOR_CANCELLED;
		case MOUSEMOVE:
			image_sample_apply(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int image_sample_cancel(bContext *C, wmOperator *op)
{
	image_sample_exit(C, op);
	return OPERATOR_CANCELLED;
}

void IMAGE_OT_sample(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Color";
	ot->idname = "IMAGE_OT_sample";
	
	/* api callbacks */
	ot->invoke = image_sample_invoke;
	ot->modal = image_sample_modal;
	ot->cancel = image_sample_cancel;
	ot->poll = space_image_image_sample_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;
}

/******************** sample line operator ********************/
static int image_sample_line_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ARegion *ar= CTX_wm_region(C);
	Scene *scene= CTX_data_scene(C);
	
	int x_start= RNA_int_get(op->ptr, "xstart");
	int y_start= RNA_int_get(op->ptr, "ystart");
	int x_end= RNA_int_get(op->ptr, "xend");
	int y_end= RNA_int_get(op->ptr, "yend");
	
	void *lock;
	ImBuf *ibuf= ED_space_image_acquire_buffer(sima, &lock);
	Histogram *hist= &sima->sample_line_hist;
	
	float x1f, y1f, x2f, y2f;
	int x1, y1, x2, y2;
	int i, x, y;
	float *fp;
	float rgb[3];
	unsigned char *cp;
	
	if (ibuf == NULL) {
		ED_space_image_release_buffer(sima, lock);
		return OPERATOR_CANCELLED;
	}
	/* hmmmm */
	if (ibuf->channels < 3) {
		ED_space_image_release_buffer(sima, lock);
		return OPERATOR_CANCELLED;
	}
	
	UI_view2d_region_to_view(&ar->v2d, x_start, y_start, &x1f, &y1f);
	UI_view2d_region_to_view(&ar->v2d, x_end, y_end, &x2f, &y2f);
	x1= 0.5f+ x1f*ibuf->x;
	x2= 0.5f+ x2f*ibuf->x;
	y1= 0.5f+ y1f*ibuf->y;
	y2= 0.5f+ y2f*ibuf->y;
	
	hist->channels = 3;
	hist->x_resolution = 256;
	hist->xmax = 1.0f;
	hist->ymax = 1.0f;
	
	for (i=0; i<256; i++) {
		x= (int)(0.5f + x1 + (float)i*(x2-x1)/255.0f);
		y= (int)(0.5f + y1 + (float)i*(y2-y1)/255.0f);
		
		if (x<0 || y<0 || x>=ibuf->x || y>=ibuf->y) {
			hist->data_luma[i] = hist->data_r[i] = hist->data_g[i]= hist->data_b[i] = 0.0f;
		} else {
			if (ibuf->rect_float) {
				fp= (ibuf->rect_float + (ibuf->channels)*(y*ibuf->x + x));

				if (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT)
					linearrgb_to_srgb_v3_v3(rgb, fp);
				else
					copy_v3_v3(rgb, fp);

				hist->data_r[i] = rgb[0];
				hist->data_g[i] = rgb[1];
				hist->data_b[i] = rgb[2];
				hist->data_luma[i] = rgb_to_luma(rgb);
			}
			else if (ibuf->rect) {
				cp= (unsigned char *)(ibuf->rect + y*ibuf->x + x);
				hist->data_r[i] = (float)cp[0]/255.0f;
				hist->data_g[i] = (float)cp[1]/255.0f;
				hist->data_b[i] = (float)cp[2]/255.0f;
				hist->data_luma[i] = (float)rgb_to_luma_byte(cp)/255.0f;
			}
		}
	}
	
	ED_space_image_release_buffer(sima, lock);
	
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

static int image_sample_line_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	
	if(!ED_space_image_has_buffer(sima))
		return OPERATOR_CANCELLED;
	
	return WM_gesture_straightline_invoke(C, op, event);
}

void IMAGE_OT_sample_line(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Line";
	ot->idname = "IMAGE_OT_sample_line";
	
	/* api callbacks */
	ot->invoke = image_sample_line_invoke;
	ot->modal = WM_gesture_straightline_modal;
	ot->exec = image_sample_line_exec;
	ot->poll = space_image_main_area_poll;
	ot->cancel = WM_gesture_straightline_cancel;
	
	/* flags */
	ot->flag = 0; /* no undo/register since this operates on the space */
	
	WM_operator_properties_gesture_straightline(ot, CURSOR_EDIT);
}

/******************** set curve point operator ********************/

void IMAGE_OT_curves_point_set(wmOperatorType *ot)
{
	static EnumPropertyItem point_items[]= {
		{0, "BLACK_POINT", 0, "Black Point", ""},
		{1, "WHITE_POINT", 0, "White Point", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Set Curves Point";
	ot->idname = "IMAGE_OT_curves_point_set";

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->invoke = image_sample_invoke;
	ot->modal = image_sample_modal;
	ot->cancel = image_sample_cancel;
	ot->poll = space_image_main_area_not_uv_brush_poll;

	/* properties */
	RNA_def_enum(ot->srna, "point", point_items, 0, "Point", "Set black point or white point for curves");
}

/******************** record composite operator *********************/

typedef struct RecordCompositeData {
	wmTimer *timer;
	int old_cfra;
	int sfra, efra;
} RecordCompositeData;

static int image_record_composite_apply(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	RecordCompositeData *rcd= op->customdata;
	Scene *scene= CTX_data_scene(C);
	ImBuf *ibuf;
	
	WM_timecursor(CTX_wm_window(C), scene->r.cfra);

	// XXX scene->nodetree->test_break= blender_test_break;
	// XXX scene->nodetree->test_break= NULL;
	
	BKE_image_all_free_anim_ibufs(scene->r.cfra);
	ntreeCompositTagAnimated(scene->nodetree);
	ntreeCompositExecTree(scene->nodetree, &scene->r, scene->r.cfra != rcd->old_cfra);	/* 1 is no previews */

	ED_area_tag_redraw(CTX_wm_area(C));
	
	ibuf= BKE_image_get_ibuf(sima->image, &sima->iuser);
	/* save memory in flipbooks */
	if(ibuf)
		imb_freerectfloatImBuf(ibuf);
	
	scene->r.cfra++;

	return (scene->r.cfra <= rcd->efra);
}

static int image_record_composite_init(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	RecordCompositeData *rcd;

	if(sima->iuser.frames < 2)
		return 0;
	if(scene->nodetree == NULL)
		return 0;
	
	op->customdata= rcd= MEM_callocN(sizeof(RecordCompositeData), "ImageRecordCompositeData");

	rcd->old_cfra= scene->r.cfra;
	rcd->sfra= sima->iuser.sfra;
	rcd->efra= sima->iuser.sfra + sima->iuser.frames-1;
	scene->r.cfra= rcd->sfra;

	return 1;
}

static void image_record_composite_exit(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SpaceImage *sima= CTX_wm_space_image(C);
	RecordCompositeData *rcd= op->customdata;

	scene->r.cfra= rcd->old_cfra;

	WM_cursor_restore(CTX_wm_window(C));

	if(rcd->timer)
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), rcd->timer);

	WM_event_add_notifier(C, NC_IMAGE|NA_EDITED, sima->image);

	// XXX play_anim(0);
	// XXX allqueue(REDRAWNODE, 1);

	MEM_freeN(rcd);
}

static int image_record_composite_exec(bContext *C, wmOperator *op)
{
	if(!image_record_composite_init(C, op))
		return OPERATOR_CANCELLED;
	
	while(image_record_composite_apply(C, op))
		;
	
	image_record_composite_exit(C, op);
	
	return OPERATOR_FINISHED;
}

static int image_record_composite_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	RecordCompositeData *rcd;
	
	if(!image_record_composite_init(C, op))
		return OPERATOR_CANCELLED;

	rcd= op->customdata;
	rcd->timer= WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.0f);
	WM_event_add_modal_handler(C, op);

	if(!image_record_composite_apply(C, op))
		return OPERATOR_FINISHED;

	return OPERATOR_RUNNING_MODAL;
}

static int image_record_composite_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	RecordCompositeData *rcd= op->customdata;

	switch(event->type) {
		case TIMER:
			if(rcd->timer == event->customdata) {
				if(!image_record_composite_apply(C, op)) {
					image_record_composite_exit(C, op);
					return OPERATOR_FINISHED;
				}
			}
			break;
		case ESCKEY:
			image_record_composite_exit(C, op);
			return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int image_record_composite_cancel(bContext *C, wmOperator *op)
{
	image_record_composite_exit(C, op);
	return OPERATOR_CANCELLED;
}

void IMAGE_OT_record_composite(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Record Composite";
	ot->idname = "IMAGE_OT_record_composite";
	
	/* api callbacks */
	ot->exec = image_record_composite_exec;
	ot->invoke = image_record_composite_invoke;
	ot->modal = image_record_composite_modal;
	ot->cancel = image_record_composite_cancel;
	ot->poll = space_image_buffer_exists_poll;
}

/********************* cycle render slot operator *********************/

static int image_cycle_render_slot_poll(bContext *C)
{
	Image *ima= CTX_data_edit_image(C);

	return (ima && ima->type == IMA_TYPE_R_RESULT);
}

static int image_cycle_render_slot_exec(bContext *C, wmOperator *op)
{
	Image *ima= CTX_data_edit_image(C);
	int a, slot, cur= ima->render_slot;
	const short use_reverse= RNA_boolean_get(op->ptr, "reverse");

	for(a=1; a<IMA_MAX_RENDER_SLOT; a++) {
		slot= (cur + (use_reverse ? -a:a))%IMA_MAX_RENDER_SLOT;
		if(slot<0) slot+=IMA_MAX_RENDER_SLOT;

		if(ima->renders[slot] || slot == ima->last_render_slot) {
			ima->render_slot= slot;
			break;
		}
		else if((slot - 1) == ima->last_render_slot && slot < IMA_MAX_RENDER_SLOT) {
			ima->render_slot= slot;
			break;
		}
	}

	if(a == IMA_MAX_RENDER_SLOT)
		ima->render_slot= ((cur == 1)? 0: 1);
	
	WM_event_add_notifier(C, NC_IMAGE|ND_DRAW, NULL);

	/* no undo push for browsing existing */
	if(ima->renders[ima->render_slot] || ima->render_slot==ima->last_render_slot)
		return OPERATOR_CANCELLED;
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_cycle_render_slot(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cycle Render Slot";
	ot->idname = "IMAGE_OT_cycle_render_slot";
	
	/* api callbacks */
	ot->exec = image_cycle_render_slot_exec;
	ot->poll = image_cycle_render_slot_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "reverse", 0, "Cycle in Reverse", "");
}

/******************** TODO ********************/

/* XXX notifier? */

/* goes over all ImageUsers, and sets frame numbers if auto-refresh is set */

void ED_image_update_frame(const Main *mainp, int cfra)
{
	wmWindowManager *wm;
	wmWindow *win;
	Tex *tex;
	
	/* texture users */
	for(tex= mainp->tex.first; tex; tex= tex->id.next) {
		if(tex->type==TEX_IMAGE && tex->ima) {
			if(ELEM(tex->ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
				if(tex->iuser.flag & IMA_ANIM_ALWAYS)
					BKE_image_user_calc_frame(&tex->iuser, cfra, 0);
			}
		}
	}
	
	/* image window, compo node users */
	for(wm=mainp->wm.first; wm; wm= wm->id.next) { /* only 1 wm */
		for(win= wm->windows.first; win; win= win->next) {
			ScrArea *sa;
			for(sa= win->screen->areabase.first; sa; sa= sa->next) {
				if(sa->spacetype==SPACE_VIEW3D) {
					View3D *v3d= sa->spacedata.first;
					BGpic *bgpic;
					for(bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next)
						if(bgpic->iuser.flag & IMA_ANIM_ALWAYS)
							BKE_image_user_calc_frame(&bgpic->iuser, cfra, 0);
				}
				else if(sa->spacetype==SPACE_IMAGE) {
					SpaceImage *sima= sa->spacedata.first;
					if(sima->iuser.flag & IMA_ANIM_ALWAYS)
						BKE_image_user_calc_frame(&sima->iuser, cfra, 0);
				}
				else if(sa->spacetype==SPACE_NODE) {
					SpaceNode *snode= sa->spacedata.first;
					if((snode->treetype==NTREE_COMPOSIT) && (snode->nodetree)) {
						bNode *node;
						for(node= snode->nodetree->nodes.first; node; node= node->next) {
							if(node->id && node->type==CMP_NODE_IMAGE) {
								Image *ima= (Image *)node->id;
								ImageUser *iuser= node->storage;
								if(ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE))
									if(iuser->flag & IMA_ANIM_ALWAYS)
										BKE_image_user_calc_frame(iuser, cfra, 0);
							}
						}
					}
				}
			}
		}
	}
}

