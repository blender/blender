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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenlib/intern/lasso.c
 *  \ingroup bli
 */

#include "DNA_vec_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BLI_lasso.h" /* own include */

void BLI_lasso_boundbox(rcti *rect, int mcords[][2], short moves)
{
	short a;

	rect->xmin = rect->xmax = mcords[0][0];
	rect->ymin = rect->ymax = mcords[0][1];

	for (a = 1; a < moves; a++) {
		if      (mcords[a][0] < rect->xmin) rect->xmin = mcords[a][0];
		else if (mcords[a][0] > rect->xmax) rect->xmax = mcords[a][0];
		if      (mcords[a][1] < rect->ymin) rect->ymin = mcords[a][1];
		else if (mcords[a][1] > rect->ymax) rect->ymax = mcords[a][1];
	}
}


int BLI_lasso_is_point_inside(int mcords[][2], short moves,
                              const int sx, const int sy,
                              const int error_value)
{
	/* we do the angle rule, define that all added angles should be about zero or (2 * PI) */
	float angletot = 0.0, dot, ang, cross, fp1[2], fp2[2];
	int a;
	int *p1, *p2;

	if (sx == error_value) {
		return 0;
	}

	p1 = mcords[moves - 1];
	p2 = mcords[0];

	/* first vector */
	fp1[0] = (float)(p1[0] - sx);
	fp1[1] = (float)(p1[1] - sy);
	normalize_v2(fp1);

	for (a = 0; a < moves; a++) {
		/* second vector */
		fp2[0] = (float)(p2[0] - sx);
		fp2[1] = (float)(p2[1] - sy);
		normalize_v2(fp2);

		/* dot and angle and cross */
		dot = fp1[0] * fp2[0] + fp1[1] * fp2[1];
		ang = fabs(saacos(dot));

		cross = (float)((p1[1] - p2[1]) * (p1[0] - sx) + (p2[0] - p1[0]) * (p1[1] - sy));

		if (cross < 0.0f) angletot -= ang;
		else angletot += ang;

		/* circulate */
		fp1[0] = fp2[0]; fp1[1] = fp2[1];
		p1 = p2;
		p2 = mcords[a + 1];
	}

	if (fabsf(angletot) > 4.0f) return 1;
	return 0;
}

/* edge version for lasso select. we assume boundbox check was done */
int BLI_lasso_is_edge_inside(int mcords[][2], short moves,
                             int x0, int y0, int x1, int y1,
                             const int error_value)
{
	int v1[2], v2[2];
	int a;

	if (x0 == error_value || x1 == error_value) {
		return 0;
	}

	v1[0] = x0, v1[1] = y0;
	v2[0] = x1, v2[1] = y1;

	/* check points in lasso */
	if (BLI_lasso_is_point_inside(mcords, moves, v1[0], v1[1], error_value)) return 1;
	if (BLI_lasso_is_point_inside(mcords, moves, v2[0], v2[1], error_value)) return 1;

	/* no points in lasso, so we have to intersect with lasso edge */

	if (isect_line_line_v2_int(mcords[0], mcords[moves - 1], v1, v2) > 0) return 1;
	for (a = 0; a < moves - 1; a++) {
		if (isect_line_line_v2_int(mcords[a], mcords[a + 1], v1, v2) > 0) return 1;
	}

	return 0;
}
