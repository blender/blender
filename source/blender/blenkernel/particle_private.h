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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/particle_private.h
 *  \ingroup bke
 */

#ifndef __PARTICLE_PRIVATE_H__
#define __PARTICLE_PRIVATE_H__

void do_kink(ParticleKey *state, const float par_co[3], const float par_vel[3], const float par_rot[4], float time, float freq, float shape, float amplitude, float flat,
             short type, short axis, float obmat[4][4], int smooth_start);
float do_clump(ParticleKey *state, const float par_co[3], float time, const float orco_offset[3], float clumpfac, float clumppow, float pa_clump,
               bool use_clump_noise, float clump_noise_size, CurveMapping *clumpcurve);
void do_child_modifiers(ParticleThreadContext *ctx, ParticleSimulationData *sim,
                        ParticleTexture *ptex, const float par_co[3], const float par_vel[3], const float par_rot[4], const float par_orco[3],
                        ChildParticle *cpa, const float orco[3], float mat[4][4], ParticleKey *state, float t);

#endif /* __PARTICLE_PRIVATE_H__ */
