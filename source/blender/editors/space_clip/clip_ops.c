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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_ops.c
 *  \ingroup spclip
 */

#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_scene_types.h"	/* min/max frames */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_movieclip.h"
#include "BKE_sound.h"
#include "BKE_tracking.h"

#include "WM_api.h"
#include "WM_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "clip_intern.h"	// own include

/******************** view navigation utilities *********************/

static void sclip_zoom_set(SpaceClip *sc, ARegion *ar, float zoom, float location[2])
{
	float oldzoom= sc->zoom;
	int width, height;

	sc->zoom= zoom;

	if (sc->zoom < 0.1f || sc->zoom > 4.0f) {
		/* check zoom limits */
		ED_space_clip_size(sc, &width, &height);

		width*= sc->zoom;
		height*= sc->zoom;

		if ((width < 4) && (height < 4))
			sc->zoom= oldzoom;
		else if ((ar->winrct.xmax - ar->winrct.xmin) <= sc->zoom)
			sc->zoom= oldzoom;
		else if ((ar->winrct.ymax - ar->winrct.ymin) <= sc->zoom)
			sc->zoom= oldzoom;
	}

	if ((U.uiflag & USER_ZOOM_TO_MOUSEPOS) && location) {
		ED_space_clip_size(sc, &width, &height);

		sc->xof+= ((location[0]-0.5f)*width-sc->xof)*(sc->zoom-oldzoom)/sc->zoom;
		sc->yof+= ((location[1]-0.5f)*height-sc->yof)*(sc->zoom-oldzoom)/sc->zoom;
	}
}

static void sclip_zoom_set_factor(SpaceClip *sc, ARegion *ar, float zoomfac, float location[2])
{
	sclip_zoom_set(sc, ar, sc->zoom*zoomfac, location);
}

static void sclip_zoom_set_factor_exec(bContext *C, wmEvent *event, float factor)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ARegion *ar= CTX_wm_region(C);
	float location[2], *mpos = NULL;

	if (event) {
		ED_clip_mouse_pos(C, event, location);
		mpos = location;
	}

	sclip_zoom_set_factor(sc, ar, factor, mpos);

	ED_region_tag_redraw(CTX_wm_region(C));
}

/******************** open clip operator ********************/

static void clip_filesel(bContext *C, wmOperator *op, const char *path)
{
	RNA_string_set(op->ptr, "filepath", path);
	WM_event_add_fileselect(C, op);
}

static void open_init(bContext *C, wmOperator *op)
{
	PropertyPointerRNA *pprop;

	op->customdata= pprop= MEM_callocN(sizeof(PropertyPointerRNA), "OpenPropertyPointerRNA");
	uiIDContextProperty(C, &pprop->ptr, &pprop->prop);
}

static int open_cancel(bContext *UNUSED(C), wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;

	return OPERATOR_CANCELLED;
}

static int open_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	PropertyPointerRNA *pprop;
	PointerRNA idptr;
	MovieClip *clip= NULL;
	char str[FILE_MAX];

	RNA_string_get(op->ptr, "filepath", str);
	/* default to frame 1 if there's no scene in context */

	errno= 0;

	clip= BKE_add_movieclip_file(str);

	if (!clip) {
		if (op->customdata)
			MEM_freeN(op->customdata);

		BKE_reportf(op->reports, RPT_ERROR, "Can't read: \"%s\", %s.", str, errno ? strerror(errno) : "Unsupported movie clip format");

		return OPERATOR_CANCELLED;
	}

	if (!op->customdata)
		open_init(C, op);

	/* hook into UI */
	pprop= op->customdata;

	if (pprop->prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		 * pointer se also increases user, so this compensates it */
		clip->id.us--;

		RNA_id_pointer_create(&clip->id, &idptr);
		RNA_property_pointer_set(&pprop->ptr, pprop->prop, idptr);
		RNA_property_update(C, &pprop->ptr, pprop->prop);
	}
	else if (sc) {
		ED_space_clip_set(C, sc, clip);
	}

	WM_event_add_notifier(C, NC_MOVIECLIP|NA_ADDED, clip);

	MEM_freeN(op->customdata);

	return OPERATOR_FINISHED;
}

