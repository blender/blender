/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* effect_minmaxz_frag permutation inputs. */
GPU_SHADER_CREATE_INFO(eevee_legacy_minmaxz_common)
    .additional_info("draw_fullscreen")
    .fragment_source("effect_minmaxz_frag.glsl")
    .fragment_out(0, Type::VEC4, "fragColor") /* Needed by certain drivers. */
    .depth_write(DepthWrite::ANY);

GPU_SHADER_CREATE_INFO(eevee_legacy_minmaxz_layered_common)
    .define("LAYERED")
    .sampler(0, ImageType::DEPTH_2D_ARRAY, "depthBuffer")
    .push_constant(Type::INT, "depthLayer");

GPU_SHADER_CREATE_INFO(eevee_legacy_minmaxz_non_layered_common)
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer");

GPU_SHADER_CREATE_INFO(eevee_legacy_minmaxz_non_copy).push_constant(Type::VEC2, "texelSize");

GPU_SHADER_CREATE_INFO(eevee_legacy_minmaxz_copy).define("COPY_DEPTH");

/* Permutations. */
GPU_SHADER_CREATE_INFO(eevee_legacy_minz_downlevel)
    .define("MIN_PASS")
    .additional_info("eevee_legacy_minmaxz_common")
    .additional_info("eevee_legacy_minmaxz_non_layered_common")
    .additional_info("eevee_legacy_minmaxz_non_copy")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_maxz_downlevel)
    .define("MAX_PASS")
    .additional_info("eevee_legacy_minmaxz_common")
    .additional_info("eevee_legacy_minmaxz_non_layered_common")
    .additional_info("eevee_legacy_minmaxz_non_copy")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_minz_downdepth)
    .define("MIN_PASS")
    .additional_info("eevee_legacy_minmaxz_common")
    .additional_info("eevee_legacy_minmaxz_non_layered_common")
    .additional_info("eevee_legacy_minmaxz_non_copy")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_maxz_downdepth)
    .define("MAX_PASS")
    .additional_info("eevee_legacy_minmaxz_common")
    .additional_info("eevee_legacy_minmaxz_non_layered_common")
    .additional_info("eevee_legacy_minmaxz_non_copy")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_minz_downdepth_layer)
    .define("MIN_PASS")
    .additional_info("eevee_legacy_minmaxz_layered_common")
    .additional_info("eevee_legacy_minmaxz_common")
    .additional_info("eevee_legacy_minmaxz_non_copy")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_maxz_downdepth_layer)
    .define("MAX_PASS")
    .additional_info("eevee_legacy_minmaxz_layered_common")
    .additional_info("eevee_legacy_minmaxz_common")
    .additional_info("eevee_legacy_minmaxz_non_copy")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_maxz_copydepth_layer)
    .define("MAX_PASS")
    .additional_info("eevee_legacy_minmaxz_copy")
    .additional_info("eevee_legacy_minmaxz_layered_common")
    .additional_info("eevee_legacy_minmaxz_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_minz_copydepth)
    .define("MIN_PASS")
    .additional_info("eevee_legacy_minmaxz_copy")
    .additional_info("eevee_legacy_minmaxz_common")
    .additional_info("eevee_legacy_minmaxz_non_layered_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_maxz_copydepth)
    .define("MAX_PASS")
    .additional_info("eevee_legacy_minmaxz_copy")
    .additional_info("eevee_legacy_minmaxz_common")
    .additional_info("eevee_legacy_minmaxz_non_layered_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_update_noise_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_update_noise)
    .sampler(0, ImageType::FLOAT_2D, "blueNoise")
    .push_constant(Type::VEC3, "offsets")
    .fragment_out(0, Type::VEC4, "FragColor")
    .additional_info("draw_fullscreen")
    .fragment_source("update_noise_frag.glsl")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_taa_resolve_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_taa_resolve)
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::FLOAT_2D, "colorHistoryBuffer")
    .fragment_out(0, Type::VEC4, "FragColor")
    .additional_info("draw_fullscreen")
    .additional_info("draw_view")
    .fragment_source("effect_temporal_aa.glsl")
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_taa_resolve_basic)
    .push_constant(Type::FLOAT, "alpha")
    .additional_info("eevee_legacy_taa_resolve")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_taa_resolve_reprojection)
    .define("USE_REPROJECTION")
    .sampler(2, ImageType::DEPTH_2D, "depthBuffer")
    .push_constant(Type::MAT4, "prevViewProjectionMatrix")
    .additional_info("eevee_legacy_taa_resolve")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_velocity_resolve_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_velocity_resolve)
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .push_constant(Type::MAT4, "prevViewProjMatrix")
    .push_constant(Type::MAT4, "currViewProjMatrixInv")
    .push_constant(Type::MAT4, "nextViewProjMatrix")
    .fragment_out(0, Type::VEC4, "outData")
    .additional_info("draw_fullscreen")
    .fragment_source("effect_velocity_resolve_frag.glsl")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_effect_downsample_sh_get */

