/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

#pragma once

/* EEVEE defines. */
GPU_SHADER_CREATE_INFO(eevee_legacy_defines_info).typedef_source("engine_eevee_shared_defines.h");

/* Only specifies bindings for common_uniform_lib.glsl. */
GPU_SHADER_CREATE_INFO(eevee_legacy_common_lib)
    .typedef_source("engine_eevee_shared_defines.h")
    .typedef_source("engine_eevee_legacy_shared.h")
    .uniform_buf(1, "CommonUniformBlock", "common_block", Frequency::PASS);

/* Only specifies bindings for irradiance_lib.glsl. */
GPU_SHADER_CREATE_INFO(eevee_legacy_irradiance_lib)
    .additional_info("eevee_legacy_common_lib")
    .sampler(1, ImageType::FLOAT_2D_ARRAY, "irradianceGrid");

/* Utiltex Lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_common_utiltex_lib)
    .sampler(2, ImageType::FLOAT_2D_ARRAY, "utilTex");

/* Ray-trace lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_raytrace_lib)
    .additional_info("draw_view")
    .additional_info("eevee_legacy_common_lib")
    .sampler(3, ImageType::FLOAT_2D, "maxzBuffer")
    .sampler(4, ImageType::DEPTH_2D_ARRAY, "planarDepth");

/* Ambient occlusion lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_ambient_occlusion_lib)
    .additional_info("eevee_legacy_raytrace_lib")
    .sampler(5, ImageType::FLOAT_2D, "horizonBuffer");

/* Light-probe lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_lib)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_ambient_occlusion_lib")
    .additional_info("eevee_legacy_irradiance_lib")
    .sampler(6, ImageType::FLOAT_2D_ARRAY, "probePlanars")
    .sampler(7, ImageType::FLOAT_CUBE_ARRAY, "probeCubes")
    .uniform_buf(2, "ProbeBlock", "probe_block", Frequency::PASS)
    .uniform_buf(3, "GridBlock", "grid_block", Frequency::PASS)
    .uniform_buf(4, "PlanarBlock", "planar_block", Frequency::PASS);

/* LTC Lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_ltc_lib).additional_info("eevee_legacy_common_utiltex_lib");

/* Lights lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_lights_lib)
    .additional_info("eevee_legacy_ltc_lib")
    .additional_info("eevee_legacy_raytrace_lib")
    .uniform_buf(5, "ShadowBlock", "shadow_block", Frequency::PASS)
    .uniform_buf(6, "LightBlock", "light_block", Frequency::PASS)
    .sampler(8, ImageType::SHADOW_2D_ARRAY, "shadowCubeTexture")
    .sampler(9, ImageType::SHADOW_2D_ARRAY, "shadowCascadeTexture");

/* Hair lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_hair_lib)
    .additional_info("draw_hair")
    .sampler(10, ImageType::UINT_BUFFER, "hairStrandBuffer")
    .sampler(11, ImageType::UINT_BUFFER, "hairStrandSegBuffer");

/* SSR Lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_ssr_lib)
    .additional_info("eevee_legacy_raytrace_lib")
    .push_constant(Type::FLOAT, "refractionDepth")
    .sampler(12, ImageType::FLOAT_2D, "refractColorBuffer");

/* renderpass_lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_renderpass_lib)
    .additional_info("eevee_legacy_common_lib")
    .uniform_buf(12, "RenderpassBlock", "renderpass_block", Frequency::PASS);

/* Reflection lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_reflection_lib)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .push_constant(Type::IVEC2, "halfresOffset");

/* Volumetric lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_volumetric_lib)
    .additional_info("eevee_legacy_lights_lib")
    .additional_info("eevee_legacy_lightprobe_lib")
    .additional_info("eevee_legacy_irradiance_lib")
    .sampler(13, ImageType::FLOAT_3D, "inScattering")
    .sampler(14, ImageType::FLOAT_3D, "inTransmittance");

/* eevee_legacy_cryptomatte_lib. */
GPU_SHADER_CREATE_INFO(eevee_legacy_cryptomatte_lib).additional_info("draw_curves_infos");

/* ----- SURFACE LIB ----- */
/* Surface lib has several different components depending on how it is used.
 * Differing root permutations need to be generated and included depending
 * on use-case. */

/* SURFACE LIB INTERFACES */
GPU_SHADER_INTERFACE_INFO(eevee_legacy_surface_common_iface, "")
    .smooth(Type::VEC3, "worldPosition")
    .smooth(Type::VEC3, "viewPosition")
    .smooth(Type::VEC3, "worldNormal")
    .smooth(Type::VEC3, "viewNormal");

