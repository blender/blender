/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

#define image_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D, name, Frequency::PASS)
#define image_array_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D_ARRAY, name, Frequency::PASS)

/**
 * Specific deferred pass accumulate the computed lighting to either:
 * - a split diffuse / specular temporary light buffer.
 * or to
 * - the combined pass & the light render-pass (if needed).
 *
 * This is in order to minimize the number of blending step.
 */
GPU_SHADER_CREATE_INFO(eevee_deferred_base)
    /* Early fragment test is needed to avoid processing fragments without correct GBuffer data. */
    .early_fragment_test(true)
    /* Select which output to write to. */
    .push_constant(Type::BOOL, "is_last_eval_pass")
    /* Combined pass output. */
    .fragment_out(0, Type::VEC4, "out_radiance", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_transmittance", DualBlend::SRC_1)
    /* Chaining to next pass. */
    .image_out(2, Qualifier::WRITE, GPU_RGBA16F, "out_diffuse_light_img")
    .image_out(3, Qualifier::WRITE, GPU_RGBA16F, "out_specular_light_img");

GPU_SHADER_CREATE_INFO(eevee_deferred_light_base)
    .fragment_source("eevee_deferred_light_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D_ARRAY, "gbuffer_closure_tx")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "gbuffer_color_tx")
    .image(4,
           RAYTRACE_RADIANCE_FORMAT,
           Qualifier::READ,
           ImageType::FLOAT_2D,
           "indirect_reflection_img")
    .image(5,
           RAYTRACE_RADIANCE_FORMAT,
           Qualifier::READ,
           ImageType::FLOAT_2D,
           "indirect_refraction_img")
    .define("SSS_TRANSMITTANCE")
    .additional_info("eevee_shared",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_light_data",
                     "eevee_reflection_probe_data",
                     "eevee_lightprobe_data",
                     "eevee_shadow_data",
                     "eevee_deferred_base",
                     "eevee_hiz_data",
                     "eevee_render_pass_out",
                     "draw_view",
                     "draw_fullscreen");

GPU_SHADER_CREATE_INFO(eevee_deferred_light)
    .additional_info("eevee_deferred_light_base")
    .define("DO_REFLECTION_PROBES")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_light_diffuse)
    .additional_info("eevee_deferred_light_base")
    .do_static_compilation(true);

#undef image_array_out