static int open_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	char *path= U.textudir;
	MovieClip *clip= NULL;

	if (sc)
		clip= ED_space_clip(sc);

	if (clip)
		path= clip->name;

	if (!RNA_struct_property_is_set(op->ptr, "relative_path"))
		RNA_boolean_set(op->ptr, "relative_path", U.flag & USER_RELPATHS);

	if (RNA_struct_property_is_set(op->ptr, "filepath"))
		return open_exec(C, op);

	open_init(C, op);

	clip_filesel(C, op, path);

	return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_open(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Open Clip";
	ot->description = "Load a sequence of frames or a movie file";
	ot->idname = "CLIP_OT_open";

	/* api callbacks */
	ot->exec = open_exec;
	ot->invoke = open_invoke;
	ot->cancel = open_cancel;

	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	WM_operator_properties_filesel(ot, FOLDERFILE|IMAGEFILE|MOVIEFILE, FILE_SPECIAL, FILE_OPENFILE, WM_FILESEL_FILEPATH|WM_FILESEL_RELPATH, FILE_DEFAULTDISPLAY);
}

/******************* reload clip operator *********************/

static int reload_exec(bContext *C, wmOperator *UNUSED(op))
{
	MovieClip *clip= CTX_data_edit_movieclip(C);

	if (!clip)
		return OPERATOR_CANCELLED;

	BKE_movieclip_reload(clip);

	WM_event_add_notifier(C, NC_MOVIECLIP|NA_EDITED, clip);

	return OPERATOR_FINISHED;
}

void CLIP_OT_reload(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reload Clip";
	ot->description = "Reload clip";
	ot->idname = "CLIP_OT_reload";

	/* api callbacks */
	ot->exec = reload_exec;
}

/********************** view pan operator *********************/

typedef struct ViewPanData {
	float x, y;
	float xof, yof, xorig, yorig;
	int event_type;
	float *vec;
} ViewPanData;

