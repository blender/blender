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

/** \file blender/editors/sculpt_paint/paint_vertex_color_utils.c
 *  \ingroup edsculpt
 *
 * Intended for use by `paint_vertex.c` & `paint_vertex_color_ops.c`.
 */

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_base.h"
#include "BLI_math_color.h"

#include "IMB_colormanagement.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_mesh.h"

#include "ED_mesh.h"

#include "paint_intern.h"  /* own include */

#define EPS_SATURATION 0.0005f

/**
 * Apply callback to each vertex of the active vertex color layer.
 */
bool ED_vpaint_color_transform(
        struct Object *ob,
        VPaintTransform_Callback vpaint_tx_fn,
        const void *user_data)
{
	Mesh *me;
	const MPoly *mp;

	if (((me = BKE_mesh_from_object(ob)) == NULL) ||
	    (ED_mesh_color_ensure(me, NULL) == false))
	{
		return false;
	}

	const bool use_face_sel = (me->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
	mp = me->mpoly;

	for (int i = 0; i < me->totpoly; i++, mp++) {
		MLoopCol *lcol = &me->mloopcol[mp->loopstart];

		if (use_face_sel && !(mp->flag & ME_FACE_SEL)) {
			continue;
		}

		for (int j = 0; j < mp->totloop; j++, lcol++) {
			float col[3];
			rgb_uchar_to_float(col, &lcol->r);

			vpaint_tx_fn(col, user_data, col);

			rgb_float_to_uchar(&lcol->r, col);
		}
	}

	/* remove stale me->mcol, will be added later */
	BKE_mesh_tessface_clear(me);

	DAG_id_tag_update(&me->id, 0);

	return true;
}

/* -------------------------------------------------------------------- */
/** \name Color Blending Modes
 * \{ */

BLI_INLINE uint mcol_blend(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	/* Updated to use the rgb squared color model which blends nicer. */
	int r1 = cp1[0] * cp1[0];
	int g1 = cp1[1] * cp1[1];
	int b1 = cp1[2] * cp1[2];
	int a1 = cp1[3] * cp1[3];

	int r2 = cp2[0] * cp2[0];
	int g2 = cp2[1] * cp2[1];
	int b2 = cp2[2] * cp2[2];
	int a2 = cp2[3] * cp2[3];

	cp[0] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * r1 + fac * r2), 255)));
	cp[1] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * g1 + fac * g2), 255)));
	cp[2] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * b1 + fac * b2), 255)));
	cp[3] = round_fl_to_uchar(sqrtf(divide_round_i((mfac * a1 + fac * a2), 255)));

	return col;
}

BLI_INLINE uint mcol_add(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	temp = cp1[0] + divide_round_i((fac * cp2[0]), 255);
	cp[0] = (temp > 254) ? 255 : temp;
	temp = cp1[1] + divide_round_i((fac * cp2[1]), 255);
	cp[1] = (temp > 254) ? 255 : temp;
	temp = cp1[2] + divide_round_i((fac * cp2[2]), 255);
	cp[2] = (temp > 254) ? 255 : temp;
	temp = cp1[3] + divide_round_i((fac * cp2[3]), 255);
	cp[3] = (temp > 254) ? 255 : temp;

	return col;
}

BLI_INLINE uint mcol_sub(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	temp = cp1[0] - divide_round_i((fac * cp2[0]), 255);
	cp[0] = (temp < 0) ? 0 : temp;
	temp = cp1[1] - divide_round_i((fac * cp2[1]), 255);
	cp[1] = (temp < 0) ? 0 : temp;
	temp = cp1[2] - divide_round_i((fac * cp2[2]), 255);
	cp[2] = (temp < 0) ? 0 : temp;
	temp = cp1[3] - divide_round_i((fac * cp2[3]), 255);
	cp[3] = (temp < 0) ? 0 : temp;

	return col;
}

