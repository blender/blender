/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

#pragma once

/* Volumetric iface. */
GPU_SHADER_INTERFACE_INFO(legacy_volume_vert_geom_iface, "volumetric_vert_iface")
    .smooth(Type::VEC4, "vPos");

GPU_SHADER_INTERFACE_INFO(legacy_volume_geom_frag_iface, "volumetric_geom_iface")
    .flat(Type::INT, "slice");

/* EEVEE_shaders_volumes_clear_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_clear)
    .define("STANDALONE")
    .define("VOLUMETRICS")
    .define("CLEAR")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("draw_resource_id_varying")
    .additional_info("eevee_legacy_volumetric_lib")
    .vertex_source("volumetric_vert.glsl")
    .geometry_source("volumetric_geom.glsl")
    .fragment_source("volumetric_frag.glsl")
    .vertex_out(legacy_volume_vert_geom_iface)
    .geometry_out(legacy_volume_geom_frag_iface)
    .geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3)
    .fragment_out(0, Type::VEC4, "volumeScattering")
    .fragment_out(1, Type::VEC4, "volumeExtinction")
    .fragment_out(2, Type::VEC4, "volumeEmissive")
    .fragment_out(3, Type::VEC4, "volumePhase")
    .do_static_compilation(true)
    .auto_resource_location(true);

#ifdef WITH_METAL_BACKEND
/* Non-geometry shader equivalent for multilayered rendering.
 * NOTE: Layer selection can be done in vertex shader, and thus
 * vertex shader emits both vertex and geometry shader output
 * interfaces. */
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_clear_no_geom)
    .define("STANDALONE")
    .define("VOLUMETRICS")
    .define("CLEAR")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("draw_resource_id_varying")
    .additional_info("eevee_legacy_volumetric_lib")
    .vertex_source("volumetric_vert.glsl")
    .fragment_source("volumetric_frag.glsl")
    .vertex_out(legacy_volume_vert_geom_iface)
    .vertex_out(legacy_volume_geom_frag_iface)
    .fragment_out(0, Type::VEC4, "volumeScattering")
    .fragment_out(1, Type::VEC4, "volumeExtinction")
    .fragment_out(2, Type::VEC4, "volumeEmissive")
    .fragment_out(3, Type::VEC4, "volumePhase")
    .metal_backend_only(true)
    .do_static_compilation(true)
    .auto_resource_location(true);
#endif

/* EEVEE_shaders_volumes_scatter_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_scatter_common)
    .define("STANDALONE")
    .define("VOLUMETRICS")
    .define("VOLUME_SHADOW")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("draw_resource_id_varying")
    .additional_info("eevee_legacy_volumetric_lib")
    /* NOTE: Unique sampler IDs assigned for consistency between library includes,
     * and to avoid unique assignment collision validation error.
     * However, resources will be auto assigned locations within shader usage. */
    .sampler(15, ImageType::FLOAT_3D, "volumeScattering")
    .sampler(16, ImageType::FLOAT_3D, "volumeExtinction")
    .sampler(17, ImageType::FLOAT_3D, "volumeEmission")
    .sampler(18, ImageType::FLOAT_3D, "volumePhase")
    .sampler(19, ImageType::FLOAT_3D, "historyScattering")
    .sampler(20, ImageType::FLOAT_3D, "historyTransmittance")

    .fragment_out(0, Type::VEC4, "outScattering")
    .fragment_out(1, Type::VEC4, "outTransmittance")
    .vertex_source("volumetric_vert.glsl")
    .fragment_source("volumetric_scatter_frag.glsl")
    .vertex_out(legacy_volume_vert_geom_iface);

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_scatter)
    .additional_info("eevee_legacy_volumes_scatter_common")
    .geometry_source("volumetric_geom.glsl")
    .geometry_out(legacy_volume_geom_frag_iface)
    .geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3)
    .do_static_compilation(true)
    .auto_resource_location(true);

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_scatter_no_geom)
    .additional_info("eevee_legacy_volumes_scatter_common")
    .vertex_out(legacy_volume_geom_frag_iface)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .auto_resource_location(true);
#endif

