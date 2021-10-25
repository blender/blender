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
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_sound.h"
#include "BKE_scene.h"

#include "GPU_draw.h"
#include "GPU_buffers.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_moviecache.h"
#include "intern/openexr/openexr_multi.h"

#include "RE_pipeline.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_image.h"
#include "ED_mask.h"
#include "ED_paint.h"
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

#include "PIL_time.h"

#include "RE_engine.h"

#include "image_intern.h"

/******************** view navigation utilities *********************/

static void sima_zoom_set(SpaceImage *sima, ARegion *ar, float zoom, const float location[2])
{
	float oldzoom = sima->zoom;
	int width, height;

	sima->zoom = zoom;

	if (sima->zoom < 0.1f || sima->zoom > 4.0f) {
		/* check zoom limits */
		ED_space_image_get_size(sima, &width, &height);

		width *= sima->zoom;
		height *= sima->zoom;

		if ((width < 4) && (height < 4) && sima->zoom < oldzoom)
			sima->zoom = oldzoom;
		else if (BLI_rcti_size_x(&ar->winrct) <= sima->zoom)
			sima->zoom = oldzoom;
		else if (BLI_rcti_size_y(&ar->winrct) <= sima->zoom)
			sima->zoom = oldzoom;
	}

	if ((U.uiflag & USER_ZOOM_TO_MOUSEPOS) && location) {
		float aspx, aspy, w, h;

		ED_space_image_get_size(sima, &width, &height);
		ED_space_image_get_aspect(sima, &aspx, &aspy);

		w = width * aspx;
		h = height * aspy;

		sima->xof += ((location[0] - 0.5f) * w - sima->xof) * (sima->zoom - oldzoom) / sima->zoom;
		sima->yof += ((location[1] - 0.5f) * h - sima->yof) * (sima->zoom - oldzoom) / sima->zoom;
	}
}

static void sima_zoom_set_factor(SpaceImage *sima, ARegion *ar, float zoomfac, const float location[2])
{
	sima_zoom_set(sima, ar, sima->zoom * zoomfac, location);
}

/**
 * Fits the view to the bounds exactly, caller should add margin if needed.
 */
static void sima_zoom_set_from_bounds(SpaceImage *sima, ARegion *ar, const rctf *bounds)
{
	int image_size[2];
	float aspx, aspy;

	ED_space_image_get_size(sima, &image_size[0], &image_size[1]);
	ED_space_image_get_aspect(sima, &aspx, &aspy);

	image_size[0] = image_size[0] * aspx;
	image_size[1] = image_size[1] * aspy;

	/* adjust offset and zoom */
	sima->xof = roundf((BLI_rctf_cent_x(bounds) - 0.5f) * image_size[0]);
	sima->yof = roundf((BLI_rctf_cent_y(bounds) - 0.5f) * image_size[1]);

	float size_xy[2], size;
	size_xy[0] = BLI_rcti_size_x(&ar->winrct) / (BLI_rctf_size_x(bounds) * image_size[0]);
	size_xy[1] = BLI_rcti_size_y(&ar->winrct) / (BLI_rctf_size_y(bounds) * image_size[1]);

	size = min_ff(size_xy[0], size_xy[1]);
	CLAMP_MAX(size, 100.0f);

	sima_zoom_set(sima, ar, size, NULL);
}

#if 0 // currently unused
static int image_poll(bContext *C)
{
	return (CTX_data_edit_image(C) != NULL);
}
#endif

static int space_image_buffer_exists_poll(bContext *C)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	if (sima && ED_space_image_has_buffer(sima)) {
		return true;
	}
	return false;
}

static int image_not_packed_poll(bContext *C)
{
	SpaceImage *sima = CTX_wm_space_image(C);

	/* Do not run 'replace' on packed images, it does not give user expected results at all. */
	if (sima && sima->image && BLI_listbase_is_empty(&sima->image->packedfiles)) {
		return true;
	}
	return false;
}

static bool imbuf_format_writeable(const ImBuf *ibuf)
{
	ImageFormatData im_format;
	ImbFormatOptions options_dummy;
	BKE_imbuf_to_image_format(&im_format, ibuf);
	return (BKE_image_imtype_to_ftype(im_format.imtype, &options_dummy) == ibuf->ftype);
}

static int space_image_file_exists_poll(bContext *C)
{
	if (space_image_buffer_exists_poll(C)) {
		Main *bmain = CTX_data_main(C);
		SpaceImage *sima = CTX_wm_space_image(C);
		ImBuf *ibuf;
		void *lock;
		bool ret = false;
		char name[FILE_MAX];

		ibuf = ED_space_image_acquire_buffer(sima, &lock);
		if (ibuf) {
			BLI_strncpy(name, ibuf->name, FILE_MAX);
			BLI_path_abs(name, bmain->name);

			if (BLI_exists(name) == false) {
				CTX_wm_operator_poll_msg_set(C, "image file not found");
			}
			else if (!BLI_file_is_writable(name)) {
				CTX_wm_operator_poll_msg_set(C, "image path can't be written to");
			}
			else if (!imbuf_format_writeable(ibuf)) {
				CTX_wm_operator_poll_msg_set(C, "image format is read-only");
			}
			else {
				ret = true;
			}
		}
		ED_space_image_release_buffer(sima, ibuf, lock);

		return ret;
	}
	return false;
}

#if 0  /* UNUSED */
static int space_image_poll(bContext *C)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	if (sima && sima->image) {
		return true;
	}
	return false;
}
#endif

int space_image_main_region_poll(bContext *C)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	/* XXX ARegion *ar = CTX_wm_region(C); */

	if (sima) {
		return true;  /* XXX (ar && ar->type->regionid == RGN_TYPE_WINDOW); */
	}
	return false;
}

/* For IMAGE_OT_curves_point_set to avoid sampling when in uv smooth mode or editmode */
static int space_image_main_area_not_uv_brush_poll(bContext *C)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *toolsettings = scene->toolsettings;

	if (sima && !toolsettings->uvsculpt && !scene->obedit)
		return 1;

	return 0;
}

static int image_sample_poll(bContext *C)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	if (sima) {
		Scene *scene = CTX_data_scene(C);
		Object *obedit = CTX_data_edit_object(C);
		ToolSettings *toolsettings = scene->toolsettings;

		if (obedit) {
			if (ED_space_image_show_uvedit(sima, obedit) && (toolsettings->use_uv_sculpt))
				return false;
		}
		else if (sima->mode != SI_MODE_VIEW) {
			return false;
		}

		return space_image_main_region_poll(C);
	}
	else {
		return false;
	}
}
/********************** view pan operator *********************/

typedef struct ViewPanData {
	float x, y;
	float xof, yof;
	int event_type;
} ViewPanData;

static void image_view_pan_init(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ViewPanData *vpd;

	op->customdata = vpd = MEM_callocN(sizeof(ViewPanData), "ImageViewPanData");
	WM_cursor_modal_set(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->x = event->x;
	vpd->y = event->y;
	vpd->xof = sima->xof;
	vpd->yof = sima->yof;
	vpd->event_type = event->type;

	WM_event_add_modal_handler(C, op);
}

static void image_view_pan_exit(bContext *C, wmOperator *op, bool cancel)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ViewPanData *vpd = op->customdata;

	if (cancel) {
		sima->xof = vpd->xof;
		sima->yof = vpd->yof;
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	WM_cursor_modal_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int image_view_pan_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	float offset[2];

	RNA_float_get_array(op->ptr, "offset", offset);
	sima->xof += offset[0];
	sima->yof += offset[1];

	ED_region_tag_redraw(CTX_wm_region(C));

	/* XXX notifier? */
#if 0
	if (image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif
	
	return OPERATOR_FINISHED;
}

static int image_view_pan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (event->type == MOUSEPAN) {
		SpaceImage *sima = CTX_wm_space_image(C);
		float offset[2];
		
		offset[0] = (event->prevx - event->x) / sima->zoom;
		offset[1] = (event->prevy - event->y) / sima->zoom;
		RNA_float_set_array(op->ptr, "offset", offset);

		image_view_pan_exec(C, op);
		return OPERATOR_FINISHED;
	}
	else {
		image_view_pan_init(C, op, event);
		return OPERATOR_RUNNING_MODAL;
	}
}

static int image_view_pan_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ViewPanData *vpd = op->customdata;
	float offset[2];

	switch (event->type) {
		case MOUSEMOVE:
			sima->xof = vpd->xof;
			sima->yof = vpd->yof;
			offset[0] = (vpd->x - event->x) / sima->zoom;
			offset[1] = (vpd->y - event->y) / sima->zoom;
			RNA_float_set_array(op->ptr, "offset", offset);
			image_view_pan_exec(C, op);
			break;
		default:
			if (event->type == vpd->event_type && event->val == KM_RELEASE) {
				image_view_pan_exit(C, op, false);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void image_view_pan_cancel(bContext *C, wmOperator *op)
{
	image_view_pan_exit(C, op, true);
}

void IMAGE_OT_view_pan(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Pan";
	ot->idname = "IMAGE_OT_view_pan";
	ot->description = "Pan the view";
	
	/* api callbacks */
	ot->exec = image_view_pan_exec;
	ot->invoke = image_view_pan_invoke;
	ot->modal = image_view_pan_modal;
	ot->cancel = image_view_pan_cancel;
	ot->poll = space_image_main_region_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR | OPTYPE_LOCK_BYPASS;
	
	/* properties */
	RNA_def_float_vector(ot->srna, "offset", 2, NULL, -FLT_MAX, FLT_MAX,
	                     "Offset", "Offset in floating point units, 1.0 is the width and height of the image", -FLT_MAX, FLT_MAX);
}

/********************** view zoom operator *********************/

typedef struct ViewZoomData {
	float origx, origy;
	float zoom;
	int event_type;
	float location[2];

	/* needed for continuous zoom */
	wmTimer *timer;
	double timer_lastdraw;

	/* */
	SpaceImage *sima;
	ARegion *ar;
} ViewZoomData;

static void image_view_zoom_init(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);
	ViewZoomData *vpd;

	op->customdata = vpd = MEM_callocN(sizeof(ViewZoomData), "ImageViewZoomData");
	WM_cursor_modal_set(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->origx = event->x;
	vpd->origy = event->y;
	vpd->zoom = sima->zoom;
	vpd->event_type = event->type;

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &vpd->location[0], &vpd->location[1]);

	if (U.viewzoom == USER_ZOOM_CONT) {
		/* needs a timer to continue redrawing */
		vpd->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);
		vpd->timer_lastdraw = PIL_check_seconds_timer();
	}

	vpd->sima = sima;
	vpd->ar = ar;

	WM_event_add_modal_handler(C, op);
}

static void image_view_zoom_exit(bContext *C, wmOperator *op, bool cancel)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ViewZoomData *vpd = op->customdata;

	if (cancel) {
		sima->zoom = vpd->zoom;
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	if (vpd->timer)
		WM_event_remove_timer(CTX_wm_manager(C), vpd->timer->win, vpd->timer);

	WM_cursor_modal_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int image_view_zoom_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);

	sima_zoom_set_factor(sima, ar, RNA_float_get(op->ptr, "factor"), NULL);

	ED_region_tag_redraw(ar);

	/* XXX notifier? */
#if 0
	if (image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif
	
	return OPERATOR_FINISHED;
}

enum {
	VIEW_PASS = 0,
	VIEW_APPLY,
	VIEW_CONFIRM
};

static int image_view_zoom_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (event->type == MOUSEZOOM || event->type == MOUSEPAN) {
		SpaceImage *sima = CTX_wm_space_image(C);
		ARegion *ar = CTX_wm_region(C);
		float delta, factor, location[2];

		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);

		delta = event->prevx - event->x + event->prevy - event->y;

		if (U.uiflag & USER_ZOOM_INVERT)
			delta *= -1;

		factor = 1.0f + delta / 300.0f;
		RNA_float_set(op->ptr, "factor", factor);
		sima_zoom_set(sima, ar, sima->zoom * factor, location);
		ED_region_tag_redraw(ar);
		
		return OPERATOR_FINISHED;
	}
	else {
		image_view_zoom_init(C, op, event);
		return OPERATOR_RUNNING_MODAL;
	}
}

static void image_zoom_apply(ViewZoomData *vpd, wmOperator *op, const int x, const int y, const short viewzoom, const short zoom_invert)
{
	float factor;

	if (viewzoom == USER_ZOOM_CONT) {
		double time = PIL_check_seconds_timer();
		float time_step = (float)(time - vpd->timer_lastdraw);
		float fac;
		float zfac;

		if (U.uiflag & USER_ZOOM_HORIZ) {
			fac = (float)(x - vpd->origx);
		}
		else {
			fac = (float)(y - vpd->origy);
		}

		if (zoom_invert) {
			fac = -fac;
		}

		/* oldstyle zoom */
		zfac = 1.0f + ((fac / 20.0f) * time_step);
		vpd->timer_lastdraw = time;
		/* this is the final zoom, but instead make it into a factor */
		//zoom = vpd->sima->zoom * zfac;
		factor = (vpd->sima->zoom * zfac) / vpd->zoom;
	}
	else {
		/* for now do the same things for scale and dolly */
		float delta = x - vpd->origx + y - vpd->origy;

		if (zoom_invert)
			delta *= -1.0f;

		factor = 1.0f + delta / 300.0f;
	}

	RNA_float_set(op->ptr, "factor", factor);
	sima_zoom_set(vpd->sima, vpd->ar, vpd->zoom * factor, vpd->location);
	ED_region_tag_redraw(vpd->ar);
}