BLI_INLINE uint mcol_mul(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	/* first mul, then blend the fac */
	cp[0] = divide_round_i(mfac * cp1[0] * 255 + fac * cp2[0] * cp1[0], 255 * 255);
	cp[1] = divide_round_i(mfac * cp1[1] * 255 + fac * cp2[1] * cp1[1], 255 * 255);
	cp[2] = divide_round_i(mfac * cp1[2] * 255 + fac * cp2[2] * cp1[2], 255 * 255);
	cp[3] = divide_round_i(mfac * cp1[3] * 255 + fac * cp2[3] * cp1[3], 255 * 255);

	return col;
}

BLI_INLINE uint mcol_lighten(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}
	else if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	/* See if are lighter, if so mix, else don't do anything.
	 * if the paint col is darker then the original, then ignore */
	if (IMB_colormanagement_get_luminance_byte(cp1) > IMB_colormanagement_get_luminance_byte(cp2)) {
		return col1;
	}

	cp[0] = divide_round_i(mfac * cp1[0] + fac * cp2[0], 255);
	cp[1] = divide_round_i(mfac * cp1[1] + fac * cp2[1], 255);
	cp[2] = divide_round_i(mfac * cp1[2] + fac * cp2[2], 255);
	cp[3] = divide_round_i(mfac * cp1[3] + fac * cp2[3], 255);

	return col;
}

BLI_INLINE uint mcol_darken(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}
	else if (fac >= 255) {
		return col2;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp  = (uchar *)&col;

	/* See if were darker, if so mix, else don't do anything.
	 * if the paint col is brighter then the original, then ignore */
	if (IMB_colormanagement_get_luminance_byte(cp1) < IMB_colormanagement_get_luminance_byte(cp2)) {
		return col1;
	}

	cp[0] = divide_round_i((mfac * cp1[0] + fac * cp2[0]), 255);
	cp[1] = divide_round_i((mfac * cp1[1] + fac * cp2[1]), 255);
	cp[2] = divide_round_i((mfac * cp1[2] + fac * cp2[2]), 255);
	cp[3] = divide_round_i((mfac * cp1[3] + fac * cp2[3]), 255);
	return col;
}

BLI_INLINE uint mcol_colordodge(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac, temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	temp = (cp2[0] == 255) ? 255 : min_ii((cp1[0] * 225) / (255 - cp2[0]), 255);
	cp[0] = (mfac * cp1[0] + temp * fac) / 255;
	temp = (cp2[1] == 255) ? 255 : min_ii((cp1[1] * 225) / (255 - cp2[1]), 255);
	cp[1] = (mfac * cp1[1] + temp * fac) / 255;
	temp = (cp2[2] == 255) ? 255 : min_ii((cp1[2] * 225) / (255 - cp2[2]), 255);
	cp[2] = (mfac * cp1[2] + temp * fac) / 255;
	temp = (cp2[3] == 255) ? 255 : min_ii((cp1[3] * 225) / (255 - cp2[3]), 255);
	cp[3] = (mfac * cp1[3] + temp * fac) / 255;
	return col;
}

BLI_INLINE uint mcol_difference(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac, temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	temp = abs(cp1[0] - cp2[0]);
	cp[0] = (mfac * cp1[0] + temp * fac) / 255;
	temp = abs(cp1[1] - cp2[1]);
	cp[1] = (mfac * cp1[1] + temp * fac) / 255;
	temp = abs(cp1[2] - cp2[2]);
	cp[2] = (mfac * cp1[2] + temp * fac) / 255;
	temp = abs(cp1[3] - cp2[3]);
	cp[3] = (mfac * cp1[3] + temp * fac) / 255;
	return col;
}

BLI_INLINE uint mcol_screen(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac, temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	temp = max_ii(255 - (((255 - cp1[0]) * (255 - cp2[0])) / 255), 0);
	cp[0] = (mfac * cp1[0] + temp * fac) / 255;
	temp = max_ii(255 - (((255 - cp1[1]) * (255 - cp2[1])) / 255), 0);
	cp[1] = (mfac * cp1[1] + temp * fac) / 255;
	temp = max_ii(255 - (((255 - cp1[2]) * (255 - cp2[2])) / 255), 0);
	cp[2] = (mfac * cp1[2] + temp * fac) / 255;
	temp = max_ii(255 - (((255 - cp1[3]) * (255 - cp2[3])) / 255), 0);
	cp[3] = (mfac * cp1[3] + temp * fac) / 255;
	return col;
}

