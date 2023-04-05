/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation */

/** \file
 * \ingroup intern_mantaflow
 */

#ifndef MANTA_API_H
#define MANTA_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct MANTA;

/* Fluid functions */
struct MANTA *manta_init(int *res, struct FluidModifierData *fmd);
void manta_free(struct MANTA *fluid);
bool manta_ensure_obstacle(struct MANTA *fluid, struct FluidModifierData *fmd);
bool manta_ensure_guiding(struct MANTA *fluid, struct FluidModifierData *fmd);
bool manta_ensure_invelocity(struct MANTA *fluid, struct FluidModifierData *fmd);
bool manta_ensure_outflow(struct MANTA *fluid, struct FluidModifierData *fmd);
bool manta_write_config(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_write_data(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_write_noise(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_read_config(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_read_data(struct MANTA *fluid,
                     struct FluidModifierData *fmd,
                     int framenr,
                     bool resumable);
bool manta_read_noise(struct MANTA *fluid,
                      struct FluidModifierData *fmd,
                      int framenr,
                      bool resumable);
bool manta_read_mesh(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_read_particles(struct MANTA *fluid,
                          struct FluidModifierData *fmd,
                          int framenr,
                          bool resumable);
bool manta_read_guiding(struct MANTA *fluid,
                        struct FluidModifierData *fmd,
                        int framenr,
                        bool sourceDomain);
bool manta_bake_data(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_bake_noise(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_bake_mesh(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_bake_particles(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_bake_guiding(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_has_data(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_has_noise(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_has_mesh(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_has_particles(struct MANTA *fluid, struct FluidModifierData *fmd, int framenr);
bool manta_has_guiding(struct MANTA *fluid,
                       struct FluidModifierData *fmd,
                       int framenr,
                       bool domain);

void manta_update_variables(struct MANTA *fluid, struct FluidModifierData *fmd);
int manta_get_frame(struct MANTA *fluid);
float manta_get_timestep(struct MANTA *fluid);
void manta_adapt_timestep(struct MANTA *fluid);
bool manta_needs_realloc(struct MANTA *fluid, struct FluidModifierData *fmd);
void manta_update_pointers(struct MANTA *fluid, struct FluidModifierData *fmd, bool flush);

/* Fluid accessors */
size_t manta_get_index(int x, int max_x, int y, int max_y, int z /*, int max_z */);
size_t manta_get_index2d(int x, int max_x, int y /*, int max_y, int z, int max_z */);
float *manta_get_velocity_x(struct MANTA *fluid);
float *manta_get_velocity_y(struct MANTA *fluid);
float *manta_get_velocity_z(struct MANTA *fluid);
float *manta_get_ob_velocity_x(struct MANTA *fluid);
float *manta_get_ob_velocity_y(struct MANTA *fluid);
float *manta_get_ob_velocity_z(struct MANTA *fluid);
float *manta_get_guide_velocity_x(struct MANTA *fluid);
float *manta_get_guide_velocity_y(struct MANTA *fluid);
float *manta_get_guide_velocity_z(struct MANTA *fluid);
float *manta_get_in_velocity_x(struct MANTA *fluid);
float *manta_get_in_velocity_y(struct MANTA *fluid);
float *manta_get_in_velocity_z(struct MANTA *fluid);
float *manta_get_force_x(struct MANTA *fluid);
float *manta_get_force_y(struct MANTA *fluid);
float *manta_get_force_z(struct MANTA *fluid);
float *manta_get_phiguide_in(struct MANTA *fluid);
float *manta_get_num_obstacle(struct MANTA *fluid);
float *manta_get_num_guide(struct MANTA *fluid);
int manta_get_res_x(struct MANTA *fluid);
int manta_get_res_y(struct MANTA *fluid);
int manta_get_res_z(struct MANTA *fluid);
float *manta_get_phi_in(struct MANTA *fluid);
float *manta_get_phistatic_in(struct MANTA *fluid);
float *manta_get_phiobs_in(struct MANTA *fluid);
float *manta_get_phiobsstatic_in(struct MANTA *fluid);
float *manta_get_phiout_in(struct MANTA *fluid);
float *manta_get_phioutstatic_in(struct MANTA *fluid);
float *manta_get_phi(struct MANTA *fluid);
float *manta_get_pressure(struct MANTA *fluid);

/* Smoke functions */
bool manta_smoke_export_script(struct MANTA *smoke, struct FluidModifierData *fmd);
void manta_smoke_get_rgba(struct MANTA *smoke, float *data, int sequential);
void manta_noise_get_rgba(struct MANTA *smoke, float *data, int sequential);
void manta_smoke_get_rgba_fixed_color(struct MANTA *smoke,
                                      float color[3],
                                      float *data,
                                      int sequential);
void manta_noise_get_rgba_fixed_color(struct MANTA *smoke,
                                      float color[3],
                                      float *data,
                                      int sequential);
bool manta_smoke_ensure_heat(struct MANTA *smoke, struct FluidModifierData *fmd);
bool manta_smoke_ensure_fire(struct MANTA *smoke, struct FluidModifierData *fmd);
bool manta_smoke_ensure_colors(struct MANTA *smoke, struct FluidModifierData *fmd);

/* Smoke accessors */
float *manta_smoke_get_density(struct MANTA *smoke);
float *manta_smoke_get_fuel(struct MANTA *smoke);
float *manta_smoke_get_react(struct MANTA *smoke);
float *manta_smoke_get_heat(struct MANTA *smoke);
float *manta_smoke_get_flame(struct MANTA *smoke);
float *manta_smoke_get_shadow(struct MANTA *fluid);
float *manta_smoke_get_color_r(struct MANTA *smoke);
float *manta_smoke_get_color_g(struct MANTA *smoke);
float *manta_smoke_get_color_b(struct MANTA *smoke);
int *manta_smoke_get_flags(struct MANTA *smoke);
float *manta_smoke_get_density_in(struct MANTA *smoke);
float *manta_smoke_get_heat_in(struct MANTA *smoke);
float *manta_smoke_get_color_r_in(struct MANTA *smoke);
float *manta_smoke_get_color_g_in(struct MANTA *smoke);
float *manta_smoke_get_color_b_in(struct MANTA *smoke);
float *manta_smoke_get_fuel_in(struct MANTA *smoke);
float *manta_smoke_get_react_in(struct MANTA *smoke);
float *manta_smoke_get_emission_in(struct MANTA *smoke);
bool manta_smoke_has_heat(struct MANTA *smoke);
bool manta_smoke_has_fuel(struct MANTA *smoke);
bool manta_smoke_has_colors(struct MANTA *smoke);
float *manta_noise_get_density(struct MANTA *smoke);
float *manta_noise_get_fuel(struct MANTA *smoke);
float *manta_noise_get_react(struct MANTA *smoke);
float *manta_noise_get_color_r(struct MANTA *smoke);
float *manta_noise_get_color_g(struct MANTA *smoke);
float *manta_noise_get_color_b(struct MANTA *smoke);
float *manta_noise_get_texture_u(struct MANTA *smoke);
float *manta_noise_get_texture_v(struct MANTA *smoke);
float *manta_noise_get_texture_w(struct MANTA *smoke);
float *manta_noise_get_texture_u2(struct MANTA *smoke);
float *manta_noise_get_texture_v2(struct MANTA *smoke);
float *manta_noise_get_texture_w2(struct MANTA *smoke);
float *manta_noise_get_flame(struct MANTA *smoke);
bool manta_noise_has_fuel(struct MANTA *smoke);
bool manta_noise_has_colors(struct MANTA *smoke);
void manta_noise_get_res(struct MANTA *smoke, int *res);
int manta_noise_get_cells(struct MANTA *smoke);

/* Liquid functions */
bool manta_liquid_export_script(struct MANTA *smoke, struct FluidModifierData *fmd);
bool manta_liquid_ensure_sndparts(struct MANTA *fluid, struct FluidModifierData *fmd);

/* Liquid accessors */
int manta_liquid_get_particle_res_x(struct MANTA *liquid);
int manta_liquid_get_particle_res_y(struct MANTA *liquid);
int manta_liquid_get_particle_res_z(struct MANTA *liquid);
int manta_liquid_get_mesh_res_x(struct MANTA *liquid);
int manta_liquid_get_mesh_res_y(struct MANTA *liquid);
int manta_liquid_get_mesh_res_z(struct MANTA *liquid);
int manta_liquid_get_particle_upres(struct MANTA *liquid);
int manta_liquid_get_mesh_upres(struct MANTA *liquid);
int manta_liquid_get_num_verts(struct MANTA *liquid);
int manta_liquid_get_num_normals(struct MANTA *liquid);
int manta_liquid_get_num_triangles(struct MANTA *liquid);
float manta_liquid_get_vertex_x_at(struct MANTA *liquid, int i);
float manta_liquid_get_vertex_y_at(struct MANTA *liquid, int i);
float manta_liquid_get_vertex_z_at(struct MANTA *liquid, int i);
float manta_liquid_get_normal_x_at(struct MANTA *liquid, int i);
float manta_liquid_get_normal_y_at(struct MANTA *liquid, int i);
float manta_liquid_get_normal_z_at(struct MANTA *liquid, int i);
int manta_liquid_get_triangle_x_at(struct MANTA *liquid, int i);
int manta_liquid_get_triangle_y_at(struct MANTA *liquid, int i);
int manta_liquid_get_triangle_z_at(struct MANTA *liquid, int i);
float manta_liquid_get_vertvel_x_at(struct MANTA *liquid, int i);
float manta_liquid_get_vertvel_y_at(struct MANTA *liquid, int i);
float manta_liquid_get_vertvel_z_at(struct MANTA *liquid, int i);
int manta_liquid_get_num_flip_particles(struct MANTA *liquid);
int manta_liquid_get_num_snd_particles(struct MANTA *liquid);
int manta_liquid_get_flip_particle_flag_at(struct MANTA *liquid, int i);
int manta_liquid_get_snd_particle_flag_at(struct MANTA *liquid, int i);
float manta_liquid_get_flip_particle_position_x_at(struct MANTA *liquid, int i);
float manta_liquid_get_flip_particle_position_y_at(struct MANTA *liquid, int i);
float manta_liquid_get_flip_particle_position_z_at(struct MANTA *liquid, int i);
float manta_liquid_get_flip_particle_velocity_x_at(struct MANTA *liquid, int i);
float manta_liquid_get_flip_particle_velocity_y_at(struct MANTA *liquid, int i);
float manta_liquid_get_flip_particle_velocity_z_at(struct MANTA *liquid, int i);
float manta_liquid_get_snd_particle_position_x_at(struct MANTA *liquid, int i);
float manta_liquid_get_snd_particle_position_y_at(struct MANTA *liquid, int i);
float manta_liquid_get_snd_particle_position_z_at(struct MANTA *liquid, int i);
float manta_liquid_get_snd_particle_velocity_x_at(struct MANTA *liquid, int i);
float manta_liquid_get_snd_particle_velocity_y_at(struct MANTA *liquid, int i);
float manta_liquid_get_snd_particle_velocity_z_at(struct MANTA *liquid, int i);

#ifdef __cplusplus
}
#endif

#endif /* MANTA_API_H_ */
