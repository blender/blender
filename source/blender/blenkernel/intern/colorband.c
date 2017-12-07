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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/colorband.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"

#include "DNA_key_types.h"
#include "DNA_texture_types.h"

#include "BKE_colorband.h"
#include "BKE_material.h"
#include "BKE_key.h"

void BKE_colorband_init(ColorBand *coba, bool rangetype)
{
	int a;

	coba->data[0].pos = 0.0;
	coba->data[1].pos = 1.0;

	if (rangetype == 0) {
		coba->data[0].r = 0.0;
		coba->data[0].g = 0.0;
		coba->data[0].b = 0.0;
		coba->data[0].a = 0.0;

		coba->data[1].r = 1.0;
		coba->data[1].g = 1.0;
		coba->data[1].b = 1.0;
		coba->data[1].a = 1.0;
	}
	else {
		coba->data[0].r = 0.0;
		coba->data[0].g = 0.0;
		coba->data[0].b = 0.0;
		coba->data[0].a = 1.0;

		coba->data[1].r = 1.0;
		coba->data[1].g = 1.0;
		coba->data[1].b = 1.0;
		coba->data[1].a = 1.0;
	}

	for (a = 2; a < MAXCOLORBAND; a++) {
		coba->data[a].r = 0.5;
		coba->data[a].g = 0.5;
		coba->data[a].b = 0.5;
		coba->data[a].a = 1.0;
		coba->data[a].pos = 0.5;
	}

	coba->tot = 2;
	coba->color_mode = COLBAND_BLEND_RGB;
}

ColorBand *BKE_colorband_add(bool rangetype)
{
	ColorBand *coba;

	coba = MEM_callocN(sizeof(ColorBand), "colorband");
	BKE_colorband_init(coba, rangetype);

	return coba;
}

/* ------------------------------------------------------------------------- */

static float colorband_hue_interp(
        const int ipotype_hue,
        const float mfac, const float fac,
        float h1, float h2)
{
	float h_interp;
	int mode = 0;

#define HUE_INTERP(h_a, h_b) ((mfac * (h_a)) + (fac * (h_b)))
#define HUE_MOD(h) (((h) < 1.0f) ? (h) : (h) - 1.0f)

	h1 = HUE_MOD(h1);
	h2 = HUE_MOD(h2);

	BLI_assert(h1 >= 0.0f && h1 < 1.0f);
	BLI_assert(h2 >= 0.0f && h2 < 1.0f);

	switch (ipotype_hue) {
		case COLBAND_HUE_NEAR:
		{
			if      ((h1 < h2) && (h2 - h1) > +0.5f) mode = 1;
			else if ((h1 > h2) && (h2 - h1) < -0.5f) mode = 2;
			else                                     mode = 0;
			break;
		}
		case COLBAND_HUE_FAR:
		{
			if      ((h1 < h2) && (h2 - h1) < +0.5f) mode = 1;
			else if ((h1 > h2) && (h2 - h1) > -0.5f) mode = 2;
			else                                     mode = 0;
			break;
		}
		case COLBAND_HUE_CCW:
		{
			if (h1 > h2) mode = 2;
			else         mode = 0;
			break;
		}
		case COLBAND_HUE_CW:
		{
			if (h1 < h2) mode = 1;
			else         mode = 0;
			break;
		}
	}

	switch (mode) {
		case 0:
			h_interp = HUE_INTERP(h1, h2);
			break;
		case 1:
			h_interp = HUE_INTERP(h1 + 1.0f, h2);
			h_interp = HUE_MOD(h_interp);
			break;
		case 2:
			h_interp = HUE_INTERP(h1, h2 + 1.0f);
			h_interp = HUE_MOD(h_interp);
			break;
	}

	BLI_assert(h_interp >= 0.0f && h_interp < 1.0f);

#undef HUE_INTERP
#undef HUE_MOD

	return h_interp;
}