BLI_INLINE uint mcol_hardlight(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac, temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	int i = 0;

	for (i = 0; i < 4; i++) {
		if (cp2[i] > 127) {
			temp = 255 - ((255 - 2 * (cp2[i] - 127)) * (255 - cp1[i]) / 255);
		}
		else {
			temp = (2 * cp2[i] * cp1[i]) >> 8;
		}
		cp[i] = min_ii((mfac * cp1[i] + temp * fac) / 255, 255);
	}
	return col;
}

BLI_INLINE uint mcol_overlay(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac, temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	int i = 0;

	for (i = 0; i < 4; i++) {
		if (cp1[i] > 127) {
			temp = 255 - ((255 - 2 * (cp1[i] - 127)) * (255 - cp2[i]) / 255);
		}
		else {
			temp = (2 * cp2[i] * cp1[i]) >> 8;
		}
		cp[i] = min_ii((mfac * cp1[i] + temp * fac) / 255, 255);
	}
	return col;
}

BLI_INLINE uint mcol_softlight(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac, temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	int i = 0;

	for (i = 0; i < 4; i++) {
		if (cp1[i] < 127) {
			temp = ((2 * ((cp2[i] / 2) + 64)) * cp1[i]) / 255;
		}
		else {
			temp = 255 - (2 * (255 - ((cp2[i] / 2) + 64)) * (255 - cp1[i]) / 255);
		}
		cp[i] = (temp * fac + cp1[i] * mfac) / 255;
	}
	return col;
}

BLI_INLINE uint mcol_exclusion(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac, temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	int i = 0;

	for (i = 0; i < 4; i++) {
		temp = 127 - ((2 * (cp1[i] - 127) * (cp2[i] - 127)) / 255);
		cp[i] = (temp * fac + cp1[i] * mfac) / 255;
	}
	return col;
}

BLI_INLINE uint mcol_luminosity(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	float h1, s1, v1;
	float h2, s2, v2;
	float r, g, b;
	rgb_to_hsv(cp1[0] / 255.0f, cp1[1] / 255.0f, cp1[2] / 255.0f, &h1, &s1, &v1);
	rgb_to_hsv(cp2[0] / 255.0f, cp2[1] / 255.0f, cp2[2] / 255.0f, &h2, &s2, &v2);

	v1 = v2;

	hsv_to_rgb(h1, s1, v1, &r, &g, &b);

	cp[0] = ((int)(r * 255.0f) * fac + mfac * cp1[0]) / 255;
	cp[1] = ((int)(g * 255.0f) * fac + mfac * cp1[1]) / 255;
	cp[2] = ((int)(b * 255.0f) * fac + mfac * cp1[2]) / 255;
	cp[3] = ((int)(cp2[3])     * fac + mfac * cp1[3]) / 255;
	return col;
}

BLI_INLINE uint mcol_saturation(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	float h1, s1, v1;
	float h2, s2, v2;
	float r, g, b;
	rgb_to_hsv(cp1[0] / 255.0f, cp1[1] / 255.0f, cp1[2] / 255.0f, &h1, &s1, &v1);
	rgb_to_hsv(cp2[0] / 255.0f, cp2[1] / 255.0f, cp2[2] / 255.0f, &h2, &s2, &v2);

	if (s1 > EPS_SATURATION) {
		s1 = s2;
	}

	hsv_to_rgb(h1, s1, v1, &r, &g, &b);

	cp[0] = ((int)(r * 255.0f) * fac + mfac * cp1[0]) / 255;
	cp[1] = ((int)(g * 255.0f) * fac + mfac * cp1[1]) / 255;
	cp[2] = ((int)(b * 255.0f) * fac + mfac * cp1[2]) / 255;
	return col;
}

