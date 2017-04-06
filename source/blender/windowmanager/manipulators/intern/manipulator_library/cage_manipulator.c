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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/manipulator_library/cage_manipulator.c
 *  \ingroup wm
 *
 * \name Cage Manipulator
 *
 * 2D Manipulator
 *
 * \brief Rectangular manipulator acting as a 'cage' around its content.
 * Interacting scales or translates the manipulator.
 */

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "DNA_manipulator_types.h"

#include "ED_screen.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"


/* wmManipulator->highlighted_part */
enum {
	MANIPULATOR_RECT_TRANSFORM_INTERSECT_TRANSLATE     = 1,
	MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT   = 2,
	MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT  = 3,
	MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_UP     = 4,
	MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN   = 5
};

#define MANIPULATOR_RECT_MIN_WIDTH 15.0f
#define MANIPULATOR_RESIZER_WIDTH  20.0f

typedef struct RectTransformManipulator {
	wmManipulator manipulator;
	float w, h;      /* dimensions of manipulator */
	float rotation;  /* rotation of the rectangle */
	float scale[2]; /* scaling for the manipulator for non-destructive editing. */
	int style;
} RectTransformManipulator;


/* -------------------------------------------------------------------- */

static void rect_transform_draw_corners(rctf *r, const float offsetx, const float offsety)
{
	glBegin(GL_LINES);
	glVertex2f(r->xmin, r->ymin + offsety);
	glVertex2f(r->xmin, r->ymin);
	glVertex2f(r->xmin, r->ymin);
	glVertex2f(r->xmin + offsetx, r->ymin);

	glVertex2f(r->xmax, r->ymin + offsety);
	glVertex2f(r->xmax, r->ymin);
	glVertex2f(r->xmax, r->ymin);
	glVertex2f(r->xmax - offsetx, r->ymin);

	glVertex2f(r->xmax, r->ymax - offsety);
	glVertex2f(r->xmax, r->ymax);
	glVertex2f(r->xmax, r->ymax);
	glVertex2f(r->xmax - offsetx, r->ymax);

	glVertex2f(r->xmin, r->ymax - offsety);
	glVertex2f(r->xmin, r->ymax);
	glVertex2f(r->xmin, r->ymax);
	glVertex2f(r->xmin + offsetx, r->ymax);
	glEnd();
}

