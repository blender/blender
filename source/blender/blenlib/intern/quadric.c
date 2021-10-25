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
 * \note follow BLI_math naming convention here.
 *
 * \note this uses doubles for internal calculations,
 * even though input/output are floats in some cases.
 *
 * This is done because the cases quadrics are useful
 * often need high precision, see T44780.
 */

#include "BLI_math.h"
#include "BLI_strict_flags.h"

#include "BLI_quadric.h"  /* own include */


#define QUADRIC_FLT_TOT (sizeof(Quadric) / sizeof(double))

void BLI_quadric_from_plane(Quadric *q, const double v[4])
{
	q->a2 = v[0] * v[0];
	q->b2 = v[1] * v[1];
	q->c2 = v[2] * v[2];

	q->ab = v[0] * v[1];
	q->ac = v[0] * v[2];
	q->bc = v[1] * v[2];

	q->ad = v[0] * v[3];
	q->bd = v[1] * v[3];
	q->cd = v[2] * v[3];

	q->d2 = v[3] * v[3];
}

#if 0  /* UNUSED */

static void quadric_to_tensor_m3(const Quadric *q, double m[3][3])
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

#endif

/**
 * Inline inverse matrix creation.
 * Equivalent of:
 *
 * \code{.c}
 * quadric_to_tensor_m3(q, m);
 * invert_m3_db(m, eps);
 * \endcode
 */
static bool quadric_to_tensor_m3_inverse(const Quadric *q, double m[3][3], double epsilon)
{
	const double det =
	        (q->a2 * (q->b2 * q->c2 - q->bc * q->bc) -
	         q->ab * (q->ab * q->c2 - q->ac * q->bc) +
	         q->ac * (q->ab * q->bc - q->ac * q->b2));

	if (fabs(det) > epsilon) {
		const double invdet = 1.0 / det;

		m[0][0] = (q->b2 * q->c2 - q->bc * q->bc) * invdet;
		m[1][0] = (q->bc * q->ac - q->ab * q->c2) * invdet;
		m[2][0] = (q->ab * q->bc - q->b2 * q->ac) * invdet;

		m[0][1] = (q->ac * q->bc - q->ab * q->c2) * invdet;
		m[1][1] = (q->a2 * q->c2 - q->ac * q->ac) * invdet;
		m[2][1] = (q->ab * q->ac - q->a2 * q->bc) * invdet;

		m[0][2] = (q->ab * q->bc - q->ac * q->b2) * invdet;
		m[1][2] = (q->ac * q->ab - q->a2 * q->bc) * invdet;
		m[2][2] = (q->a2 * q->b2 - q->ab * q->ab) * invdet;

		return true;
	}
	else {
		return false;
	}
}

void BLI_quadric_to_vector_v3(const Quadric *q, double v[3])
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
	add_vn_vn_d((double *)a, (double *)b, QUADRIC_FLT_TOT);
}

void BLI_quadric_add_qu_ququ(Quadric *r, const Quadric *a, const Quadric *b)
{
	add_vn_vnvn_d((double *)r, (const double *)a, (const double *)b, QUADRIC_FLT_TOT);
}

void BLI_quadric_mul(Quadric *a, const double scalar)
{
	mul_vn_db((double *)a, QUADRIC_FLT_TOT, scalar);
}

double BLI_quadric_evaluate(const Quadric *q, const double v[3])
{
	return ((q->a2 * v[0] * v[0]) + (q->ab * 2 * v[0] * v[1]) + (q->ac * 2 * v[0] * v[2]) + (q->ad * 2 * v[0]) +
	        (q->b2 * v[1] * v[1]) + (q->bc * 2 * v[1] * v[2]) + (q->bd * 2 * v[1]) +
	        (q->c2 * v[2] * v[2]) + (q->cd * 2 * v[2]) +
	        (q->d2));
}

bool BLI_quadric_optimize(const Quadric *q, double v[3], const double epsilon)
{
	double m[3][3];

	if (quadric_to_tensor_m3_inverse(q, m, epsilon)) {
		BLI_quadric_to_vector_v3(q, v);
		mul_m3_v3_db(m, v);
		negate_v3_db(v);
		return true;
	}
	else {
		return false;
	}
}