static int image_view_zoom_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	ViewZoomData *vpd = op->customdata;
	short event_code = VIEW_PASS;

	/* execute the events */
	if (event->type == TIMER && event->customdata == vpd->timer) {
		/* continuous zoom */
		event_code = VIEW_APPLY;
	}
	else if (event->type == MOUSEMOVE) {
		event_code = VIEW_APPLY;
	}
	else if (event->type == vpd->event_type && event->val == KM_RELEASE) {
		event_code = VIEW_CONFIRM;
	}

	if (event_code == VIEW_APPLY) {
		image_zoom_apply(vpd, op, event->x, event->y, U.viewzoom, (U.uiflag & USER_ZOOM_INVERT) != 0);
	}
	else if (event_code == VIEW_CONFIRM) {
		image_view_zoom_exit(C, op, false);
		return OPERATOR_FINISHED;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void image_view_zoom_cancel(bContext *C, wmOperator *op)
{
	image_view_zoom_exit(C, op, true);
}

void IMAGE_OT_view_zoom(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View Zoom";
	ot->idname = "IMAGE_OT_view_zoom";
	ot->description = "Zoom in/out the image";
	
	/* api callbacks */
	ot->exec = image_view_zoom_exec;
	ot->invoke = image_view_zoom_invoke;
	ot->modal = image_view_zoom_modal;
	ot->cancel = image_view_zoom_cancel;
	ot->poll = space_image_main_region_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR | OPTYPE_LOCK_BYPASS;
	
	/* properties */
	prop = RNA_def_float(ot->srna, "factor", 0.0f, -FLT_MAX, FLT_MAX, "Factor",
	                     "Zoom factor, values higher than 1.0 zoom in, lower values zoom out", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

#ifdef WITH_INPUT_NDOF
/********************** NDOF operator *********************/

/* Combined pan/zoom from a 3D mouse device.
 * Z zooms, XY pans
 * "view" (not "paper") control -- user moves the viewpoint, not the image being viewed
 * that explains the negative signs in the code below
 */

static int image_view_ndof_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	if (event->type != NDOF_MOTION)
		return OPERATOR_CANCELLED;
	else {
		SpaceImage *sima = CTX_wm_space_image(C);
		ARegion *ar = CTX_wm_region(C);
		float pan_vec[3];

		const wmNDOFMotionData *ndof = event->customdata;
		const float speed = NDOF_PIXELS_PER_SECOND;

		WM_event_ndof_pan_get(ndof, pan_vec, true);

		mul_v2_fl(pan_vec, (speed * ndof->dt) / sima->zoom);
		pan_vec[2] *= -ndof->dt;

		sima_zoom_set_factor(sima, ar, 1.0f + pan_vec[2], NULL);
		sima->xof += pan_vec[0];
		sima->yof += pan_vec[1];

		ED_region_tag_redraw(ar);

		return OPERATOR_FINISHED;
	}
}

void IMAGE_OT_view_ndof(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "NDOF Pan/Zoom";
	ot->idname = "IMAGE_OT_view_ndof";
	ot->description = "Use a 3D mouse device to pan/zoom the view";
	
	/* api callbacks */
	ot->invoke = image_view_ndof_invoke;
	ot->poll = space_image_main_region_poll;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;
}
#endif /* WITH_INPUT_NDOF */

/********************** view all operator *********************/

/* Updates the fields of the View2D member of the SpaceImage struct.
 * Default behavior is to reset the position of the image and set the zoom to 1
 * If the image will not fit within the window rectangle, the zoom is adjusted */

static int image_view_all_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima;
	ARegion *ar;
	float aspx, aspy, zoomx, zoomy, w, h;
	int width, height;
	const bool fit_view = RNA_boolean_get(op->ptr, "fit_view");

	/* retrieve state */
	sima = CTX_wm_space_image(C);
	ar = CTX_wm_region(C);

	ED_space_image_get_size(sima, &width, &height);
	ED_space_image_get_aspect(sima, &aspx, &aspy);

	w = width * aspx;
	h = height * aspy;
	
	/* check if the image will fit in the image with (zoom == 1) */
	width  = BLI_rcti_size_x(&ar->winrct) + 1;
	height = BLI_rcti_size_y(&ar->winrct) + 1;

	if (fit_view) {
		const int margin = 5; /* margin from border */

		zoomx = (float) width / (w + 2 * margin);
		zoomy = (float) height / (h + 2 * margin);

		sima_zoom_set(sima, ar, min_ff(zoomx, zoomy), NULL);
	}
	else {
		if ((w >= width || h >= height) && (width > 0 && height > 0)) {
			zoomx = (float) width / w;
			zoomy = (float) height / h;

			/* find the zoom value that will fit the image in the image space */
			sima_zoom_set(sima, ar, 1.0f / power_of_2(1.0f / min_ff(zoomx, zoomy)), NULL);
		}
		else
			sima_zoom_set(sima, ar, 1.0f, NULL);
	}

	sima->xof = sima->yof = 0.0f;

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_all(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View All";
	ot->idname = "IMAGE_OT_view_all";
	ot->description = "View the entire image";
	
	/* api callbacks */
	ot->exec = image_view_all_exec;
	ot->poll = space_image_main_region_poll;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;

	/* properties */
	prop = RNA_def_boolean(ot->srna, "fit_view", 0, "Fit View", "Fit frame to the viewport");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/********************** view selected operator *********************/

static int image_view_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceImage *sima;
	ARegion *ar;
	Scene *scene;
	Object *obedit;
	Image *ima;

	/* retrieve state */
	sima = CTX_wm_space_image(C);
	ar = CTX_wm_region(C);
	scene = CTX_data_scene(C);
	obedit = CTX_data_edit_object(C);

	ima = ED_space_image(sima);

	/* get bounds */
	float min[2], max[2];
	if (ED_space_image_show_uvedit(sima, obedit)) {
		if (!ED_uvedit_minmax(scene, ima, obedit, min, max)) {
			return OPERATOR_CANCELLED;
		}
	}
	else if (ED_space_image_check_show_maskedit(scene, sima)) {
		if (!ED_mask_selected_minmax(C, min, max)) {
			return OPERATOR_CANCELLED;
		}
	}
	rctf bounds = {
	    .xmin = min[0], .ymin = min[1],
	    .xmax = max[0], .ymax = max[1],
	};

	/* add some margin */
	BLI_rctf_scale(&bounds, 1.4f);

	sima_zoom_set_from_bounds(sima, ar, &bounds);

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

static int image_view_selected_poll(bContext *C)
{
	return (space_image_main_region_poll(C) && (ED_operator_uvedit(C) || ED_operator_mask(C)));
}

void IMAGE_OT_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Center";
	ot->idname = "IMAGE_OT_view_selected";
	ot->description = "View all selected UVs";
	
	/* api callbacks */
	ot->exec = image_view_selected_exec;
	ot->poll = image_view_selected_poll;
}

/********************** view zoom in/out operator *********************/

static int image_view_zoom_in_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);
	float location[2];
	
	RNA_float_get_array(op->ptr, "location", location);

	sima_zoom_set_factor(sima, ar, powf(2.0f, 1.0f / 3.0f), location);

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

static int image_view_zoom_in_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float location[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return image_view_zoom_in_exec(C, op);
}

void IMAGE_OT_view_zoom_in(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View Zoom In";
	ot->idname = "IMAGE_OT_view_zoom_in";
	ot->description = "Zoom in the image (centered around 2D cursor)";
	
	/* api callbacks */
	ot->invoke = image_view_zoom_in_invoke;
	ot->exec = image_view_zoom_in_exec;
	ot->poll = space_image_main_region_poll;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;

	/* properties */
	prop = RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                            "Location", "Cursor location in screen coordinates", -10.0f, 10.0f);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

static int image_view_zoom_out_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);
	float location[2];
	
	RNA_float_get_array(op->ptr, "location", location);

	sima_zoom_set_factor(sima, ar, powf(0.5f, 1.0f / 3.0f), location);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

static int image_view_zoom_out_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float location[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return image_view_zoom_out_exec(C, op);
}

void IMAGE_OT_view_zoom_out(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "View Zoom Out";
	ot->idname = "IMAGE_OT_view_zoom_out";
	ot->description = "Zoom out the image (centered around 2D cursor)";
	
	/* api callbacks */
	ot->invoke = image_view_zoom_out_invoke;
	ot->exec = image_view_zoom_out_exec;
	ot->poll = space_image_main_region_poll;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;

	/* properties */
	prop = RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
	                            "Location", "Cursor location in screen coordinates", -10.0f, 10.0f);
	RNA_def_property_flag(prop, PROP_HIDDEN);
}

/********************** view zoom ratio operator *********************/

static int image_view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);

	sima_zoom_set(sima, ar, RNA_float_get(op->ptr, "ratio"), NULL);
	
	/* ensure pixel exact locations for draw */
	sima->xof = (int)sima->xof;
	sima->yof = (int)sima->yof;

	/* XXX notifier? */
#if 0
	if (image_preview_active(curarea, NULL, NULL)) {
		/* recalculates new preview rect */
		scrarea_do_windraw(curarea);
		image_preview_event(2);
	}
#endif

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_ratio(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom Ratio";
	ot->idname = "IMAGE_OT_view_zoom_ratio";
	ot->description = "Set zoom ratio of the view";
	
	/* api callbacks */
	ot->exec = image_view_zoom_ratio_exec;
	ot->poll = space_image_main_region_poll;

	/* flags */
	ot->flag = OPTYPE_LOCK_BYPASS;

	/* properties */
	RNA_def_float(ot->srna, "ratio", 0.0f, -FLT_MAX, FLT_MAX,
	              "Ratio", "Zoom ratio, 1.0 is 1:1, higher is zoomed in, lower is zoomed out", -FLT_MAX, FLT_MAX);
}

/********************** view border-zoom operator *********************/

static int image_view_zoom_border_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);
	rctf bounds;
	const int gesture_mode = RNA_int_get(op->ptr, "gesture_mode");

	WM_operator_properties_border_to_rctf(op, &bounds);

	UI_view2d_region_to_view_rctf(&ar->v2d, &bounds, &bounds);

	const struct {
		float xof;
		float yof;
		float zoom;
	} sima_view_prev = {
		.xof = sima->xof,
		.yof = sima->yof,
		.zoom = sima->zoom,
	};

	sima_zoom_set_from_bounds(sima, ar, &bounds);

	/* zoom out */
	if (gesture_mode == GESTURE_MODAL_OUT) {
		sima->xof = sima_view_prev.xof + (sima->xof - sima_view_prev.xof);
		sima->yof = sima_view_prev.yof + (sima->yof - sima_view_prev.yof);
		sima->zoom = sima_view_prev.zoom * (sima_view_prev.zoom / sima->zoom);
	}

	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void IMAGE_OT_view_zoom_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Zoom to Border";
	ot->description = "Zoom in the view to the nearest item contained in the border";
	ot->idname = "IMAGE_OT_view_zoom_border";

	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = image_view_zoom_border_exec;
	ot->modal = WM_border_select_modal;
	ot->cancel = WM_border_select_cancel;

	ot->poll = space_image_main_region_poll;

	/* rna */
	WM_operator_properties_gesture_border(ot, false);
}

/**************** load/replace/save callbacks ******************/
static void image_filesel(bContext *C, wmOperator *op, const char *path)
{
	RNA_string_set(op->ptr, "filepath", path);
	WM_event_add_fileselect(C, op); 
}

/******************** open image operator ********************/

typedef struct ImageOpenData {
	PropertyPointerRNA pprop;
	ImageUser *iuser;
	ImageFormatData im_format;
} ImageOpenData;

typedef struct ImageFrameRange {
	struct ImageFrameRange *next, *prev;
	ListBase frames;
	/**  The full path of the first file in the list of image files */
	char filepath[FILE_MAX];
} ImageFrameRange;

typedef struct ImageFrame {
	struct ImageFrame *next, *prev;
	int framenr;
} ImageFrame;

static void image_open_init(bContext *C, wmOperator *op)
{
	ImageOpenData *iod;
	op->customdata = iod = MEM_callocN(sizeof(ImageOpenData), __func__);
	iod->iuser = CTX_data_pointer_get_type(C, "image_user", &RNA_ImageUser).data;
	UI_context_active_but_prop_get_templateID(C, &iod->pprop.ptr, &iod->pprop.prop);
}

static void image_open_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata = NULL;
}

/**
 * \brief Get a list of frames from the list of image files matching the first file name sequence pattern
 * \param ptr [in] the RNA pointer containing the "directory" entry and "files" collection
 * \param frames_all [out] the list of frame numbers found in the files matching the first one by name
 */
