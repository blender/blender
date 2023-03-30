/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright Blender Foundation */

#pragma once

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
    float *result, const float *input, int res[3], int *pixel, float *tRay, float correct);

struct Mesh *BKE_fluid_modifier_do(struct FluidModifierData *fmd,
                                   struct Depsgraph *depsgraph,
                                   struct Scene *scene,
                                   struct Object *ob,
                                   struct Mesh *me);

void BKE_fluid_modifier_free(struct FluidModifierData *fmd);
void BKE_fluid_modifier_reset(struct FluidModifierData *fmd);
void BKE_fluid_modifier_create_type_data(struct FluidModifierData *fmd);
void BKE_fluid_modifier_copy(const struct FluidModifierData *fmd,
                             struct FluidModifierData *tfmd,
                             int flag);

bool BKE_fluid_reallocate_fluid(struct FluidDomainSettings *fds, int res[3], int free_old);
void BKE_fluid_reallocate_copy_fluid(struct FluidDomainSettings *fds,
                                     int o_res[3],
                                     int n_res[3],
                                     const int o_min[3],
                                     const int n_min[3],
                                     const int o_max[3],
                                     int o_shift[3],
                                     int n_shift[3]);
void BKE_fluid_cache_free_all(struct FluidDomainSettings *fds, struct Object *ob);
void BKE_fluid_cache_free(struct FluidDomainSettings *fds, struct Object *ob, int cache_map);
void BKE_fluid_cache_new_name_for_current_session(int maxlen, char *r_name);

/**
 * Get fluid velocity and density at given coordinates.
 * \returns fluid density or -1.0f if outside domain.
 */
float BKE_fluid_get_velocity_at(struct Object *ob, float position[3], float velocity[3]);
int BKE_fluid_get_data_flags(struct FluidDomainSettings *fds);

void BKE_fluid_particle_system_create(struct Main *bmain,
                                      struct Object *ob,
                                      const char *pset_name,
                                      const char *parts_name,
                                      const char *psys_name,
                                      int psys_type);
void BKE_fluid_particle_system_destroy(struct Object *ob, int particle_type);

void BKE_fluid_cache_startframe_set(struct FluidDomainSettings *settings, int value);
void BKE_fluid_cache_endframe_set(struct FluidDomainSettings *settings, int value);

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
void BKE_fluid_fields_sanitize(struct FluidDomainSettings *settings);
void BKE_fluid_flow_behavior_set(struct Object *object,
                                 struct FluidFlowSettings *settings,
                                 int behavior);

#ifdef __cplusplus
}
#endif
