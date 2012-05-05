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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins, Tom Musgrove.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/editors/sculpt_paint/paint_stroke.c
 *  \ingroup edsculpt
 */


#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_brush.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"

#include <float.h>
#include <math.h>

typedef struct PaintStroke {
	void *mode_data;
	void *smooth_stroke_cursor;
	wmTimer *timer;

	/* Cached values */
	ViewContext vc;
	bglMats mats;
	Brush *brush;

	float last_mouse_position[2];

	/* Set whether any stroke step has yet occurred
	 * e.g. in sculpt mode, stroke doesn't start until cursor
	 * passes over the mesh */
	int stroke_started;
	/* event that started stroke, for modal() return */
	int event_type;
	
	StrokeGetLocation get_location;
	StrokeTestStart test_start;
	StrokeUpdateStep update_step;
	StrokeDone done;
} PaintStroke;

/*** Cursor ***/
static void paint_draw_smooth_stroke(bContext *C, int x, int y, void *customdata) 
{
	Brush *brush = paint_brush(paint_get_active(CTX_data_scene(C)));
	PaintStroke *stroke = customdata;

	glColor4ubv(paint_get_active(CTX_data_scene(C))->paint_cursor_col);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	if (stroke && brush && (brush->flag & BRUSH_SMOOTH_STROKE)) {
		ARegion *ar = CTX_wm_region(C);
		sdrawline(x, y, (int)stroke->last_mouse_position[0] - ar->winrct.xmin,
		          (int)stroke->last_mouse_position[1] - ar->winrct.ymin);
	}

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* if this is a tablet event, return tablet pressure and set *pen_flip
 * to 1 if the eraser tool is being used, 0 otherwise */
static float event_tablet_data(wmEvent *event, int *pen_flip)
{
	int erasor = 0;
	float pressure = 1;

	if (event->custom == EVT_DATA_TABLET) {
		wmTabletData *wmtab = event->customdata;

		erasor = (wmtab->Active == EVT_TABLET_ERASER);
		pressure = (wmtab->Active != EVT_TABLET_NONE) ? wmtab->Pressure : 1;
	}

	if (pen_flip)
		(*pen_flip) = erasor;

	return pressure;
}

/* Put the location of the next stroke dot into the stroke RNA and apply it to the mesh */
static void paint_brush_stroke_add_step(bContext *C, wmOperator *op, wmEvent *event, float mouse_in[2])
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = paint_get_active(scene);
	Brush *brush = paint_brush(paint);
	PaintStroke *stroke = op->customdata;
	float mouse[3];
	PointerRNA itemptr;
	float location[3];
	float pressure;
	int pen_flip;

	/* see if tablet affects event */
	pressure = event_tablet_data(event, &pen_flip);

	/* TODO: as sculpt and other paint modes are unified, this
	 * separation will go away */
	if (stroke->vc.obact->sculpt) {
		float delta[2];

		BKE_brush_jitter_pos(scene, brush, mouse_in, mouse);

		/* XXX: meh, this is round about because
		 * BKE_brush_jitter_pos isn't written in the best way to
		 * be reused here */
		if (brush->flag & BRUSH_JITTER_PRESSURE) {
			sub_v2_v2v2(delta, mouse, mouse_in);
			mul_v2_fl(delta, pressure);
			add_v2_v2v2(mouse, mouse_in, delta);
		}
	}
	else {
		copy_v2_v2(mouse, mouse_in);
	}

	/* TODO: can remove the if statement once all modes have this */
	if (stroke->get_location)
		stroke->get_location(C, location, mouse);
	else
		zero_v3(location);

	/* Add to stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	RNA_float_set_array(&itemptr, "location", location);
	RNA_float_set_array(&itemptr, "mouse", mouse);
	RNA_boolean_set(&itemptr, "pen_flip", pen_flip);
	RNA_float_set(&itemptr, "pressure", pressure);

	stroke->last_mouse_position[0] = mouse[0];
	stroke->last_mouse_position[1] = mouse[1];

	stroke->update_step(C, stroke, &itemptr);
}

/* Returns zero if no sculpt changes should be made, non-zero otherwise */
static int paint_smooth_stroke(PaintStroke *stroke, float output[2], wmEvent *event)
{
	output[0] = event->x; 
	output[1] = event->y;

	if ((stroke->brush->flag & BRUSH_SMOOTH_STROKE) &&  
	    !ELEM4(stroke->brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_SNAKE_HOOK) &&
	    !(stroke->brush->flag & BRUSH_ANCHORED) &&
	    !(stroke->brush->flag & BRUSH_RESTORE_MESH))
	{
		float u = stroke->brush->smooth_stroke_factor, v = 1.0f - u;
		float dx = stroke->last_mouse_position[0] - event->x, dy = stroke->last_mouse_position[1] - event->y;

		/* If the mouse is moving within the radius of the last move,
		 * don't update the mouse position. This allows sharp turns. */
		if (dx * dx + dy * dy < stroke->brush->smooth_stroke_radius * stroke->brush->smooth_stroke_radius)
			return 0;

		output[0] = event->x * v + stroke->last_mouse_position[0] * u;
		output[1] = event->y * v + stroke->last_mouse_position[1] * u;
	}

	return 1;
}

/* For brushes with stroke spacing enabled, moves mouse in steps
 * towards the final mouse location. */
static int paint_space_stroke(bContext *C, wmOperator *op, wmEvent *event, const float final_mouse[2])
{
	PaintStroke *stroke = op->customdata;
	int cnt = 0;

	if (paint_space_stroke_enabled(stroke->brush)) {
		float mouse[2];
		float vec[2];
		float length, scale;

		copy_v2_v2(mouse, stroke->last_mouse_position);
		sub_v2_v2v2(vec, final_mouse, mouse);

		length = len_v2(vec);

		if (length > FLT_EPSILON) {
			const Scene *scene = CTX_data_scene(C);
			int steps;
			int i;
			float pressure = 1.0f;

			/* XXX mysterious :) what has 'use size' do with this here... if you don't check for it, pressure fails */
			if (BKE_brush_use_size_pressure(scene, stroke->brush))
				pressure = event_tablet_data(event, NULL);
			
			if (pressure > FLT_EPSILON) {
				scale = (BKE_brush_size_get(scene, stroke->brush) * pressure * stroke->brush->spacing / 50.0f) / length;
				if (scale > FLT_EPSILON) {
					mul_v2_fl(vec, scale);

					steps = (int)(1.0f / scale);

					for (i = 0; i < steps; ++i, ++cnt) {
						add_v2_v2(mouse, vec);
						paint_brush_stroke_add_step(C, op, event, mouse);
					}
				}
			}
		}
	}

	return cnt;
}

/**** Public API ****/

PaintStroke *paint_stroke_new(bContext *C,
                              StrokeGetLocation get_location,
                              StrokeTestStart test_start,
                              StrokeUpdateStep update_step,
                              StrokeDone done, int event_type)
{
	PaintStroke *stroke = MEM_callocN(sizeof(PaintStroke), "PaintStroke");

	stroke->brush = paint_brush(paint_get_active(CTX_data_scene(C)));
	view3d_set_viewcontext(C, &stroke->vc);
	view3d_get_transformation(stroke->vc.ar, stroke->vc.rv3d, stroke->vc.obact, &stroke->mats);

	stroke->get_location = get_location;
	stroke->test_start = test_start;
	stroke->update_step = update_step;
	stroke->done = done;
	stroke->event_type = event_type; /* for modal, return event */
	
	return stroke;
}

void paint_stroke_data_free(struct wmOperator *op)
{
	MEM_freeN(op->customdata);
	op->customdata= NULL;
}

static void stroke_done(struct bContext *C, struct wmOperator *op)
{
	struct PaintStroke *stroke = op->customdata;

	if (stroke->stroke_started && stroke->done)
		stroke->done(C, stroke);

	if (stroke->timer) {
		WM_event_remove_timer(
			CTX_wm_manager(C),
			CTX_wm_window(C),
			stroke->timer);
	}

	if (stroke->smooth_stroke_cursor)
		WM_paint_cursor_end(CTX_wm_manager(C), stroke->smooth_stroke_cursor);

	paint_stroke_data_free(op);
}

/* Returns zero if the stroke dots should not be spaced, non-zero otherwise */
int paint_space_stroke_enabled(Brush *br)
{
	return (br->flag & BRUSH_SPACE) &&
	       !(br->flag & BRUSH_ANCHORED) &&
	       !ELEM4(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_SNAKE_HOOK);
}

#define PAINT_STROKE_MODAL_CANCEL 1

/* called in paint_ops.c, on each regeneration of keymaps  */
struct wmKeyMap *paint_stroke_modal_keymap(struct wmKeyConfig *keyconf)
{
	static struct EnumPropertyItem modal_items[] = {
		{PAINT_STROKE_MODAL_CANCEL, "CANCEL", 0,
		"Cancel",
		"Cancel and undo a stroke in progress"},

		{ 0 }
	};

	static const char *name= "Paint Stroke Modal";

	struct wmKeyMap *keymap= WM_modalkeymap_get(keyconf, name);

	/* this function is called for each spacetype, only needs to add map once */
	if (!keymap) {
		keymap= WM_modalkeymap_add(keyconf, name, modal_items);

		/* items for modal map */
		WM_modalkeymap_add_item(
			keymap, ESCKEY, KM_PRESS, KM_ANY, 0, PAINT_STROKE_MODAL_CANCEL);
	}

	return keymap;
}

int paint_stroke_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	PaintStroke *stroke = op->customdata;
	float mouse[2];
	int first = 0;

	// let NDOF motion pass through to the 3D view so we can paint and rotate simultaneously!
	// this isn't perfect... even when an extra MOUSEMOVE is spoofed, the stroke discards it
	// since the 2D deltas are zero -- code in this file needs to be updated to use the
	// post-NDOF_MOTION MOUSEMOVE
	if (event->type == NDOF_MOTION)
		return OPERATOR_PASS_THROUGH;

	if (!stroke->stroke_started) {
		stroke->last_mouse_position[0] = event->x;
		stroke->last_mouse_position[1] = event->y;
		stroke->stroke_started = stroke->test_start(C, op, event);

		if (stroke->stroke_started) {
			stroke->smooth_stroke_cursor =
			    WM_paint_cursor_activate(CTX_wm_manager(C), paint_poll, paint_draw_smooth_stroke, stroke);

			if (stroke->brush->flag & BRUSH_AIRBRUSH)
				stroke->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, stroke->brush->rate);
		}

		first = 1;
		//ED_region_tag_redraw(ar);
	}

	/* Cancel */
	if (event->type == EVT_MODAL_MAP && event->val == PAINT_STROKE_MODAL_CANCEL) {
		if (op->type->cancel)
			return op->type->cancel(C, op);
		else
			return paint_stroke_cancel(C, op);
	}

	if (event->type == stroke->event_type && event->val == KM_RELEASE) {
		stroke_done(C, op);
		return OPERATOR_FINISHED;
	}
	else if ((first) ||
	         (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) ||
	         (event->type == TIMER && (event->customdata == stroke->timer)) )
	{
		if (stroke->stroke_started) {
			if (paint_smooth_stroke(stroke, mouse, event)) {
				if (paint_space_stroke_enabled(stroke->brush)) {
					if (!paint_space_stroke(C, op, event, mouse)) {
						//ED_region_tag_redraw(ar);
					}
				}
				else {
					paint_brush_stroke_add_step(C, op, event, mouse);
				}
			}
			else {
				; //ED_region_tag_redraw(ar);
			}
		}
	}

	/* we want the stroke to have the first daub at the start location
	 * instead of waiting till we have moved the space distance */
	if (first &&
	    stroke->stroke_started &&
	    paint_space_stroke_enabled(stroke->brush) &&
	    !(stroke->brush->flag & BRUSH_ANCHORED) &&
	    !(stroke->brush->flag & BRUSH_SMOOTH_STROKE))
	{
		paint_brush_stroke_add_step(C, op, event, mouse);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

int paint_stroke_exec(bContext *C, wmOperator *op)
{
	PaintStroke *stroke = op->customdata;

	/* only when executed for the first time */
	if (stroke->stroke_started == 0) {
		/* XXX stroke->last_mouse_position is unset, this may cause problems */
		stroke->test_start(C, op, NULL);
		stroke->stroke_started = 1;
	}

	RNA_BEGIN (op->ptr, itemptr, "stroke")
	{
		stroke->update_step(C, stroke, &itemptr);
	}
	RNA_END;

	stroke_done(C, op);

	return OPERATOR_FINISHED;
}

int paint_stroke_cancel(bContext *C, wmOperator *op)
{
	stroke_done(C, op);
	return OPERATOR_CANCELLED;
}

ViewContext *paint_stroke_view_context(PaintStroke *stroke)
{
	return &stroke->vc;
}

void *paint_stroke_mode_data(struct PaintStroke *stroke)
{
	return stroke->mode_data;
}

void paint_stroke_set_mode_data(PaintStroke *stroke, void *mode_data)
{
	stroke->mode_data = mode_data;
}

int paint_poll(bContext *C)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Object *ob = CTX_data_active_object(C);

	return p && ob && paint_brush(p) &&
	       CTX_wm_area(C)->spacetype == SPACE_VIEW3D &&
	       CTX_wm_region(C)->regiontype == RGN_TYPE_WINDOW;
}
