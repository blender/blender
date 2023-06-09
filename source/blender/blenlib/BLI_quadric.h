/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/* Conversion. */

void BLI_quadric_from_plane(Quadric *q, const double v[4]);
void BLI_quadric_to_vector_v3(const Quadric *q, double v[3]);

void BLI_quadric_clear(Quadric *q);

/* Math operations. */

void BLI_quadric_add_qu_qu(Quadric *a, const Quadric *b);
void BLI_quadric_add_qu_ququ(Quadric *r, const Quadric *a, const Quadric *b);
void BLI_quadric_mul(Quadric *a, double scalar);

/* Solve. */

double BLI_quadric_evaluate(const Quadric *q, const double v[3]);
bool BLI_quadric_optimize(const Quadric *q, double v[3], double epsilon);

#ifdef __cplusplus
}
#endif
