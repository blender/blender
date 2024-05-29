/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Shared
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_data)
    .define("SPHERE_PROBE")
    .uniform_buf(SPHERE_PROBE_BUF_SLOT,
                 "SphereProbeData",
                 "lightprobe_sphere_buf[SPHERE_PROBE_MAX]")
    .sampler(SPHERE_PROBE_TEX_SLOT, ImageType::FLOAT_2D_ARRAY, "lightprobe_spheres_tx");

/* Sample cubemap and remap into an octahedral texture. */
GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_remap)
    .local_group_size(SPHERE_PROBE_REMAP_GROUP_SIZE, SPHERE_PROBE_REMAP_GROUP_SIZE)
    .specialization_constant(Type::BOOL, "extract_sh", true)
    .specialization_constant(Type::BOOL, "extract_sun", true)
    .push_constant(Type::IVEC4, "probe_coord_packed")
    .push_constant(Type::IVEC4, "write_coord_packed")
    .push_constant(Type::IVEC4, "world_coord_packed")
    .sampler(0, ImageType::FLOAT_CUBE, "cubemap_tx")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "atlas_tx")
    .storage_buf(0, Qualifier::WRITE, "SphereProbeHarmonic", "out_sh[SPHERE_PROBE_MAX_HARMONIC]")
    .storage_buf(1, Qualifier::WRITE, "SphereProbeSunLight", "out_sun[SPHERE_PROBE_MAX_HARMONIC]")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D_ARRAY, "atlas_img")
    .compute_source("eevee_lightprobe_sphere_remap_comp.glsl")
    .additional_info("eevee_shared", "eevee_global_ubo")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_irradiance)
    .local_group_size(SPHERE_PROBE_SH_GROUP_SIZE)
    .push_constant(Type::IVEC3, "probe_remap_dispatch_size")
    .storage_buf(0, Qualifier::READ, "SphereProbeHarmonic", "in_sh[SPHERE_PROBE_MAX_HARMONIC]")
    .storage_buf(1, Qualifier::WRITE, "SphereProbeHarmonic", "out_sh")
    .additional_info("eevee_shared")
    .do_static_compilation(true)
    .compute_source("eevee_lightprobe_sphere_irradiance_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_sunlight)
    .local_group_size(SPHERE_PROBE_SH_GROUP_SIZE)
    .push_constant(Type::IVEC3, "probe_remap_dispatch_size")
    .storage_buf(0, Qualifier::READ, "SphereProbeSunLight", "in_sun[SPHERE_PROBE_MAX_HARMONIC]")
    .storage_buf(1, Qualifier::WRITE, "LightData", "sunlight_buf")
    .additional_info("eevee_shared")
    .do_static_compilation(true)
    .compute_source("eevee_lightprobe_sphere_sunlight_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_select)
    .local_group_size(SPHERE_PROBE_SELECT_GROUP_SIZE)
    .storage_buf(0,
                 Qualifier::READ_WRITE,
                 "SphereProbeData",
                 "lightprobe_sphere_buf[SPHERE_PROBE_MAX]")
    .push_constant(Type::INT, "lightprobe_sphere_count")
    .additional_info("eevee_shared",
                     "eevee_sampling_data",
                     "eevee_global_ubo",
                     "eevee_volume_probe_data")
    .compute_source("eevee_lightprobe_sphere_select_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_convolve)
    .local_group_size(SPHERE_PROBE_GROUP_SIZE, SPHERE_PROBE_GROUP_SIZE)
    .additional_info("eevee_shared")
    .push_constant(Type::IVEC4, "probe_coord_packed")
    .push_constant(Type::IVEC4, "write_coord_packed")
    .push_constant(Type::IVEC4, "read_coord_packed")
    .push_constant(Type::INT, "read_lod")
    .sampler(0, ImageType::FLOAT_CUBE, "cubemap_tx")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "in_atlas_mip_tx")
    .image(1, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D_ARRAY, "out_atlas_mip_img")
    .compute_source("eevee_lightprobe_sphere_convolve_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_INTERFACE_INFO(eevee_display_lightprobe_sphere_iface, "")
    .smooth(Type::VEC3, "P")
    .smooth(Type::VEC2, "lP")
    .flat(Type::INT, "probe_index");

GPU_SHADER_CREATE_INFO(eevee_display_lightprobe_sphere)
    .additional_info("eevee_shared", "draw_view", "eevee_lightprobe_sphere_data")
    .storage_buf(0, Qualifier::READ, "SphereProbeDisplayData", "display_data_buf[]")
    .vertex_source("eevee_display_lightprobe_sphere_vert.glsl")
    .vertex_out(eevee_display_lightprobe_sphere_iface)
    .fragment_source("eevee_display_lightprobe_sphere_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .do_static_compilation(true);

GPU_SHADER_INTERFACE_INFO(eevee_display_lightprobe_planar_iface, "")
    .flat(Type::VEC3, "probe_normal")
    .flat(Type::INT, "probe_index");

GPU_SHADER_CREATE_INFO(eevee_display_lightprobe_planar)
    .push_constant(Type::IVEC4, "world_coord_packed")
    .additional_info("eevee_shared",
                     "draw_view",
                     "eevee_lightprobe_planar_data",
                     "eevee_lightprobe_sphere_data")
    .storage_buf(0, Qualifier::READ, "PlanarProbeDisplayData", "display_data_buf[]")
    .vertex_source("eevee_display_lightprobe_planar_vert.glsl")
    .vertex_out(eevee_display_lightprobe_planar_iface)
    .fragment_source("eevee_display_lightprobe_planar_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .do_static_compilation(true);

/** \} */
