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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/manipulator_library/arrow2d_manipulator.c
 *  \ingroup wm
 *
 * \name 2D Arrow Manipulator
 *
 * \brief Simple arrow manipulator which is dragged into a certain direction.
 */

#include "BIF_gl.h"

#include "BKE_context.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "DNA_manipulator_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_types.h"

/* own includes */
#include "WM_manipulator_api.h"
#include "WM_manipulator_types.h"
#include "wm_manipulator_wmapi.h"
#include "WM_manipulator_library.h"
#include "wm_manipulator_intern.h"
#include "manipulator_library_intern.h"


typedef struct ArrowManipulator2D {
	struct wmManipulator manipulator;

	float angle;
	float line_len;
} ArrowManipulator2D;


static void arrow2d_draw_geom(ArrowManipulator2D *arrow, const float origin[2], const float color[4])
{
	const float size = 0.11f;
	const float size_h = size / 2.0f;
	const float len = arrow->line_len;
	const float draw_line_ofs = (arrow->manipulator.line_width * 0.5f) / arrow->manipulator.scale;

	unsigned int pos = VertexFormat_add_attrib(immVertexFormat(), "pos", COMP_F32, 2, KEEP_FLOAT);

	gpuPushMatrix();
	gpuTranslate2fv(origin);
	gpuScaleUniform(arrow->manipulator.scale);
	gpuRotate2D(RAD2DEGF(arrow->angle));
	/* local offset */
	gpuTranslate2f(arrow->manipulator.offset[0] + draw_line_ofs, arrow->manipulator.offset[1]);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	immUniformColor4fv(color);

	immBegin(PRIM_LINES, 2);
	immVertex2f(pos, 0.0f, 0.0f);
	immVertex2f(pos, 0.0f, len);
	immEnd();

	immBegin(PRIM_TRIANGLES, 3);
	immVertex2f(pos, size_h, len);
	immVertex2f(pos, -size_h, len);
	immVertex2f(pos, 0.0f, len + size * 1.7f);
	immEnd();

	immUnbindProgram();

	gpuPopMatrix();
}

static void manipulator_arrow2d_draw(const bContext *UNUSED(C), struct wmManipulator *manipulator)
{
	ArrowManipulator2D *arrow = (ArrowManipulator2D *)manipulator;
	float col[4];

	manipulator_color_get(manipulator, manipulator->state & WM_MANIPULATOR_HIGHLIGHT, col);

	glLineWidth(manipulator->line_width);
	glEnable(GL_BLEND);
	arrow2d_draw_geom(arrow, manipulator->origin, col);
	glDisable(GL_BLEND);

	if (arrow->manipulator.interaction_data) {
		ManipulatorInteraction *inter = arrow->manipulator.interaction_data;

		glEnable(GL_BLEND);
		arrow2d_draw_geom(arrow, inter->init_origin, (const float[4]){0.5f, 0.5f, 0.5f, 0.5f});
		glDisable(GL_BLEND);
	}
}

static void manipulator_arrow2d_invoke(
        bContext *UNUSED(C), struct wmManipulator *manipulator, const wmEvent *UNUSED(event))
{
	ManipulatorInteraction *inter = MEM_callocN(sizeof(ManipulatorInteraction), __func__);

	copy_v2_v2(inter->init_origin, manipulator->origin);
	manipulator->interaction_data = inter;
}

static int manipulator_arrow2d_intersect(
        bContext *UNUSED(C), struct wmManipulator *manipulator, const wmEvent *event)
{
	ArrowManipulator2D *arrow = (ArrowManipulator2D *)manipulator;
	const float mval[2] = {event->mval[0], event->mval[1]};
	const float line_len = arrow->line_len * manipulator->scale;
	float mval_local[2];

	copy_v2_v2(mval_local, mval);
	sub_v2_v2(mval_local, manipulator->origin);

	float line[2][2];
	line[0][0] = line[0][1] = line[1][0] = 0.0f;
	line[1][1] = line_len;

	/* rotate only if needed */
	if (arrow->angle != 0.0f) {
		float rot_point[2];
		copy_v2_v2(rot_point, line[1]);
		rotate_v2_v2fl(line[1], rot_point, arrow->angle);
	}

	/* arrow line intersection check */
	float isect_1[2], isect_2[2];
	const int isect = isect_line_sphere_v2(
	        line[0], line[1], mval_local, MANIPULATOR_HOTSPOT + manipulator->line_width * 0.5f,
	        isect_1, isect_2);

	if (isect > 0) {
		float line_ext[2][2]; /* extended line for segment check including hotspot */
		copy_v2_v2(line_ext[0], line[0]);
		line_ext[1][0] = line[1][0] + MANIPULATOR_HOTSPOT * ((line[1][0] - line[0][0]) / line_len);
		line_ext[1][1] = line[1][1] + MANIPULATOR_HOTSPOT * ((line[1][1] - line[0][1]) / line_len);

		const float lambda_1 = line_point_factor_v2(isect_1, line_ext[0], line_ext[1]);
		if (isect == 1) {
			return IN_RANGE_INCL(lambda_1, 0.0f, 1.0f);
		}
		else {
			BLI_assert(isect == 2);
			const float lambda_2 = line_point_factor_v2(isect_2, line_ext[0], line_ext[1]);
			return IN_RANGE_INCL(lambda_1, 0.0f, 1.0f) && IN_RANGE_INCL(lambda_2, 0.0f, 1.0f);
		}
	}

	return 0;
}

/* -------------------------------------------------------------------- */
/** \name 2D Arrow Manipulator API
 *
 * \{ */

struct wmManipulator *MANIPULATOR_arrow2d_new(wmManipulatorGroup *mgroup, const char *name)
{
	const wmManipulatorType *mpt = WM_manipulatortype_find("MANIPULATOR_WT_arrow_2d", false);
	ArrowManipulator2D *arrow = (ArrowManipulator2D *)WM_manipulator_new(mpt, mgroup, name);

	arrow->manipulator.flag |= WM_MANIPULATOR_DRAW_ACTIVE;

	arrow->line_len = 1.0f;

	return &arrow->manipulator;
}

void MANIPULATOR_arrow2d_set_angle(struct wmManipulator *manipulator, const float angle)
{
	ArrowManipulator2D *arrow = (ArrowManipulator2D *)manipulator;
	arrow->angle = angle;
}

void MANIPULATOR_arrow2d_set_line_len(struct wmManipulator *manipulator, const float len)
{
	ArrowManipulator2D *arrow = (ArrowManipulator2D *)manipulator;
	arrow->line_len = len;
}

static void MANIPULATOR_WT_arrow_2d(wmManipulatorType *wt)
{
	/* identifiers */
	wt->idname = "MANIPULATOR_WT_arrow_2d";

	/* api callbacks */
	wt->draw = manipulator_arrow2d_draw;
	wt->invoke = manipulator_arrow2d_invoke;
	wt->intersect = manipulator_arrow2d_intersect;

	wt->size = sizeof(ArrowManipulator2D);
}

void ED_manipulatortypes_arrow_2d(void)
{
	WM_manipulatortype_append(MANIPULATOR_WT_arrow_2d);
}

/** \} */ /* Arrow Manipulator API */


/* -------------------------------------------------------------------- */

void fix_linking_manipulator_arrow2d(void)
{
	(void)0;
}