static void image_sequence_get_frame_ranges(PointerRNA *ptr, ListBase *frames_all)
{
	char dir[FILE_MAXDIR];
	const bool do_frame_range = RNA_boolean_get(ptr, "use_sequence_detection");
	ImageFrameRange *frame_range = NULL;

	RNA_string_get(ptr, "directory", dir);
	RNA_BEGIN (ptr, itemptr, "files")
	{
		char base_head[FILE_MAX], base_tail[FILE_MAX];
		char head[FILE_MAX], tail[FILE_MAX];
		unsigned short digits;
		char *filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0);
		ImageFrame *frame = MEM_callocN(sizeof(ImageFrame), "image_frame");

		/* use the first file in the list as base filename */
		frame->framenr = BLI_stringdec(filename, head, tail, &digits);

		/* still in the same sequence */
		if (do_frame_range &&
		    (frame_range != NULL) &&
		    (STREQLEN(base_head, head, FILE_MAX)) &&
		    (STREQLEN(base_tail, tail, FILE_MAX)))
		{
			/* pass */
		}
		else {
			/* start a new frame range */
			frame_range = MEM_callocN(sizeof(*frame_range), __func__);
			BLI_join_dirfile(frame_range->filepath, sizeof(frame_range->filepath), dir, filename);
			BLI_addtail(frames_all, frame_range);

			BLI_strncpy(base_head, head, sizeof(base_head));
			BLI_strncpy(base_tail, tail, sizeof(base_tail));
		}

		BLI_addtail(&frame_range->frames, frame);
		MEM_freeN(filename);
	}
	RNA_END
}

static int image_cmp_frame(const void *a, const void *b)
{
	const ImageFrame *frame_a = a;
	const ImageFrame *frame_b = b;

	if (frame_a->framenr < frame_b->framenr) return -1;
	if (frame_a->framenr > frame_b->framenr) return 1;
	return 0;
}

/**
 * \brief Return the start (offset) and the length of the sequence of continuous frames in the list of frames
 * \param frames [in] the list of frame numbers, as a side-effect the list is sorted
 * \param ofs [out] offest, the first frame number in the sequence
 * \return the number of contiguous frames in the sequence
 */
static int image_sequence_get_len(ListBase *frames, int *ofs)
{
	ImageFrame *frame;

	BLI_listbase_sort(frames, image_cmp_frame);

	frame = frames->first;
	if (frame) {
		int frame_curr = frame->framenr;
		(*ofs) = frame_curr;
		while (frame && (frame->framenr == frame_curr)) {
			frame_curr++;
			frame = frame->next;
		}
		return frame_curr - (*ofs);
	}
	*ofs = 0;
	return 0;
}

static Image *image_open_single(
        wmOperator *op, const char *filepath, const char *relbase,
        bool is_relative_path, bool use_multiview, int frame_seq_len)
{
	bool exists = false;
	Image *ima = NULL;

	errno = 0;
	ima = BKE_image_load_exists_ex(filepath, &exists);

	if (!ima) {
		if (op->customdata) MEM_freeN(op->customdata);
		BKE_reportf(op->reports, RPT_ERROR, "Cannot read '%s': %s",
		            filepath, errno ? strerror(errno) : TIP_("unsupported image format"));
		return NULL;
	}

	if (!exists) {
		/* only image path after save, never ibuf */
		if (is_relative_path) {
			BLI_path_rel(ima->name, relbase);
		}

		/* handle multiview images */
		if (use_multiview) {
			ImageOpenData *iod = op->customdata;
			ImageFormatData *imf = &iod->im_format;

			ima->flag |= IMA_USE_VIEWS;
			ima->views_format = imf->views_format;
			*ima->stereo3d_format = imf->stereo3d_format;
		}
		else {
			ima->flag &= ~IMA_USE_VIEWS;
			BKE_image_free_views(ima);
		}

		if ((frame_seq_len > 1) && (ima->source == IMA_SRC_FILE)) {
			ima->source = IMA_SRC_SEQUENCE;
		}
	}

	return ima;
}


static int image_open_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	ScrArea *sa = CTX_wm_area(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	ImageUser *iuser = NULL;
	ImageOpenData *iod = op->customdata;
	PointerRNA idptr;
	Image *ima = NULL;
	char filepath[FILE_MAX];
	int frame_seq_len = 0;
	int frame_ofs = 1;

	const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
	const bool use_multiview    = RNA_boolean_get(op->ptr, "use_multiview");

	if (!op->customdata)
		image_open_init(C, op);

	RNA_string_get(op->ptr, "filepath", filepath);

	if (RNA_struct_property_is_set(op->ptr, "directory") &&
	    RNA_struct_property_is_set(op->ptr, "files"))
	{
		bool was_relative = BLI_path_is_rel(filepath);
		ListBase frame_ranges_all;

		BLI_listbase_clear(&frame_ranges_all);
		image_sequence_get_frame_ranges(op->ptr, &frame_ranges_all);
		for (ImageFrameRange *frame_range = frame_ranges_all.first; frame_range; frame_range = frame_range->next) {
			int frame_range_ofs;
			int frame_range_seq_len = image_sequence_get_len(&frame_range->frames, &frame_range_ofs);
			BLI_freelistN(&frame_range->frames);

			char filepath_range[FILE_MAX];
			BLI_strncpy(filepath_range, frame_range->filepath, sizeof(filepath_range));

			if (was_relative) {
				BLI_path_rel(filepath_range, bmain->name);
			}

			Image *ima_range = image_open_single(
			         op, filepath_range, bmain->name,
			         is_relative_path, use_multiview, frame_range_seq_len);

			/* take the first image */
			if ((ima == NULL) && ima_range) {
				ima = ima_range;
				frame_seq_len = frame_range_seq_len;
				frame_ofs = frame_range_ofs;
			}
		}
		BLI_freelistN(&frame_ranges_all);
	}
	else {
		/* for drag & drop etc. */
		ima = image_open_single(
		        op, filepath, bmain->name,
		        is_relative_path, use_multiview, 1);
	}

	if (ima == NULL) {
		return OPERATOR_CANCELLED;
	}

	/* hook into UI */
	iod = op->customdata;

	if (iod->pprop.prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer use also increases user, so this compensates it */
		id_us_min(&ima->id);

		RNA_id_pointer_create(&ima->id, &idptr);
		RNA_property_pointer_set(&iod->pprop.ptr, iod->pprop.prop, idptr);
		RNA_property_update(C, &iod->pprop.ptr, iod->pprop.prop);
	}

	if (iod->iuser) {
		iuser = iod->iuser;
	}
	else if (sa && sa->spacetype == SPACE_IMAGE) {
		SpaceImage *sima = sa->spacedata.first;
		ED_space_image_set(sima, scene, obedit, ima);
		iuser = &sima->iuser;
	}
	else if (sa && sa->spacetype == SPACE_VIEW3D) {
		View3D *v3d = sa->spacedata.first;

		for (BGpic *bgpic = v3d->bgpicbase.first; bgpic; bgpic = bgpic->next) {
			if (bgpic->ima == ima) {
				iuser = &bgpic->iuser;
				break;
			}
		}
	}
	else {
		Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
		if (tex && tex->type == TEX_IMAGE) {
			iuser = &tex->iuser;
		}
	}

	/* initialize because of new image */
	if (iuser) {
		iuser->frames = frame_seq_len;
		iuser->sfra = 1;
		iuser->framenr = 1;
		if (ima->source == IMA_SRC_MOVIE) {
			iuser->offset = 0;
		} else {
			iuser->offset = frame_ofs - 1;
		}
		iuser->fie_ima = 2;
		iuser->scene = scene;
		BKE_image_init_imageuser(ima, iuser);
	}

	/* XXX unpackImage frees image buffers */
	ED_preview_kill_jobs(CTX_wm_manager(C), bmain);
	
	BKE_image_signal(ima, iuser, IMA_SIGNAL_RELOAD);
	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
	
	MEM_freeN(op->customdata);

	return OPERATOR_FINISHED;
}

static int image_open_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SpaceImage *sima = CTX_wm_space_image(C); /* XXX other space types can call */
	const char *path = U.textudir;
	Image *ima = NULL;
	Scene *scene = CTX_data_scene(C);

	if (sima) {
		ima = sima->image;
	}

	if (ima == NULL) {
		Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
		if (tex && tex->type == TEX_IMAGE)
			ima = tex->ima;
	}

	if (ima == NULL) {
		PointerRNA ptr;
		PropertyRNA *prop;

		/* hook into UI */
		UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

		if (prop) {
			PointerRNA oldptr;
			Image *oldima;

			oldptr = RNA_property_pointer_get(&ptr, prop);
			oldima = (Image *)oldptr.id.data;
			/* unlikely to fail but better avoid strange crash */
			if (oldima && GS(oldima->id.name) == ID_IM) {
				ima = oldima;
			}
		}
	}

	if (ima)
		path = ima->name;

	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return image_open_exec(C, op);
	
	image_open_init(C, op);

	/* show multiview save options only if scene has multiviews */
	PropertyRNA *prop;
	prop = RNA_struct_find_property(op->ptr, "show_multiview");
	RNA_property_boolean_set(op->ptr, prop, (scene->r.scemode & R_MULTIVIEW) != 0);

	image_filesel(C, op, path);

	return OPERATOR_RUNNING_MODAL;
}

static bool image_open_draw_check_prop(PointerRNA *UNUSED(ptr), PropertyRNA *prop)
{
	const char *prop_id = RNA_property_identifier(prop);

	return !(STREQ(prop_id, "filepath") ||
	         STREQ(prop_id, "directory") ||
	         STREQ(prop_id, "filename")
	         );
}

static void image_open_draw(bContext *UNUSED(C), wmOperator *op)
{
	uiLayout *layout = op->layout;
	ImageOpenData *iod = op->customdata;
	ImageFormatData *imf = &iod->im_format;
	PointerRNA imf_ptr, ptr;

	/* main draw call */
	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	uiDefAutoButsRNA(layout, &ptr, image_open_draw_check_prop, '\0');

	/* image template */
	RNA_pointer_create(NULL, &RNA_ImageFormatSettings, imf, &imf_ptr);

	/* multiview template */
	if (RNA_boolean_get(op->ptr, "show_multiview"))
		uiTemplateImageFormatViews(layout, &imf_ptr, op->ptr);
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
	ot->ui = image_open_draw;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE, FILE_SPECIAL, FILE_OPENFILE,
	        WM_FILESEL_FILEPATH | WM_FILESEL_DIRECTORY | WM_FILESEL_FILES | WM_FILESEL_RELPATH,
	        FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);

	RNA_def_boolean(ot->srna, "use_sequence_detection", true, "Detect Sequences",
	                "Automatically detect animated sequences in selected images (based on file names)");
}

/******************** Match movie length operator ********************/
static int image_match_len_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	Image *ima = CTX_data_pointer_get_type(C, "edit_image", &RNA_Image).data;
	ImageUser *iuser = CTX_data_pointer_get_type(C, "edit_image_user", &RNA_ImageUser).data;

	if (!ima || !iuser) {
		/* Try to get a Texture, or a SpaceImage from context... */
		SpaceImage *sima = CTX_wm_space_image(C);
		Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
		if (tex && tex->type == TEX_IMAGE) {
			ima = tex->ima;
			iuser = &tex->iuser;
		}
		else if (sima) {
			ima = sima->image;
			iuser = &sima->iuser;
		}
		
	}

	if (!ima || !iuser || !BKE_image_has_anim(ima))
		return OPERATOR_CANCELLED;

	struct anim *anim = ((ImageAnim *)ima->anims.first)->anim;
	if (!anim)
		return OPERATOR_CANCELLED;
	iuser->frames = IMB_anim_get_duration(anim, IMB_TC_RECORD_RUN);
	BKE_image_user_frame_calc(iuser, scene->r.cfra, 0);

	return OPERATOR_FINISHED;
}

/* called by other space types too */
void IMAGE_OT_match_movie_length(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Match Movie Length";
	ot->description = "Set image's user's length to the one of this video";
	ot->idname = "IMAGE_OT_match_movie_length";
	
	/* api callbacks */
	ot->exec = image_match_len_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL/* | OPTYPE_UNDO */; /* Don't think we need undo for that. */
}

/******************** replace image operator ********************/

static int image_replace_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	char str[FILE_MAX];

	if (!sima->image)
		return OPERATOR_CANCELLED;
	
	RNA_string_get(op->ptr, "filepath", str);

	/* we cant do much if the str is longer then FILE_MAX :/ */
	BLI_strncpy(sima->image->name, str, sizeof(sima->image->name));

	if (sima->image->source == IMA_SRC_GENERATED) {
		sima->image->source = IMA_SRC_FILE;
		BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_SRC_CHANGE);
	}
	
	if (BLI_testextensie_array(str, imb_ext_movie))
		sima->image->source = IMA_SRC_MOVIE;
	else
		sima->image->source = IMA_SRC_FILE;

	/* XXX unpackImage frees image buffers */
	ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));
	
	BKE_icon_changed(BKE_icon_id_ensure(&sima->image->id));
	BKE_image_signal(sima->image, &sima->iuser, IMA_SIGNAL_RELOAD);
	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, sima->image);

	return OPERATOR_FINISHED;
}

static int image_replace_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SpaceImage *sima = CTX_wm_space_image(C);

	if (!sima->image)
		return OPERATOR_CANCELLED;

	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return image_replace_exec(C, op);

	if (!RNA_struct_property_is_set(op->ptr, "relative_path"))
		RNA_boolean_set(op->ptr, "relative_path", BLI_path_is_rel(sima->image->name));

	image_filesel(C, op, sima->image->name);

	return OPERATOR_RUNNING_MODAL;
}

void IMAGE_OT_replace(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Replace Image";
	ot->idname = "IMAGE_OT_replace";
	ot->description = "Replace current image by another one from disk";
	
	/* api callbacks */
	ot->exec = image_replace_exec;
	ot->invoke = image_replace_invoke;
	ot->poll = image_not_packed_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE, FILE_SPECIAL, FILE_OPENFILE,
	        WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);
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
	BKE_imformat_defaults(&simopts->im_format);
	simopts->filepath[0] = '\0';
}

