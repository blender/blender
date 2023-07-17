/* SPDX-FileCopyrightText: Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_kelvinlet.h"

/* Regularized Kelvinlets: Sculpting Brushes based on Fundamental Solutions of Elasticity
 * Pixar Technical Memo #17-03 */

void BKE_kelvinlet_init_params(
    KelvinletParams *params, float radius, float force, float shear_modulus, float poisson_ratio)
{
  params->a = 1.0f / (4.0f * (float)M_PI * shear_modulus);
  params->b = params->a / (4.0f * (1.0f - poisson_ratio));
  params->c = 2 * (3.0f * params->a - 2.0f * params->b);

  /* Used in scale and twist. */
  params->f = force;

  /* This can be exposed if needed */
  const float radius_e[KELVINLET_MAX_ITERATIONS] = {1.0f, 2.0f, 2.0f};
  params->radius_scaled[0] = radius * radius_e[0];
  params->radius_scaled[1] = params->radius_scaled[0] * radius_e[1];
  params->radius_scaled[2] = params->radius_scaled[1] * radius_e[2];
}

static void init_kelvinlet_grab(float radius_e[3],
                                float kelvinlet[3],
                                const float radius,
                                const KelvinletParams *params,
                                const int num_iterations)
{
  const float a = params->a;
  const float b = params->b;
  const float *radius_scaled = params->radius_scaled;

  for (int i = 0; i < num_iterations; i++) {
    radius_e[i] = sqrtf(pow2f(radius) + pow2f(params->radius_scaled[i]));
  }

  /* Regularized Kelvinlets: Formula (6) */
  for (int i = 0; i < num_iterations; i++) {
    kelvinlet[i] = ((a - b) / radius_e[i]) + ((b * pow2f(radius)) / pow3f(radius_e[i])) +
                   ((a * pow2f(radius_scaled[i])) / (2.0f * pow3f(radius_e[i])));
  }
}

void BKE_kelvinlet_grab(float radius_elem_disp[3],
                        const KelvinletParams *params,
                        const float elem_orig_co[3],
                        const float brush_location[3],
                        const float brush_delta[3])
{
  float radius_e[3], kelvinlet[3];
  const float c = params->c;
  const float radius = len_v3v3(brush_location, elem_orig_co);

  init_kelvinlet_grab(radius_e, kelvinlet, radius, params, 1);

  const float fade = kelvinlet[0] * c;

  mul_v3_v3fl(radius_elem_disp, brush_delta, fade);
}

void BKE_kelvinlet_grab_biscale(float radius_elem_disp[3],
                                const KelvinletParams *params,
                                const float elem_orig_co[3],
                                const float brush_location[3],
                                const float brush_delta[3])
{
  float radius_e[3], kelvinlet[3];
  const float c = params->c;
  const float *radius_scaled = params->radius_scaled;
  float radius = len_v3v3(brush_location, elem_orig_co);

  init_kelvinlet_grab(radius_e, kelvinlet, radius, params, 2);

  const float u = kelvinlet[0] - kelvinlet[1];
  const float fade = u * c / ((1.0f / radius_scaled[0]) - (1.0f / radius_scaled[1]));

  mul_v3_v3fl(radius_elem_disp, brush_delta, fade);
}

void BKE_kelvinlet_grab_triscale(float radius_elem_disp[3],
                                 const KelvinletParams *params,
                                 const float elem_orig_co[3],
                                 const float brush_location[3],
                                 const float brush_delta[3])
{
  float radius_e[3], kelvinlet[3], weights[3];
  const float c = params->c;
  const float *radius_scaled = params->radius_scaled;
  const float radius = len_v3v3(brush_location, elem_orig_co);

  init_kelvinlet_grab(radius_e, kelvinlet, radius, params, 3);

  weights[0] = 1.0f;
  weights[1] = -((pow2f(radius_scaled[2]) - pow2f(radius_scaled[0])) /
                 (pow2f(radius_scaled[2]) - pow2f(radius_scaled[1])));
  weights[2] = ((pow2f(radius_scaled[1]) - pow2f(radius_scaled[0])) /
                (pow2f(radius_scaled[2]) - pow2f(radius_scaled[1])));

  const float u = weights[0] * kelvinlet[0] + weights[1] * kelvinlet[1] +
                  weights[2] * kelvinlet[2];
  const float fade = u * c /
                     (weights[0] / radius_scaled[0] + weights[1] / radius_scaled[1] +
                      weights[2] / radius_scaled[2]);

  mul_v3_v3fl(radius_elem_disp, brush_delta, fade);
}