static void view_pan_init(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ViewPanData *vpd;

	op->customdata= vpd= MEM_callocN(sizeof(ViewPanData), "ClipViewPanData");
	WM_cursor_modal(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->x= event->x;
	vpd->y= event->y;

	if (sc->flag&SC_LOCK_SELECTION) vpd->vec= &sc->xlockof;
	else vpd->vec= &sc->xof;

	copy_v2_v2(&vpd->xof, vpd->vec);
	copy_v2_v2(&vpd->xorig, &vpd->xof);

	vpd->event_type= event->type;

	WM_event_add_modal_handler(C, op);
}

static void view_pan_exit(bContext *C, wmOperator *op, int cancel)
{
	ViewPanData *vpd= op->customdata;

	if (cancel) {
		copy_v2_v2(vpd->vec, &vpd->xorig);

		ED_region_tag_redraw(CTX_wm_region(C));
	}

	WM_cursor_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int view_pan_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	float offset[2];

	RNA_float_get_array(op->ptr, "offset", offset);

	if (sc->flag&SC_LOCK_SELECTION) {
		sc->xlockof+= offset[0];
		sc->ylockof+= offset[1];
	}
	else {
		sc->xof+= offset[0];
		sc->yof+= offset[1];
	}

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

static int view_pan_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (event->type==MOUSEPAN) {
		SpaceClip *sc= CTX_wm_space_clip(C);
		float offset[2];

		offset[0]= (event->x - event->prevx)/sc->zoom;
		offset[1]= (event->y - event->prevy)/sc->zoom;

		RNA_float_set_array(op->ptr, "offset", offset);

		view_pan_exec(C, op);
		return OPERATOR_FINISHED;
	}
	else {
		view_pan_init(C, op, event);
		return OPERATOR_RUNNING_MODAL;
	}
}

static int view_pan_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ViewPanData *vpd= op->customdata;
	float offset[2];

	switch(event->type) {
		case MOUSEMOVE:
			copy_v2_v2(vpd->vec, &vpd->xorig);
			offset[0]= (vpd->x - event->x)/sc->zoom;
			offset[1]= (vpd->y - event->y)/sc->zoom;
			RNA_float_set_array(op->ptr, "offset", offset);
			view_pan_exec(C, op);
			break;
		case ESCKEY:
			view_pan_exit(C, op, 1);
			return OPERATOR_CANCELLED;
		case SPACEKEY:
			view_pan_exit(C, op, 0);
			return OPERATOR_FINISHED;
		default:
			if (event->type==vpd->event_type &&  event->val==KM_RELEASE) {
				view_pan_exit(C, op, 0);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int view_pan_cancel(bContext *C, wmOperator *op)
{
	view_pan_exit(C, op, 1);

	return OPERATOR_CANCELLED;
}

void CLIP_OT_view_pan(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Pan";
	ot->idname = "CLIP_OT_view_pan";

	/* api callbacks */
	ot->exec = view_pan_exec;
	ot->invoke = view_pan_invoke;
	ot->modal = view_pan_modal;
	ot->cancel = view_pan_cancel;
	ot->poll = ED_space_clip_poll;

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

static void view_zoom_init(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ViewZoomData *vpd;

	op->customdata= vpd= MEM_callocN(sizeof(ViewZoomData), "ClipViewZoomData");
	WM_cursor_modal(CTX_wm_window(C), BC_NSEW_SCROLLCURSOR);

	vpd->x= event->x;
	vpd->y= event->y;
	vpd->zoom= sc->zoom;
	vpd->event_type= event->type;

	ED_clip_mouse_pos(C, event, vpd->location);

	WM_event_add_modal_handler(C, op);
}

static void view_zoom_exit(bContext *C, wmOperator *op, int cancel)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ViewZoomData *vpd= op->customdata;

	if (cancel) {
		sc->zoom= vpd->zoom;
		ED_region_tag_redraw(CTX_wm_region(C));
	}

	WM_cursor_restore(CTX_wm_window(C));
	MEM_freeN(op->customdata);
}

static int view_zoom_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ARegion *ar= CTX_wm_region(C);

	sclip_zoom_set_factor(sc, ar, RNA_float_get(op->ptr, "factor"), NULL);

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

static int view_zoom_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (event->type==MOUSEZOOM) {
		float factor;

		factor= 1.0f + (event->x-event->prevx+event->y-event->prevy)/300.0f;
		RNA_float_set(op->ptr, "factor", factor);

		sclip_zoom_set_factor_exec(C, event, factor);

		return OPERATOR_FINISHED;
	}
	else {
		view_zoom_init(C, op, event);

		return OPERATOR_RUNNING_MODAL;
	}
}

static int view_zoom_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ARegion *ar= CTX_wm_region(C);
	ViewZoomData *vpd= op->customdata;
	float factor;

	switch(event->type) {
		case MOUSEMOVE:
			factor= 1.0f + (vpd->x-event->x+vpd->y-event->y)/300.0f;
			RNA_float_set(op->ptr, "factor", factor);
			sclip_zoom_set(sc, ar, vpd->zoom*factor, vpd->location);
			ED_region_tag_redraw(CTX_wm_region(C));
			break;
		default:
			if (event->type==vpd->event_type && event->val==KM_RELEASE) {
				view_zoom_exit(C, op, 0);
				return OPERATOR_FINISHED;
			}
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

static int view_zoom_cancel(bContext *C, wmOperator *op)
{
	view_zoom_exit(C, op, 1);
	return OPERATOR_CANCELLED;
}

void CLIP_OT_view_zoom(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom";
	ot->idname = "CLIP_OT_view_zoom";

	/* api callbacks */
	ot->exec = view_zoom_exec;
	ot->invoke = view_zoom_invoke;
	ot->modal = view_zoom_modal;
	ot->cancel = view_zoom_cancel;
	ot->poll = ED_space_clip_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING|OPTYPE_GRAB_POINTER;

	/* properties */
	RNA_def_float(ot->srna, "factor", 0.0f, 0.0f, FLT_MAX,
		"Factor", "Zoom factor, values higher than 1.0 zoom in, lower values zoom out", -FLT_MAX, FLT_MAX);
}

/********************** view zoom in/out operator *********************/

static int view_zoom_in_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ARegion *ar= CTX_wm_region(C);
	float location[2];

	RNA_float_get_array(op->ptr, "location", location);

	sclip_zoom_set_factor(sc, ar, 1.25f, location);

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

static int view_zoom_in_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float location[2];

	ED_clip_mouse_pos(C, event, location);
	RNA_float_set_array(op->ptr, "location", location);

	return view_zoom_in_exec(C, op);
}

void CLIP_OT_view_zoom_in(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom In";
	ot->idname = "CLIP_OT_view_zoom_in";

	/* api callbacks */
	ot->exec = view_zoom_in_exec;
	ot->invoke = view_zoom_in_invoke;
	ot->poll = ED_space_clip_poll;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location", "Cursor location in screen coordinates", -10.0f, 10.0f);
}

static int view_zoom_out_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ARegion *ar= CTX_wm_region(C);
	float location[2];

	RNA_float_get_array(op->ptr, "location", location);

	sclip_zoom_set_factor(sc, ar, 0.8f, location);

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

static int view_zoom_out_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	float location[2];

	ED_clip_mouse_pos(C, event, location);
	RNA_float_set_array(op->ptr, "location", location);

	return view_zoom_out_exec(C, op);
}

void CLIP_OT_view_zoom_out(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom Out";
	ot->idname = "CLIP_OT_view_zoom_out";

	/* api callbacks */
	ot->exec = view_zoom_out_exec;
	ot->invoke = view_zoom_out_invoke;
	ot->poll = ED_space_clip_poll;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location", "Cursor location in normalised (0.0-1.0) coordinates", -10.0f, 10.0f);
}

/********************** view zoom ratio operator *********************/

static int view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ARegion *ar= CTX_wm_region(C);

	sclip_zoom_set(sc, ar, RNA_float_get(op->ptr, "ratio"), NULL);

	/* ensure pixel exact locations for draw */
	sc->xof= (int)sc->xof;
	sc->yof= (int)sc->yof;

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

void CLIP_OT_view_zoom_ratio(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Zoom Ratio";
	ot->idname = "CLIP_OT_view_zoom_ratio";

	/* api callbacks */
	ot->exec = view_zoom_ratio_exec;
	ot->poll = ED_space_clip_poll;

	/* properties */
	RNA_def_float(ot->srna, "ratio", 0.0f, 0.0f, FLT_MAX,
		"Ratio", "Zoom ratio, 1.0 is 1:1, higher is zoomed in, lower is zoomed out", -FLT_MAX, FLT_MAX);
}

/********************** view all operator *********************/

static int view_all_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc;
	ARegion *ar;
	int w, h, width, height;
	float aspx, aspy;
	int fit_view= RNA_boolean_get(op->ptr, "fit_view");
	float zoomx, zoomy;

	/* retrieve state */
	sc= CTX_wm_space_clip(C);
	ar= CTX_wm_region(C);

	ED_space_clip_size(sc, &w, &h);
	ED_space_clip_aspect(sc, &aspx, &aspy);

	w= w*aspx;
	h= h*aspy;

	/* check if the image will fit in the image with zoom==1 */
	width= ar->winrct.xmax - ar->winrct.xmin + 1;
	height= ar->winrct.ymax - ar->winrct.ymin + 1;

	if (fit_view) {
		const int margin = 5; /* margin from border */

		zoomx= (float)width / (w + 2*margin);
		zoomy= (float)height / (h + 2*margin);

		sclip_zoom_set(sc, ar, MIN2(zoomx, zoomy), NULL);
	}
	else {
		if ((w >= width || h >= height) && (width > 0 && height > 0)) {
			zoomx= (float)width/w;
			zoomy= (float)height/h;

			/* find the zoom value that will fit the image in the image space */
			sclip_zoom_set(sc, ar, 1.0f/power_of_2(1/MIN2(zoomx, zoomy)), NULL);
		}
		else
			sclip_zoom_set(sc, ar, 1.0f, NULL);
	}

	sc->xof= sc->yof= 0.0f;

	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

void CLIP_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View All";
	ot->idname = "CLIP_OT_view_all";

	/* api callbacks */
	ot->exec = view_all_exec;
	ot->poll = ED_space_clip_poll;

	/* properties */
	RNA_def_boolean(ot->srna, "fit_view", 0, "Fit View", "Fit frame to the viewport");
}

