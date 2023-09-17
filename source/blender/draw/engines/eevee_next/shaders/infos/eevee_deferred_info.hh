/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

#define image_out(slot, format, name) \
  image(slot, format, Qualifier::WRITE, ImageType::FLOAT_2D, name, Frequency::PASS)
#define image_in(slot, format, name) \
  image(slot, format, Qualifier::READ, ImageType::FLOAT_2D, name, Frequency::PASS)

GPU_SHADER_CREATE_INFO(eevee_deferred_light)
    .fragment_source("eevee_deferred_light_frag.glsl")
    /* Early fragment test is needed to avoid processing fragments without correct GBuffer data. */
    .early_fragment_test(true)
    /* Inputs. */
    .sampler(0, ImageType::FLOAT_2D_ARRAY, "gbuffer_closure_tx")
    /* Chaining to next pass. */
    /* TODO(@fclem): These could use the sub-pass feature. */
    .image_out(2, GPU_RGBA16F, "direct_diffuse_img")
    .image_out(3, GPU_RGBA16F, "direct_reflect_img")
    .image_out(4, GPU_RGBA16F, "direct_refract_img")
    .define("SSS_TRANSMITTANCE")
    .additional_info("eevee_shared",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_light_data",
                     "eevee_shadow_data",
                     "eevee_hiz_data",
                     "eevee_render_pass_out",
                     "draw_view",
                     "draw_fullscreen")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_combine)
    /* Early fragment test is needed to avoid processing fragments without correct GBuffer data. */
    .early_fragment_test(true)
    /* Inputs. */
    .image_in(2, GPU_RGBA16F, "direct_diffuse_img")
    .image_in(3, GPU_RGBA16F, "direct_reflect_img")
    .image_in(4, GPU_RGBA16F, "direct_refract_img")
    .image_in(5, RAYTRACE_RADIANCE_FORMAT, "indirect_diffuse_img")
    .image_in(6, RAYTRACE_RADIANCE_FORMAT, "indirect_reflect_img")
    .image_in(7, RAYTRACE_RADIANCE_FORMAT, "indirect_refract_img")
    .sampler(0, ImageType::FLOAT_2D_ARRAY, "gbuffer_closure_tx")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "gbuffer_color_tx")
    .fragment_out(0, Type::VEC4, "out_combined")
    .additional_info("eevee_shared", "eevee_render_pass_out", "draw_fullscreen")
    .fragment_source("eevee_deferred_combine_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_capture_eval)
    /* Early fragment test is needed to avoid processing fragments without correct GBuffer data. */
    .early_fragment_test(true)
    /* Inputs. */
    .sampler(0, ImageType::FLOAT_2D_ARRAY, "gbuffer_closure_tx")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "gbuffer_color_tx")
    .fragment_out(0, Type::VEC4, "out_radiance")
    .define("SSS_TRANSMITTANCE")
    .additional_info("eevee_shared",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_light_data",
                     "eevee_lightprobe_data",
                     "eevee_shadow_data",
                     "eevee_hiz_data",
                     "draw_view",
                     "draw_fullscreen")
    .fragment_source("eevee_deferred_capture_frag.glsl")
    .do_static_compilation(true);

#undef image_out
#undef image_in
