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

/** \file blender/windowmanager/manipulators/intern/manipulator_library/arrow_manipulator.c
 *  \ingroup wm
 *
 * \name Arrow Manipulator
 *
 * 3D Manipulator
 *
 * \brief Simple arrow manipulator which is dragged into a certain direction.
 * The arrow head can have varying shapes, e.g. cone, box, etc.
 */

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_math.h"

#include "DNA_manipulator_types.h"
#include "DNA_view3d_types.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_types.h"
#include "WM_api.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"
#include "manipulator_geometry.h"
#include "manipulator_library_intern.h"

/* to use custom arrows exported to geom_arrow_manipulator.c */
//#define USE_MANIPULATOR_CUSTOM_ARROWS

/* ArrowManipulator->flag */
enum {
	ARROW_UP_VECTOR_SET    = (1 << 0),
	ARROW_CUSTOM_RANGE_SET = (1 << 1),
};

typedef struct ArrowManipulator {
	wmManipulator manipulator;

	ManipulatorCommonData data;

	int style;
	int flag;

	float len;          /* arrow line length */
	float direction[3];
	float up[3];
	float aspect[2];    /* cone style only */
} ArrowManipulator;


/* -------------------------------------------------------------------- */

static void manipulator_arrow_get_final_pos(wmManipulator *manipulator, float r_pos[3])
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;

	mul_v3_v3fl(r_pos, arrow->direction, arrow->data.offset);
	add_v3_v3(r_pos, arrow->manipulator.origin);
}

static void arrow_draw_geom(const ArrowManipulator *arrow, const bool select, const float color[4])
{
	/* USE_IMM for other arrow types */
	glColor4fv(color);

	if (arrow->style & MANIPULATOR_ARROW_STYLE_CROSS) {
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_LIGHTING);
		glBegin(GL_LINES);
		glVertex2f(-1.0, 0.f);
		glVertex2f(1.0, 0.f);
		glVertex2f(0.f, -1.0);
		glVertex2f(0.f, 1.0);
		glEnd();

		glPopAttrib();
	}
	else if (arrow->style & MANIPULATOR_ARROW_STYLE_CONE) {
		const float unitx = arrow->aspect[0];
		const float unity = arrow->aspect[1];
		const float vec[4][3] = {
			{-unitx, -unity, 0},
			{ unitx, -unity, 0},
			{ unitx,  unity, 0},
			{-unitx,  unity, 0},
		};

		glLineWidth(arrow->manipulator.line_width);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, vec);
		glDrawArrays(GL_LINE_LOOP, 0, ARRAY_SIZE(vec));
		glDisableClientState(GL_VERTEX_ARRAY);
		glLineWidth(1.0);
	}
	else {
#ifdef USE_MANIPULATOR_CUSTOM_ARROWS
		wm_manipulator_geometryinfo_draw(&wm_manipulator_geom_data_arrow, select);
#else
		const float vec[2][3] = {
			{0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, arrow->len},
		};

		glLineWidth(arrow->manipulator.line_width);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, vec);
		glDrawArrays(GL_LINE_STRIP, 0, ARRAY_SIZE(vec));
		glDisableClientState(GL_VERTEX_ARRAY);
		glLineWidth(1.0);


		/* *** draw arrow head *** */

		gpuPushMatrix();

		if (arrow->style & MANIPULATOR_ARROW_STYLE_BOX) {
			const float size = 0.05f;

			/* translate to line end with some extra offset so box starts exactly where line ends */
			gpuTranslate3f(0.0f, 0.0f, arrow->len + size);
			/* scale down to box size */
			gpuScale3f(size, size, size);

			/* draw cube */
			wm_manipulator_geometryinfo_draw(&wm_manipulator_geom_data_cube, select);
		}
		else {
			const float len = 0.25f;
			const float width = 0.06f;
			const bool use_lighting = select == false && ((U.manipulator_flag & V3D_SHADED_MANIPULATORS) != 0);

			/* translate to line end */
			unsigned int pos = VertexFormat_add_attrib(immVertexFormat(), "pos", GL_FLOAT, 3, KEEP_FLOAT);
			gpuTranslate3f(0.0f, 0.0f, arrow->len);

			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
			immUniformColor4fv(color);

			if (use_lighting) {
				glShadeModel(GL_SMOOTH);
			}

			imm_draw_circle_fill_3d(pos, 0.0, 0.0, width, 8);
			imm_draw_cylinder_fill_3d(pos, width, 0.0, len, 8, 1);

			immUnbindProgram();

			if (use_lighting) {
				glShadeModel(GL_FLAT);
			}
		}

		gpuPopMatrix();