/********************** view selected operator *********************/

static int view_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	ARegion *ar= CTX_wm_region(C);

	sc->xlockof= 0.0f;
	sc->ylockof= 0.0f;

	ED_clip_view_selection(sc, ar, 1);
	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_FINISHED;
}

void CLIP_OT_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Selected";
	ot->idname = "CLIP_OT_view_selected";

	/* api callbacks */
	ot->exec = view_selected_exec;
	ot->poll = ED_space_clip_poll;
}

/********************** change frame operator *********************/

static int change_frame_poll(bContext *C)
{
	/* prevent changes during render */
	if (G.rendering)
		return 0;

	return ED_space_clip_poll(C);
}

static void change_frame_apply(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);

	/* set the new frame number */
	CFRA= RNA_int_get(op->ptr, "frame");
	FRAMENUMBER_MIN_CLAMP(CFRA);
	SUBFRA = 0.0f;

	/* do updates */
	sound_seek_scene(CTX_data_main(C), CTX_data_scene(C));
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, scene);
}

static int change_frame_exec(bContext *C, wmOperator *op)
{
	change_frame_apply(C, op);

	return OPERATOR_FINISHED;
}

static int frame_from_event(bContext *C, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	Scene *scene= CTX_data_scene(C);
	int framenr= 0;

	if (ar->regiontype == RGN_TYPE_WINDOW) {
		float sfra= SFRA, efra= EFRA, framelen= ar->winx/(efra-sfra+1);

		framenr= sfra+event->mval[0]/framelen;
	}
	else {
		float viewx, viewy;

		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &viewx, &viewy);

		framenr= (int)floor(viewx+0.5f);
	}

	return framenr;
}

