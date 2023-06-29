/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct CurveMapping;

typedef struct ParticleChildModifierContext {
  ParticleThreadContext *thread_ctx;
  ParticleSimulationData *sim;
  ParticleTexture *ptex;
  ChildParticle *cpa;
  const float *par_co;   /* float3 */
  const float *par_vel;  /* float3 */
  const float *par_rot;  /* float4 */
  const float *par_orco; /* float3 */
  const float *orco;     /* float3 */
  ParticleCacheKey *parent_keys;
} ParticleChildModifierContext;

void do_kink(ParticleKey *state,
             const float par_co[3],
             const float par_vel[3],
             const float par_rot[4],
             float time,
             float freq,
             float shape,
             float amplitude,
             float flat,
             short type,
             short axis,
             float obmat[4][4],
             int smooth_start);
float do_clump(ParticleKey *state,
               const float par_co[3],
               float time,
               const float orco_offset[3],
               float clumpfac,
               float clumppow,
               float pa_clump,
               bool use_clump_noise,
               float clump_noise_size,
               const struct CurveMapping *clumpcurve);
void do_child_modifiers(const ParticleChildModifierContext *modifier_ctx,
                        float mat[4][4],
                        ParticleKey *state,
                        float t);

#ifdef __cplusplus
}
#endif
