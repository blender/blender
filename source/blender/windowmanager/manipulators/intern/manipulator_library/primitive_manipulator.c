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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/manipulator_library/primitive_manipulator.c
 *  \ingroup wm
 *
 * \name Primitive Manipulator
 *
 * 3D Manipulator
 *
 * \brief Manipulator with primitive drawing type (plane, cube, etc.).
 * Currently only plane primitive supported without own handling, use with operator only.
 */

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_math.h"

#include "DNA_view3d_types.h"
#include "DNA_manipulator_types.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

/* own includes */
#include "WM_manipulator_types.h"
#include "WM_manipulator_library.h"
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"
#include "manipulator_library_intern.h"


/* PrimitiveManipulator->flag */
enum {
	PRIM_UP_VECTOR_SET = (1 << 0),
};

typedef struct PrimitiveManipulator {
	wmManipulator manipulator;

	float direction[3];
	float up[3];
	int style;
	int flag;
} PrimitiveManipulator;


static float verts_plane[4][3] = {
	{-1, -1, 0},
	{ 1, -1, 0},
	{ 1,  1, 0},
	{-1,  1, 0},
};


/* -------------------------------------------------------------------- */

static void manipulator_primitive_draw_geom(
        const float col_inner[4], const float col_outer[4], const int style)
{
	float (*verts)[3];
	float vert_count;
	unsigned int pos = VertexFormat_add_attrib(immVertexFormat(), "pos", COMP_F32, 3, KEEP_FLOAT);

	if (style == MANIPULATOR_PRIMITIVE_STYLE_PLANE) {
		verts = verts_plane;
		vert_count = ARRAY_SIZE(verts_plane);
	}

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	wm_manipulator_vec_draw(col_inner, verts, vert_count, pos, PRIM_TRIANGLE_FAN);
	wm_manipulator_vec_draw(col_outer, verts, vert_count, pos, PRIM_LINE_LOOP);
	immUnbindProgram();
}

static void manipulator_primitive_draw_intern(
        PrimitiveManipulator *prim, const bool UNUSED(select),
        const bool highlight)
{
	float col_inner[4], col_outer[4];
	float rot[3][3];
	float mat[4][4];

	if (prim->flag & PRIM_UP_VECTOR_SET) {
		copy_v3_v3(rot[2], prim->direction);
		copy_v3_v3(rot[1], prim->up);
		cross_v3_v3v3(rot[0], prim->up, prim->direction);
	}
	else {
		const float up[3] = {0.0f, 0.0f, 1.0f};
		rotation_between_vecs_to_mat3(rot, up, prim->direction);
	}

	copy_m4_m3(mat, rot);
	copy_v3_v3(mat[3], prim->manipulator.origin);
	mul_mat3_m4_fl(mat, prim->manipulator.scale);

	gpuPushMatrix();
	gpuMultMatrix3D(mat);

	manipulator_color_get(&prim->manipulator, highlight, col_outer);
	copy_v4_v4(col_inner, col_outer);
	col_inner[3] *= 0.5f;

	glEnable(GL_BLEND);
	gpuTranslate3fv(prim->manipulator.offset);
	manipulator_primitive_draw_geom(col_inner, col_outer, prim->style);
	glDisable(GL_BLEND);

	gpuPopMatrix();

	if (prim->manipulator.interaction_data) {
		ManipulatorInteraction *inter = prim->manipulator.interaction_data;

		copy_v4_fl(col_inner, 0.5f);
		copy_v3_fl(col_outer, 0.5f);
		col_outer[3] = 0.8f;

		copy_m4_m3(mat, rot);
		copy_v3_v3(mat[3], inter->init_origin);
		mul_mat3_m4_fl(mat, inter->init_scale);

		gpuPushMatrix();
		gpuMultMatrix3D(mat);

		glEnable(GL_BLEND);
		gpuTranslate3fv(prim->manipulator.offset);
		manipulator_primitive_draw_geom(col_inner, col_outer, prim->style);
		glDisable(GL_BLEND);

		gpuPopMatrix();
	}
}

static void manipulator_primitive_render_3d_intersect(
        const bContext *UNUSED(C), wmManipulator *manipulator,
        int selectionbase)
{
	GPU_select_load_id(selectionbase);
	manipulator_primitive_draw_intern((PrimitiveManipulator *)manipulator, true, false);
}

static void manipulator_primitive_draw(const bContext *UNUSED(C), wmManipulator *manipulator)
{
	manipulator_primitive_draw_intern(
	            (PrimitiveManipulator *)manipulator, false,
	            (manipulator->state & WM_MANIPULATOR_HIGHLIGHT));
}

static int manipulator_primitive_invoke(
        bContext *UNUSED(C), const wmEvent *UNUSED(event), wmManipulator *manipulator)
{
	ManipulatorInteraction *inter = MEM_callocN(sizeof(ManipulatorInteraction), __func__);

	copy_v3_v3(inter->init_origin, manipulator->origin);
	inter->init_scale = manipulator->scale;

	manipulator->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}


/* -------------------------------------------------------------------- */
/** \name Primitive Manipulator API
 *
 * \{ */

wmManipulator *MANIPULATOR_primitive_new(wmManipulatorGroup *mgroup, const char *name, const int style)
{
	PrimitiveManipulator *prim = MEM_callocN(sizeof(PrimitiveManipulator), name);
	const float dir_default[3] = {0.0f, 0.0f, 1.0f};

	prim->manipulator.draw = manipulator_primitive_draw;
	prim->manipulator.invoke = manipulator_primitive_invoke;
	prim->manipulator.intersect = NULL;
	prim->manipulator.render_3d_intersection = manipulator_primitive_render_3d_intersect;
	prim->manipulator.flag |= WM_MANIPULATOR_DRAW_ACTIVE;
	prim->style = style;

	/* defaults */
	copy_v3_v3(prim->direction, dir_default);

	wm_manipulator_register(mgroup, &prim->manipulator, name);

	return (wmManipulator *)prim;
}

/**
 * Define direction the primitive will point towards
 */
void MANIPULATOR_primitive_set_direction(wmManipulator *manipulator, const float direction[3])
{
	PrimitiveManipulator *prim = (PrimitiveManipulator *)manipulator;

	normalize_v3_v3(prim->direction, direction);
}

/**
 * Define up-direction of the primitive manipulator
 */
void MANIPULATOR_primitive_set_up_vector(wmManipulator *manipulator, const float direction[3])
{
	PrimitiveManipulator *prim = (PrimitiveManipulator *)manipulator;

	if (direction) {
		normalize_v3_v3(prim->up, direction);
		prim->flag |= PRIM_UP_VECTOR_SET;
	}
	else {
		prim->flag &= ~PRIM_UP_VECTOR_SET;
	}
}

/** \} */ // Primitive Manipulator API


/* -------------------------------------------------------------------- */

void fix_linking_manipulator_primitive(void)
{
	(void)0;
}