static int change_frame_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);

	if (ar->regiontype == RGN_TYPE_WINDOW) {
		if (event->mval[1]>16)
			return OPERATOR_PASS_THROUGH;
	}

	RNA_int_set(op->ptr, "frame", frame_from_event(C, event));

	change_frame_apply(C, op);

	/* add temp handler */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int change_frame_modal(bContext *C, wmOperator *op, wmEvent *event)
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
			if (event->val==KM_RELEASE)
				return OPERATOR_FINISHED;
			break;
	}

	return OPERATOR_RUNNING_MODAL;
}

void CLIP_OT_change_frame(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Change frame";
	ot->idname = "CLIP_OT_change_frame";
	ot->description = "Interactively change the current frame number";

	/* api callbacks */
	ot->exec = change_frame_exec;
	ot->invoke = change_frame_invoke;
	ot->modal = change_frame_modal;
	ot->poll = change_frame_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING|OPTYPE_UNDO;

	/* rna */
	RNA_def_int(ot->srna, "frame", 0, MINAFRAME, MAXFRAME, "Frame", "", MINAFRAME, MAXFRAME);
}

/********************** rebuild proxies operator *********************/

typedef struct ProxyBuildJob {
	Scene *scene;
	struct Main *main;
	MovieClip *clip;
	int clip_flag, stop;
	struct IndexBuildContext *index_context;
} ProxyJob;

