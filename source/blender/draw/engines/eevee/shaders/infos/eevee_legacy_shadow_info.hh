/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* EEVEE_shaders_shadow_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_shader_shadow)
    .additional_info("draw_view")
    .additional_info("draw_modelmat")
    .additional_info("draw_curves_infos")
    .additional_info("eevee_legacy_hair_lib")
    .additional_info("eevee_legacy_surface_lib_common")
    .additional_info("eevee_legacy_surface_lib_hair")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    .vertex_source("shadow_vert.glsl")
    .fragment_source("shadow_frag.glsl")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_shadow_accum_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_shader_shadow_accum)
    .additional_info("draw_fullscreen")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_lights_lib")
    .fragment_source("shadow_accum_frag.glsl")
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .fragment_out(0, Type::VEC4, "fragColor")
    .auto_resource_location(true)
    .do_static_compilation(true);