bool BKE_colorband_evaluate(const ColorBand *coba, float in, float out[4])
{
	const CBData *cbd1, *cbd2, *cbd0, *cbd3;
	float fac;
	int ipotype;
	int a;

	if (coba == NULL || coba->tot == 0) return false;

	cbd1 = coba->data;

	ipotype = (coba->color_mode == COLBAND_BLEND_RGB) ? coba->ipotype : COLBAND_INTERP_LINEAR;

	if (coba->tot == 1) {
		out[0] = cbd1->r;
		out[1] = cbd1->g;
		out[2] = cbd1->b;
		out[3] = cbd1->a;
	}
	else if ((in <= cbd1->pos) && ELEM(ipotype, COLBAND_INTERP_LINEAR, COLBAND_INTERP_EASE)) {
		out[0] = cbd1->r;
		out[1] = cbd1->g;
		out[2] = cbd1->b;
		out[3] = cbd1->a;
	}
	else {
		CBData left, right;

		/* we're looking for first pos > in */
		for (a = 0; a < coba->tot; a++, cbd1++) {
			if (cbd1->pos > in) {
				break;
			}
		}

		if (a == coba->tot) {
			cbd2 = cbd1 - 1;
			right = *cbd2;
			right.pos = 1.0f;
			cbd1 = &right;
		}
		else if (a == 0) {
			left = *cbd1;
			left.pos = 0.0f;
			cbd2 = &left;
		}
		else {
			cbd2 = cbd1 - 1;
		}

		if ((in >= cbd1->pos) && ELEM(ipotype, COLBAND_INTERP_LINEAR, COLBAND_INTERP_EASE)) {
			out[0] = cbd1->r;
			out[1] = cbd1->g;
			out[2] = cbd1->b;
			out[3] = cbd1->a;
		}
		else {

			if (cbd2->pos != cbd1->pos) {
				fac = (in - cbd1->pos) / (cbd2->pos - cbd1->pos);
			}
			else {
				/* was setting to 0.0 in 2.56 & previous, but this
				 * is incorrect for the last element, see [#26732] */
				fac = (a != coba->tot) ? 0.0f : 1.0f;
			}

			if (ipotype == COLBAND_INTERP_CONSTANT) {
				/* constant */
				out[0] = cbd2->r;
				out[1] = cbd2->g;
				out[2] = cbd2->b;
				out[3] = cbd2->a;
			}
			else if (ipotype >= COLBAND_INTERP_B_SPLINE) {
				/* ipo from right to left: 3 2 1 0 */
				float t[4];

				if (a >= coba->tot - 1) cbd0 = cbd1;
				else cbd0 = cbd1 + 1;
				if (a < 2) cbd3 = cbd2;
				else cbd3 = cbd2 - 1;

				CLAMP(fac, 0.0f, 1.0f);

				if (ipotype == COLBAND_INTERP_CARDINAL) {
					key_curve_position_weights(fac, t, KEY_CARDINAL);
				}
				else {
					key_curve_position_weights(fac, t, KEY_BSPLINE);
				}

				out[0] = t[3] * cbd3->r + t[2] * cbd2->r + t[1] * cbd1->r + t[0] * cbd0->r;
				out[1] = t[3] * cbd3->g + t[2] * cbd2->g + t[1] * cbd1->g + t[0] * cbd0->g;
				out[2] = t[3] * cbd3->b + t[2] * cbd2->b + t[1] * cbd1->b + t[0] * cbd0->b;
				out[3] = t[3] * cbd3->a + t[2] * cbd2->a + t[1] * cbd1->a + t[0] * cbd0->a;
				CLAMP(out[0], 0.0f, 1.0f);
				CLAMP(out[1], 0.0f, 1.0f);
				CLAMP(out[2], 0.0f, 1.0f);
				CLAMP(out[3], 0.0f, 1.0f);
			}
			else {
				float mfac;

				if (ipotype == COLBAND_INTERP_EASE) {
					mfac = fac * fac;
					fac = 3.0f * mfac - 2.0f * mfac * fac;
				}

				mfac = 1.0f - fac;

				if (UNLIKELY(coba->color_mode == COLBAND_BLEND_HSV)) {
					float col1[3], col2[3];

					rgb_to_hsv_v(&cbd1->r, col1);
					rgb_to_hsv_v(&cbd2->r, col2);

					out[0] = colorband_hue_interp(coba->ipotype_hue, mfac, fac, col1[0], col2[0]);
					out[1] = mfac * col1[1] + fac * col2[1];
					out[2] = mfac * col1[2] + fac * col2[2];
					out[3] = mfac * cbd1->a + fac * cbd2->a;

					hsv_to_rgb_v(out, out);
				}
				else if (UNLIKELY(coba->color_mode == COLBAND_BLEND_HSL)) {
					float col1[3], col2[3];

					rgb_to_hsl_v(&cbd1->r, col1);
					rgb_to_hsl_v(&cbd2->r, col2);

					out[0] = colorband_hue_interp(coba->ipotype_hue, mfac, fac, col1[0], col2[0]);
					out[1] = mfac * col1[1] + fac * col2[1];
					out[2] = mfac * col1[2] + fac * col2[2];
					out[3] = mfac * cbd1->a + fac * cbd2->a;

					hsl_to_rgb_v(out, out);
				}
				else {
					/* COLBAND_BLEND_RGB */
					out[0] = mfac * cbd1->r + fac * cbd2->r;
					out[1] = mfac * cbd1->g + fac * cbd2->g;
					out[2] = mfac * cbd1->b + fac * cbd2->b;
					out[3] = mfac * cbd1->a + fac * cbd2->a;
				}
			}
		}
	}
	return true;   /* OK */
}