GPU_SHADER_CREATE_INFO(eevee_legacy_downsample_shared)
    .additional_info("draw_fullscreen")
    .sampler(0, ImageType::FLOAT_2D, "source")
    .push_constant(Type::FLOAT, "fireflyFactor")
    .fragment_out(0, Type::VEC4, "FragColor")
    .fragment_source("effect_downsample_frag.glsl")
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_downsample)
    .additional_info("eevee_legacy_downsample_shared")
    .push_constant(Type::VEC2, "texelSize")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_effect_color_copy_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_color_copy)
    .define("COPY_SRC")
    .additional_info("eevee_legacy_downsample_shared")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_effect_ambient_occlusion_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_ambient_occlusion)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_ambient_occlusion_lib")
    .additional_info("draw_fullscreen")
    .sampler(0, ImageType::FLOAT_2D, "normalBuffer")
    .push_constant(Type::FLOAT, "fireflyFactor")
    .fragment_out(0, Type::VEC4, "FragColor")
    .fragment_source("effect_gtao_frag.glsl")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_effect_ambient_occlusion_debug_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_ambient_occlusion_debug)
    .define("DEBUG_AO")
    .define("ENABLE_DEFERED_AO")
    .additional_info("eevee_legacy_ambient_occlusion")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_effect_reflection_trace_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_effect_reflection_trace)
    .additional_info("eevee_legacy_surface_lib_step_raytrace")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_raytrace_lib")
    .additional_info("eevee_legacy_lightprobe_lib")
    .additional_info("eevee_legacy_reflection_lib")
    .additional_info("draw_fullscreen")
    .sampler(0, ImageType::FLOAT_2D, "normalBuffer")
    .sampler(1, ImageType::FLOAT_2D, "specroughBuffer")
    .push_constant(Type::VEC2, "targetSize")
    .push_constant(Type::FLOAT, "randomScale")
    .fragment_out(0, Type::VEC4, "hitData")
    .fragment_out(1, Type::FLOAT, "hitDepth")
    .fragment_source("effect_reflection_trace_frag.glsl")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_effect_reflection_resolve_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_effect_reflection_resolve)
    .additional_info("eevee_legacy_surface_lib_step_resolve")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_raytrace_lib")
    .additional_info("eevee_legacy_lightprobe_lib")
    .additional_info("eevee_legacy_reflection_lib")
    .additional_info("eevee_legacy_closure_eval_glossy_lib")
    .additional_info("draw_fullscreen")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::FLOAT_2D, "normalBuffer")
    .sampler(2, ImageType::FLOAT_2D, "specroughBuffer")
    .sampler(3, ImageType::FLOAT_2D, "hitBuffer")
    .sampler(4, ImageType::FLOAT_2D, "hitDepth")
    .push_constant(Type::INT, "samplePoolOffset")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_source("effect_reflection_resolve_frag.glsl")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* Split reflection resolve support for Intel-based MacBooks. */
