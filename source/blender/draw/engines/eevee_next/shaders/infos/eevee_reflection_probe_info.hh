/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Shared
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_reflection_probe_data)
    .define("SPHERE_PROBE")
    .uniform_buf(SPHERE_PROBE_BUF_SLOT,
                 "SphereProbeData",
                 "reflection_probe_buf[SPHERE_PROBE_MAX]")
    .sampler(SPHERE_PROBE_TEX_SLOT, ImageType::FLOAT_2D_ARRAY, "reflection_probes_tx");

/* Sample cubemap and remap into an octahedral texture. */
GPU_SHADER_CREATE_INFO(eevee_reflection_probe_remap)
    .local_group_size(SPHERE_PROBE_GROUP_SIZE, SPHERE_PROBE_GROUP_SIZE)
    .push_constant(Type::IVEC4, "probe_coord_packed")
    .push_constant(Type::IVEC4, "write_coord_packed")
    .push_constant(Type::IVEC4, "world_coord_packed")
    .push_constant(Type::FLOAT, "probe_brightness_clamp")
    .sampler(0, ImageType::FLOAT_CUBE, "cubemap_tx")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "atlas_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D_ARRAY, "atlas_img")
    .compute_source("eevee_reflection_probe_remap_comp.glsl")
    .additional_info("eevee_shared")
    .do_static_compilation(true);

/* Extract spherical harmonics band L0 + L1 from octahedral mapped reflection probe and update the
 * world brick of the irradiance cache. */
GPU_SHADER_CREATE_INFO(eevee_reflection_probe_update_irradiance)
    .local_group_size(SPHERE_PROBE_SH_GROUP_SIZE, 1)
    .define("SPHERE_PROBE")
    .push_constant(Type::IVEC4, "world_coord_packed")
    .sampler(0, ImageType::FLOAT_2D_ARRAY, "reflection_probes_tx")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "irradiance_atlas_img")
    .additional_info("eevee_shared")
    .compute_source("eevee_reflection_probe_update_irradiance_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_reflection_probe_select)
    .local_group_size(SPHERE_PROBE_SELECT_GROUP_SIZE)
    .storage_buf(0,
                 Qualifier::READ_WRITE,
                 "SphereProbeData",
                 "reflection_probe_buf[SPHERE_PROBE_MAX]")
    .push_constant(Type::INT, "reflection_probe_count")
    .additional_info("eevee_shared", "eevee_sampling_data", "eevee_volume_probe_data")
    .compute_source("eevee_reflection_probe_select_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_reflection_probe_convolve)
    .local_group_size(SPHERE_PROBE_GROUP_SIZE, SPHERE_PROBE_GROUP_SIZE)
    .additional_info("eevee_shared")
    .push_constant(Type::IVEC4, "probe_coord_packed")
    .push_constant(Type::IVEC4, "write_coord_packed")
    .push_constant(Type::IVEC4, "read_coord_packed")
    .push_constant(Type::INT, "read_lod")
    .sampler(0, ImageType::FLOAT_CUBE, "cubemap_tx")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "in_atlas_mip_tx")
    .image(1, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D_ARRAY, "out_atlas_mip_img")
    .compute_source("eevee_reflection_probe_convolve_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_INTERFACE_INFO(eevee_display_probe_reflection_iface, "")
    .smooth(Type::VEC2, "lP")
    .flat(Type::INT, "probe_index");

GPU_SHADER_CREATE_INFO(eevee_display_probe_reflection)
    .additional_info("eevee_shared", "draw_view", "eevee_reflection_probe_data")
    .storage_buf(0, Qualifier::READ, "SphereProbeDisplayData", "display_data_buf[]")
    .vertex_source("eevee_display_probe_reflection_vert.glsl")
    .vertex_out(eevee_display_probe_reflection_iface)
    .fragment_source("eevee_display_probe_reflection_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .do_static_compilation(true);

GPU_SHADER_INTERFACE_INFO(eevee_display_probe_planar_iface, "").flat(Type::INT, "probe_index");

GPU_SHADER_CREATE_INFO(eevee_display_probe_planar)
    .additional_info("eevee_shared", "draw_view", "eevee_lightprobe_planar_data")
    .storage_buf(0, Qualifier::READ, "PlanarProbeDisplayData", "display_data_buf[]")
    .vertex_source("eevee_display_probe_planar_vert.glsl")
    .vertex_out(eevee_display_probe_planar_iface)
    .fragment_source("eevee_display_probe_planar_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .do_static_compilation(true);

/** \} */
