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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_paint.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLI_arithb.h"

#include "PIL_time.h"

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

	/* Set whether any stroke step has yet occured
	   e.g. in sculpt mode, stroke doesn't start until cursor
	   passes over the mesh */
	int stroke_started;

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

	if(stroke && brush && (brush->flag & BRUSH_SMOOTH_STROKE)) {
		ARegion *ar = CTX_wm_region(C);
		sdrawline(x, y, (int)stroke->last_mouse_position[0] - ar->winrct.xmin,
			  (int)stroke->last_mouse_position[1] - ar->winrct.ymin);
	}

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

static void paint_draw_cursor(bContext *C, int x, int y, void *customdata)
{
	Brush *brush = paint_brush(paint_get_active(CTX_data_scene(C)));

	glColor4ubv(paint_get_active(CTX_data_scene(C))->paint_cursor_col);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	glTranslatef((float)x, (float)y, 0.0f);
	glutil_draw_lined_arc(0.0, M_PI*2.0, brush->size, 40);
	glTranslatef((float)-x, (float)-y, 0.0f);

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* Put the location of the next stroke dot into the stroke RNA and apply it to the mesh */
static void paint_brush_stroke_add_step(bContext *C, wmOperator *op, wmEvent *event, float mouse[2])
{
	PointerRNA itemptr;
	float cur_depth, pressure = 1;
	float center[3];
	PaintStroke *stroke = op->customdata;

	cur_depth = read_cached_depth(&stroke->vc, mouse[0], mouse[1]);
	view3d_unproject(&stroke->mats, center, mouse[0], mouse[1], cur_depth);	

	/* Tablet */
	if(event->custom == EVT_DATA_TABLET) {
		wmTabletData *wmtab= event->customdata;
		if(wmtab->Active != EVT_TABLET_NONE)
			pressure= wmtab->Pressure;
	}
				
	/* Add to stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);
	RNA_float_set_array(&itemptr, "location", center);
	RNA_float_set_array(&itemptr, "mouse", mouse);
	RNA_boolean_set(&itemptr, "flip", event->shift);
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

	if(stroke->brush->flag & BRUSH_SMOOTH_STROKE && stroke->brush->sculpt_tool != SCULPT_TOOL_GRAB) {
		float u = stroke->brush->smooth_stroke_factor, v = 1.0 - u;
		float dx = stroke->last_mouse_position[0] - event->x, dy = stroke->last_mouse_position[1] - event->y;

		/* If the mouse is moving within the radius of the last move,
		   don't update the mouse position. This allows sharp turns. */
		if(dx*dx + dy*dy < stroke->brush->smooth_stroke_radius * stroke->brush->smooth_stroke_radius)
			return 0;

		output[0] = event->x * v + stroke->last_mouse_position[0] * u;
		output[1] = event->y * v + stroke->last_mouse_position[1] * u;
	}

	return 1;
}

/* Returns zero if the stroke dots should not be spaced, non-zero otherwise */
static int paint_space_stroke_enabled(Brush *br)
{
	return (br->flag & BRUSH_SPACE) && !(br->flag & BRUSH_ANCHORED) && (br->sculpt_tool != SCULPT_TOOL_GRAB);
}

/* For brushes with stroke spacing enabled, moves mouse in steps
   towards the final mouse location. */
static int paint_space_stroke(bContext *C, wmOperator *op, wmEvent *event, const float final_mouse[2])
{
	PaintStroke *stroke = op->customdata;
	int cnt = 0;

	if(paint_space_stroke_enabled(stroke->brush)) {
		float mouse[2] = {stroke->last_mouse_position[0], stroke->last_mouse_position[1]};
		float vec[2] = {final_mouse[0] - mouse[0], final_mouse[1] - mouse[1]};
		float length, scale;
		int steps = 0, i;

		/* Normalize the vector between the last stroke dot and the goal */
		length = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		if(length > FLT_EPSILON) {
			scale = stroke->brush->spacing / length;
			vec[0] *= scale;
			vec[1] *= scale;

			steps = (int)(length / stroke->brush->spacing);
			for(i = 0; i < steps; ++i, ++cnt) {
				mouse[0] += vec[0];
				mouse[1] += vec[1];
				paint_brush_stroke_add_step(C, op, event, mouse);
			}
		}
	}

	return cnt;
}

/**** Public API ****/

PaintStroke *paint_stroke_new(bContext *C, StrokeTestStart test_start,
			      StrokeUpdateStep update_step, StrokeDone done)
{
	PaintStroke *stroke = MEM_callocN(sizeof(PaintStroke), "PaintStroke");

	stroke->brush = paint_brush(paint_get_active(CTX_data_scene(C)));
	view3d_set_viewcontext(C, &stroke->vc);
	view3d_get_transformation(&stroke->vc, stroke->vc.obact, &stroke->mats);

	stroke->test_start = test_start;
	stroke->update_step = update_step;
	stroke->done = done;

	return stroke;
}

int paint_stroke_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	PaintStroke *stroke = op->customdata;
	float mouse[2];

	if(event->type == TIMER && (event->customdata != stroke->timer))
		return OPERATOR_RUNNING_MODAL;

	if(!stroke->stroke_started) {
		stroke->last_mouse_position[0] = event->x;
		stroke->last_mouse_position[1] = event->y;
		stroke->stroke_started = stroke->test_start(C, op, event);

		if(stroke->stroke_started) {
			stroke->smooth_stroke_cursor =
				WM_paint_cursor_activate(CTX_wm_manager(C), paint_poll, paint_draw_smooth_stroke, stroke);

			if(stroke->brush->flag & BRUSH_AIRBRUSH)
				stroke->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, stroke->brush->rate);
		}

		ED_region_tag_redraw(ar);
	}

	if(stroke->stroke_started) {
		if(paint_smooth_stroke(stroke, mouse, event)) {
			if(paint_space_stroke_enabled(stroke->brush)) {
				if(!paint_space_stroke(C, op, event, mouse))
					ED_region_tag_redraw(ar);
			}
			else
				paint_brush_stroke_add_step(C, op, event, mouse);
		}
		else
			ED_region_tag_redraw(ar);
	}

	/* TODO: fix hardcoded event here */
	if(event->type == LEFTMOUSE && event->val == KM_RELEASE) {
		/* Exit stroke, free data */

		if(stroke->smooth_stroke_cursor)
			WM_paint_cursor_end(CTX_wm_manager(C), stroke->smooth_stroke_cursor);

		if(stroke->timer)
			WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), stroke->timer);

		stroke->done(C, stroke);
		MEM_freeN(stroke);
		return OPERATOR_FINISHED;
	}
	else
		return OPERATOR_RUNNING_MODAL;
}

int paint_stroke_exec(bContext *C, wmOperator *op)
{
	PaintStroke *stroke = op->customdata;

	RNA_BEGIN(op->ptr, itemptr, "stroke") {
		stroke->update_step(C, stroke, &itemptr);
	}
	RNA_END;

	MEM_freeN(stroke);
	op->customdata = NULL;

	return OPERATOR_FINISHED;
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

void paint_cursor_start(bContext *C, int (*poll)(bContext *C))
{
	Paint *p = paint_get_active(CTX_data_scene(C));

	if(p && !p->paint_cursor)
		p->paint_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), poll, paint_draw_cursor, NULL);
}