static void rect_transform_draw_interaction(
        const float col[4], const int highlighted,
        const float half_w, const float half_h,
        const float w, const float h, const float line_width)
{
	float verts[4][2];

	switch (highlighted) {
		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT:
			verts[0][0] = -half_w + w;
			verts[0][1] = -half_h;
			verts[1][0] = -half_w;
			verts[1][1] = -half_h;
			verts[2][0] = -half_w;
			verts[2][1] = half_h;
			verts[3][0] = -half_w + w;
			verts[3][1] = half_h;
			break;

		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT:
			verts[0][0] = half_w - w;
			verts[0][1] = -half_h;
			verts[1][0] = half_w;
			verts[1][1] = -half_h;
			verts[2][0] = half_w;
			verts[2][1] = half_h;
			verts[3][0] = half_w - w;
			verts[3][1] = half_h;
			break;

		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN:
			verts[0][0] = -half_w;
			verts[0][1] = -half_h + h;
			verts[1][0] = -half_w;
			verts[1][1] = -half_h;
			verts[2][0] = half_w;
			verts[2][1] = -half_h;
			verts[3][0] = half_w;
			verts[3][1] = -half_h + h;
			break;

		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_UP:
			verts[0][0] = -half_w;
			verts[0][1] = half_h - h;
			verts[1][0] = -half_w;
			verts[1][1] = half_h;
			verts[2][0] = half_w;
			verts[2][1] = half_h;
			verts[3][0] = half_w;
			verts[3][1] = half_h - h;
			break;

		default:
			return;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glLineWidth(line_width + 3.0);
	glColor3f(0.0, 0.0, 0.0);
	glDrawArrays(GL_LINE_STRIP, 0, 3);
	glLineWidth(line_width);
	glColor3fv(col);
	glDrawArrays(GL_LINE_STRIP, 0, 3);
	glLineWidth(1.0);
}

static void manipulator_rect_transform_draw(const bContext *UNUSED(C), wmManipulator *manipulator)
{
	RectTransformManipulator *cage = (RectTransformManipulator *)manipulator;
	rctf r;
	float w = cage->w;
	float h = cage->h;
	float aspx = 1.0f, aspy = 1.0f;
	const float half_w = w / 2.0f;
	const float half_h = h / 2.0f;

	r.xmin = -half_w;
	r.ymin = -half_h;
	r.xmax = half_w;
	r.ymax = half_h;

	glPushMatrix();
	glTranslatef(manipulator->origin[0] + manipulator->offset[0],
	        manipulator->origin[1] + manipulator->offset[1], 0.0f);
	if (cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM)
		glScalef(cage->scale[0], cage->scale[0], 1.0);
	else
		glScalef(cage->scale[0], cage->scale[1], 1.0);

	if (w > h)
		aspx = h / w;
	else
		aspy = w / h;
	w = min_ff(aspx * w / MANIPULATOR_RESIZER_WIDTH, MANIPULATOR_RESIZER_WIDTH / cage->scale[0]);
	h = min_ff(aspy * h / MANIPULATOR_RESIZER_WIDTH, MANIPULATOR_RESIZER_WIDTH / 
	           ((cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) ? cage->scale[0] : cage->scale[1]));

	/* corner manipulators */
	glColor3f(0.0, 0.0, 0.0);
	glLineWidth(cage->manipulator.line_width + 3.0f);

	rect_transform_draw_corners(&r, w, h);

	/* corner manipulators */
	glColor3fv(manipulator->col);
	glLineWidth(cage->manipulator.line_width);
	rect_transform_draw_corners(&r, w, h);

	rect_transform_draw_interaction(manipulator->col, manipulator->highlighted_part, half_w, half_h,
	                                w, h, cage->manipulator.line_width);

	glLineWidth(1.0);
	glPopMatrix();
}

static int manipulator_rect_transform_get_cursor(wmManipulator *manipulator)
{
	switch (manipulator->highlighted_part) {
		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_TRANSLATE:
			return BC_HANDCURSOR;
		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT:
		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT:
			return CURSOR_X_MOVE;
		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN:
		case MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_UP:
			return CURSOR_Y_MOVE;
		default:
			return CURSOR_STD;
	}
}

static int manipulator_rect_transform_intersect(bContext *UNUSED(C), const wmEvent *event, wmManipulator *manipulator)
{
	RectTransformManipulator *cage = (RectTransformManipulator *)manipulator;
	const float mouse[2] = {event->mval[0], event->mval[1]};
	//float matrot[2][2];
	float point_local[2];
	float w = cage->w;
	float h = cage->h;
	float half_w = w / 2.0f;
	float half_h = h / 2.0f;
	float aspx = 1.0f, aspy = 1.0f;

	/* rotate mouse in relation to the center and relocate it */
	sub_v2_v2v2(point_local, mouse, manipulator->origin);
	point_local[0] -= manipulator->offset[0];
	point_local[1] -= manipulator->offset[1];
	//rotate_m2(matrot, -cage->transform.rotation);

	if (cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM)
		mul_v2_fl(point_local, 1.0f / cage->scale[0]);
	else {
		point_local[0] /= cage->scale[0];
		point_local[1] /= cage->scale[0];
	}

	if (cage->w > cage->h)
		aspx = h / w;
	else
		aspy = w / h;
	w = min_ff(aspx * w / MANIPULATOR_RESIZER_WIDTH, MANIPULATOR_RESIZER_WIDTH / cage->scale[0]);
	h = min_ff(aspy * h / MANIPULATOR_RESIZER_WIDTH, MANIPULATOR_RESIZER_WIDTH / 
	           ((cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) ? cage->scale[0] : cage->scale[1]));


	rctf r;

	r.xmin = -half_w + w;
	r.ymin = -half_h + h;
	r.xmax = half_w - w;
	r.ymax = half_h - h;

	bool isect = BLI_rctf_isect_pt_v(&r, point_local);

	if (isect)
		return MANIPULATOR_RECT_TRANSFORM_INTERSECT_TRANSLATE;

	/* if manipulator does not have a scale intersection, don't do it */
	if (cage->style & (MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE | MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM)) {
		r.xmin = -half_w;
		r.ymin = -half_h;
		r.xmax = -half_w + w;
		r.ymax = half_h;

		isect = BLI_rctf_isect_pt_v(&r, point_local);

		if (isect)
			return MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT;

		r.xmin = half_w - w;
		r.ymin = -half_h;
		r.xmax = half_w;
		r.ymax = half_h;

		isect = BLI_rctf_isect_pt_v(&r, point_local);

		if (isect)
			return MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT;

		r.xmin = -half_w;
		r.ymin = -half_h;
		r.xmax = half_w;
		r.ymax = -half_h + h;

		isect = BLI_rctf_isect_pt_v(&r, point_local);

		if (isect)
			return MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN;

		r.xmin = -half_w;
		r.ymin = half_h - h;
		r.xmax = half_w;
		r.ymax = half_h;

		isect = BLI_rctf_isect_pt_v(&r, point_local);

		if (isect)
			return MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_UP;
	}

	return 0;
}

typedef struct RectTransformInteraction {
	float orig_mouse[2];
	float orig_offset[2];
	float orig_scale[2];
} RectTransformInteraction;

static bool manipulator_rect_transform_get_prop_value(wmManipulator *manipulator, const int slot, float *value)
{
	PropertyType type = RNA_property_type(manipulator->props[slot]);

	if (type != PROP_FLOAT) {
		fprintf(stderr, "Rect Transform manipulator can only be bound to float properties");
		return false;
	}
	else {
		if (slot == RECT_TRANSFORM_SLOT_OFFSET) {
			if (RNA_property_array_length(&manipulator->ptr[slot], manipulator->props[slot]) != 2) {
				fprintf(stderr, "Rect Transform manipulator offset not only be bound to array float property");
				return false;
			}
			RNA_property_float_get_array(&manipulator->ptr[slot], manipulator->props[slot], value);
		}
		else if (slot == RECT_TRANSFORM_SLOT_SCALE) {
			RectTransformManipulator *cage = (RectTransformManipulator *)manipulator;
			if (cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
				*value = RNA_property_float_get(&manipulator->ptr[slot], manipulator->props[slot]);
			}
			else {
				if (RNA_property_array_length(&manipulator->ptr[slot], manipulator->props[slot]) != 2) {
					fprintf(stderr, "Rect Transform manipulator scale not only be bound to array float property");
					return false;
				}
				RNA_property_float_get_array(&manipulator->ptr[slot], manipulator->props[slot], value);
			}
		}
	}

	return true;
}

static int manipulator_rect_transform_invoke(bContext *UNUSED(C), const wmEvent *event, wmManipulator *manipulator)
{
	RectTransformManipulator *cage = (RectTransformManipulator *)manipulator;
	RectTransformInteraction *data = MEM_callocN(sizeof(RectTransformInteraction), "cage_interaction");

	copy_v2_v2(data->orig_offset, manipulator->offset);
	copy_v2_v2(data->orig_scale, cage->scale);

	data->orig_mouse[0] = event->mval[0];
	data->orig_mouse[1] = event->mval[1];

	manipulator->interaction_data = data;

	return OPERATOR_RUNNING_MODAL;
}

static int manipulator_rect_transform_handler(
        bContext *C, const wmEvent *event, wmManipulator *manipulator,
        const int UNUSED(flag))
{
	RectTransformManipulator *cage = (RectTransformManipulator *)manipulator;
	RectTransformInteraction *data = manipulator->interaction_data;
	/* needed here as well in case clamping occurs */
	const float orig_ofx = manipulator->offset[0], orig_ofy = manipulator->offset[1];

	const float valuex = (event->mval[0] - data->orig_mouse[0]);
	const float valuey = (event->mval[1] - data->orig_mouse[1]);


	if (manipulator->highlighted_part == MANIPULATOR_RECT_TRANSFORM_INTERSECT_TRANSLATE) {
		manipulator->offset[0] = data->orig_offset[0] + valuex;
		manipulator->offset[1] = data->orig_offset[1] + valuey;
	}
	else if (manipulator->highlighted_part == MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_LEFT) {
		manipulator->offset[0] = data->orig_offset[0] + valuex / 2.0;
		cage->scale[0] = (cage->w * data->orig_scale[0] - valuex) / cage->w;
	}
	else if (manipulator->highlighted_part == MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEX_RIGHT) {
		manipulator->offset[0] = data->orig_offset[0] + valuex / 2.0;
		cage->scale[0] = (cage->w * data->orig_scale[0] + valuex) / cage->w;
	}
	else if (manipulator->highlighted_part == MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_DOWN) {
		manipulator->offset[1] = data->orig_offset[1] + valuey / 2.0;

		if (cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
			cage->scale[0] = (cage->h * data->orig_scale[0] - valuey) / cage->h;
		}
		else {
			cage->scale[1] = (cage->h * data->orig_scale[1] - valuey) / cage->h;
		}
	}
	else if (manipulator->highlighted_part == MANIPULATOR_RECT_TRANSFORM_INTERSECT_SCALEY_UP) {
		manipulator->offset[1] = data->orig_offset[1] + valuey / 2.0;

		if (cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
			cage->scale[0] = (cage->h * data->orig_scale[0] + valuey) / cage->h;
		}
		else {
			cage->scale[1] = (cage->h * data->orig_scale[1] + valuey) / cage->h;
		}
	}

	/* clamping - make sure manipulator is at least 5 pixels wide */
	if (cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
		if (cage->scale[0] < MANIPULATOR_RECT_MIN_WIDTH / cage->h || 
		    cage->scale[0] < MANIPULATOR_RECT_MIN_WIDTH / cage->w) 
		{
			cage->scale[0] = max_ff(MANIPULATOR_RECT_MIN_WIDTH / cage->h, MANIPULATOR_RECT_MIN_WIDTH / cage->w);
			manipulator->offset[0] = orig_ofx;
			manipulator->offset[1] = orig_ofy;
		}
	}
	else {
		if (cage->scale[0] < MANIPULATOR_RECT_MIN_WIDTH / cage->w) {
			cage->scale[0] = MANIPULATOR_RECT_MIN_WIDTH / cage->w;
			manipulator->offset[0] = orig_ofx;
		}
		if (cage->scale[1] < MANIPULATOR_RECT_MIN_WIDTH / cage->h) {
			cage->scale[1] = MANIPULATOR_RECT_MIN_WIDTH / cage->h;
			manipulator->offset[1] = orig_ofy;
		}
	}

	if (manipulator->props[RECT_TRANSFORM_SLOT_OFFSET]) {
		PointerRNA ptr = manipulator->ptr[RECT_TRANSFORM_SLOT_OFFSET];
		PropertyRNA *prop = manipulator->props[RECT_TRANSFORM_SLOT_OFFSET];

		RNA_property_float_set_array(&ptr, prop, manipulator->offset);
		RNA_property_update(C, &ptr, prop);
	}

	if (manipulator->props[RECT_TRANSFORM_SLOT_SCALE]) {
		PointerRNA ptr = manipulator->ptr[RECT_TRANSFORM_SLOT_SCALE];
		PropertyRNA *prop = manipulator->props[RECT_TRANSFORM_SLOT_SCALE];

		if (cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
			RNA_property_float_set(&ptr, prop, cage->scale[0]);
		}
		else {
			RNA_property_float_set_array(&ptr, prop, cage->scale);
		}
		RNA_property_update(C, &ptr, prop);
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(CTX_wm_region(C));

	return OPERATOR_PASS_THROUGH;
}

static void manipulator_rect_transform_prop_data_update(wmManipulator *manipulator, const int slot)
{
	RectTransformManipulator *cage = (RectTransformManipulator *)manipulator;

	if (slot == RECT_TRANSFORM_SLOT_OFFSET)
		manipulator_rect_transform_get_prop_value(manipulator, RECT_TRANSFORM_SLOT_OFFSET, manipulator->offset);
	if (slot == RECT_TRANSFORM_SLOT_SCALE)
		manipulator_rect_transform_get_prop_value(manipulator, RECT_TRANSFORM_SLOT_SCALE, cage->scale);
}

static void manipulator_rect_transform_exit(bContext *C, wmManipulator *manipulator, const bool cancel)
{
	RectTransformManipulator *cage = (RectTransformManipulator *)manipulator;
	RectTransformInteraction *data = manipulator->interaction_data;

	if (!cancel)
		return;

	/* reset properties */
	if (manipulator->props[RECT_TRANSFORM_SLOT_OFFSET]) {
		PointerRNA ptr = manipulator->ptr[RECT_TRANSFORM_SLOT_OFFSET];
		PropertyRNA *prop = manipulator->props[RECT_TRANSFORM_SLOT_OFFSET];

		RNA_property_float_set_array(&ptr, prop, data->orig_offset);
		RNA_property_update(C, &ptr, prop);
	}
	if (manipulator->props[RECT_TRANSFORM_SLOT_SCALE]) {
		PointerRNA ptr = manipulator->ptr[RECT_TRANSFORM_SLOT_SCALE];
		PropertyRNA *prop = manipulator->props[RECT_TRANSFORM_SLOT_SCALE];

		if (cage->style & MANIPULATOR_RECT_TRANSFORM_STYLE_SCALE_UNIFORM) {
			RNA_property_float_set(&ptr, prop, data->orig_scale[0]);
		}
		else {
			RNA_property_float_set_array(&ptr, prop, data->orig_scale);
		}
		RNA_property_update(C, &ptr, prop);
	}
}


/* -------------------------------------------------------------------- */
/** \name Cage Manipulator API
 *
 * \{ */

wmManipulator *MANIPULATOR_rect_transform_new(wmManipulatorGroup *mgroup, const char *name, const int style)
{
	RectTransformManipulator *cage = MEM_callocN(sizeof(RectTransformManipulator), name);

	cage->manipulator.draw = manipulator_rect_transform_draw;
	cage->manipulator.invoke = manipulator_rect_transform_invoke;
	cage->manipulator.prop_data_update = manipulator_rect_transform_prop_data_update;
	cage->manipulator.handler = manipulator_rect_transform_handler;
	cage->manipulator.intersect = manipulator_rect_transform_intersect;
	cage->manipulator.exit = manipulator_rect_transform_exit;
	cage->manipulator.get_cursor = manipulator_rect_transform_get_cursor;
	cage->manipulator.max_prop = 2;
	cage->manipulator.flag |= WM_MANIPULATOR_DRAW_ACTIVE;
	cage->scale[0] = cage->scale[1] = 1.0f;
	cage->style = style;

	wm_manipulator_register(mgroup, &cage->manipulator, name);

	return (wmManipulator *)cage;
}

void MANIPULATOR_rect_transform_set_dimensions(wmManipulator *manipulator, const float width, const float height)
{
	RectTransformManipulator *cage = (RectTransformManipulator *)manipulator;
	cage->w = width;
	cage->h = height;
}

/** \} */ // Cage Manipulator API


/* -------------------------------------------------------------------- */

void fix_linking_manipulator_cage(void)
{
	(void)0;
}