#endif  /* USE_MANIPULATOR_CUSTOM_ARROWS */
	}
}

static void arrow_draw_intern(ArrowManipulator *arrow, const bool select, const bool highlight)
{
	const float up[3] = {0.0f, 0.0f, 1.0f};
	float col[4];
	float rot[3][3];
	float mat[4][4];
	float final_pos[3];

	manipulator_color_get(&arrow->manipulator, highlight, col);
	manipulator_arrow_get_final_pos(&arrow->manipulator, final_pos);

	if (arrow->flag & ARROW_UP_VECTOR_SET) {
		copy_v3_v3(rot[2], arrow->direction);
		copy_v3_v3(rot[1], arrow->up);
		cross_v3_v3v3(rot[0], arrow->up, arrow->direction);
	}
	else {
		rotation_between_vecs_to_mat3(rot, up, arrow->direction);
	}
	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], final_pos);
	mul_mat3_m4_fl(mat, arrow->manipulator.scale);

	gpuPushMatrix();
	gpuMultMatrix3D(mat);

	gpuTranslate3fv(arrow->manipulator.offset);

	glEnable(GL_BLEND);
	arrow_draw_geom(arrow, select, col);
	glDisable(GL_BLEND);

	gpuPopMatrix();

	if (arrow->manipulator.interaction_data) {
		ManipulatorInteraction *inter = arrow->manipulator.interaction_data;

		copy_m4_m3(mat, rot);
		copy_v3_v3(mat[3], inter->init_origin);
		mul_mat3_m4_fl(mat, inter->init_scale);

		gpuPushMatrix();
		gpuMultMatrix3D(mat);
		gpuTranslate3fv(arrow->manipulator.offset);

		glEnable(GL_BLEND);
		arrow_draw_geom(arrow, select, (const float [4]){0.5f, 0.5f, 0.5f, 0.5f});
		glDisable(GL_BLEND);

		gpuPopMatrix();
	}
}

static void manipulator_arrow_render_3d_intersect(
        const bContext *UNUSED(C), wmManipulator *manipulator,
        int selectionbase)
{
	GPU_select_load_id(selectionbase);
	arrow_draw_intern((ArrowManipulator *)manipulator, true, false);
}

static void manipulator_arrow_draw(const bContext *UNUSED(C), wmManipulator *manipulator)
{
	arrow_draw_intern((ArrowManipulator *)manipulator, false, (manipulator->state & WM_MANIPULATOR_HIGHLIGHT) != 0);
}

/**
 * Calculate arrow offset independent from prop min value,
 * meaning the range will not be offset by min value first.
 */
#define USE_ABS_HANDLE_RANGE