GPU_SHADER_INTERFACE_INFO(eevee_legacy_surface_point_cloud_iface, "")
    .smooth(Type::FLOAT, "pointRadius")
    .smooth(Type::FLOAT, "pointPosition")
    .flat(Type::INT, "pointID");

GPU_SHADER_INTERFACE_INFO(eevee_legacy_surface_hair_iface, "")
    .smooth(Type::VEC3, "hairTangent")
    .smooth(Type::FLOAT, "hairThickTime")
    .smooth(Type::FLOAT, "hairThickness")
    .smooth(Type::FLOAT, "hairTime")
    .flat(Type::INT, "hairStrandID")
    .smooth(Type::VEC2, "hairBary");

/* Surface lib components */
GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_common)
    .vertex_out(eevee_legacy_surface_common_iface);

GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_hair)
    .define("USE_SURFACE_LIB_HAIR")
    /* Hair still uses the common interface as well. */
    .additional_info("eevee_legacy_surface_lib_common")
    .vertex_out(eevee_legacy_surface_hair_iface);

GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_pointcloud)
    .define("USE_SURFACE_LIB_POINTCLOUD")
    /* Point-cloud still uses the common interface as well. */
    .additional_info("eevee_legacy_surface_lib_common")
    .vertex_out(eevee_legacy_surface_point_cloud_iface);

GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_step_resolve).define("STEP_RESOLVE");

GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_step_raytrace).define("STEP_RAYTRACE");

GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_world_background).define("WORLD_BACKGROUND");

GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_step_probe_capture).define("PROBE_CAPTURE");

GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_use_barycentrics).define("USE_BARYCENTRICS");

GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_codegen_lib).define("CODEGEN_LIB");

/* Surface lib permutations. */

/* Basic - lookdev world frag */
GPU_SHADER_CREATE_INFO(eevee_legacy_surface_lib_lookdev)
    .additional_info("eevee_legacy_surface_lib_common");

/** Closure evaluation libraries **/

/* eevee_legacy_closure_type_lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_closure_type_lib)
    .push_constant(Type::INT, "outputSsrId")
    .push_constant(Type::INT, "outputSssId");

/* eevee_legacy_closure_eval_lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_closure_eval_lib)
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_lights_lib")
    .additional_info("eevee_legacy_lightprobe_lib");

/* eevee_legacy_closure_eval_diffuse_lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_closure_eval_diffuse_lib)
    .additional_info("eevee_legacy_lights_lib")
    .additional_info("eevee_legacy_lightprobe_lib")
    .additional_info("eevee_legacy_ambient_occlusion_lib")
    .additional_info("eevee_legacy_closure_eval_lib")
    .additional_info("eevee_legacy_renderpass_lib");

/* eevee_legacy_closure_eval_glossy_lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_closure_eval_glossy_lib)
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_lights_lib")
    .additional_info("eevee_legacy_lightprobe_lib")
    .additional_info("eevee_legacy_ambient_occlusion_lib")
    .additional_info("eevee_legacy_closure_eval_lib")
    .additional_info("eevee_legacy_renderpass_lib");

/* eevee_legacy_closure_eval_refraction_lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_closure_eval_refraction_lib)
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_lights_lib")
    .additional_info("eevee_legacy_lightprobe_lib")
    .additional_info("eevee_legacy_ambient_occlusion_lib")
    .additional_info("eevee_legacy_ssr_lib")
    .additional_info("eevee_legacy_closure_eval_lib")
    .additional_info("eevee_legacy_renderpass_lib");

/* eevee_legacy_closure_eval_translucent_lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_closure_eval_translucent_lib)
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_lights_lib")
    .additional_info("eevee_legacy_lightprobe_lib")
    .additional_info("eevee_legacy_ambient_occlusion_lib")
    .additional_info("eevee_legacy_closure_eval_lib")
    .additional_info("eevee_legacy_renderpass_lib");

/* eevee_legacy_closure_eval_surface_lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_closure_eval_surface_lib)
    .additional_info("eevee_legacy_closure_eval_diffuse_lib")
    .additional_info("eevee_legacy_closure_eval_glossy_lib")
    .additional_info("eevee_legacy_closure_eval_refraction_lib")
    .additional_info("eevee_legacy_closure_eval_translucent_lib")
    .additional_info("eevee_legacy_renderpass_lib");