static char imtype_best_depth(ImBuf *ibuf, const char imtype)
{
	const char depth_ok = BKE_imtype_valid_depths(imtype);

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

static int save_image_options_init(SaveImageOptions *simopts, SpaceImage *sima, Scene *scene,
                                   const bool guess_path, const bool save_as_render)
{
	void *lock;
	ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock);

	if (ibuf) {
		Image *ima = sima->image;
		bool is_depth_set = false;

		if (ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
			/* imtype */
			simopts->im_format = scene->r.im_format;
			is_depth_set = true;
			if (!BKE_image_is_multiview(ima)) {
				/* In case multiview is disabled, render settings would be invalid for render result in this area. */
				simopts->im_format.stereo3d_format = *ima->stereo3d_format;
				simopts->im_format.views_format = ima->views_format;
			}
		}
		else {
			if (ima->source == IMA_SRC_GENERATED) {
				simopts->im_format.imtype = R_IMF_IMTYPE_PNG;
				simopts->im_format.compress = ibuf->foptions.quality;
				simopts->im_format.planes = ibuf->planes;
			}
			else {
				BKE_imbuf_to_image_format(&simopts->im_format, ibuf);
			}

			/* use the multiview image settings as the default */
			simopts->im_format.stereo3d_format = *ima->stereo3d_format;
			simopts->im_format.views_format = ima->views_format;
		}

		//simopts->subimtype = scene->r.subimtype; /* XXX - this is lame, we need to make these available too! */

		BLI_strncpy(simopts->filepath, ibuf->name, sizeof(simopts->filepath));

		/* sanitize all settings */

		/* unlikely but just in case */
		if (ELEM(simopts->im_format.planes, R_IMF_PLANES_BW, R_IMF_PLANES_RGB, R_IMF_PLANES_RGBA) == 0) {
			simopts->im_format.planes = R_IMF_PLANES_RGBA;
		}

		/* depth, account for float buffer and format support */
		if (is_depth_set == false) {
			simopts->im_format.depth = imtype_best_depth(ibuf, simopts->im_format.imtype);
		}

		/* some formats don't use quality so fallback to scenes quality */
		if (simopts->im_format.quality == 0) {
			simopts->im_format.quality = scene->r.im_format.quality;
		}

		/* check for empty path */
		if (guess_path && simopts->filepath[0] == 0) {
			const bool is_prev_save = !STREQ(G.ima, "//");
			if (save_as_render) {
				if (is_prev_save) {
					BLI_strncpy(simopts->filepath, G.ima, sizeof(simopts->filepath));
				}
				else {
					BLI_strncpy(simopts->filepath, "//untitled", sizeof(simopts->filepath));
					BLI_path_abs(simopts->filepath, G.main->name);
				}
			}
			else {
				BLI_snprintf(simopts->filepath, sizeof(simopts->filepath), "//%s", ima->id.name + 2);
				BLI_path_abs(simopts->filepath, is_prev_save ? G.ima : G.main->name);
			}
		}

		/* color management */
		BKE_color_managed_display_settings_copy(&simopts->im_format.display_settings, &scene->display_settings);
		BKE_color_managed_view_settings_copy(&simopts->im_format.view_settings, &scene->view_settings);
	}

	ED_space_image_release_buffer(sima, ibuf, lock);

	return (ibuf != NULL);
}

static void save_image_options_from_op(SaveImageOptions *simopts, wmOperator *op)
{
	if (op->customdata) {
		BKE_color_managed_view_settings_free(&simopts->im_format.view_settings);

		simopts->im_format = *(ImageFormatData *)op->customdata;
	}

	if (RNA_struct_property_is_set(op->ptr, "filepath")) {
		RNA_string_get(op->ptr, "filepath", simopts->filepath);
		BLI_path_abs(simopts->filepath, G.main->name);
	}
}

static void save_image_options_to_op(SaveImageOptions *simopts, wmOperator *op)
{
	if (op->customdata) {
		BKE_color_managed_view_settings_free(&((ImageFormatData *)op->customdata)->view_settings);

		*(ImageFormatData *)op->customdata = simopts->im_format;
	}

	RNA_string_set(op->ptr, "filepath", simopts->filepath);
}

static void save_image_post(
        wmOperator *op, ImBuf *ibuf, Image *ima, int ok, int save_copy,
        const char *relbase, int relative, int do_newpath, const char *filepath)
{
	if (ok) {
		if (!save_copy) {
			ColorManagedColorspaceSettings old_colorspace_settings;

			if (do_newpath) {
				BLI_strncpy(ibuf->name, filepath, sizeof(ibuf->name));
				BLI_strncpy(ima->name, filepath, sizeof(ima->name));
			}

			ibuf->userflags &= ~IB_BITMAPDIRTY;

			/* change type? */
			if (ima->type == IMA_TYPE_R_RESULT) {
				ima->type = IMA_TYPE_IMAGE;

				/* workaround to ensure the render result buffer is no longer used
				 * by this image, otherwise can crash when a new render result is
				 * created. */
				if (ibuf->rect && !(ibuf->mall & IB_rect))
					imb_freerectImBuf(ibuf);
				if (ibuf->rect_float && !(ibuf->mall & IB_rectfloat))
					imb_freerectfloatImBuf(ibuf);
				if (ibuf->zbuf && !(ibuf->mall & IB_zbuf))
					IMB_freezbufImBuf(ibuf);
				if (ibuf->zbuf_float && !(ibuf->mall & IB_zbuffloat))
					IMB_freezbuffloatImBuf(ibuf);
			}
			if (ELEM(ima->source, IMA_SRC_GENERATED, IMA_SRC_VIEWER)) {
				ima->source = IMA_SRC_FILE;
				ima->type = IMA_TYPE_IMAGE;
			}

			/* only image path, never ibuf */
			if (relative) {
				BLI_path_rel(ima->name, relbase); /* only after saving */
			}

			BKE_color_managed_colorspace_settings_copy(&old_colorspace_settings,
			                                           &ima->colorspace_settings);
			IMB_colormanagment_colorspace_from_ibuf_ftype(&ima->colorspace_settings, ibuf);
			if (!BKE_color_managed_colorspace_settings_equals(&old_colorspace_settings,
			                                                  &ima->colorspace_settings))
			{
				BKE_image_signal(ima, NULL, IMA_SIGNAL_COLORMANAGE);
			}
		}
	}
	else {
		BKE_reportf(op->reports, RPT_ERROR, "Could not write image: %s", strerror(errno));
	}
}

static void save_imbuf_post(ImBuf *ibuf, ImBuf *colormanaged_ibuf)
{
	if (colormanaged_ibuf != ibuf) {
		/* This guys might be modified by image buffer write functions,
		 * need to copy them back from color managed image buffer to an
		 * original one, so file type of image is being properly updated.
		 */
		ibuf->ftype = colormanaged_ibuf->ftype;
		ibuf->foptions = colormanaged_ibuf->foptions;
		ibuf->planes = colormanaged_ibuf->planes;

		IMB_freeImBuf(colormanaged_ibuf);
	}
}

/**
 * \return success.
 * \note ``ima->name`` and ``ibuf->name`` should end up the same.
 * \note for multiview the first ``ibuf`` is important to get the settings.
 */
static bool save_image_doit(bContext *C, SpaceImage *sima, wmOperator *op, SaveImageOptions *simopts, bool do_newpath)
{
	Image *ima = ED_space_image(sima);
	void *lock;
	ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock);
	Scene *scene;
	RenderResult *rr = NULL;
	bool ok = false;

	WM_cursor_wait(1);

	if (ibuf) {
		ImBuf *colormanaged_ibuf = NULL;
		const char *relbase = ID_BLEND_PATH(CTX_data_main(C), &ima->id);
		const bool relative = (RNA_struct_find_property(op->ptr, "relative_path") && RNA_boolean_get(op->ptr, "relative_path"));
		const bool save_copy = (RNA_struct_find_property(op->ptr, "copy") && RNA_boolean_get(op->ptr, "copy"));
		const bool save_as_render = (RNA_struct_find_property(op->ptr, "save_as_render") && RNA_boolean_get(op->ptr, "save_as_render"));
		ImageFormatData *imf = &simopts->im_format;

		const bool is_multilayer = imf->imtype == R_IMF_IMTYPE_MULTILAYER;
		bool is_mono;

		/* old global to ensure a 2nd save goes to same dir */
		BLI_strncpy(G.ima, simopts->filepath, sizeof(G.ima));

		if (ima->type == IMA_TYPE_R_RESULT) {
			/* enforce user setting for RGB or RGBA, but skip BW */
			if (simopts->im_format.planes == R_IMF_PLANES_RGBA) {
				ibuf->planes = R_IMF_PLANES_RGBA;
			}
			else if (simopts->im_format.planes == R_IMF_PLANES_RGB) {
				ibuf->planes = R_IMF_PLANES_RGB;
			}
		}
		else {
			/* TODO, better solution, if a 24bit image is painted onto it may contain alpha */
			if ((simopts->im_format.planes == R_IMF_PLANES_RGBA) &&
			    /* it has been painted onto */
			    (ibuf->userflags & IB_BITMAPDIRTY))
			{
				/* checks each pixel, not ideal */
				ibuf->planes = BKE_imbuf_alpha_test(ibuf) ? R_IMF_PLANES_RGBA : R_IMF_PLANES_RGB;
			}
		}

		/* we need renderresult for exr and rendered multiview */
		scene = CTX_data_scene(C);
		rr = BKE_image_acquire_renderresult(scene, ima);
		is_mono = rr ? BLI_listbase_count_ex(&rr->views, 2) < 2 : BLI_listbase_count_ex(&ima->views, 2) < 2;

		/* error handling */
		if (!rr) {
			if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
				BKE_report(op->reports, RPT_ERROR, "Did not write, no Multilayer Image");
				goto cleanup;
			}
		}
		else {
			if (imf->views_format == R_IMF_VIEWS_STEREO_3D) {
				if (!BKE_image_is_stereo(ima)) {
					BKE_reportf(op->reports, RPT_ERROR, "Did not write, the image doesn't have a \"%s\" and \"%s\" views",
					           STEREO_LEFT_NAME, STEREO_RIGHT_NAME);
					goto cleanup;
				}

				/* it shouldn't ever happen*/
				if ((BLI_findstring(&rr->views, STEREO_LEFT_NAME, offsetof(RenderView, name)) == NULL) ||
				    (BLI_findstring(&rr->views, STEREO_RIGHT_NAME, offsetof(RenderView, name)) == NULL))
				{
					BKE_reportf(op->reports, RPT_ERROR, "Did not write, the image doesn't have a \"%s\" and \"%s\" views",
					           STEREO_LEFT_NAME, STEREO_RIGHT_NAME);
					goto cleanup;
				}
			}
			BKE_imbuf_stamp_info(rr, ibuf);
		}

		/* fancy multiview OpenEXR */
		if ((imf->imtype == R_IMF_IMTYPE_MULTILAYER) && (imf->views_format == R_IMF_VIEWS_MULTIVIEW)) {
			ok = RE_WriteRenderResult(op->reports, rr, simopts->filepath, imf, true, NULL);
			save_image_post(op, ibuf, ima, ok, true, relbase, relative, do_newpath, simopts->filepath);
			ED_space_image_release_buffer(sima, ibuf, lock);
		}
		else if ((imf->imtype == R_IMF_IMTYPE_OPENEXR) && (imf->views_format == R_IMF_VIEWS_MULTIVIEW)) {
			/* treat special Openexr case separetely (this is the singlelayer multiview OpenEXR */
			BKE_imbuf_write_prepare(ibuf, imf);
			ok = BKE_image_save_openexr_multiview(ima, ibuf, simopts->filepath, (IB_rect | IB_zbuf | IB_zbuffloat | IB_multiview));
			ED_space_image_release_buffer(sima, ibuf, lock);
		}
		/* regular mono pipeline */
		else if (is_mono) {
			if (is_multilayer) {
				ok = RE_WriteRenderResult(op->reports, rr, simopts->filepath, imf, false, NULL);
			}
			else {
				colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(ibuf, save_as_render, true, &imf->view_settings, &imf->display_settings, imf);
				ok = BKE_imbuf_write_as(colormanaged_ibuf, simopts->filepath, imf, save_copy);
				save_imbuf_post(ibuf, colormanaged_ibuf);
			}
			save_image_post(op, ibuf, ima, ok, (is_multilayer ? true : save_copy), relbase, relative, do_newpath, simopts->filepath);
			ED_space_image_release_buffer(sima, ibuf, lock);
		}
		/* individual multiview images */
		else if (imf->views_format == R_IMF_VIEWS_INDIVIDUAL) {
			int i;
			unsigned char planes = ibuf->planes;
			const int totviews = (rr ? BLI_listbase_count(&rr->views) : BLI_listbase_count(&ima->views));

			if (!is_multilayer) {
				ED_space_image_release_buffer(sima, ibuf, lock);
			}

			for (i = 0; i < totviews; i++) {
				char filepath[FILE_MAX];
				bool ok_view = false;
				const char *view = rr ? ((RenderView *) BLI_findlink(&rr->views, i))->name :
				                        ((ImageView *) BLI_findlink(&ima->views, i))->name;

				if (is_multilayer) {
					BKE_scene_multiview_view_filepath_get(&scene->r, simopts->filepath, view, filepath);
					ok_view = RE_WriteRenderResult(op->reports, rr, filepath, imf, false, view);
					save_image_post(op, ibuf, ima, ok_view, true, relbase, relative, do_newpath, filepath);
				}
				else {
					/* copy iuser to get the correct ibuf for this view */
					ImageUser iuser = sima->iuser;
					iuser.view = i;
					iuser.flag &= ~IMA_SHOW_STEREO;

					if (rr)
						BKE_image_multilayer_index(rr, &iuser);
					else
						BKE_image_multiview_index(ima, &iuser);

					ibuf = BKE_image_acquire_ibuf(sima->image, &iuser, &lock);
					ibuf->planes = planes;

					BKE_scene_multiview_view_filepath_get(&scene->r, simopts->filepath, view, filepath);

					colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(ibuf, save_as_render, true, &imf->view_settings, &imf->display_settings, imf);
					ok_view = BKE_imbuf_write_as(colormanaged_ibuf, filepath, &simopts->im_format, save_copy);
					save_imbuf_post(ibuf, colormanaged_ibuf);
					save_image_post(op, ibuf, ima, ok_view, true, relbase, relative, do_newpath, filepath);
					BKE_image_release_ibuf(sima->image, ibuf, lock);
				}
				ok &= ok_view;
			}

			if (is_multilayer) {
				ED_space_image_release_buffer(sima, ibuf, lock);
			}
		}
		/* stereo (multiview) images */
		else if (simopts->im_format.views_format == R_IMF_VIEWS_STEREO_3D) {
			if (imf->imtype == R_IMF_IMTYPE_MULTILAYER) {
				ok = RE_WriteRenderResult(op->reports, rr, simopts->filepath, imf, false, NULL);
				save_image_post(op, ibuf, ima, ok, true, relbase, relative, do_newpath, simopts->filepath);
				ED_space_image_release_buffer(sima, ibuf, lock);
			}
			else {
				ImBuf *ibuf_stereo[2] = {NULL};

				unsigned char planes = ibuf->planes;
				const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
				int i;

				/* we need to get the specific per-view buffers */
				ED_space_image_release_buffer(sima, ibuf, lock);

				for (i = 0; i < 2; i ++) {
					ImageUser iuser = sima->iuser;
					iuser.flag &= ~IMA_SHOW_STEREO;

					if (rr) {
						int id = BLI_findstringindex(&rr->views, names[i], offsetof(RenderView, name));
						iuser.view = id;
						BKE_image_multilayer_index(rr, &iuser);
					}
					else {
						iuser.view = i;
						BKE_image_multiview_index(ima, &iuser);
					}

					ibuf = BKE_image_acquire_ibuf(sima->image, &iuser, &lock);

					if (ibuf == NULL) {
						BKE_report(op->reports, RPT_ERROR, "Did not write, unexpected error when saving stereo image");
						goto cleanup;
					}

					ibuf->planes = planes;

					/* color manage the ImBuf leaving it ready for saving */
					colormanaged_ibuf = IMB_colormanagement_imbuf_for_write(ibuf, save_as_render, true,
					                                                        &imf->view_settings, &imf->display_settings, imf);

					BKE_imbuf_write_prepare(colormanaged_ibuf, imf);
					IMB_prepare_write_ImBuf(IMB_isfloat(colormanaged_ibuf), colormanaged_ibuf);

					/* duplicate buffer to prevent locker issue when using render result */
					ibuf_stereo[i] = IMB_dupImBuf(colormanaged_ibuf);

					save_imbuf_post(ibuf, colormanaged_ibuf);
					BKE_image_release_ibuf(sima->image, ibuf, lock);
				}

				ibuf = IMB_stereo3d_ImBuf(imf, ibuf_stereo[0], ibuf_stereo[1]);

				/* save via traditional path */
				ok = BKE_imbuf_write_as(ibuf, simopts->filepath, imf, save_copy);

				IMB_freeImBuf(ibuf);

				for (i = 0; i < 2; i ++) {
					IMB_freeImBuf(ibuf_stereo[i]);
				}
			}
		}

		WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, sima->image);

	}
	else {
cleanup:
		ED_space_image_release_buffer(sima, ibuf, lock);
	}

	if (rr) {
		BKE_image_release_renderresult(scene, ima);
	}

	WM_cursor_wait(0);

	return ok;
}