static void proxy_freejob(void *pjv)
{
	ProxyJob *pj= pjv;

	MEM_freeN(pj);
}

static int proxy_bitflag_to_array(int size_flag, int build_sizes[4], int undistort)
{
	int build_count = 0;
	int size_flags[2][4] = {{MCLIP_PROXY_SIZE_25,
	                         MCLIP_PROXY_SIZE_50,
	                         MCLIP_PROXY_SIZE_75,
	                         MCLIP_PROXY_SIZE_100},
	                        {MCLIP_PROXY_UNDISTORTED_SIZE_25,
	                         MCLIP_PROXY_UNDISTORTED_SIZE_50,
	                         MCLIP_PROXY_UNDISTORTED_SIZE_75,
	                         MCLIP_PROXY_UNDISTORTED_SIZE_100}};
	int size_nr = undistort ? 1 : 0;

	if (size_flag & size_flags[size_nr][0]) build_sizes[build_count++]= MCLIP_PROXY_RENDER_SIZE_25;
	if (size_flag & size_flags[size_nr][1]) build_sizes[build_count++]= MCLIP_PROXY_RENDER_SIZE_50;
	if (size_flag & size_flags[size_nr][2]) build_sizes[build_count++]= MCLIP_PROXY_RENDER_SIZE_75;
	if (size_flag & size_flags[size_nr][3]) build_sizes[build_count++]= MCLIP_PROXY_RENDER_SIZE_100;

	return build_count;
}

/* only this runs inside thread */
static void proxy_startjob(void *pjv, short *stop, short *do_update, float *progress)
{
	ProxyJob *pj= pjv;
	Scene *scene=pj->scene;
	MovieClip *clip= pj->clip;
	struct MovieDistortion *distortion= NULL;
	short size_flag;
	int cfra, sfra= SFRA, efra= EFRA;
	int build_sizes[4], build_count= 0;
	int build_undistort_sizes[4], build_undistort_count= 0;

	size_flag= clip->proxy.build_size_flag;

	build_count= proxy_bitflag_to_array(size_flag, build_sizes, 0);
	build_undistort_count= proxy_bitflag_to_array(size_flag, build_undistort_sizes, 1);

	if (clip->source == MCLIP_SRC_MOVIE) {
		if (pj->index_context)
			IMB_anim_index_rebuild(pj->index_context, stop, do_update, progress);

		if (!build_undistort_count) {
			if (*stop)
				pj->stop = 1;

			return;
		}
		else {
			sfra= 1;
			efra= IMB_anim_get_duration(clip->anim, IMB_TC_NONE);
		}
	}

	if (build_undistort_count)
		distortion= BKE_tracking_distortion_create();

	for (cfra= sfra; cfra<=efra; cfra++) {
		if (clip->source != MCLIP_SRC_MOVIE)
			BKE_movieclip_build_proxy_frame(clip, pj->clip_flag, NULL, cfra, build_sizes, build_count, 0);

		BKE_movieclip_build_proxy_frame(clip, pj->clip_flag, distortion, cfra, build_undistort_sizes, build_undistort_count, 1);

		if (*stop || G.afbreek)
			break;

		*do_update= 1;
		*progress= ((float)cfra)/(efra-sfra);
	}

	if (distortion)
		BKE_tracking_distortion_destroy(distortion);

	if (*stop)
		pj->stop = 1;
}

static void proxy_endjob(void *pjv)
{
	ProxyJob *pj = pjv;

	if (pj->clip->anim)
		IMB_close_anim_proxies(pj->clip->anim);

	if (pj->index_context)
		IMB_anim_index_rebuild_finish(pj->index_context, pj->stop);

	BKE_movieclip_reload(pj->clip);

	WM_main_add_notifier(NC_MOVIECLIP|ND_DISPLAY, pj->clip);
}

