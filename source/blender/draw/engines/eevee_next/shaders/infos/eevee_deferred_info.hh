/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

#define image_out(slot, format, name) \
  image(slot, format, Qualifier::WRITE, ImageType::FLOAT_2D, name, Frequency::PASS)
#define uimage_out(slot, format, name) \
  image(slot, format, Qualifier::WRITE, ImageType::UINT_2D, name, Frequency::PASS)
#define image_in(slot, format, name) \
  image(slot, format, Qualifier::READ, ImageType::FLOAT_2D, name, Frequency::PASS)
#define image_array_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D_ARRAY, name, Frequency::PASS)

/* -------------------------------------------------------------------- */
/** \name Thickness Amend
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_deferred_thickness_amend)
    .do_static_compilation(true)
    .define("GBUFFER_LOAD")
    .sampler(0, ImageType::UINT_2D, "gbuf_header_tx")
    .image(0, GPU_RG16, Qualifier::READ_WRITE, ImageType::FLOAT_2D_ARRAY, "gbuf_normal_img")
    /* Early fragment test is needed to discard fragment that do not need this processing. */
    .early_fragment_test(true)
    .fragment_source("eevee_deferred_thickness_amend_frag.glsl")
    .additional_info("draw_view",
                     "draw_fullscreen",
                     "eevee_sampling_data",
                     "eevee_shared",
                     "eevee_light_data",
                     "eevee_shadow_data",
                     "eevee_hiz_data");

/** \} */

GPU_SHADER_CREATE_INFO(eevee_gbuffer_data)
    .define("GBUFFER_LOAD")
    .sampler(12, ImageType::UINT_2D, "gbuf_header_tx")
    .sampler(13, ImageType::FLOAT_2D_ARRAY, "gbuf_closure_tx")
    .sampler(14, ImageType::FLOAT_2D_ARRAY, "gbuf_normal_tx");

GPU_SHADER_CREATE_INFO(eevee_deferred_tile_classify)
    .fragment_source("eevee_deferred_tile_classify_frag.glsl")
    .additional_info("eevee_shared", "draw_fullscreen")
    .subpass_in(1, Type::UINT, "in_gbuffer_header", DEFERRED_GBUFFER_ROG_ID)
    .typedef_source("draw_shader_shared.hh")
    .push_constant(Type::INT, "current_bit")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_light)
    .fragment_source("eevee_deferred_light_frag.glsl")
    /* Early fragment test is needed to avoid processing background fragments. */
    .early_fragment_test(true)
    .fragment_out(0, Type::VEC4, "out_combined")
    /* Chaining to next pass. */
    .uimage_out(2, DEFERRED_RADIANCE_FORMAT, "direct_radiance_1_img")
    .uimage_out(3, DEFERRED_RADIANCE_FORMAT, "direct_radiance_2_img")
    .uimage_out(4, DEFERRED_RADIANCE_FORMAT, "direct_radiance_3_img")
    /* Optimized out if use_split_indirect is false. */
    .image_out(5, RAYTRACE_RADIANCE_FORMAT, "indirect_radiance_1_img")
    .image_out(6, RAYTRACE_RADIANCE_FORMAT, "indirect_radiance_2_img")
    .image_out(7, RAYTRACE_RADIANCE_FORMAT, "indirect_radiance_3_img")
    .specialization_constant(Type::BOOL, "use_split_indirect", false)
    .specialization_constant(Type::BOOL, "use_lightprobe_eval", false)
    .specialization_constant(Type::BOOL, "use_transmission", false)
    .specialization_constant(Type::INT, "render_pass_shadow_id", -1)
    .define("SPECIALIZED_SHADOW_PARAMS")
    .specialization_constant(Type::INT, "shadow_ray_count", 1)
    .specialization_constant(Type::INT, "shadow_ray_step_count", 6)
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_light_data",
                     "eevee_shadow_data",
                     "eevee_hiz_data",
                     "eevee_lightprobe_data",
                     "eevee_render_pass_out",
                     "draw_fullscreen",
                     "draw_view");

GPU_SHADER_CREATE_INFO(eevee_deferred_light_single)
    .additional_info("eevee_deferred_light")
    .define("LIGHT_CLOSURE_EVAL_COUNT", "1")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_light_double)
    .additional_info("eevee_deferred_light")
    .define("LIGHT_CLOSURE_EVAL_COUNT", "2")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_light_triple)
    .additional_info("eevee_deferred_light")
    .define("LIGHT_CLOSURE_EVAL_COUNT", "3")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_combine)
    /* Early fragment test is needed to avoid processing fragments background fragments. */
    .early_fragment_test(true)
    /* Inputs. */
    .sampler(2, ImageType::UINT_2D, "direct_radiance_1_tx")
    .sampler(3, ImageType::UINT_2D, "direct_radiance_2_tx")
    .sampler(4, ImageType::UINT_2D, "direct_radiance_3_tx")
    .sampler(5, ImageType::FLOAT_2D, "indirect_radiance_1_tx")
    .sampler(6, ImageType::FLOAT_2D, "indirect_radiance_2_tx")
    .sampler(7, ImageType::FLOAT_2D, "indirect_radiance_3_tx")
    .image(5, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "radiance_feedback_img")
    .fragment_out(0, Type::VEC4, "out_combined")
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_render_pass_out",
                     "draw_fullscreen")
    .fragment_source("eevee_deferred_combine_frag.glsl")
    /* NOTE: Both light IDs have a valid specialized assignment of '-1' so only when default is
     * present will we instead dynamically look-up ID from the uniform buffer. */
    .specialization_constant(Type::BOOL, "render_pass_diffuse_light_enabled", true)
    .specialization_constant(Type::BOOL, "render_pass_specular_light_enabled", true)
    .specialization_constant(Type::BOOL, "render_pass_normal_enabled", true)
    .specialization_constant(Type::BOOL, "use_radiance_feedback", false)
    .specialization_constant(Type::BOOL, "use_split_radiance", false)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_capture_eval)
    /* Early fragment test is needed to avoid processing fragments without correct GBuffer data. */
    .early_fragment_test(true)
    /* Inputs. */
    .fragment_out(0, Type::VEC4, "out_radiance")
    .define("LIGHT_CLOSURE_EVAL_COUNT", "1")
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_light_data",
                     "eevee_shadow_data",
                     "eevee_hiz_data",
                     "eevee_volume_probe_data",
                     "draw_view",
                     "draw_fullscreen")
    .fragment_source("eevee_deferred_capture_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_planar_eval)
    /* Early fragment test is needed to avoid processing fragments without correct GBuffer data. */
    .early_fragment_test(true)
    /* Inputs. */
    .fragment_out(0, Type::VEC4, "out_radiance")
    .define("SPHERE_PROBE")
    .define("LIGHT_CLOSURE_EVAL_COUNT", "1")
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_light_data",
                     "eevee_lightprobe_data",
                     "eevee_shadow_data",
                     "eevee_hiz_data",
                     "draw_view",
                     "draw_fullscreen")
    .fragment_source("eevee_deferred_planar_frag.glsl")
    .do_static_compilation(true);

#undef image_array_out
#undef image_out
#undef image_in

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_debug_gbuffer)
    .do_static_compilation(true)
    .fragment_out(0, Type::VEC4, "out_color_add", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_color_mul", DualBlend::SRC_1)
    .push_constant(Type::INT, "debug_mode")
    .fragment_source("eevee_debug_gbuffer_frag.glsl")
    .additional_info("draw_view", "draw_fullscreen", "eevee_shared", "eevee_gbuffer_data");

/** \} */
