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
#include "BLI_rand.h"

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

typedef struct PaintSample {
	float mouse[2];

	/* TODO: other input properties, e.g. tablet pressure */
} PaintSample;

typedef struct PaintStroke {
	void *mode_data;
	void *smooth_stroke_cursor;
	wmTimer *timer;

	/* Cached values */
	ViewContext vc;
	bglMats mats;
	Brush *brush;

	/* Paint stroke can use up to PAINT_MAX_INPUT_SAMPLES prior inputs
	 * to smooth the stroke */
	PaintSample samples[PAINT_MAX_INPUT_SAMPLES];
	int num_samples;
	int cur_sample;

	float last_mouse_position[2];

	/* Set whether any stroke step has yet occurred
	 * e.g. in sculpt mode, stroke doesn't start until cursor
	 * passes over the mesh */
	int stroke_started;
	/* event that started stroke, for modal() return */
	int event_type;
	
	bool brush_init;
	float initial_mouse[2];
	float cached_pressure;

	float zoom_2d;

	StrokeGetLocation get_location;
	StrokeTestStart test_start;
	StrokeUpdateStep update_step;
	StrokeDone done;
} PaintStroke;

/*** Cursor ***/
static void paint_draw_smooth_stroke(bContext *C, int x, int y, void *customdata) 
{
	Paint *paint = paint_get_active_from_context(C);
	Brush *brush = paint_brush(paint);
	PaintStroke *stroke = customdata;

	if (stroke && brush && (brush->flag & BRUSH_SMOOTH_STROKE)) {
		glColor4ubv(paint->paint_cursor_col);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);

		sdrawline(x, y, (int)stroke->last_mouse_position[0],
		          (int)stroke->last_mouse_position[1]);
		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);
	}
}

/* if this is a tablet event, return tablet pressure and set *pen_flip
 * to 1 if the eraser tool is being used, 0 otherwise */
static float event_tablet_data(const wmEvent *event, int *pen_flip)
{
	int erasor = 0;
	float pressure = 1;

	if (event->tablet_data) {
		wmTabletData *wmtab = event->tablet_data;

		erasor = (wmtab->Active == EVT_TABLET_ERASER);
		pressure = (wmtab->Active != EVT_TABLET_NONE) ? wmtab->Pressure : 1;
	}

	if (pen_flip)
		(*pen_flip) = erasor;

	return pressure;
}


/* Initialize the stroke cache variants from operator properties */
static void paint_brush_update(bContext *C, Brush *brush, PaintMode mode,
                                         struct PaintStroke *stroke,
                                         const float mouse[2], float pressure)
{
	Scene *scene = CTX_data_scene(C);
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;

	/* XXX: Use pressure value from first brush step for brushes which don't
	 *      support strokes (grab, thumb). They depends on initial state and
	 *      brush coord/pressure/etc.
	 *      It's more an events design issue, which doesn't split coordinate/pressure/angle
	 *      changing events. We should avoid this after events system re-design */
	if (paint_supports_dynamic_size(brush, mode) || !stroke->brush_init) {
		copy_v2_v2(stroke->initial_mouse, mouse);
		copy_v2_v2(ups->tex_mouse, mouse);
		stroke->cached_pressure = pressure;
	}

	/* Truly temporary data that isn't stored in properties */

	ups->draw_pressure = TRUE;
	ups->pressure_value = stroke->cached_pressure;

	ups->pixel_radius = BKE_brush_size_get(scene, brush);

	if (BKE_brush_use_size_pressure(scene, brush) && paint_supports_dynamic_size(brush, mode)) {
		ups->pixel_radius *= stroke->cached_pressure;
	}

	if (paint_supports_dynamic_tex_coords(brush, mode)) {
		if (((brush->mtex.brush_map_mode == MTEX_MAP_MODE_VIEW) ||
		    (brush->mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM)) &&
		    !(brush->flag & BRUSH_RAKE))
		{
			if (brush->flag & BRUSH_RANDOM_ROTATION)
				ups->brush_rotation = 2.0f * (float)M_PI * BLI_frand();
			else
				ups->brush_rotation = 0.0f;
		}

		if ((brush->mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM))
			BKE_brush_randomize_texture_coordinates(ups);
		else
			copy_v2_v2(ups->tex_mouse, mouse);
	}

	if (brush->flag & BRUSH_ANCHORED) {
		bool hit = false;
		float halfway[2];

		const float dx = mouse[0] - stroke->initial_mouse[0];
		const float dy = mouse[1] - stroke->initial_mouse[1];

		ups->anchored_size = ups->pixel_radius = sqrt(dx * dx + dy * dy);

		ups->brush_rotation = atan2(dx, dy) + M_PI;

		if (brush->flag & BRUSH_EDGE_TO_EDGE) {
			float out[3];

			halfway[0] = dx * 0.5f + stroke->initial_mouse[0];
			halfway[1] = dy * 0.5f + stroke->initial_mouse[1];

			if (stroke->get_location) {
				if (stroke->get_location(C, out, halfway)) {
					hit = true;
				}
			}
			else {
				hit = true;
			}
		}
		if (hit) {
			copy_v2_v2(ups->anchored_initial_mouse, halfway);
			copy_v2_v2(ups->tex_mouse, halfway);
			ups->anchored_size /= 2.0f;
			ups->pixel_radius  /= 2.0f;
		}
		else
			copy_v2_v2(ups->anchored_initial_mouse, stroke->initial_mouse);

		ups->draw_anchored = 1;
	}
	else if (brush->flag & BRUSH_RAKE) {
		if (!stroke->brush_init)
			copy_v2_v2(ups->last_rake, mouse);
		else
			paint_calculate_rake_rotation(ups, mouse);
	}

	stroke->brush_init = TRUE;
}


