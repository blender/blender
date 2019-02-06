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

#ifndef __BKE_SMOKE_H__
#define __BKE_SMOKE_H__

/** \file \ingroup bke
 */

typedef float (*bresenham_callback)(float *result, float *input, int res[3], int *pixel, float *tRay, float correct);

struct Mesh *smokeModifier_do(
        struct SmokeModifierData *smd, struct Depsgraph *depsgraph,
        struct Scene *scene,
        struct Object *ob, struct Mesh *me);

void smoke_reallocate_fluid(struct SmokeDomainSettings *sds, float dx, int res[3], int free_old);
void smoke_reallocate_highres_fluid(struct SmokeDomainSettings *sds, float dx, int res[3], int free_old);
void smokeModifier_free(struct SmokeModifierData *smd);
void smokeModifier_reset(struct SmokeModifierData *smd);
void smokeModifier_reset_turbulence(struct SmokeModifierData *smd);
void smokeModifier_createType(struct SmokeModifierData *smd);
void smokeModifier_copy(const SmokeModifierData *smd, struct SmokeModifierData *tsmd, const int flag);

float smoke_get_velocity_at(struct Object *ob, float position[3], float velocity[3]);
int smoke_get_data_flags(struct SmokeDomainSettings *sds);

#endif /* __BKE_SMOKE_H__ */
