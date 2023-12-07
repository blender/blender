/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

#define image_out(slot, format, name) \
  image(slot, format, Qualifier::WRITE, ImageType::FLOAT_2D, name, Frequency::PASS)
#define image_in(slot, format, name) \
  image(slot, format, Qualifier::READ, ImageType::FLOAT_2D, name, Frequency::PASS)
#define image_array_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D_ARRAY, name, Frequency::PASS)

GPU_SHADER_CREATE_INFO(eevee_gbuffer_data)
    .sampler(8, ImageType::UINT_2D, "gbuf_header_tx")
    .sampler(9, ImageType::FLOAT_2D_ARRAY, "gbuf_closure_tx")
    .sampler(10, ImageType::FLOAT_2D_ARRAY, "gbuf_color_tx");

GPU_SHADER_CREATE_INFO(eevee_deferred_tile_classify)
    .fragment_source("eevee_deferred_tile_classify_frag.glsl")
    /* Early fragment test is needed to avoid processing background fragments. */
    .early_fragment_test(true)
    .additional_info("eevee_shared", "draw_fullscreen")
    .subpass_in(1, Type::UINT, "in_gbuffer_header", DEFERRED_GBUFFER_ROG_ID)
    .typedef_source("draw_shader_shared.h")
    .image(0, GPU_R8UI, Qualifier::WRITE, ImageType::UINT_2D_ARRAY, "tile_mask_img")
    .push_constant(Type::INT, "closure_tile_size_shift")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_tile_compact)
    .additional_info("eevee_shared")
    .typedef_source("draw_shader_shared.h")
    .vertex_source("eevee_deferred_tile_compact_vert.glsl")
    /* Reuse dummy stencil frag. */
    .fragment_source("eevee_deferred_tile_stencil_frag.glsl")
    .storage_buf(0, Qualifier::READ_WRITE, "DrawCommand", "closure_single_draw_buf")
    .storage_buf(1, Qualifier::READ_WRITE, "DrawCommand", "closure_double_draw_buf")
    .storage_buf(2, Qualifier::READ_WRITE, "DrawCommand", "closure_triple_draw_buf")
    .storage_buf(3, Qualifier::WRITE, "uint", "closure_single_tile_buf[]")
    .storage_buf(4, Qualifier::WRITE, "uint", "closure_double_tile_buf[]")
    .storage_buf(5, Qualifier::WRITE, "uint", "closure_triple_tile_buf[]")
    .sampler(0, ImageType::UINT_2D_ARRAY, "tile_mask_tx")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_tile_stencil)
    .vertex_source("eevee_deferred_tile_stencil_vert.glsl")
    .fragment_source("eevee_deferred_tile_stencil_frag.glsl")
    .additional_info("eevee_shared")
    /* Only for texture size. */
    .sampler(0, ImageType::FLOAT_2D, "direct_radiance_tx")
    .storage_buf(4, Qualifier::READ, "uint", "closure_tile_buf[]")
    .push_constant(Type::INT, "closure_tile_size_shift")
    .typedef_source("draw_shader_shared.h")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_light)
    .fragment_source("eevee_deferred_light_frag.glsl")
    /* Early fragment test is needed to avoid processing background fragments. */
    .early_fragment_test(true)
    /* Chaining to next pass. */
    .image_out(2, DEFERRED_RADIANCE_FORMAT, "direct_radiance_1_img")
    .image_out(3, DEFERRED_RADIANCE_FORMAT, "direct_radiance_2_img")
    .image_out(4, DEFERRED_RADIANCE_FORMAT, "direct_radiance_3_img")
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_light_data",
                     "eevee_shadow_data",
                     "eevee_hiz_data",
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
    .define("SSS_TRANSMITTANCE")
    .define("LIGHT_CLOSURE_EVAL_COUNT", "3")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_combine)
    /* Early fragment test is needed to avoid processing fragments background fragments. */
    .early_fragment_test(true)
    /* Inputs. */
    .image_in(2, DEFERRED_RADIANCE_FORMAT, "direct_radiance_1_img")
    .image_in(3, DEFERRED_RADIANCE_FORMAT, "direct_radiance_2_img")
    .image_in(4, DEFERRED_RADIANCE_FORMAT, "direct_radiance_3_img")
    .image_in(5, RAYTRACE_RADIANCE_FORMAT, "indirect_diffuse_img")
    .image_in(6, RAYTRACE_RADIANCE_FORMAT, "indirect_reflect_img")
    .image_in(7, RAYTRACE_RADIANCE_FORMAT, "indirect_refract_img")
    .fragment_out(0, Type::VEC4, "out_combined")
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_render_pass_out",
                     "draw_fullscreen")
    .fragment_source("eevee_deferred_combine_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_deferred_capture_eval)
    /* Early fragment test is needed to avoid processing fragments without correct GBuffer data. */
    .early_fragment_test(true)
    /* Inputs. */
    .fragment_out(0, Type::VEC4, "out_radiance")
    .define("SSS_TRANSMITTANCE")
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
    .define("REFLECTION_PROBE")
    .define("SSS_TRANSMITTANCE")
    .define("LIGHT_CLOSURE_EVAL_COUNT", "2")
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