/* Put the location of the next stroke dot into the stroke RNA and apply it to the mesh */
static void paint_brush_stroke_add_step(bContext *C, wmOperator *op, const wmEvent *event, const float mouse_in[2])
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = paint_get_active_from_context(C);
	PaintMode mode = paintmode_get_active_from_context(C);
	Brush *brush = paint_brush(paint);
	PaintStroke *stroke = op->customdata;
	float mouse_out[2];
	PointerRNA itemptr;
	float location[3];
	float pressure;
	int pen_flip;

	/* see if tablet affects event */
	pressure = event_tablet_data(event, &pen_flip);

/* the following code is adapted from texture paint. It may not be needed but leaving here
 * just in case for reference (code in texpaint removed as part of refactoring).
 * It's strange that only texpaint had these guards. */
#if 0
	/* special exception here for too high pressure values on first touch in
	 * windows for some tablets, then we just skip first touch ..  */
	if (tablet && (pressure >= 0.99f) && ((pop->s.brush->flag & BRUSH_SPACING_PRESSURE) || BKE_brush_use_alpha_pressure(scene, pop->s.brush) || BKE_brush_use_size_pressure(scene, pop->s.brush)))
		return;

	/* This can be removed once fixed properly in
	 * BKE_brush_painter_paint(BrushPainter *painter, BrushFunc func, float *pos, double time, float pressure, void *user)
	 * at zero pressure we should do nothing 1/2^12 is 0.0002 which is the sensitivity of the most sensitive pen tablet available */
	if (tablet && (pressure < 0.0002f) && ((pop->s.brush->flag & BRUSH_SPACING_PRESSURE) || BKE_brush_use_alpha_pressure(scene, pop->s.brush) || BKE_brush_use_size_pressure(scene, pop->s.brush)))
		return;
