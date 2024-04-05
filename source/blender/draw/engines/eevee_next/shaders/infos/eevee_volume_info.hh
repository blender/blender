/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

#pragma once

/* Used for shaders that need the final accumulated volume transmittance and scattering. */
GPU_SHADER_CREATE_INFO(eevee_volume_lib)
    .additional_info("eevee_shared", "eevee_global_ubo", "draw_view")
    .sampler(VOLUME_SCATTERING_TEX_SLOT, ImageType::FLOAT_3D, "volume_scattering_tx")
    .sampler(VOLUME_TRANSMITTANCE_TEX_SLOT, ImageType::FLOAT_3D, "volume_transmittance_tx");

GPU_SHADER_CREATE_INFO(eevee_volume_properties_data)
    .additional_info("eevee_global_ubo")
    .image(VOLUME_PROP_SCATTERING_IMG_SLOT,
           GPU_R11F_G11F_B10F,
           Qualifier::READ,
           ImageType::FLOAT_3D,
           "in_scattering_img")
    .image(VOLUME_PROP_EXTINCTION_IMG_SLOT,
           GPU_R11F_G11F_B10F,
           Qualifier::READ,
           ImageType::FLOAT_3D,
           "in_extinction_img")
    .image(VOLUME_PROP_EMISSION_IMG_SLOT,
           GPU_R11F_G11F_B10F,
           Qualifier::READ,
           ImageType::FLOAT_3D,
           "in_emission_img")
    .image(VOLUME_PROP_PHASE_IMG_SLOT,
           GPU_RG16F,
           Qualifier::READ,
           ImageType::FLOAT_3D,
           "in_phase_img");

GPU_SHADER_CREATE_INFO(eevee_volume_scatter)
    .local_group_size(VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE)
    .additional_info("eevee_shared")
    .additional_info("eevee_global_ubo")
    .additional_info("draw_resource_id_varying")
    .additional_info("draw_view")
    .additional_info("eevee_light_data")
    .additional_info("eevee_lightprobe_data")
    .additional_info("eevee_shadow_data")
    .additional_info("eevee_sampling_data")
    .additional_info("eevee_utility_texture")
    .additional_info("eevee_volume_properties_data")
    .sampler(0, ImageType::FLOAT_3D, "scattering_history_tx")
    .sampler(1, ImageType::FLOAT_3D, "extinction_history_tx")
    .image(4, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_3D, "out_scattering_img")
    .image(5, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_3D, "out_extinction_img")
    .compute_source("eevee_volume_scatter_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_volume_scatter_with_lights)
    .additional_info("eevee_volume_scatter")
    .define("VOLUME_LIGHTING")
    .define("VOLUME_IRRADIANCE")
    .define("VOLUME_SHADOW")
    .sampler(9, ImageType::FLOAT_3D, "extinction_tx")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_volume_occupancy_convert)
    .additional_info("eevee_shared", "eevee_global_ubo", "draw_fullscreen")
    .builtins(BuiltinBits::TEXTURE_ATOMIC)
    .image(VOLUME_HIT_DEPTH_SLOT, GPU_R32F, Qualifier::READ, ImageType::FLOAT_3D, "hit_depth_img")
    .image(VOLUME_HIT_COUNT_SLOT,
           GPU_R32UI,
           Qualifier::READ_WRITE,
           ImageType::UINT_2D,
           "hit_count_img")
    .image(VOLUME_OCCUPANCY_SLOT,
           GPU_R32UI,
           Qualifier::READ_WRITE,
           ImageType::UINT_3D_ATOMIC,
           "occupancy_img")
    .fragment_source("eevee_occupancy_convert_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_volume_integration)
    .additional_info("eevee_shared", "eevee_global_ubo", "draw_view")
    .additional_info("eevee_sampling_data")
    .compute_source("eevee_volume_integration_comp.glsl")
    .local_group_size(VOLUME_INTEGRATION_GROUP_SIZE, VOLUME_INTEGRATION_GROUP_SIZE, 1)
    /* Inputs. */
    .sampler(0, ImageType::FLOAT_3D, "in_scattering_tx")
    .sampler(1, ImageType::FLOAT_3D, "in_extinction_tx")
    /* Outputs. */
    .image(0, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_3D, "out_scattering_img")
    .image(1, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_3D, "out_transmittance_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_volume_resolve)
    .additional_info("eevee_shared")
    .additional_info("eevee_volume_lib")
    .additional_info("draw_fullscreen")
    .additional_info("eevee_render_pass_out")
    .additional_info("eevee_hiz_data")
    .fragment_source("eevee_volume_resolve_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_radiance", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_transmittance", DualBlend::SRC_1)
    /** TODO(Miguel Pozo): Volume RenderPasses. */
    .do_static_compilation(true);
