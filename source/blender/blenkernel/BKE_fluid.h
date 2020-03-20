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

#ifndef __BKE_FLUID_H__
#define __BKE_FLUID_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct FluidDomainSettings;
struct FluidEffectorSettings;
struct FluidFlowSettings;
struct FluidModifierData;
struct Main;
struct Scene;

typedef float (*BKE_Fluid_BresenhamFn)(
    float *result, float *input, int res[3], int *pixel, float *tRay, float correct);

struct Mesh *BKE_fluid_modifier_do(struct FluidModifierData *mmd,
                                   struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   struct Mesh *me);

void BKE_fluid_modifier_free(struct FluidModifierData *mmd);
void BKE_fluid_modifier_reset(struct FluidModifierData *mmd);
void BKE_fluid_modifier_create_type_data(struct FluidModifierData *mmd);
void BKE_fluid_modifier_copy(const struct FluidModifierData *mmd,
                             struct FluidModifierData *tmmd,
                             const int flag);

bool BKE_fluid_reallocate_fluid(struct FluidDomainSettings *mds, int res[3], int free_old);
void BKE_fluid_reallocate_copy_fluid(struct FluidDomainSettings *mds,
                                     int o_res[3],
                                     int n_res[3],
                                     int o_min[3],
                                     int n_min[3],
                                     int o_max[3],
                                     int o_shift[3],
                                     int n_shift[3]);
void BKE_fluid_cache_free(struct FluidDomainSettings *mds, struct Object *ob, int cache_map);
void BKE_fluid_cache_new_name_for_current_session(int maxlen, char *r_name);

float BKE_fluid_get_velocity_at(struct Object *ob, float position[3], float velocity[3]);
int BKE_fluid_get_data_flags(struct FluidDomainSettings *mds);

void BKE_fluid_particle_system_create(struct Main *bmain,
                                      struct Object *ob,
                                      const char *pset_name,
                                      const char *parts_name,
                                      const char *psys_name,
                                      const int psys_type);
void BKE_fluid_particle_system_destroy(struct Object *ob, const int particle_type);

void BKE_fluid_cachetype_mesh_set(struct FluidDomainSettings *settings, int cache_mesh_format);
void BKE_fluid_cachetype_data_set(struct FluidDomainSettings *settings, int cache_data_format);
void BKE_fluid_cachetype_particle_set(struct FluidDomainSettings *settings,
                                      int cache_particle_format);
void BKE_fluid_cachetype_noise_set(struct FluidDomainSettings *settings, int cache_noise_format);
void BKE_fluid_collisionextents_set(struct FluidDomainSettings *settings, int value, bool clear);
void BKE_fluid_particles_set(struct FluidDomainSettings *settings, int value, bool clear);

void BKE_fluid_domain_type_set(struct Object *object,
                               struct FluidDomainSettings *settings,
                               int type);
void BKE_fluid_flow_type_set(struct Object *object, struct FluidFlowSettings *settings, int type);
void BKE_fluid_effector_type_set(struct Object *object,
                                 struct FluidEffectorSettings *settings,
                                 int type);
void BKE_fluid_flow_behavior_set(struct Object *object,
                                 struct FluidFlowSettings *settings,
                                 int behavior);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_FLUID_H__ */