static void image_save_as_free(wmOperator *op)
{
	if (op->customdata) {
		ImageFormatData *im_format = (ImageFormatData *)op->customdata;
		BKE_color_managed_view_settings_free(&im_format->view_settings);

		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}
}

static int image_save_as_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	SaveImageOptions simopts;

	save_image_options_defaults(&simopts);

	/* just in case to initialize values,
	 * these should be set on invoke or by the caller. */
	save_image_options_init(&simopts, sima, CTX_data_scene(C), false, false);

	save_image_options_from_op(&simopts, op);

	save_image_doit(C, sima, op, &simopts, true);

	image_save_as_free(op);
	return OPERATOR_FINISHED;
}


static bool image_save_as_check(bContext *UNUSED(C), wmOperator *op)
{
	ImageFormatData *imf = op->customdata;
	return WM_operator_filesel_ensure_ext_imtype(op, imf);
}

static int image_save_as_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Image *ima = ED_space_image(sima);
	Scene *scene = CTX_data_scene(C);
	SaveImageOptions simopts;
	PropertyRNA *prop;
	const bool save_as_render = ((ima->source == IMA_SRC_VIEWER) || (ima->flag & IMA_VIEW_AS_RENDER));

	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return image_save_as_exec(C, op);

	save_image_options_defaults(&simopts);

	if (save_image_options_init(&simopts, sima, scene, true, save_as_render) == 0)
		return OPERATOR_CANCELLED;
	save_image_options_to_op(&simopts, op);

	/* enable save_copy by default for render results */
	if (ELEM(ima->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE) && !RNA_struct_property_is_set(op->ptr, "copy")) {
		RNA_boolean_set(op->ptr, "copy", true);
	}

	RNA_boolean_set(op->ptr, "save_as_render", save_as_render);

	op->customdata = MEM_mallocN(sizeof(simopts.im_format), __func__);
	memcpy(op->customdata, &simopts.im_format, sizeof(simopts.im_format));

	/* show multiview save options only if image has multiviews */
	prop = RNA_struct_find_property(op->ptr, "show_multiview");
	RNA_property_boolean_set(op->ptr, prop, BKE_image_is_multiview(ima));
	prop = RNA_struct_find_property(op->ptr, "use_multiview");
	RNA_property_boolean_set(op->ptr, prop, BKE_image_is_multiview(ima));

	image_filesel(C, op, simopts.filepath);

	return OPERATOR_RUNNING_MODAL;
}

static void image_save_as_cancel(bContext *UNUSED(C), wmOperator *op)
{
	image_save_as_free(op);
}

static bool image_save_as_draw_check_prop(PointerRNA *ptr, PropertyRNA *prop)
{
	const char *prop_id = RNA_property_identifier(prop);

	return !(STREQ(prop_id, "filepath") ||
	         STREQ(prop_id, "directory") ||
	         STREQ(prop_id, "filename") ||
	         /* when saving a copy, relative path has no effect */
	         ((STREQ(prop_id, "relative_path")) && RNA_boolean_get(ptr, "copy"))
	         );
}

static void image_save_as_draw(bContext *UNUSED(C), wmOperator *op)
{
	uiLayout *layout = op->layout;
	ImageFormatData *imf = op->customdata;
	PointerRNA imf_ptr, ptr;
	const bool is_multiview = RNA_boolean_get(op->ptr, "show_multiview");

	/* image template */
	RNA_pointer_create(NULL, &RNA_ImageFormatSettings, imf, &imf_ptr);
	uiTemplateImageSettings(layout, &imf_ptr, false);

	/* main draw call */
	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
	uiDefAutoButsRNA(layout, &ptr, image_save_as_draw_check_prop, '\0');

	/* multiview template */
	if (is_multiview)
		uiTemplateImageFormatViews(layout, &imf_ptr, op->ptr);
}

static int image_save_as_poll(bContext *C)
{
	if (space_image_buffer_exists_poll(C)) {
		if (G.is_rendering) {
			/* no need to NULL check here */
			SpaceImage *sima = CTX_wm_space_image(C);
			Image *ima = ED_space_image(sima);

			if (ima->source == IMA_SRC_VIEWER) {
				CTX_wm_operator_poll_msg_set(C, "can't save image while rendering");
				return false;
			}
		}
		return true;
	}
	return false;
}

void IMAGE_OT_save_as(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Save As Image";
	ot->idname = "IMAGE_OT_save_as";
	ot->description = "Save the image with another name and/or settings";
	
	/* api callbacks */
	ot->exec = image_save_as_exec;
	ot->check = image_save_as_check;
	ot->invoke = image_save_as_invoke;
	ot->cancel = image_save_as_cancel;
	ot->ui = image_save_as_draw;
	ot->poll = image_save_as_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "save_as_render", 0, "Save As Render", "Apply render part of display transform when saving byte image");
	RNA_def_boolean(ot->srna, "copy", 0, "Copy", "Create a new image file without modifying the current image in blender");

	WM_operator_properties_filesel(
	        ot, FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE, FILE_SPECIAL, FILE_SAVE,
	        WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY, FILE_SORT_ALPHA);
}

/******************** save image operator ********************/

static int image_save_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	SaveImageOptions simopts;

	save_image_options_defaults(&simopts);
	if (save_image_options_init(&simopts, sima, scene, false, false) == 0)
		return OPERATOR_CANCELLED;
	save_image_options_from_op(&simopts, op);

	if (BLI_exists(simopts.filepath) && BLI_file_is_writable(simopts.filepath)) {
		if (save_image_doit(C, sima, op, &simopts, false)) {
			/* report since this can be called from key-shortcuts */
			BKE_reportf(op->reports, RPT_INFO, "Saved Image '%s'", simopts.filepath);
		}
	}
	else {
		BKE_reportf(op->reports, RPT_ERROR, "Cannot save image, path '%s' is not writable", simopts.filepath);
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

void IMAGE_OT_save(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Save Image";
	ot->idname = "IMAGE_OT_save";
	ot->description = "Save the image with current name and settings";
	
	/* api callbacks */
	ot->exec = image_save_exec;
	ot->poll = space_image_file_exists_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************* save sequence operator ********************/

static int image_save_sequence_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	SpaceImage *sima = CTX_wm_space_image(C);
	ImBuf *ibuf, *first_ibuf = NULL;
	int tot = 0;
	char di[FILE_MAX];
	struct MovieCacheIter *iter;
	
	if (sima->image == NULL)
		return OPERATOR_CANCELLED;

	if (sima->image->source != IMA_SRC_SEQUENCE) {
		BKE_report(op->reports, RPT_ERROR, "Can only save sequence on image sequences");
		return OPERATOR_CANCELLED;
	}

	if (sima->image->type == IMA_TYPE_MULTILAYER) {
		BKE_report(op->reports, RPT_ERROR, "Cannot save multilayer sequences");
		return OPERATOR_CANCELLED;
	}
	
	/* get total dirty buffers and first dirty buffer which is used for menu */
	ibuf = NULL;
	if (sima->image->cache != NULL) {
		iter = IMB_moviecacheIter_new(sima->image->cache);
		while (!IMB_moviecacheIter_done(iter)) {
			ibuf = IMB_moviecacheIter_getImBuf(iter);
			if (ibuf->userflags & IB_BITMAPDIRTY) {
				if (first_ibuf == NULL) {
					first_ibuf = ibuf;
				}
				tot++;
			}
			IMB_moviecacheIter_step(iter);
		}
		IMB_moviecacheIter_free(iter);
	}
	
	if (tot == 0) {
		BKE_report(op->reports, RPT_WARNING, "No images have been changed");
		return OPERATOR_CANCELLED;
	}

	/* get a filename for menu */
	BLI_split_dir_part(first_ibuf->name, di, sizeof(di));
	BKE_reportf(op->reports, RPT_INFO, "%d image(s) will be saved in %s", tot, di);

	iter = IMB_moviecacheIter_new(sima->image->cache);
	while (!IMB_moviecacheIter_done(iter)) {
		ibuf = IMB_moviecacheIter_getImBuf(iter);

		if (ibuf->userflags & IB_BITMAPDIRTY) {
			char name[FILE_MAX];
			BLI_strncpy(name, ibuf->name, sizeof(name));

			BLI_path_abs(name, bmain->name);

			if (0 == IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat)) {
				BKE_reportf(op->reports, RPT_ERROR, "Could not write image: %s", strerror(errno));
				break;
			}

			BKE_reportf(op->reports, RPT_INFO, "Saved %s", ibuf->name);
			ibuf->userflags &= ~IB_BITMAPDIRTY;
		}

		IMB_moviecacheIter_step(iter);
	}
	IMB_moviecacheIter_free(iter);

	return OPERATOR_FINISHED;
}