static int clip_rebuild_proxy_exec(bContext *C, wmOperator *UNUSED(op))
{
	wmJob * steve;
	ProxyJob *pj;
	Scene *scene= CTX_data_scene(C);
	ScrArea *sa= CTX_wm_area(C);
	SpaceClip *sc= CTX_wm_space_clip(C);
	MovieClip *clip= ED_space_clip(sc);

	if ((clip->flag&MCLIP_USE_PROXY)==0)
		return OPERATOR_CANCELLED;

	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), sa, "Building Proxies", WM_JOB_PROGRESS);

	pj= MEM_callocN(sizeof(ProxyJob), "proxy rebuild job");
	pj->scene= scene;
	pj->main= CTX_data_main(C);
	pj->clip= clip;
	pj->clip_flag= clip->flag&MCLIP_TIMECODE_FLAGS;

	if (clip->anim) {
		pj->index_context = IMB_anim_index_rebuild_context(clip->anim, clip->proxy.build_tc_flag,
					clip->proxy.build_size_flag, clip->proxy.quality);
	}

	WM_jobs_customdata(steve, pj, proxy_freejob);
	WM_jobs_timer(steve, 0.2, NC_MOVIECLIP|ND_DISPLAY, 0);
	WM_jobs_callbacks(steve, proxy_startjob, NULL, NULL, proxy_endjob);

	G.afbreek= 0;
	WM_jobs_start(CTX_wm_manager(C), steve);

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

void CLIP_OT_rebuild_proxy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rebuild Proxy and Timecode Indices";
	ot->idname = "CLIP_OT_rebuild_proxy";
	ot->description = "Rebuild all selected proxies and timecode indices in the background";

	/* api callbacks */
	ot->exec = clip_rebuild_proxy_exec;
	ot->poll = ED_space_clip_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER;
}

/********************** mode set operator *********************/

static int mode_set_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc= CTX_wm_space_clip(C);
	int mode= RNA_enum_get(op->ptr, "mode");
	int toggle= RNA_boolean_get(op->ptr, "toggle");

	if (sc->mode==mode) {
		if (toggle)
			sc->mode= SC_MODE_TRACKING;
	}
	else {
		sc->mode= mode;
	}

	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_CLIP, NULL);

	return OPERATOR_FINISHED;
}

void CLIP_OT_mode_set(wmOperatorType *ot)
{
	static EnumPropertyItem mode_items[] = {
		{SC_MODE_TRACKING, "TRACKING", 0, "Tracking", "Show tracking and solving tools"},
		{SC_MODE_RECONSTRUCTION, "RECONSTRUCTION", 0, "Reconstruction", "Show tracking/reconstruction tools"},
		{SC_MODE_DISTORTION, "DISTORTION", 0, "Distortion", "Show distortion tools"},
		{0, NULL, 0, NULL, NULL}};


	/* identifiers */
	ot->name = "Set Clip Mode";
	ot->description = "Set the clip interaction mode";
	ot->idname = "CLIP_OT_mode_set";

	/* api callbacks */
	ot->exec = mode_set_exec;

	ot->poll = ED_space_clip_poll;

	/* properties */
	RNA_def_enum(ot->srna, "mode", mode_items, SC_MODE_TRACKING, "Mode", "");
	RNA_def_boolean(ot->srna, "toggle", 0, "Toggle", "");
}

/********************** macroses *********************/

void ED_operatormacros_clip(void)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;

	ot = WM_operatortype_append_macro("CLIP_OT_add_marker_move", "Add Marker and Move", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Add new marker and move it on movie";
	WM_operatortype_macro_define(ot, "CLIP_OT_add_marker");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_struct_idprops_unset(otmacro->ptr, "release_confirm");

	ot = WM_operatortype_append_macro("CLIP_OT_add_marker_slide", "Add Marker and Slide", OPTYPE_UNDO|OPTYPE_REGISTER);
	ot->description = "Add new marker and slide it with mouse until mouse button release";
	WM_operatortype_macro_define(ot, "CLIP_OT_add_marker");
	otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
	RNA_boolean_set(otmacro->ptr, "release_confirm", TRUE);
}