#endif

	/* copy last position -before- jittering, or space fill code
	 * will create too many dabs */
	copy_v2_v2(stroke->last_mouse_position, mouse_in);

	paint_brush_update(C, brush, mode, stroke, mouse_in, pressure);

	/* TODO: as sculpt and other paint modes are unified, this
	 * separation will go away */
	if (paint_supports_jitter(mode)) {
		float delta[2];
		float factor = stroke->zoom_2d;

		if (brush->flag & BRUSH_JITTER_PRESSURE)
			factor *= pressure;

		BKE_brush_jitter_pos(scene, brush, mouse_in, mouse_out);

		/* XXX: meh, this is round about because
		 * BKE_brush_jitter_pos isn't written in the best way to
		 * be reused here */
		if (factor != 1.0f) {
			sub_v2_v2v2(delta, mouse_out, mouse_in);
			mul_v2_fl(delta, factor);
			add_v2_v2v2(mouse_out, mouse_in, delta);
		}
	}
	else {
		copy_v2_v2(mouse_out, mouse_in);
	}

	/* TODO: can remove the if statement once all modes have this */
	if (stroke->get_location)
		stroke->get_location(C, location, mouse_out);
	else
		zero_v3(location);

	/* Add to stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	RNA_float_set_array(&itemptr, "location", location);
	RNA_float_set_array(&itemptr, "mouse", mouse_out);
	RNA_boolean_set(&itemptr, "pen_flip", pen_flip);
	RNA_float_set(&itemptr, "pressure", pressure);

	stroke->update_step(C, stroke, &itemptr);
}

/* Returns zero if no sculpt changes should be made, non-zero otherwise */
static int paint_smooth_stroke(PaintStroke *stroke, float output[2],
                               const PaintSample *sample, PaintMode mode)
{
	output[0] = sample->mouse[0];
	output[1] = sample->mouse[1];

	if (paint_supports_smooth_stroke(stroke->brush, mode)) {
		float radius = stroke->brush->smooth_stroke_radius * stroke->zoom_2d;
		float u = stroke->brush->smooth_stroke_factor, v = 1.0f - u;
		float dx = stroke->last_mouse_position[0] - sample->mouse[0];
		float dy = stroke->last_mouse_position[1] - sample->mouse[1];

		/* If the mouse is moving within the radius of the last move,
		 * don't update the mouse position. This allows sharp turns. */
		if (dx * dx + dy * dy <  radius * radius)
			return 0;

		output[0] = sample->mouse[0] * v + stroke->last_mouse_position[0] * u;
		output[1] = sample->mouse[1] * v + stroke->last_mouse_position[1] * u;
	}

	return 1;
}

/* For brushes with stroke spacing enabled, moves mouse in steps
 * towards the final mouse location. */