BLI_INLINE uint mcol_hue(uint col1, uint col2, int fac)
{
	uchar *cp1, *cp2, *cp;
	int mfac;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	mfac = 255 - fac;

	cp1 = (uchar *)&col1;
	cp2 = (uchar *)&col2;
	cp = (uchar *)&col;

	float h1, s1, v1;
	float h2, s2, v2;
	float r, g, b;
	rgb_to_hsv(cp1[0] / 255.0f, cp1[1] / 255.0f, cp1[2] / 255.0f, &h1, &s1, &v1);
	rgb_to_hsv(cp2[0] / 255.0f, cp2[1] / 255.0f, cp2[2] / 255.0f, &h2, &s2, &v2);

	h1 = h2;

	hsv_to_rgb(h1, s1, v1, &r, &g, &b);

	cp[0] = ((int)(r * 255.0f) * fac + mfac * cp1[0]) / 255;
	cp[1] = ((int)(g * 255.0f) * fac + mfac * cp1[1]) / 255;
	cp[2] = ((int)(b * 255.0f) * fac + mfac * cp1[2]) / 255;
	cp[3] = ((int)(cp2[3])     * fac + mfac * cp1[3]) / 255;
	return col;
}

BLI_INLINE uint mcol_alpha_add(uint col1, int fac)
{
	uchar *cp1, *cp;
	int temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	cp1 = (uchar *)&col1;
	cp  = (uchar *)&col;

	temp = cp1[3] + fac;
	cp[3] = (temp > 254) ? 255 : temp;

	return col;
}

BLI_INLINE uint mcol_alpha_sub(uint col1, int fac)
{
	uchar *cp1, *cp;
	int temp;
	uint col = 0;

	if (fac == 0) {
		return col1;
	}

	cp1 = (uchar *)&col1;
	cp  = (uchar *)&col;

	temp = cp1[3] - fac;
	cp[3] = temp < 0 ? 0 : temp;

	return col;
}

/* wpaint has 'ED_wpaint_blend_tool' */
uint ED_vpaint_blend_tool(
        const int tool, const uint col,
        const uint paintcol, const int alpha_i)
{
	switch (tool) {
		case PAINT_BLEND_MIX:
		case PAINT_BLEND_BLUR:       return mcol_blend(col, paintcol, alpha_i);
		case PAINT_BLEND_AVERAGE:    return mcol_blend(col, paintcol, alpha_i);
		case PAINT_BLEND_SMEAR:      return mcol_blend(col, paintcol, alpha_i);
		case PAINT_BLEND_ADD:        return mcol_add(col, paintcol, alpha_i);
		case PAINT_BLEND_SUB:        return mcol_sub(col, paintcol, alpha_i);
		case PAINT_BLEND_MUL:        return mcol_mul(col, paintcol, alpha_i);
		case PAINT_BLEND_LIGHTEN:    return mcol_lighten(col, paintcol, alpha_i);
		case PAINT_BLEND_DARKEN:     return mcol_darken(col, paintcol, alpha_i);
		case PAINT_BLEND_COLORDODGE: return mcol_colordodge(col, paintcol, alpha_i);
		case PAINT_BLEND_DIFFERENCE: return mcol_difference(col, paintcol, alpha_i);
		case PAINT_BLEND_SCREEN:     return mcol_screen(col, paintcol, alpha_i);
		case PAINT_BLEND_HARDLIGHT:  return mcol_hardlight(col, paintcol, alpha_i);
		case PAINT_BLEND_OVERLAY:    return mcol_overlay(col, paintcol, alpha_i);
		case PAINT_BLEND_SOFTLIGHT:  return mcol_softlight(col, paintcol, alpha_i);
		case PAINT_BLEND_EXCLUSION:  return mcol_exclusion(col, paintcol, alpha_i);
		case PAINT_BLEND_LUMINOCITY: return mcol_luminosity(col, paintcol, alpha_i);
		case PAINT_BLEND_SATURATION: return mcol_saturation(col, paintcol, alpha_i);
		case PAINT_BLEND_HUE:        return mcol_hue(col, paintcol, alpha_i);
		/* non-color */
		case PAINT_BLEND_ALPHA_SUB:  return mcol_alpha_sub(col, alpha_i);
		case PAINT_BLEND_ALPHA_ADD:  return mcol_alpha_add(col, alpha_i);
		default:
			BLI_assert(0);
			return 0;
	}
}

/** \} */