typedef void (*kelvinlet_fn)(
    float[3], const float *, const float *, const float *, const KelvinletParams *);

static void sculpt_kelvinet_integrate(kelvinlet_fn kelvinlet,
                                      float r_disp[3],
                                      const float vertex_co[3],
                                      const float location[3],
                                      const float normal[3],
                                      const KelvinletParams *p)
{
  float k[4][3], k_it[4][3];
  kelvinlet(k[0], vertex_co, location, normal, p);
  copy_v3_v3(k_it[0], k[0]);
  mul_v3_fl(k_it[0], 0.5f);
  add_v3_v3v3(k_it[0], vertex_co, k_it[0]);
  kelvinlet(k[1], k_it[0], location, normal, p);
  copy_v3_v3(k_it[1], k[1]);
  mul_v3_fl(k_it[1], 0.5f);
  add_v3_v3v3(k_it[1], vertex_co, k_it[1]);
  kelvinlet(k[2], k_it[1], location, normal, p);
  copy_v3_v3(k_it[2], k[2]);
  add_v3_v3v3(k_it[2], vertex_co, k_it[2]);
  sub_v3_v3v3(k_it[2], k_it[2], location);
  kelvinlet(k[3], k_it[2], location, normal, p);
  copy_v3_v3(r_disp, k[0]);
  madd_v3_v3fl(r_disp, k[1], 2.0f);
  madd_v3_v3fl(r_disp, k[2], 2.0f);
  add_v3_v3(r_disp, k[3]);
  mul_v3_fl(r_disp, 1.0f / 6.0f);
}

/* Regularized Kelvinlets: Formula (16) */
static void kelvinlet_scale(float disp[3],
                            const float vertex_co[3],
                            const float location[3],
                            const float[3] /*normal*/,
                            const KelvinletParams *p)
{
  float radius_vertex[3];
  sub_v3_v3v3(radius_vertex, vertex_co, location);
  const float radius = len_v3(radius_vertex);
  const float radius_e = sqrtf(pow2f(radius) + pow2f(p->radius_scaled[0]));
  const float u = (2.0f * p->b - p->a) * (1.0f / pow3f(radius_e)) +
                  ((3.0f * pow2f(p->radius_scaled[0])) / (2.0f * pow5f(radius_e)));
  const float fade = u * p->c;
  mul_v3_v3fl(disp, radius_vertex, fade * p->f);
}

void BKE_kelvinlet_scale(float radius_elem_disp[3],
                         const KelvinletParams *params,
                         const float elem_orig_co[3],
                         const float brush_location[3],
                         const float surface_normal[3])
{
  sculpt_kelvinet_integrate(
      kelvinlet_scale, radius_elem_disp, elem_orig_co, brush_location, surface_normal, params);
}

/* Regularized Kelvinlets: Formula (15) */
static void kelvinlet_twist(float disp[3],
                            const float vertex_co[3],
                            const float location[3],
                            const float normal[3],
                            const KelvinletParams *p)
{
  float radius_vertex[3], q_r[3];
  sub_v3_v3v3(radius_vertex, vertex_co, location);
  const float radius = len_v3(radius_vertex);
  const float radius_e = sqrtf(pow2f(radius) + pow2f(p->radius_scaled[0]));
  const float u = -p->a * (1.0f / pow3f(radius_e)) +
                  ((3.0f * pow2f(p->radius_scaled[0])) / (2.0f * pow5f(radius_e)));
  const float fade = u * p->c;
  cross_v3_v3v3(q_r, normal, radius_vertex);
  mul_v3_v3fl(disp, q_r, fade * p->f);
}

void BKE_kelvinlet_twist(float radius_elem_disp[3],
                         const KelvinletParams *params,
                         const float elem_orig_co[3],
                         const float brush_location[3],
                         const float surface_normal[3])
{
  sculpt_kelvinet_integrate(
      kelvinlet_twist, radius_elem_disp, elem_orig_co, brush_location, surface_normal, params);
}