static int paint_space_stroke(bContext *C, wmOperator *op, const wmEvent *event, const float final_mouse[2])
{
	PaintStroke *stroke = op->customdata;
	PaintMode mode = paintmode_get_active_from_context(C);

	int cnt = 0;

	if (paint_space_stroke_enabled(stroke->brush, mode)) {
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
			float size_pressure = 1.0f;
			float pressure = event_tablet_data(event, NULL);

			/* XXX mysterious :) what has 'use size' do with this here... if you don't check for it, pressure fails */
			if (BKE_brush_use_size_pressure(scene, stroke->brush))
				size_pressure = pressure;
			
			if (size_pressure > FLT_EPSILON) {
				/* brushes can have a minimum size of 1.0 but with pressure it can be smaller then a pixel
				 * causing very high step sizes, hanging blender [#32381] */
				const float size_clamp = max_ff(1.0f, BKE_brush_size_get(scene, stroke->brush) * size_pressure);
				float spacing = stroke->brush->spacing;

				/* stroke system is used for 2d paint too, so we need to account for
				 * the fact that brush can be scaled there. */

				if (stroke->brush->flag & BRUSH_SPACING_PRESSURE)
					spacing = max_ff(1.0f, spacing * (1.5f - pressure));

				spacing *= stroke->zoom_2d;

				scale = (size_clamp * spacing / 50.0f) / length;
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

	stroke->brush = paint_brush(paint_get_active_from_context(C));
	view3d_set_viewcontext(C, &stroke->vc);
	if (stroke->vc.v3d)
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
	op->customdata = NULL;
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
bool paint_space_stroke_enabled(Brush *br, PaintMode mode)
{
	return (br->flag & BRUSH_SPACE) && paint_supports_dynamic_size(br, mode);
}

/* return true if the brush size can change during paint (normally used for pressure) */
bool paint_supports_dynamic_size(Brush *br, PaintMode mode)
{
	if (br->flag & BRUSH_ANCHORED)
		return false;

	switch (mode) {
		case PAINT_SCULPT:
			if (ELEM4(br->sculpt_tool,
			          SCULPT_TOOL_GRAB,
			          SCULPT_TOOL_THUMB,
			          SCULPT_TOOL_ROTATE,
			          SCULPT_TOOL_SNAKE_HOOK))
			{
				return false;
			}
		default:
			;
	}
	return true;
}

bool paint_supports_smooth_stroke(Brush *br, PaintMode mode)
{
	if (!(br->flag & BRUSH_SMOOTH_STROKE) ||
	     (br->flag & BRUSH_ANCHORED) ||
	     (br->flag & BRUSH_RESTORE_MESH))
	{
		return false;
	}

	switch (mode) {
		case PAINT_SCULPT:
			if (ELEM4(br->sculpt_tool,
			          SCULPT_TOOL_GRAB,
			          SCULPT_TOOL_THUMB,
			          SCULPT_TOOL_ROTATE,
			          SCULPT_TOOL_SNAKE_HOOK))
			{
				return false;
			}
		default:
			;
	}
	return true;
}

/* return true if the brush size can change during paint (normally used for pressure) */
bool paint_supports_dynamic_tex_coords(Brush *br, PaintMode mode)
{
	if (br->flag & BRUSH_ANCHORED)
		return false;

	switch (mode) {
		case PAINT_SCULPT:
			if (ELEM4(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_SNAKE_HOOK))
				return false;
		default:
			;
		}
	return true;
}

bool paint_supports_jitter(PaintMode mode)
{
	return ELEM3(mode, PAINT_SCULPT, PAINT_TEXTURE_PROJECTIVE, PAINT_TEXTURE_2D);
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

	static const char *name = "Paint Stroke Modal";

	struct wmKeyMap *keymap = WM_modalkeymap_get(keyconf, name);

	/* this function is called for each spacetype, only needs to add map once */
	if (!keymap) {
		keymap = WM_modalkeymap_add(keyconf, name, modal_items);

		/* items for modal map */
		WM_modalkeymap_add_item(
			keymap, ESCKEY, KM_PRESS, KM_ANY, 0, PAINT_STROKE_MODAL_CANCEL);
	}

	return keymap;
}

static void paint_stroke_add_sample(const Paint *paint,
                                    PaintStroke *stroke,
                                    float x, float y)
{
	PaintSample *sample = &stroke->samples[stroke->cur_sample];
	int max_samples = MIN2(PAINT_MAX_INPUT_SAMPLES,
	                       MAX2(paint->num_input_samples, 1));

	sample->mouse[0] = x;
	sample->mouse[1] = y;

	stroke->cur_sample++;
	if (stroke->cur_sample >= max_samples)
		stroke->cur_sample = 0;
	if (stroke->num_samples < max_samples)
		stroke->num_samples++;
}

static void paint_stroke_sample_average(const PaintStroke *stroke,
                                        PaintSample *average)
{
	int i;
	
	memset(average, 0, sizeof(*average));

	BLI_assert(stroke->num_samples > 0);
	
	for (i = 0; i < stroke->num_samples; i++)
		add_v2_v2(average->mouse, stroke->samples[i].mouse);

	mul_v2_fl(average->mouse, 1.0f / stroke->num_samples);

	/*printf("avg=(%f, %f), num=%d\n", average->mouse[0], average->mouse[1], stroke->num_samples);*/
}

int paint_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Paint *p = paint_get_active_from_context(C);
	PaintMode mode = paintmode_get_active_from_context(C);
	PaintStroke *stroke = op->customdata;
	PaintSample sample_average;
	float mouse[2];
	int first = 0;
	float zoomx, zoomy;

	paint_stroke_add_sample(p, stroke, event->mval[0], event->mval[1]);
	paint_stroke_sample_average(stroke, &sample_average);

	get_imapaint_zoom(C, &zoomx, &zoomy);
	stroke->zoom_2d = max_ff(zoomx, zoomy);

	/* let NDOF motion pass through to the 3D view so we can paint and rotate simultaneously!
	 * this isn't perfect... even when an extra MOUSEMOVE is spoofed, the stroke discards it
	 * since the 2D deltas are zero -- code in this file needs to be updated to use the
	 * post-NDOF_MOTION MOUSEMOVE */
	if (event->type == NDOF_MOTION)
		return OPERATOR_PASS_THROUGH;

	if (!stroke->stroke_started) {
		copy_v2_v2(stroke->last_mouse_position, sample_average.mouse);
		stroke->stroke_started = stroke->test_start(C, op, sample_average.mouse);
		BLI_assert((stroke->stroke_started & ~1) == 0);  /* 0/1 */

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
			if (paint_smooth_stroke(stroke, mouse, &sample_average, mode)) {
				if (paint_space_stroke_enabled(stroke->brush, mode)) {
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
	    paint_space_stroke_enabled(stroke->brush, mode) &&
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
	Paint *p = paint_get_active_from_context(C);
	Object *ob = CTX_data_active_object(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);

	return p && ob && paint_brush(p) &&
	       (sa && sa->spacetype == SPACE_VIEW3D) &&
	       (ar && ar->regiontype == RGN_TYPE_WINDOW);
}