/* EEVEE_shaders_volumes_scatter_with_lights_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_scatter_with_lights_common)
    .define("VOLUME_LIGHTING")
    .define("IRRADIANCE_HL2");

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_scatter_with_lights)
    .additional_info("eevee_legacy_volumes_scatter_with_lights_common")
    .additional_info("eevee_legacy_volumes_scatter")
    .do_static_compilation(true)
    .auto_resource_location(true);

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_scatter_with_lights_no_geom)
    .additional_info("eevee_legacy_volumes_scatter_with_lights_common")
    .additional_info("eevee_legacy_volumes_scatter_no_geom")
    .metal_backend_only(true)
    .do_static_compilation(true)
    .auto_resource_location(true);
#endif

/* EEVEE_shaders_volumes_integration_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration_common)
    .define("STANDALONE")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_volumetric_lib")
    .additional_info("draw_resource_id_varying")
    /* NOTE: Unique sampler IDs assigned for consistency between library includes,
     * and to avoid unique assignment collision validation error.
     * However, resources will be auto assigned locations within shader usage. */
    .sampler(20, ImageType::FLOAT_3D, "volumeScattering")
    .sampler(21, ImageType::FLOAT_3D, "volumeExtinction")
    .vertex_out(legacy_volume_vert_geom_iface)
    .vertex_source("volumetric_vert.glsl")
    .fragment_source("volumetric_integration_frag.glsl");

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration_common_opti)
    .define("USE_VOLUME_OPTI")
    .image(0, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_3D, "finalScattering_img")
    .image(1, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_3D, "finalTransmittance_img");

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration_common_no_opti)
    .fragment_out(0, Type::VEC3, "finalScattering")
    .fragment_out(1, Type::VEC3, "finalTransmittance");

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration_common_geom)
    .additional_info("eevee_legacy_volumes_integration_common")
    .geometry_source("volumetric_geom.glsl")
    .geometry_out(legacy_volume_geom_frag_iface)
    .geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3);

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration_common_no_geom)
    .additional_info("eevee_legacy_volumes_integration_common")
    .vertex_out(legacy_volume_geom_frag_iface);
#endif

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration)
    .additional_info("eevee_legacy_volumes_integration_common_geom")
    .additional_info("eevee_legacy_volumes_integration_common_no_opti")
    .do_static_compilation(true)
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration_OPTI)
    .additional_info("eevee_legacy_volumes_integration_common_geom")
    .additional_info("eevee_legacy_volumes_integration_common_opti")
    .do_static_compilation(true)
    .auto_resource_location(true);

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration_no_geom)
    .additional_info("eevee_legacy_volumes_integration_common_no_geom")
    .additional_info("eevee_legacy_volumes_integration_common_no_opti")
    .metal_backend_only(true)
    .do_static_compilation(true)
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_integration_OPTI_no_geom)
    .additional_info("eevee_legacy_volumes_integration_common_no_geom")
    .additional_info("eevee_legacy_volumes_integration_common_opti")
    .metal_backend_only(true)
    .do_static_compilation(true)
    .auto_resource_location(true);
#endif

/* EEVEE_shaders_volumes_resolve_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_resolve_common)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_volumetric_lib")
    .sampler(0, ImageType::DEPTH_2D, "inSceneDepth")
    .fragment_source("volumetric_resolve_frag.glsl")
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_resolve)
    .additional_info("eevee_legacy_volumes_resolve_common")
    .fragment_out(0, Type::VEC4, "FragColor0", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "FragColor1", DualBlend::SRC_1)
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_resolve_accum)
    .define("VOLUMETRICS_ACCUM")
    .additional_info("eevee_legacy_volumes_resolve_common")
    .fragment_out(0, Type::VEC4, "FragColor0")
    .fragment_out(1, Type::VEC4, "FragColor1")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_volumes_accum_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_volumes_accum)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_volumetric_lib")
    .fragment_out(0, Type::VEC4, "FragColor0")
    .fragment_out(1, Type::VEC4, "FragColor1")
    .fragment_source("volumetric_accum_frag.glsl")
    .auto_resource_location(true)
    .do_static_compilation(true);