GPU_SHADER_CREATE_INFO(eevee_legacy_effect_reflection_resolve_probe)
    .define("RESOLVE_PROBE")
    .additional_info("eevee_legacy_effect_reflection_resolve")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_effect_reflection_resolve_ssr)
    .define("RESOLVE_SSR")
    .additional_info("eevee_legacy_effect_reflection_resolve")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_subsurface_first_pass_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_shader_effect_subsurface_common)
    .additional_info("draw_fullscreen")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_common_lib")
    .fragment_out(0, Type::VEC4, "sssRadiance")
    .fragment_source("effect_subsurface_frag.glsl")
    .uniform_buf(0, "SSSProfileBlock", "sssProfile", Frequency::PASS)
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(1, ImageType::FLOAT_2D, "sssIrradiance")
    .sampler(2, ImageType::FLOAT_2D, "sssRadius")
    .sampler(3, ImageType::FLOAT_2D, "sssAlbedo")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_shader_effect_subsurface_common_FIRST_PASS)
    .define("FIRST_PASS")
    .additional_info("eevee_legacy_shader_effect_subsurface_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_shader_effect_subsurface_common_SECOND_PASS)
    .define("SECOND_PASS")
    .additional_info("eevee_legacy_shader_effect_subsurface_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_subsurface_translucency_sh_get */

GPU_SHADER_CREATE_INFO(eevee_legacy_shader_effect_subsurface_translucency)
    .define("EEVEE_TRANSLUCENCY")
    .additional_info("draw_fullscreen")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("eevee_legacy_lights_lib")
    .fragment_source("effect_translucency_frag.glsl")
    .fragment_out(0, Type::VEC4, "FragColor")
    .sampler(1, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(1, ImageType::FLOAT_1D, "sssTexProfile")
    .sampler(1, ImageType::FLOAT_2D, "sssRadius")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "sssShadowCubes")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "sssShadowCascades")
    .uniform_buf(0, "SSSProfileBlock", "sssProfile", Frequency::PASS)
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_renderpasses_post_process_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_post_process)
    .additional_info("draw_fullscreen")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_lib")
    .fragment_source("renderpass_postprocess_frag.glsl")
    .push_constant(Type::INT, "postProcessType")
    .push_constant(Type::INT, "currentSample")
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(1, ImageType::FLOAT_2D, "inputBuffer")
    .sampler(2, ImageType::FLOAT_2D, "inputSecondLightBuffer")
    .sampler(3, ImageType::FLOAT_2D, "inputColorBuffer")
    .sampler(4, ImageType::FLOAT_2D, "inputTransmittanceBuffer")
    .fragment_out(0, Type::VEC4, "fragColor")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_renderpasses_accumulate_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_renderpass_accumulate)
    .additional_info("draw_fullscreen")
    .fragment_source("renderpass_accumulate_frag.glsl")
    .sampler(1, ImageType::FLOAT_2D, "inputBuffer")
    .fragment_out(0, Type::VEC4, "fragColor")
    .do_static_compilation(true);

/* EEVEE_shaders_effect_mist_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_effect_mist_FIRST_PASS)
    .define("FIRST_PASS")
    .additional_info("draw_fullscreen")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_lib")
    .fragment_source("effect_mist_frag.glsl")
    .push_constant(Type::VEC3, "mistSettings")
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .fragment_out(0, Type::VEC4, "fragColor")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_ggx_lut_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_ggx_lut_bsdf)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .fragment_source("bsdf_lut_frag.glsl")
    .push_constant(Type::FLOAT, "sampleCount")
    .fragment_out(0, Type::VEC2, "FragColor")
    .do_static_compilation(true);

/* EEVEE_shaders_ggx_lut_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_ggx_lut_btdf)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .fragment_source("btdf_lut_frag.glsl")
    .push_constant(Type::FLOAT, "sampleCount")
    .push_constant(Type::FLOAT, "z_factor")
    .fragment_out(0, Type::VEC4, "FragColor")
    .do_static_compilation(true);

/* Cryptomatte */
GPU_SHADER_CREATE_INFO(eevee_legacy_cryptomatte_common)
    .additional_info("eevee_legacy_closure_type_lib")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_cryptomatte_lib")
    .push_constant(Type::VEC4, "cryptohash")
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("cryptomatte_vert.glsl")
    .fragment_source("cryptomatte_frag.glsl");

GPU_SHADER_CREATE_INFO(eevee_legacy_cryptomatte_hair)
    .define("HAIR_SHADER")
    .define("NO_ATTRIB_LOAD")
    .additional_info("eevee_legacy_cryptomatte_common")
    .additional_info("eevee_legacy_mateiral_surface_vert_hair")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_cryptomatte_mesh)
    .define("MESH_SHADER")
    .define("NO_ATTRIB_LOAD")
    .additional_info("eevee_legacy_cryptomatte_common")
    .additional_info("eevee_legacy_material_surface_vert")
    .auto_resource_location(true)
    .do_static_compilation(true);