void BKE_colorband_evaluate_table_rgba(const ColorBand *coba, float **array, int *size)
{
	int a;

	*size = CM_TABLE + 1;
	*array = MEM_callocN(sizeof(float) * (*size) * 4, "ColorBand");

	for (a = 0; a < *size; a++)
		BKE_colorband_evaluate(coba, (float)a / (float)CM_TABLE, &(*array)[a * 4]);
}

static int vergcband(const void *a1, const void *a2)
{
	const CBData *x1 = a1, *x2 = a2;

	if (x1->pos > x2->pos) return 1;
	else if (x1->pos < x2->pos) return -1;
	return 0;
}

void BKE_colorband_update_sort(ColorBand *coba)
{
	int a;

	if (coba->tot < 2)
		return;

	for (a = 0; a < coba->tot; a++)
		coba->data[a].cur = a;

	qsort(coba->data, coba->tot, sizeof(CBData), vergcband);

	for (a = 0; a < coba->tot; a++) {
		if (coba->data[a].cur == coba->cur) {
			coba->cur = a;
			break;
		}
	}
}

CBData *BKE_colorband_element_add(struct ColorBand *coba, float position)
{
	if (coba->tot == MAXCOLORBAND) {
		return NULL;
	}
	else {
		CBData *xnew;

		xnew = &coba->data[coba->tot];
		xnew->pos = position;

		if (coba->tot != 0) {
			BKE_colorband_evaluate(coba, position, &xnew->r);
		}
		else {
			zero_v4(&xnew->r);
		}
	}

	coba->tot++;
	coba->cur = coba->tot - 1;

	BKE_colorband_update_sort(coba);

	return coba->data + coba->cur;
}

int BKE_colorband_element_remove(struct ColorBand *coba, int index)
{
	int a;

	if (coba->tot < 2)
		return 0;

	if (index < 0 || index >= coba->tot)
		return 0;

	coba->tot--;
	for (a = index; a < coba->tot; a++) {
		coba->data[a] = coba->data[a + 1];
	}
	if (coba->cur) coba->cur--;
	return 1;
}
