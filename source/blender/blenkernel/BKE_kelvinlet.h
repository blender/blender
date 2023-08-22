/* SPDX-FileCopyrightText: Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

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
void BKE_kelvinlet_grab(float radius_elem_disp[3],
                        const KelvinletParams *params,
                        const float elem_orig_co[3],
                        const float brush_location[3],
                        const float brush_delta[3]);
void BKE_kelvinlet_grab_biscale(float radius_elem_disp[3],
                                const KelvinletParams *params,
                                const float elem_orig_co[3],
                                const float brush_location[3],
                                const float brush_delta[3]);
void BKE_kelvinlet_grab_triscale(float radius_elem_disp[3],
                                 const KelvinletParams *params,
                                 const float elem_orig_co[3],
                                 const float brush_location[3],
                                 const float brush_delta[3]);
void BKE_kelvinlet_scale(float radius_elem_disp[3],
                         const KelvinletParams *params,
                         const float elem_orig_co[3],
                         const float brush_location[3],
                         const float surface_normal[3]);
void BKE_kelvinlet_twist(float radius_elem_disp[3],
                         const KelvinletParams *params,
                         const float elem_orig_co[3],
                         const float brush_location[3],
                         const float surface_normal[3]);

#ifdef __cplusplus
}
#endif
