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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Fluid Domain Settings Struct
 * \{ */

#define _DNA_DEFAULT_FluidDomainSettings \
  { \
    .fmd = NULL, \
    .fluid = NULL, \
    .fluid_old = NULL, \
    .fluid_mutex = NULL, \
    .fluid_group = NULL, \
    .force_group = NULL, \
    .effector_group = NULL, \
    .tex_density = NULL, \
    .tex_color = NULL, \
    .tex_wt = NULL, \
    .tex_shadow = NULL, \
    .tex_flame = NULL, \
    .tex_flame_coba = NULL, \
    .tex_coba = NULL, \
    .tex_field = NULL, \
    .tex_velocity_x = NULL, \
    .tex_velocity_y = NULL, \
    .tex_velocity_z = NULL, \
    .tex_flags = NULL, \
    .tex_range_field = NULL, \
    .guide_parent = NULL, \
    .effector_weights = NULL, /* #BKE_effector_add_weights. */ \
    .p0 = {0.0f, 0.0f, 0.0f}, \
    .p1 = {0.0f, 0.0f, 0.0f}, \
    .dp0 = {0.0f, 0.0f, 0.0f}, \
    .cell_size = {0.0f, 0.0f, 0.0f}, \
    .global_size = {0.0f, 0.0f, 0.0f}, \
    .prev_loc = {0.0f, 0.0f, 0.0f}, \
    .shift = {0, 0, 0}, \
    .shift_f = {0.0f, 0.0f, 0.0f}, \
    .obj_shift_f = {0.0f, 0.0f, 0.0f}, \
    .imat = _DNA_DEFAULT_UNIT_M4, \
    .obmat = _DNA_DEFAULT_UNIT_M4, \
    .fluidmat = _DNA_DEFAULT_UNIT_M4, \
    .fluidmat_wt = _DNA_DEFAULT_UNIT_M4, \
    .base_res = {0, 0, 0}, \
    .res_min = {0, 0, 0}, \
    .res_max = {0, 0, 0}, \
    .res = {0, 0, 0}, \
    .total_cells = 0, \
    .dx = 0, \
    .scale = 0.0f, \
    .boundary_width = 1, \
    .gravity_final = {0.0f, 0.0f, 0.0f}, \
    .adapt_margin = 4, \
    .adapt_res = 0, \
    .adapt_threshold = 0.02f, \
    .maxres = 32, \
    .solver_res = 3, \
    .border_collisions = 0, \
    .flags = FLUID_DOMAIN_USE_DISSOLVE_LOG | FLUID_DOMAIN_USE_ADAPTIVE_TIME, \
    .gravity = {0.0f, 0.0f, -9.81f}, \
    .active_fields = 0, \
    .type = FLUID_DOMAIN_TYPE_GAS, \
    .alpha = 1.0f, \
    .beta = 1.0f, \
    .diss_speed = 5, \
    .vorticity = 0.0f, \
    .active_color = {0.0f, 0.0f, 0.0f}, \
    .highres_sampling = SM_HRES_FULLSAMPLE, \
    .burning_rate = 0.75f, \
    .flame_smoke = 1.0f, \
    .flame_vorticity = 0.5f, \
    .flame_ignition = 1.5f, \
    .flame_max_temp = 3.0f, \
    .flame_smoke_color = {0.7f, 0.7f, 0.7f}, \
    .noise_strength = 1.0f, \
    .noise_pos_scale = 2.0f, \
    .noise_time_anim = 0.1f, \
    .res_noise = {0, 0, 0}, \
    .noise_scale = 2, \
    .particle_randomness = 0.1f, \
    .particle_number = 2, \
    .particle_minimum = 8, \
    .particle_maximum = 16, \
    .particle_radius = 1.0f, \
    .particle_band_width = 3.0f, \
    .fractions_threshold = 0.05f, \
    .fractions_distance = 0.5f, \
    .flip_ratio = 0.97f, \
    .sys_particle_maximum = 0, \
    .simulation_method = FLUID_DOMAIN_METHOD_FLIP, \
    .viscosity_value = 0.05f, \
    .surface_tension = 0.0f, \
    .viscosity_base = 1.0f, \
    .viscosity_exponent = 6.0f, \
    .mesh_concave_upper = 3.5f, \
    .mesh_concave_lower = 0.4f, \
    .mesh_particle_radius = 2.0f, \
    .mesh_smoothen_pos = 1, \
    .mesh_smoothen_neg = 1, \
    .mesh_scale = 2, \
    .mesh_generator = FLUID_DOMAIN_MESH_IMPROVED, \
    .particle_type = 0, \
    .particle_scale = 1, \
    .sndparticle_tau_min_wc = 2.0f, \
    .sndparticle_tau_max_wc = 8.0f, \
    .sndparticle_tau_min_ta = 5.0f, \
    .sndparticle_tau_max_ta = 20.0f, \
    .sndparticle_tau_min_k = 1.0f, \
    .sndparticle_tau_max_k = 5.0f, \
    .sndparticle_k_wc = 200, \
    .sndparticle_k_ta = 40, \
    .sndparticle_k_b = 0.5f, \
    .sndparticle_k_d = 0.6f, \
    .sndparticle_l_min = 10.0f, \
    .sndparticle_l_max = 25.0f, \
    .sndparticle_potential_radius = 2, \
    .sndparticle_update_radius = 2, \
    .sndparticle_boundary = SNDPARTICLE_BOUNDARY_DELETE, \
    .sndparticle_combined_export = SNDPARTICLE_COMBINED_EXPORT_OFF, \
    .guide_alpha = 2.0f, \
    .guide_beta = 5, \
    .guide_vel_factor = 2.0f, \
    .guide_res = {0, 0, 0}, \
    .guide_source = FLUID_DOMAIN_GUIDE_SRC_DOMAIN, \
    .cache_frame_start = 1, \
    .cache_frame_end = 250, \
    .cache_frame_pause_data = 0, \
    .cache_frame_pause_noise = 0, \
    .cache_frame_pause_mesh = 0, \
    .cache_frame_pause_particles = 0, \
    .cache_frame_pause_guide = 0, \
    .cache_frame_offset = 0, \
    .cache_flag = 0, \
    .cache_mesh_format = FLUID_DOMAIN_FILE_BIN_OBJECT, \
    .cache_data_format = FLUID_DOMAIN_FILE_OPENVDB, \
    .cache_particle_format = FLUID_DOMAIN_FILE_OPENVDB, \
    .cache_noise_format = FLUID_DOMAIN_FILE_OPENVDB, \
    .cache_directory = "", \
    .error = "", \
    .cache_type = FLUID_DOMAIN_CACHE_REPLAY, \
    .cache_id = "", \
    .dt = 0.0f, \
    .time_total = 0.0f, \
    .time_per_frame = 0.0f, \
    .frame_length = 0.0f, \
    .time_scale = 1.0f, \
    .cfl_condition = 4.0f, \
    .timesteps_minimum = 1, \
    .timesteps_maximum = 4, \
    .slice_per_voxel = 5.0f, \
    .slice_depth = 0.5f, \
    .display_thickness = 1.0f, \
    .grid_scale = 1.0f, \
    .coba = NULL, \
    .vector_scale = 1.0f, \
    .gridlines_lower_bound = 0.0f, \
    .gridlines_upper_bound = 1.0f, \
    .gridlines_range_color = {1.0f, 0.0f, 0.0f, 1.0f}, \
    .axis_slice_method = AXIS_SLICE_FULL, \
    .slice_axis = 0, \
    .show_gridlines = false, \
    .draw_velocity = false, \
    .vector_draw_type = VECTOR_DRAW_NEEDLE, \
    .vector_field = FLUID_DOMAIN_VECTOR_FIELD_VELOCITY, \
    .vector_scale_with_magnitude = true, \
    .vector_draw_mac_components = VECTOR_DRAW_MAC_X | VECTOR_DRAW_MAC_Y | VECTOR_DRAW_MAC_Z, \
    .use_coba = false, \
    .coba_field = FLUID_DOMAIN_FIELD_DENSITY, \
    .interp_method = FLUID_DISPLAY_INTERP_LINEAR, \
    .gridlines_color_field = 0, \
    .gridlines_cell_filter = FLUID_CELL_TYPE_NONE, \
    .openvdb_compression = VDB_COMPRESSION_BLOSC, \
    .clipping = 1e-6f, \
    .openvdb_data_depth = 0, \
    .viewsettings = 0, \
    .point_cache = {NULL, NULL}, /* Use #BKE_ptcache_add. */ \
    .ptcaches = {{NULL}}, \
    .cache_comp = SM_CACHE_LIGHT, \
    .cache_high_comp = SM_CACHE_LIGHT, \
    .cache_file_format = 0, \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fluid Flow Settings Struct
 * \{ */

#define _DNA_DEFAULT_FluidFlowSettings \
  { \
    .fmd = NULL, \
    .mesh = NULL, \
    .psys = NULL, \
    .noise_texture = NULL, \
    .verts_old = NULL, \
    .numverts = 0, \
    .vel_multi = 1.0f, \
    .vel_normal = 0.0f, \
    .vel_random = 0.0f, \
    .vel_coord = {0.0f, 0.0f, 0.0f}, \
    .density = 1.0f, \
    .color = {0.7f, 0.7f, 0.7f}, \
    .fuel_amount = 1.0f, \
    .temperature = 1.0f, \
    .volume_density = 0.0f, \
    .surface_distance = 1.5f, \
    .particle_size = 1.0f, \
    .subframes = 0, \
    .texture_size = 1.0f, \
    .texture_offset = 0.0f, \
    .uvlayer_name = "", \
    .vgroup_density = 0, \
    .type = FLUID_FLOW_TYPE_SMOKE, \
    .behavior = FLUID_FLOW_BEHAVIOR_GEOMETRY, \
    .source = FLUID_FLOW_SOURCE_MESH, \
    .texture_type = 0, \
    .flags = FLUID_FLOW_ABSOLUTE | FLUID_FLOW_USE_PART_SIZE | FLUID_FLOW_USE_INFLOW, \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name Fluid Effector Settings Struct
 * \{ */

#define _DNA_DEFAULT_FluidEffectorSettings \
  { \
    .fmd = NULL, \
    .mesh = NULL, \
    .verts_old = NULL, \
    .numverts = 0, \
    .surface_distance = 0.0f, \
    .flags = FLUID_EFFECTOR_USE_EFFEC, \
    .subframes = 0, \
    .type = FLUID_EFFECTOR_TYPE_COLLISION, \
    .vel_multi = 1.0f, \
    .guide_mode = FLUID_EFFECTOR_GUIDE_OVERRIDE, \
  }

/** \} */

/* clang-format on */
