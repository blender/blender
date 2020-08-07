/*
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
 */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Quadric {
  double a2, ab, ac, ad, b2, bc, bd, c2, cd, d2;
} Quadric;

/* conversion */
void BLI_quadric_from_plane(Quadric *q, const double v[4]);
void BLI_quadric_to_vector_v3(const Quadric *q, double v[3]);

void BLI_quadric_clear(Quadric *q);

/* math */
void BLI_quadric_add_qu_qu(Quadric *a, const Quadric *b);
void BLI_quadric_add_qu_ququ(Quadric *r, const Quadric *a, const Quadric *b);
void BLI_quadric_mul(Quadric *a, const double scalar);

/* solve */
double BLI_quadric_evaluate(const Quadric *q, const double v[3]);
bool BLI_quadric_optimize(const Quadric *q, double v[3], const double epsilon);

#ifdef __cplusplus
}
#endif
