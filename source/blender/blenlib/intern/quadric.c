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
 * Contributor(s): Laurence Bourn, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/quadric.c
 *  \ingroup bli
 *
 * \note This isn't fully complete,
 * possible there are other useful functions to add here.
 *
 * \note try to follow BLI_math naming convention here.
 */

//#include <string.h>

#include "BLI_math.h"
#include "BLI_strict_flags.h"

#include "BLI_quadric.h"  /* own include */


#define QUADRIC_FLT_TOT (sizeof(Quadric) / sizeof(float))

void BLI_quadric_from_v3_dist(Quadric *q, const float v[3], const float offset)
{
	q->a2 = v[0] * v[0];
	q->b2 = v[1] * v[1];
	q->c2 = v[2] * v[2];

	q->ab = v[0] * v[1];
	q->ac = v[0] * v[2];
	q->bc = v[1] * v[2];

	q->ad = v[0] * offset;
	q->bd = v[1] * offset;
	q->cd = v[2] * offset;

	q->d2 = offset * offset;
}

void BLI_quadric_to_tensor_m3(const Quadric *q, float m[3][3])
{
	m[0][0] = q->a2;
	m[0][1] = q->ab;
	m[0][2] = q->ac;

	m[1][0] = q->ab;
	m[1][1] = q->b2;
	m[1][2] = q->bc;

	m[2][0] = q->ac;
	m[2][1] = q->bc;
	m[2][2] = q->c2;
}

void BLI_quadric_to_vector_v3(const Quadric *q, float v[3])
{
	v[0] = q->ad;
	v[1] = q->bd;
	v[2] = q->cd;
}

void BLI_quadric_clear(Quadric *q)
{
	memset(q, 0, sizeof(*q));
}

void BLI_quadric_add_qu_qu(Quadric *a, const Quadric *b)
{
	add_vn_vn((float *)a, (float *)b, QUADRIC_FLT_TOT);
}

void BLI_quadric_add_qu_ququ(Quadric *r, const Quadric *a, const Quadric *b)
{
	add_vn_vnvn((float *)r, (const float *)a, (const float *)b, QUADRIC_FLT_TOT);
}

void BLI_quadric_mul(Quadric *a, const float scalar)
{
	mul_vn_fl((float *)a, QUADRIC_FLT_TOT, scalar);
}

float BLI_quadric_evaluate(const Quadric *q, const float v[3])
{
	return  (v[0] * v[0] * q->a2 + 2.0f * v[0] * v[1] * q->ab + 2.0f * v[0] * v[2] * q->ac + 2.0f * v[0] * q->ad +
	         v[1] * v[1] * q->b2 + 2.0f * v[1] * v[2] * q->bc + 2.0f * v[1] * q->bd +
	         v[2] * v[2] * q->c2 + 2.0f * v[2] * q->cd +
	         q->d2);
}

int BLI_quadric_optimize(const Quadric *q, float v[3], const float epsilon)
{
	float m[3][3];

	BLI_quadric_to_tensor_m3(q, m);

	if (invert_m3_ex(m, epsilon)) {
		BLI_quadric_to_vector_v3(q, v);
		mul_m3_v3(m, v);
		negate_v3(v);

		return TRUE;
	}
	else {
		return FALSE;
	}
}