void IMAGE_OT_save_sequence(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Save Sequence";
	ot->idname = "IMAGE_OT_save_sequence";
	ot->description = "Save a sequence of images";
	
	/* api callbacks */
	ot->exec = image_save_sequence_exec;
	ot->poll = space_image_buffer_exists_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/******************** reload image operator ********************/

static int image_reload_exec(bContext *C, wmOperator *UNUSED(op))
{
	Image *ima = CTX_data_edit_image(C);
	SpaceImage *sima = CTX_wm_space_image(C);

	if (!ima)
		return OPERATOR_CANCELLED;

	/* XXX unpackImage frees image buffers */
	ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));
	
	// XXX other users?
	BKE_image_signal(ima, (sima) ? &sima->iuser : NULL, IMA_SIGNAL_RELOAD);
	DAG_id_tag_update(&ima->id, 0);

	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_reload(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reload Image";
	ot->idname = "IMAGE_OT_reload";
	ot->description = "Reload current image from disk";
	
	/* api callbacks */
	ot->exec = image_reload_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER; /* no undo, image buffer is not handled by undo */
}

/********************** new image operator *********************/
#define IMA_DEF_NAME N_("Untitled")

enum {
	GEN_CONTEXT_NONE = 0,
	GEN_CONTEXT_PAINT_CANVAS = 1,
	GEN_CONTEXT_PAINT_STENCIL = 2
};

static int image_new_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima;
	Scene *scene;
	Object *obedit;
	Image *ima;
	Main *bmain;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	char _name[MAX_ID_NAME - 2];
	char *name = _name;
	float color[4];
	int width, height, floatbuf, gen_type, alpha;
	int gen_context;
	int stereo3d;

	/* retrieve state */
	sima = CTX_wm_space_image(C);
	scene = CTX_data_scene(C);
	obedit = CTX_data_edit_object(C);
	bmain = CTX_data_main(C);

	prop = RNA_struct_find_property(op->ptr, "name");
	RNA_property_string_get(op->ptr, prop, name);
	if (!RNA_property_is_set(op->ptr, prop)) {
		/* Default value, we can translate! */
		name = (char *)DATA_(name);
	}
	width = RNA_int_get(op->ptr, "width");
	height = RNA_int_get(op->ptr, "height");
	floatbuf = RNA_boolean_get(op->ptr, "float");
	gen_type = RNA_enum_get(op->ptr, "generated_type");
	RNA_float_get_array(op->ptr, "color", color);
	alpha = RNA_boolean_get(op->ptr, "alpha");
	gen_context = RNA_enum_get(op->ptr, "gen_context");
	stereo3d = RNA_boolean_get(op->ptr, "use_stereo_3d");

	if (!alpha)
		color[3] = 1.0f;

	ima = BKE_image_add_generated(bmain, width, height, name, alpha ? 32 : 24, floatbuf, gen_type, color, stereo3d);

	if (!ima)
		return OPERATOR_CANCELLED;

	/* hook into UI */
	UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

	if (prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer use also increases user, so this compensates it */
		id_us_min(&ima->id);

		RNA_id_pointer_create(&ima->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	else if (sima) {
		ED_space_image_set(sima, scene, obedit, ima);
	}
	else if (gen_context == GEN_CONTEXT_PAINT_CANVAS) {
		bScreen *sc;
		Object *ob = CTX_data_active_object(C);
		
		GPU_drawobject_free(ob->derivedFinal);	
		if (scene->toolsettings->imapaint.canvas)
			id_us_min(&scene->toolsettings->imapaint.canvas->id);
		scene->toolsettings->imapaint.canvas = ima;
		
		for (sc = bmain->screen.first; sc; sc = sc->id.next) {
			ScrArea *sa;
			for (sa = sc->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_IMAGE) {
						SpaceImage *sima_other = (SpaceImage *)sl;
						
						if (!sima_other->pin) {
							ED_space_image_set(sima_other, scene, scene->obedit, ima);
						}
					}
				}
			}
		}
		BKE_paint_proj_mesh_data_check(scene, ob, NULL, NULL, NULL, NULL);
		WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);
	}
	else if (gen_context == GEN_CONTEXT_PAINT_STENCIL) {
		Object *ob = CTX_data_active_object(C);
		if (scene->toolsettings->imapaint.stencil)
			id_us_min(&scene->toolsettings->imapaint.stencil->id);
		scene->toolsettings->imapaint.stencil = ima;
		BKE_paint_proj_mesh_data_check(scene, ob, NULL, NULL, NULL, NULL);	
		WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, NULL);		
	}
	else {
		Tex *tex = CTX_data_pointer_get_type(C, "texture", &RNA_Texture).data;
		if (tex && tex->type == TEX_IMAGE) {
			if (tex->ima)
				id_us_min(&tex->ima->id);
			tex->ima = ima;
			ED_area_tag_redraw(CTX_wm_area(C));
		}
	}

	BKE_image_signal(ima, (sima) ? &sima->iuser : NULL, IMA_SIGNAL_USER_NEW_IMAGE);
	
	WM_event_add_notifier(C, NC_IMAGE | NA_ADDED, ima);
	
	return OPERATOR_FINISHED;
}

/* XXX, Ton is not a fan of OK buttons but using this function to avoid undo/redo bug while in mesh-editmode, - campbell */
/* XXX Note: the WM_operator_props_dialog_popup() doesn't work for UI_context_active_but_prop_get_templateID(), image is not being that way */
static int image_new_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	/* Better for user feedback. */
	RNA_string_set(op->ptr, "name", DATA_(IMA_DEF_NAME));
	return WM_operator_props_dialog_popup(C, op, 15 * UI_UNIT_X, 5 * UI_UNIT_Y);
}

static void image_new_draw(bContext *UNUSED(C), wmOperator *op)
{
	uiLayout *split, *col[2];
	uiLayout *layout = op->layout;
	PointerRNA ptr;
#if 0
	Scene *scene = CTX_data_scene(C);
	const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;
#endif

	RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);

	/* copy of WM_operator_props_dialog_popup() layout */

	split = uiLayoutSplit(layout, 0.5f, false);
	col[0] = uiLayoutColumn(split, false);
	col[1] = uiLayoutColumn(split, false);

	uiItemL(col[0], IFACE_("Name"), ICON_NONE);
	uiItemR(col[1], &ptr, "name", 0, "", ICON_NONE);

	uiItemL(col[0], IFACE_("Width"), ICON_NONE);
	uiItemR(col[1], &ptr, "width", 0, "", ICON_NONE);

	uiItemL(col[0], IFACE_("Height"), ICON_NONE);
	uiItemR(col[1], &ptr, "height", 0, "", ICON_NONE);

	uiItemL(col[0], IFACE_("Color"), ICON_NONE);
	uiItemR(col[1], &ptr, "color", 0, "", ICON_NONE);

	uiItemL(col[0], "", ICON_NONE);
	uiItemR(col[1], &ptr, "alpha", 0, NULL, ICON_NONE);

	uiItemL(col[0], IFACE_("Generated Type"), ICON_NONE);
	uiItemR(col[1], &ptr, "generated_type", 0, "", ICON_NONE);

	uiItemL(col[0], "", ICON_NONE);
	uiItemR(col[1], &ptr, "float", 0, NULL, ICON_NONE);

#if 0
	if (is_multiview) {
		uiItemL(col[0], "", ICON_NONE);
		uiItemR(col[1], &ptr, "use_stereo_3d", 0, NULL, ICON_NONE);
	}
#endif
}

void IMAGE_OT_new(wmOperatorType *ot)
{
	PropertyRNA *prop;
	static float default_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	static EnumPropertyItem gen_context_items[] = {
		{GEN_CONTEXT_NONE, "NONE", 0, "None", ""},
		{GEN_CONTEXT_PAINT_CANVAS, "PAINT_CANVAS", 0, "Paint Canvas", ""},
		{GEN_CONTEXT_PAINT_STENCIL, "PAINT_STENCIL", 0, "Paint Stencil", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "New Image";
	ot->description = "Create a new image";
	ot->idname = "IMAGE_OT_new";
	
	/* api callbacks */
	ot->exec = image_new_exec;
	ot->invoke = image_new_invoke;
	ot->ui = image_new_draw;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* properties */
	RNA_def_string(ot->srna, "name", IMA_DEF_NAME, MAX_ID_NAME - 2, "Name", "Image data-block name");
	prop = RNA_def_int(ot->srna, "width", 1024, 1, INT_MAX, "Width", "Image width", 1, 16384);
	RNA_def_property_subtype(prop, PROP_PIXEL);
	prop = RNA_def_int(ot->srna, "height", 1024, 1, INT_MAX, "Height", "Image height", 1, 16384);
	RNA_def_property_subtype(prop, PROP_PIXEL);
	prop = RNA_def_float_color(ot->srna, "color", 4, NULL, 0.0f, FLT_MAX, "Color", "Default fill color", 0.0f, 1.0f);
	RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
	RNA_def_property_float_array_default(prop, default_color);
	RNA_def_boolean(ot->srna, "alpha", 1, "Alpha", "Create an image with an alpha channel");
	RNA_def_enum(ot->srna, "generated_type", rna_enum_image_generated_type_items, IMA_GENTYPE_BLANK,
	             "Generated Type", "Fill the image with a grid for UV map testing");
	RNA_def_boolean(ot->srna, "float", 0, "32 bit Float", "Create image with 32 bit floating point bit depth");
	prop = RNA_def_enum(ot->srna, "gen_context", gen_context_items, 0, "Gen Context", "Generation context");
	RNA_def_property_flag(prop, PROP_HIDDEN);
	prop = RNA_def_boolean(ot->srna, "use_stereo_3d", 0, "Stereo 3D", "Create an image with left and right views");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

#undef IMA_DEF_NAME

/********************* invert operators *********************/

static int image_invert_poll(bContext *C)
{
	Image *ima = CTX_data_edit_image(C);

	return BKE_image_has_ibuf(ima, NULL);
}

static int image_invert_exec(bContext *C, wmOperator *op)
{
	Image *ima = CTX_data_edit_image(C);
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
	SpaceImage *sima = CTX_wm_space_image(C);
	/* undo is supported only on image paint mode currently */
	bool support_undo = ((sima != NULL) && (sima->mode == SI_MODE_PAINT));

	/* flags indicate if this channel should be inverted */
	const bool r = RNA_boolean_get(op->ptr, "invert_r");
	const bool g = RNA_boolean_get(op->ptr, "invert_g");
	const bool b = RNA_boolean_get(op->ptr, "invert_b");
	const bool a = RNA_boolean_get(op->ptr, "invert_a");

	size_t i;

	if (ibuf == NULL)  /* TODO: this should actually never happen, but does for render-results -> cleanup */
		return OPERATOR_CANCELLED;

	if (support_undo) {
		ED_undo_paint_push_begin(UNDO_PAINT_IMAGE, op->type->name,
		                         ED_image_undo_restore, ED_image_undo_free, NULL);
		/* not strictly needed, because we only imapaint_dirty_region to invalidate all tiles
		 * but better do this right in case someone copies this for a tool that uses partial redraw better */
		ED_imapaint_clear_partial_redraw();
		ED_imapaint_dirty_region(ima, ibuf, 0, 0, ibuf->x, ibuf->y, false);
	}
	/* TODO: make this into an IMB_invert_channels(ibuf,r,g,b,a) method!? */
	if (ibuf->rect_float) {
		
		float *fp = (float *) ibuf->rect_float;
		for (i = ((size_t)ibuf->x) * ibuf->y; i > 0; i--, fp += 4) {
			if (r) fp[0] = 1.0f - fp[0];
			if (g) fp[1] = 1.0f - fp[1];
			if (b) fp[2] = 1.0f - fp[2];
			if (a) fp[3] = 1.0f - fp[3];
		}

		if (ibuf->rect) {
			IMB_rect_from_float(ibuf);
		}
	}
	else if (ibuf->rect) {
		
		char *cp = (char *) ibuf->rect;
		for (i = ((size_t)ibuf->x) * ibuf->y; i > 0; i--, cp += 4) {
			if (r) cp[0] = 255 - cp[0];
			if (g) cp[1] = 255 - cp[1];
			if (b) cp[2] = 255 - cp[2];
			if (a) cp[3] = 255 - cp[3];
		}
	}
	else {
		BKE_image_release_ibuf(ima, ibuf, NULL);
		return OPERATOR_CANCELLED;
	}

	ibuf->userflags |= IB_BITMAPDIRTY | IB_DISPLAY_BUFFER_INVALID;

	if (ibuf->mipmap[0])
		ibuf->userflags |= IB_MIPMAP_INVALID;

	if (support_undo)
		ED_undo_paint_push_end(UNDO_PAINT_IMAGE);

	/* force GPU reupload, all image is invalid */
	GPU_free_image(ima);

	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

	BKE_image_release_ibuf(ima, ibuf, NULL);

	return OPERATOR_FINISHED;
}

void IMAGE_OT_invert(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Invert Channels";
	ot->idname = "IMAGE_OT_invert";
	ot->description = "Invert image's channels";
	
	/* api callbacks */
	ot->exec = image_invert_exec;
	ot->poll = image_invert_poll;
	
	/* properties */
	prop = RNA_def_boolean(ot->srna, "invert_r", 0, "Red", "Invert Red Channel");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "invert_g", 0, "Green", "Invert Green Channel");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "invert_b", 0, "Blue", "Invert Blue Channel");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_boolean(ot->srna, "invert_a", 0, "Alpha", "Invert Alpha Channel");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* pack operator *********************/

static bool image_pack_test(bContext *C, wmOperator *op)
{
	Image *ima = CTX_data_edit_image(C);
	const bool as_png = RNA_boolean_get(op->ptr, "as_png");

	if (!ima)
		return 0;
	if (!as_png && BKE_image_has_packedfile(ima))
		return 0;

	if (ima->source == IMA_SRC_SEQUENCE || ima->source == IMA_SRC_MOVIE) {
		BKE_report(op->reports, RPT_ERROR, "Packing movies or image sequences not supported");
		return 0;
	}

	return 1;
}

static int image_pack_exec(bContext *C, wmOperator *op)
{
	struct Main *bmain = CTX_data_main(C);
	Image *ima = CTX_data_edit_image(C);
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
	const bool as_png = RNA_boolean_get(op->ptr, "as_png");

	if (!image_pack_test(C, op))
		return OPERATOR_CANCELLED;
	
	if (!as_png && (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))) {
		BKE_report(op->reports, RPT_ERROR, "Cannot pack edited image from disk, only as internal PNG");
		return OPERATOR_CANCELLED;
	}

	if (as_png)
		BKE_image_memorypack(ima);
	else
		BKE_image_packfiles(op->reports, ima, ID_BLEND_PATH(bmain, &ima->id));

	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

	BKE_image_release_ibuf(ima, ibuf, NULL);

	return OPERATOR_FINISHED;
}

static int image_pack_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Image *ima = CTX_data_edit_image(C);
	ImBuf *ibuf;
	uiPopupMenu *pup;
	uiLayout *layout;
	const bool as_png = RNA_boolean_get(op->ptr, "as_png");

	if (!image_pack_test(C, op))
		return OPERATOR_CANCELLED;

	ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);

	if (!as_png && (ibuf && (ibuf->userflags & IB_BITMAPDIRTY))) {
		pup = UI_popup_menu_begin(C, IFACE_("OK"), ICON_QUESTION);
		layout = UI_popup_menu_layout(pup);
		uiItemBooleanO(layout, IFACE_("Can't pack edited image from disk, pack as internal PNG?"), ICON_NONE,
		               op->idname, "as_png", 1);
		UI_popup_menu_end(C, pup);

		BKE_image_release_ibuf(ima, ibuf, NULL);

		return OPERATOR_INTERFACE;
	}

	BKE_image_release_ibuf(ima, ibuf, NULL);

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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "as_png", 0, "Pack As PNG", "Pack image as lossless PNG");
}

