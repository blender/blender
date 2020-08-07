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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Regularized Kelvinlets: Sculpting Brushes based on Fundamental Solutions of Elasticity
 * Pixar Technical Memo #17-03 */

#define KELVINLET_MAX_ITERATIONS 3

typedef struct KelvinletParams {
  float a;
  float b;
  float c;

  float f;

  float radius_scaled[KELVINLET_MAX_ITERATIONS];
} KelvinletParams;

/* Initialize KelvinletParams to store the parameters that will affect the deformation produced by
 * a Kelvinlet */
void BKE_kelvinlet_init_params(
    KelvinletParams *params, float radius, float force, float shear_modulus, float poisson_ratio);

/* Regularized Kelvinlets */
/* All these functions output the displacement that should be applied to each element. */
/* The initial coordinates of that element should not be modified during the transformation */
void BKE_kelvinlet_grab(float r_elem_disp[3],
                        const KelvinletParams *params,
                        const float elem_orig_co[3],
                        const float brush_location[3],
                        const float brush_delta[3]);
void BKE_kelvinlet_grab_biscale(float r_elem_disp[3],
                                const KelvinletParams *params,
                                const float elem_orig_co[3],
                                const float brush_location[3],
                                const float brush_delta[3]);
void BKE_kelvinlet_grab_triscale(float r_elem_disp[3],
                                 const KelvinletParams *params,
                                 const float elem_orig_co[3],
                                 const float brush_location[3],
                                 const float brush_delta[3]);
void BKE_kelvinlet_scale(float r_elem_disp[3],
                         const KelvinletParams *params,
                         const float elem_orig_co[3],
                         const float brush_location[3],
                         const float surface_normal[3]);
void BKE_kelvinlet_twist(float r_elem_disp[3],
                         const KelvinletParams *params,
                         const float elem_orig_co[3],
                         const float brush_location[3],
                         const float surface_normal[3]);

#ifdef __cplusplus
}
#endif