static int manipulator_arrow_handler(bContext *C, const wmEvent *event, wmManipulator *manipulator, const int flag)
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;
	ManipulatorInteraction *inter = manipulator->interaction_data;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	float orig_origin[4];
	float viewvec[3], tangent[3], plane[3];
	float offset[4];
	float m_diff[2];
	float dir_2d[2], dir2d_final[2];
	float facdir = 1.0f;
	bool use_vertical = false;


	copy_v3_v3(orig_origin, inter->init_origin);
	orig_origin[3] = 1.0f;
	add_v3_v3v3(offset, orig_origin, arrow->direction);
	offset[3] = 1.0f;

	/* calculate view vector */
	if (rv3d->is_persp) {
		sub_v3_v3v3(viewvec, orig_origin, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(viewvec, rv3d->viewinv[2]);
	}
	normalize_v3(viewvec);

	/* first determine if view vector is really close to the direction. If it is, we use
	 * vertical movement to determine offset, just like transform system does */
	if (RAD2DEG(acos(dot_v3v3(viewvec, arrow->direction))) > 5.0f) {
		/* multiply to projection space */
		mul_m4_v4(rv3d->persmat, orig_origin);
		mul_v4_fl(orig_origin, 1.0f / orig_origin[3]);
		mul_m4_v4(rv3d->persmat, offset);
		mul_v4_fl(offset, 1.0f / offset[3]);

		sub_v2_v2v2(dir_2d, offset, orig_origin);
		dir_2d[0] *= ar->winx;
		dir_2d[1] *= ar->winy;
		normalize_v2(dir_2d);
	}
	else {
		dir_2d[0] = 0.0f;
		dir_2d[1] = 1.0f;
		use_vertical = true;
	}

	/* find mouse difference */
	m_diff[0] = event->mval[0] - inter->init_mval[0];
	m_diff[1] = event->mval[1] - inter->init_mval[1];

	/* project the displacement on the screen space arrow direction */
	project_v2_v2v2(dir2d_final, m_diff, dir_2d);

	float zfac = ED_view3d_calc_zfac(rv3d, orig_origin, NULL);
	ED_view3d_win_to_delta(ar, dir2d_final, offset, zfac);

	add_v3_v3v3(orig_origin, offset, inter->init_origin);

	/* calculate view vector for the new position */
	if (rv3d->is_persp) {
		sub_v3_v3v3(viewvec, orig_origin, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(viewvec, rv3d->viewinv[2]);
	}

	normalize_v3(viewvec);
	if (!use_vertical) {
		float fac;
		/* now find a plane parallel to the view vector so we can intersect with the arrow direction */
		cross_v3_v3v3(tangent, viewvec, offset);
		cross_v3_v3v3(plane, tangent, viewvec);
		fac = dot_v3v3(plane, offset) / dot_v3v3(arrow->direction, plane);

		facdir = (fac < 0.0) ? -1.0 : 1.0;
		mul_v3_v3fl(offset, arrow->direction, fac);
	}
	else {
		facdir = (m_diff[1] < 0.0) ? -1.0 : 1.0;
	}


	ManipulatorCommonData *data = &arrow->data;
	const float ofs_new = facdir * len_v3(offset);
	const int slot = ARROW_SLOT_OFFSET_WORLD_SPACE;

	/* set the property for the operator and call its modal function */
	if (manipulator->props[slot]) {
		const bool constrained = arrow->style & MANIPULATOR_ARROW_STYLE_CONSTRAINED;
		const bool inverted = arrow->style & MANIPULATOR_ARROW_STYLE_INVERTED;
		const bool use_precision = flag & WM_MANIPULATOR_TWEAK_PRECISE;
		float value = manipulator_value_from_offset(data, inter, ofs_new, constrained, inverted, use_precision);

		manipulator_property_value_set(C, manipulator, slot, value);
		/* get clamped value */
		value = manipulator_property_value_get(manipulator, slot);

		data->offset = manipulator_offset_from_value(data, value, constrained, inverted);
	}
	else {
		data->offset = ofs_new;
	}

	/* tag the region for redraw */
	ED_region_tag_redraw(ar);
	WM_event_add_mousemove(C);

	return OPERATOR_PASS_THROUGH;
}


static int manipulator_arrow_invoke(bContext *UNUSED(C), const wmEvent *event, wmManipulator *manipulator)
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;
	ManipulatorInteraction *inter = MEM_callocN(sizeof(ManipulatorInteraction), __func__);
	PointerRNA ptr = manipulator->ptr[ARROW_SLOT_OFFSET_WORLD_SPACE];
	PropertyRNA *prop = manipulator->props[ARROW_SLOT_OFFSET_WORLD_SPACE];

	if (prop) {
		inter->init_value = RNA_property_float_get(&ptr, prop);
	}

	inter->init_offset = arrow->data.offset;

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

	inter->init_scale = manipulator->scale;

	manipulator_arrow_get_final_pos(manipulator, inter->init_origin);

	manipulator->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

static void manipulator_arrow_prop_data_update(wmManipulator *manipulator, const int slot)
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;
	manipulator_property_data_update(
	            manipulator, &arrow->data, slot,
	            arrow->style & MANIPULATOR_ARROW_STYLE_CONSTRAINED,
	            arrow->style & MANIPULATOR_ARROW_STYLE_INVERTED);
}