/********************* unpack operator *********************/

static int image_unpack_exec(bContext *C, wmOperator *op)
{
	Image *ima = CTX_data_edit_image(C);
	int method = RNA_enum_get(op->ptr, "method");

	/* find the suppplied image by name */
	if (RNA_struct_property_is_set(op->ptr, "id")) {
		char imaname[MAX_ID_NAME - 2];
		RNA_string_get(op->ptr, "id", imaname);
		ima = BLI_findstring(&CTX_data_main(C)->image, imaname, offsetof(ID, name) + 2);
		if (!ima) ima = CTX_data_edit_image(C);
	}
	
	if (!ima || !BKE_image_has_packedfile(ima))
		return OPERATOR_CANCELLED;

	if (ima->source == IMA_SRC_SEQUENCE || ima->source == IMA_SRC_MOVIE) {
		BKE_report(op->reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
		return OPERATOR_CANCELLED;
	}

	if (G.fileflags & G_AUTOPACK)
		BKE_report(op->reports, RPT_WARNING, "AutoPack is enabled, so image will be packed again on file save");
	
	/* XXX unpackImage frees image buffers */
	ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));
	
	unpackImage(op->reports, ima, method);
	
	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);

	return OPERATOR_FINISHED;
}

static int image_unpack_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Image *ima = CTX_data_edit_image(C);

	if (RNA_struct_property_is_set(op->ptr, "id"))
		return image_unpack_exec(C, op);
		
	if (!ima || !BKE_image_has_packedfile(ima))
		return OPERATOR_CANCELLED;

	if (ima->source == IMA_SRC_SEQUENCE || ima->source == IMA_SRC_MOVIE) {
		BKE_report(op->reports, RPT_ERROR, "Unpacking movies or image sequences not supported");
		return OPERATOR_CANCELLED;
	}

	if (G.fileflags & G_AUTOPACK)
		BKE_report(op->reports, RPT_WARNING, "AutoPack is enabled, so image will be packed again on file save");

	unpack_menu(C, "IMAGE_OT_unpack", ima->id.name + 2, ima->name, "textures", BKE_image_has_packedfile(ima) ? ((ImagePackedFile *)ima->packedfiles.first)->packedfile : NULL);

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
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_enum(ot->srna, "method", rna_enum_unpack_method_items, PF_USE_LOCAL, "Method", "How to unpack");
	RNA_def_string(ot->srna, "id", NULL, MAX_ID_NAME - 2, "Image Name", "Image data-block name to unpack"); /* XXX, weark!, will fail with library, name collisions */
}

/******************** sample image operator ********************/

typedef struct ImageSampleInfo {
	ARegionType *art;
	void *draw_handle;
	int x, y;
	int channels;

	unsigned char col[4];
	float colf[4];
	float linearcol[4];
	int z;
	float zf;

	unsigned char *colp;
	const float *colfp;
	int *zp;
	float *zfp;

	bool draw;
	bool color_manage;
	int use_default_view;
} ImageSampleInfo;

static void image_sample_draw(const bContext *C, ARegion *ar, void *arg_info)
{
	ImageSampleInfo *info = arg_info;
	if (info->draw) {
		Scene *scene = CTX_data_scene(C);

		ED_image_draw_info(scene, ar, info->color_manage, info->use_default_view, info->channels,
		                   info->x, info->y, info->colp, info->colfp, info->linearcol, info->zp, info->zfp);
	}
}

/* Returns color in linear space, matching ED_space_node_color_sample(). */
bool ED_space_image_color_sample(SpaceImage *sima, ARegion *ar, int mval[2], float r_col[3])
{
	void *lock;
	ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock);
	float fx, fy;
	bool ret = false;

	if (ibuf == NULL) {
		ED_space_image_release_buffer(sima, ibuf, lock);
		return false;
	}

	UI_view2d_region_to_view(&ar->v2d, mval[0], mval[1], &fx, &fy);

	if (fx >= 0.0f && fy >= 0.0f && fx < 1.0f && fy < 1.0f) {
		const float *fp;
		unsigned char *cp;
		int x = (int)(fx * ibuf->x), y = (int)(fy * ibuf->y);

		CLAMP(x, 0, ibuf->x - 1);
		CLAMP(y, 0, ibuf->y - 1);

		if (ibuf->rect_float) {
			fp = (ibuf->rect_float + (ibuf->channels) * (y * ibuf->x + x));
			copy_v3_v3(r_col, fp);
			ret = true;
		}
		else if (ibuf->rect) {
			cp = (unsigned char *)(ibuf->rect + y * ibuf->x + x);
			rgb_uchar_to_float(r_col, cp);
			IMB_colormanagement_colorspace_to_scene_linear_v3(r_col, ibuf->rect_colorspace);
			ret = true;
		}
	}

	ED_space_image_release_buffer(sima, ibuf, lock);
	return ret;
}

static void image_sample_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);
	void *lock;
	ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock);
	ImageSampleInfo *info = op->customdata;
	float fx, fy;
	Scene *scene = CTX_data_scene(C);
	CurveMapping *curve_mapping = scene->view_settings.curve_mapping;

	if (ibuf == NULL) {
		ED_space_image_release_buffer(sima, ibuf, lock);
		info->draw = false;
		return;
	}

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fx, &fy);

	if (fx >= 0.0f && fy >= 0.0f && fx < 1.0f && fy < 1.0f) {
		const float *fp;
		unsigned char *cp;
		int x = (int)(fx * ibuf->x), y = (int)(fy * ibuf->y);
		Image *image = ED_space_image(sima);

		CLAMP(x, 0, ibuf->x - 1);
		CLAMP(y, 0, ibuf->y - 1);

		info->x = x;
		info->y = y;
		info->draw = true;
		info->channels = ibuf->channels;

		info->colp = NULL;
		info->colfp = NULL;
		info->zp = NULL;
		info->zfp = NULL;

		info->use_default_view = (image->flag & IMA_VIEW_AS_RENDER) ? false : true;

		if (ibuf->rect) {
			cp = (unsigned char *)(ibuf->rect + y * ibuf->x + x);

			info->col[0] = cp[0];
			info->col[1] = cp[1];
			info->col[2] = cp[2];
			info->col[3] = cp[3];
			info->colp = info->col;

			info->colf[0] = (float)cp[0] / 255.0f;
			info->colf[1] = (float)cp[1] / 255.0f;
			info->colf[2] = (float)cp[2] / 255.0f;
			info->colf[3] = (float)cp[3] / 255.0f;
			info->colfp = info->colf;

			copy_v4_v4(info->linearcol, info->colf);
			IMB_colormanagement_colorspace_to_scene_linear_v4(info->linearcol, false, ibuf->rect_colorspace);

			info->color_manage = true;
		}
		if (ibuf->rect_float) {
			fp = (ibuf->rect_float + (ibuf->channels) * (y * ibuf->x + x));

			if (ibuf->channels == 4) {
				info->colf[0] = fp[0];
				info->colf[1] = fp[1];
				info->colf[2] = fp[2];
				info->colf[3] = fp[3];
			}
			else if (ibuf->channels == 3) {
				info->colf[0] = fp[0];
				info->colf[1] = fp[1];
				info->colf[2] = fp[2];
				info->colf[3] = 1.0f;
			}
			else {
				info->colf[0] = fp[0];
				info->colf[1] = fp[0];
				info->colf[2] = fp[0];
				info->colf[3] = 1.0f;
			}
			info->colfp = info->colf;

			copy_v4_v4(info->linearcol, info->colf);

			info->color_manage = true;
		}

		if (ibuf->zbuf) {
			info->z = ibuf->zbuf[y * ibuf->x + x];
			info->zp = &info->z;
			if (ibuf->zbuf == (int *)ibuf->rect) {
				info->colp = NULL;
			}
		}
		if (ibuf->zbuf_float) {
			info->zf = ibuf->zbuf_float[y * ibuf->x + x];
			info->zfp = &info->zf;
			if (ibuf->zbuf_float == ibuf->rect_float) {
				info->colfp = NULL;
			}
		}

		if (curve_mapping && ibuf->channels == 4) {
			/* we reuse this callback for set curves point operators */
			if (RNA_struct_find_property(op->ptr, "point")) {
				int point = RNA_enum_get(op->ptr, "point");

				if (point == 1) {
					curvemapping_set_black_white(curve_mapping, NULL, info->linearcol);
				}
				else if (point == 0) {
					curvemapping_set_black_white(curve_mapping, info->linearcol, NULL);
				}
				WM_event_add_notifier(C, NC_WINDOW, NULL);
			}
		}

		// XXX node curve integration ..
#if 0
		{
			ScrArea *sa, *cur = curarea;
			
			node_curvemap_sample(fp);   /* sends global to node editor */
			for (sa = G.curscreen->areabase.first; sa; sa = sa->next) {
				if (sa->spacetype == SPACE_NODE) {
					areawinset(sa->win);
					scrarea_do_windraw(sa);
				}
			}
			node_curvemap_sample(NULL);     /* clears global in node editor */
			curarea = cur;
		}
#endif
	}
	else {
		info->draw = 0;
	}

	ED_space_image_release_buffer(sima, ibuf, lock);
	ED_area_tag_redraw(CTX_wm_area(C));
}

static void image_sample_exit(bContext *C, wmOperator *op)
{
	ImageSampleInfo *info = op->customdata;

	ED_region_draw_cb_exit(info->art, info->draw_handle);
	ED_area_tag_redraw(CTX_wm_area(C));
	MEM_freeN(info);
}

static int image_sample_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);
	ImageSampleInfo *info;

	if (ar->regiontype == RGN_TYPE_WINDOW) {
		if (event->mval[1] <= 16 && ED_space_image_show_cache(sima)) {
			return OPERATOR_PASS_THROUGH;
		}
	}

	if (!ED_space_image_has_buffer(sima))
		return OPERATOR_CANCELLED;
	
	info = MEM_callocN(sizeof(ImageSampleInfo), "ImageSampleInfo");
	info->art = ar->type;
	info->draw_handle = ED_region_draw_cb_activate(ar->type, image_sample_draw, info, REGION_DRAW_POST_PIXEL);
	op->customdata = info;

	image_sample_apply(C, op, event);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int image_sample_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	switch (event->type) {
		case LEFTMOUSE:
		case RIGHTMOUSE: // XXX hardcoded
			if (event->val == KM_RELEASE) {
				image_sample_exit(C, op);
				return OPERATOR_CANCELLED;
			}
			break;
		case MOUSEMOVE:
			image_sample_apply(C, op, event);
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static void image_sample_cancel(bContext *C, wmOperator *op)
{
	image_sample_exit(C, op);
}

void IMAGE_OT_sample(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Color";
	ot->idname = "IMAGE_OT_sample";
	ot->description = "Use mouse to sample a color in current image";
	
	/* api callbacks */
	ot->invoke = image_sample_invoke;
	ot->modal = image_sample_modal;
	ot->cancel = image_sample_cancel;
	ot->poll = image_sample_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;
}

/******************** sample line operator ********************/
static int image_sample_line_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);

	int x_start = RNA_int_get(op->ptr, "xstart");
	int y_start = RNA_int_get(op->ptr, "ystart");
	int x_end = RNA_int_get(op->ptr, "xend");
	int y_end = RNA_int_get(op->ptr, "yend");
	
	void *lock;
	ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock);
	Histogram *hist = &sima->sample_line_hist;
	
	float x1f, y1f, x2f, y2f;
	
	if (ibuf == NULL) {
		ED_space_image_release_buffer(sima, ibuf, lock);
		return OPERATOR_CANCELLED;
	}
	/* hmmmm */
	if (ibuf->channels < 3) {
		ED_space_image_release_buffer(sima, ibuf, lock);
		return OPERATOR_CANCELLED;
	}
	
	UI_view2d_region_to_view(&ar->v2d, x_start, y_start, &x1f, &y1f);
	UI_view2d_region_to_view(&ar->v2d, x_end, y_end, &x2f, &y2f);

	hist->co[0][0] = x1f;
	hist->co[0][1] = y1f;
	hist->co[1][0] = x2f;
	hist->co[1][1] = y2f;

	/* enable line drawing */
	hist->flag |= HISTO_FLAG_SAMPLELINE;

	BKE_histogram_update_sample_line(hist, ibuf, &scene->view_settings, &scene->display_settings);
	
	/* reset y zoom */
	hist->ymax = 1.0f;

	ED_space_image_release_buffer(sima, ibuf, lock);
	
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

