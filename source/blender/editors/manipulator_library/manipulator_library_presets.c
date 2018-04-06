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

/** \file blender/editors/manipulator_library/manipulator_library_presets.c
 *  \ingroup wm
 *
 * \name Manipulator Lib Presets
 *
 * \brief Preset shapes that can be drawn from any manipulator type.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"

#include "BIF_gl.h"

#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_select.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"

#include "WM_types.h"
#include "WM_api.h"

#include "ED_view3d.h"
#include "ED_screen.h"

/* own includes */
#include "ED_manipulator_library.h"  /* own include */
#include "manipulator_library_intern.h"  /* own include */

/* TODO, this is to be used by RNA. might move to ED_manipulator_library */

/**
 * Given a single axis, orient the matrix to a different direction.
 */
static void single_axis_convert(
        int src_axis, float src_mat[4][4],
        int dst_axis, float dst_mat[4][4])
{
	copy_m4_m4(dst_mat, src_mat);
	if (src_axis == dst_axis) {
		return;
	}

	float rotmat[3][3];
	mat3_from_axis_conversion_single(src_axis, dst_axis, rotmat);
	transpose_m3(rotmat);
	mul_m4_m4m3(dst_mat, src_mat, rotmat);
}

/**
 * Use for all geometry.
 */
static void ed_manipulator_draw_preset_geometry(
        const struct wmManipulator *mpr, float mat[4][4], int select_id,
        const ManipulatorGeomInfo *info)
{
	const bool is_select = (select_id != -1);
	const bool is_highlight = is_select && (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;

	float color[4];
	manipulator_color_get(mpr, is_highlight, color);

	if (is_select) {
		GPU_select_load_id(select_id);
	}

	gpuPushMatrix();
	gpuMultMatrix(mat);
	wm_manipulator_geometryinfo_draw(info, is_select, color);
	gpuPopMatrix();

	if (is_select) {
		GPU_select_load_id(-1);
	}
}

void ED_manipulator_draw_preset_box(
        const struct wmManipulator *mpr, float mat[4][4], int select_id)
{
	ed_manipulator_draw_preset_geometry(mpr, mat, select_id, &wm_manipulator_geom_data_cube);
}

void ED_manipulator_draw_preset_arrow(
        const struct wmManipulator *mpr, float mat[4][4], int axis, int select_id)
{
	float mat_rotate[4][4];
	single_axis_convert(OB_POSZ, mat, axis, mat_rotate);
	ed_manipulator_draw_preset_geometry(mpr, mat_rotate, select_id, &wm_manipulator_geom_data_arrow);
}

void ED_manipulator_draw_preset_circle(
        const struct wmManipulator *mpr, float mat[4][4], int axis, int select_id)
{
	float mat_rotate[4][4];
	single_axis_convert(OB_POSZ, mat, axis, mat_rotate);
	ed_manipulator_draw_preset_geometry(mpr, mat_rotate, select_id, &wm_manipulator_geom_data_dial);
}

void ED_manipulator_draw_preset_facemap(
        const bContext *C, const struct wmManipulator *mpr, struct Scene *scene, Object *ob,  const int facemap, int select_id)
{
	const bool is_select = (select_id != -1);
	const bool is_highlight = is_select && (mpr->state & WM_MANIPULATOR_STATE_HIGHLIGHT) != 0;

	float color[4];
	manipulator_color_get(mpr, is_highlight, color);

	if (is_select) {
		GPU_select_load_id(select_id);
	}

	gpuPushMatrix();
	gpuMultMatrix(ob->obmat);
	ED_draw_object_facemap(CTX_data_depsgraph(C), scene, ob, color, facemap);
	gpuPopMatrix();

	if (is_select) {
		GPU_select_load_id(-1);
	}
}