static void manipulator_arrow_exit(bContext *C, wmManipulator *manipulator, const bool cancel)
{
	if (!cancel)
		return;

	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;
	ManipulatorCommonData *data = &arrow->data;
	ManipulatorInteraction *inter = manipulator->interaction_data;

	manipulator_property_value_reset(C, manipulator, inter, ARROW_SLOT_OFFSET_WORLD_SPACE);
	data->offset = inter->init_offset;
}


/* -------------------------------------------------------------------- */
/** \name Arrow Manipulator API
 *
 * \{ */

wmManipulator *MANIPULATOR_arrow_new(wmManipulatorGroup *mgroup, const char *name, const int style)
{
	int real_style = style;

	/* inverted only makes sense in a constrained arrow */
	if (real_style & MANIPULATOR_ARROW_STYLE_INVERTED) {
		real_style |= MANIPULATOR_ARROW_STYLE_CONSTRAINED;
	}


	ArrowManipulator *arrow = MEM_callocN(sizeof(ArrowManipulator), name);
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	arrow->manipulator.draw = manipulator_arrow_draw;
	arrow->manipulator.get_final_position = manipulator_arrow_get_final_pos;
	arrow->manipulator.intersect = NULL;
	arrow->manipulator.handler = manipulator_arrow_handler;
	arrow->manipulator.invoke = manipulator_arrow_invoke;
	arrow->manipulator.render_3d_intersection = manipulator_arrow_render_3d_intersect;
	arrow->manipulator.prop_data_update = manipulator_arrow_prop_data_update;
	arrow->manipulator.exit = manipulator_arrow_exit;
	arrow->manipulator.flag |= WM_MANIPULATOR_DRAW_ACTIVE;

	arrow->style = real_style;
	arrow->len = 1.0f;
	arrow->data.range_fac = 1.0f;
	copy_v3_v3(arrow->direction, dir_default);

	wm_manipulator_register(mgroup, &arrow->manipulator, name);

	return (wmManipulator *)arrow;
}

/**
 * Define direction the arrow will point towards
 */
void MANIPULATOR_arrow_set_direction(wmManipulator *manipulator, const float direction[3])
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;

	copy_v3_v3(arrow->direction, direction);
	normalize_v3(arrow->direction);
}

/**
 * Define up-direction of the arrow manipulator
 */
void MANIPULATOR_arrow_set_up_vector(wmManipulator *manipulator, const float direction[3])
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;

	if (direction) {
		copy_v3_v3(arrow->up, direction);
		normalize_v3(arrow->up);
		arrow->flag |= ARROW_UP_VECTOR_SET;
	}
	else {
		arrow->flag &= ~ARROW_UP_VECTOR_SET;
	}
}

/**
 * Define a custom arrow line length
 */
void MANIPULATOR_arrow_set_line_len(wmManipulator *manipulator, const float len)
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;
	arrow->len = len;
}

/**
 * Define a custom property UI range
 *
 * \note Needs to be called before WM_manipulator_set_property!
 */
void MANIPULATOR_arrow_set_ui_range(wmManipulator *manipulator, const float min, const float max)
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;

	BLI_assert(min < max);
	BLI_assert(!(arrow->manipulator.props[0] && "Make sure this function "
	           "is called before WM_manipulator_set_property"));

	arrow->data.range = max - min;
	arrow->data.min = min;
	arrow->data.flag |= MANIPULATOR_CUSTOM_RANGE_SET;
}

/**
 * Define a custom factor for arrow min/max distance
 *
 * \note Needs to be called before WM_manipulator_set_property!
 */
void MANIPULATOR_arrow_set_range_fac(wmManipulator *manipulator, const float range_fac)
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;

	BLI_assert(!(arrow->manipulator.props[0] && "Make sure this function "
	           "is called before WM_manipulator_set_property"));

	arrow->data.range_fac = range_fac;
}

/**
 * Define xy-aspect for arrow cone
 */
void MANIPULATOR_arrow_cone_set_aspect(wmManipulator *manipulator, const float aspect[2])
{
	ArrowManipulator *arrow = (ArrowManipulator *)manipulator;

	copy_v2_v2(arrow->aspect, aspect);
}

/** \} */ /* Arrow Manipulator API */


/* -------------------------------------------------------------------- */

void fix_linking_manipulator_arrow(void)
{
	(void)0;
}