static int image_sample_line_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceImage *sima = CTX_wm_space_image(C);

	Histogram *hist = &sima->sample_line_hist;
	hist->flag &= ~HISTO_FLAG_SAMPLELINE;

	if (!ED_space_image_has_buffer(sima))
		return OPERATOR_CANCELLED;
	
	return WM_gesture_straightline_invoke(C, op, event);
}

void IMAGE_OT_sample_line(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Line";
	ot->idname = "IMAGE_OT_sample_line";
	ot->description = "Sample a line and show it in Scope panels";
	
	/* api callbacks */
	ot->invoke = image_sample_line_invoke;
	ot->modal = WM_gesture_straightline_modal;
	ot->exec = image_sample_line_exec;
	ot->poll = space_image_main_region_poll;
	ot->cancel = WM_gesture_straightline_cancel;
	
	/* flags */
	ot->flag = 0; /* no undo/register since this operates on the space */
	
	WM_operator_properties_gesture_straightline(ot, CURSOR_EDIT);
}

/******************** set curve point operator ********************/

void IMAGE_OT_curves_point_set(wmOperatorType *ot)
{
	static EnumPropertyItem point_items[] = {
		{0, "BLACK_POINT", 0, "Black Point", ""},
		{1, "WHITE_POINT", 0, "White Point", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Set Curves Point";
	ot->idname = "IMAGE_OT_curves_point_set";
	ot->description = "Set black point or white point for curves";

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* api callbacks */
	ot->invoke = image_sample_invoke;
	ot->modal = image_sample_modal;
	ot->cancel = image_sample_cancel;
	ot->poll = space_image_main_area_not_uv_brush_poll;

	/* properties */
	RNA_def_enum(ot->srna, "point", point_items, 0, "Point", "Set black point or white point for curves");
}

#if 0 /* Not ported to 2.5x yet */
/******************** record composite operator *********************/

typedef struct RecordCompositeData {
	wmTimer *timer;
	int old_cfra;
	int sfra, efra;
} RecordCompositeData;

static int image_record_composite_apply(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	RecordCompositeData *rcd = op->customdata;
	Scene *scene = CTX_data_scene(C);
	ImBuf *ibuf;
	
	WM_cursor_time(CTX_wm_window(C), scene->r.cfra);

	// XXX scene->nodetree->test_break = BKE_blender_test_break;
	// XXX scene->nodetree->test_break = NULL;
	
	BKE_image_all_free_anim_ibufs(scene->r.cfra);
	ntreeCompositTagAnimated(scene->nodetree);
	ntreeCompositExecTree(scene->nodetree, &scene->r, 0, scene->r.cfra != rcd->old_cfra,
	                      &scene->view_settings, &scene->display_settings);  /* 1 is no previews */

	ED_area_tag_redraw(CTX_wm_area(C));
	
	ibuf = BKE_image_acquire_ibuf(sima->image, &sima->iuser, NULL);
	/* save memory in flipbooks */
	if (ibuf)
		imb_freerectfloatImBuf(ibuf);

	BKE_image_release_ibuf(sima->image, ibuf, NULL);

	scene->r.cfra++;

	return (scene->r.cfra <= rcd->efra);
}

static int image_record_composite_init(bContext *C, wmOperator *op)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	RecordCompositeData *rcd;

	if (sima->iuser.frames < 2)
		return 0;
	if (scene->nodetree == NULL)
		return 0;
	
	op->customdata = rcd = MEM_callocN(sizeof(RecordCompositeData), "ImageRecordCompositeData");

	rcd->old_cfra = scene->r.cfra;
	rcd->sfra = sima->iuser.sfra;
	rcd->efra = sima->iuser.sfra + sima->iuser.frames - 1;
	scene->r.cfra = rcd->sfra;

	return 1;
}

static void image_record_composite_exit(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SpaceImage *sima = CTX_wm_space_image(C);
	RecordCompositeData *rcd = op->customdata;

	scene->r.cfra = rcd->old_cfra;

	WM_cursor_modal_restore(CTX_wm_window(C));

	if (rcd->timer)
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), rcd->timer);

	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, sima->image);

	// XXX play_anim(0);
	// XXX allqueue(REDRAWNODE, 1);

	MEM_freeN(rcd);
}

static int image_record_composite_exec(bContext *C, wmOperator *op)
{
	if (!image_record_composite_init(C, op))
		return OPERATOR_CANCELLED;
	
	while (image_record_composite_apply(C, op)) {}
	
	image_record_composite_exit(C, op);
	
	return OPERATOR_FINISHED;
}

static int image_record_composite_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	RecordCompositeData *rcd;
	
	if (!image_record_composite_init(C, op))
		return OPERATOR_CANCELLED;

	rcd = op->customdata;
	rcd->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.0f);
	WM_event_add_modal_handler(C, op);

	if (!image_record_composite_apply(C, op))
		return OPERATOR_FINISHED;

	return OPERATOR_RUNNING_MODAL;
}

static int image_record_composite_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	RecordCompositeData *rcd = op->customdata;

	switch (event->type) {
		case TIMER:
			if (rcd->timer == event->customdata) {
				if (!image_record_composite_apply(C, op)) {
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

static void image_record_composite_cancel(bContext *C, wmOperator *op)
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

#endif

/********************* cycle render slot operator *********************/

static int image_cycle_render_slot_poll(bContext *C)
{
	Image *ima = CTX_data_edit_image(C);

	return (ima && ima->type == IMA_TYPE_R_RESULT);
}

static int image_cycle_render_slot_exec(bContext *C, wmOperator *op)
{
	Image *ima = CTX_data_edit_image(C);
	const int direction = RNA_boolean_get(op->ptr, "reverse") ? -1 : 1;

	if (!ED_image_slot_cycle(ima, direction)) {
		return OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_IMAGE | ND_DRAW, NULL);

	/* no undo push for browsing existing */
	if (ima->renders[ima->render_slot] || ima->render_slot == ima->last_render_slot)
		return OPERATOR_CANCELLED;
	
	return OPERATOR_FINISHED;
}

void IMAGE_OT_cycle_render_slot(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cycle Render Slot";
	ot->idname = "IMAGE_OT_cycle_render_slot";
	ot->description = "Cycle through all non-void render slots";
	
	/* api callbacks */
	ot->exec = image_cycle_render_slot_exec;
	ot->poll = image_cycle_render_slot_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER;

	RNA_def_boolean(ot->srna, "reverse", 0, "Cycle in Reverse", "");
}

/********************** change frame operator *********************/

static int change_frame_poll(bContext *C)
{
	/* prevent changes during render */
	if (G.is_rendering)
		return 0;

	return space_image_main_region_poll(C);
}

static void change_frame_apply(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);

	/* set the new frame number */
	CFRA = RNA_int_get(op->ptr, "frame");
	FRAMENUMBER_MIN_CLAMP(CFRA);
	SUBFRA = 0.0f;

	/* do updates */
	BKE_sound_seek_scene(CTX_data_main(C), scene);
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
}

static int change_frame_exec(bContext *C, wmOperator *op)
{
	change_frame_apply(C, op);

	return OPERATOR_FINISHED;
}

static int frame_from_event(bContext *C, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	int framenr = 0;

	if (ar->regiontype == RGN_TYPE_WINDOW) {
		float sfra = SFRA, efra = EFRA, framelen = ar->winx / (efra - sfra + 1);

		framenr = sfra + event->mval[0] / framelen;
	}
	else {
		float viewx, viewy;

		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &viewx, &viewy);

		framenr = iroundf(viewx);
	}

	return framenr;
}

static int change_frame_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);

	if (ar->regiontype == RGN_TYPE_WINDOW) {
		SpaceImage *sima = CTX_wm_space_image(C);
		if (event->mval[1] > 16 || !ED_space_image_show_cache(sima)) {
			return OPERATOR_PASS_THROUGH;
		}
	}

	RNA_int_set(op->ptr, "frame", frame_from_event(C, event));

	change_frame_apply(C, op);

	/* add temp handler */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int change_frame_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_FINISHED;

		case MOUSEMOVE:
			RNA_int_set(op->ptr, "frame", frame_from_event(C, event));
			change_frame_apply(C, op);
			break;

		case LEFTMOUSE:
		case RIGHTMOUSE:
			if (event->val == KM_RELEASE)
				return OPERATOR_FINISHED;
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

void IMAGE_OT_change_frame(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change Frame";
	ot->idname = "IMAGE_OT_change_frame";
	ot->description = "Interactively change the current frame number";

	/* api callbacks */
	ot->exec = change_frame_exec;
	ot->invoke = change_frame_invoke;
	ot->modal = change_frame_modal;
	ot->poll = change_frame_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_UNDO;

	/* rna */
	RNA_def_int(ot->srna, "frame", 0, MINAFRAME, MAXFRAME, "Frame", "", MINAFRAME, MAXFRAME);
}

/* Reload cached render results... */
/* goes over all scenes, reads render layers */
static int image_read_renderlayers_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	SpaceImage *sima = CTX_wm_space_image(C);
	Image *ima;

	ima = BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result");
	if (sima->image == NULL) {
		ED_space_image_set(sima, scene, NULL, ima);
	}

	RE_ReadRenderResult(scene, scene);

	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
	return OPERATOR_FINISHED;
}

void IMAGE_OT_read_renderlayers(wmOperatorType *ot)
{
	ot->name = "Read Render Layers";
	ot->idname = "IMAGE_OT_read_renderlayers";
	ot->description = "Read all the current scene's render layers from cache, as needed";

	ot->poll = space_image_main_region_poll;
	ot->exec = image_read_renderlayers_exec;

	/* flags */
	ot->flag = 0;
}

/* ********************* Render border operator ****************** */

static int render_border_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	Render *re = RE_GetRender(scene->id.name);
	RenderData *rd;
	rctf border;

	if (re == NULL) {
		/* Shouldn't happen, but better be safe close to the release. */
		return OPERATOR_CANCELLED;
	}

	rd = RE_engine_get_render_data(re);
	if ((rd->mode & (R_BORDER | R_CROP)) == (R_BORDER | R_CROP)) {
		BKE_report(op->reports, RPT_INFO, "Can not set border from a cropped render");
		return OPERATOR_CANCELLED;
	}

	/* get rectangle from operator */
	WM_operator_properties_border_to_rctf(op, &border);
	UI_view2d_region_to_view_rctf(&ar->v2d, &border, &border);

	/* actually set border */
	CLAMP(border.xmin, 0.0f, 1.0f);
	CLAMP(border.ymin, 0.0f, 1.0f);
	CLAMP(border.xmax, 0.0f, 1.0f);
	CLAMP(border.ymax, 0.0f, 1.0f);
	scene->r.border = border;

	/* drawing a border surrounding the entire camera view switches off border rendering
	 * or the border covers no pixels */
	if ((border.xmin <= 0.0f && border.xmax >= 1.0f &&
	     border.ymin <= 0.0f && border.ymax >= 1.0f) ||
	    (border.xmin == border.xmax || border.ymin == border.ymax))
	{
		scene->r.mode &= ~R_BORDER;
	}
	else {
		scene->r.mode |= R_BORDER;
	}

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	return OPERATOR_FINISHED;

}

void IMAGE_OT_render_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Render Border";
	ot->description = "Set the boundaries of the border render and enable border render";
	ot->idname = "IMAGE_OT_render_border";

	/* api callbacks */
	ot->invoke = WM_border_select_invoke;
	ot->exec = render_border_exec;
	ot->modal = WM_border_select_modal;
	ot->cancel = WM_border_select_cancel;
	ot->poll = image_cycle_render_slot_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* rna */
	WM_operator_properties_border(ot);
}

/* ********************* Clear render border operator ****************** */

static int clear_render_border_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	scene->r.mode &= ~R_BORDER;
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);
	BLI_rctf_init(&scene->r.border, 0.0f, 1.0f, 0.0f, 1.0f);
	return OPERATOR_FINISHED;

}

void IMAGE_OT_clear_render_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Render Border";
	ot->description = "Clear the boundaries of the border render and disable border render";
	ot->idname = "IMAGE_OT_clear_render_border";

	/* api callbacks */
	ot->exec = clear_render_border_exec;
	ot->poll = image_cycle_render_slot_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
