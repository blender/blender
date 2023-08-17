/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* EEVEE_shaders_probe_filter_glossy_sh_get */
GPU_SHADER_INTERFACE_INFO(eevee_legacy_lightprobe_vert_geom_iface, "vert_iface")
    .smooth(Type::VEC4, "vPos");
GPU_SHADER_INTERFACE_INFO(eevee_legacy_lightprobe_vert_geom_flat_iface, "vert_iface_flat")
    .flat(Type::INT, "face");

GPU_SHADER_INTERFACE_INFO(eevee_legacy_lightprobe_geom_frag_iface, "geom_iface")
    .smooth(Type::VEC3, "worldPosition")
    .smooth(Type::VEC3, "viewPosition")
    .smooth(Type::VEC3, "worldNormal")
    .smooth(Type::VEC3, "viewNormal");
GPU_SHADER_INTERFACE_INFO(eevee_legacy_lightprobe_geom_frag_flat_iface, "geom_iface_flat")
    .flat(Type::INT, "fFace");

GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_vert)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_source("lightprobe_vert.glsl")
    .vertex_out(eevee_legacy_lightprobe_vert_geom_iface)
    .vertex_out(eevee_legacy_lightprobe_vert_geom_flat_iface)
    .builtins(BuiltinBits::INSTANCE_ID);

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_vert_no_geom)
    .vertex_in(0, Type::VEC3, "pos")
    .push_constant(Type::INT, "Layer")
    .vertex_source("lightprobe_vert_no_geom.glsl")
    .vertex_out(eevee_legacy_lightprobe_geom_frag_iface)
    .vertex_out(eevee_legacy_lightprobe_geom_frag_flat_iface)
    .builtins(BuiltinBits::INSTANCE_ID);
#endif

GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_geom)
    .geometry_source("lightprobe_geom.glsl")
    .geometry_out(eevee_legacy_lightprobe_geom_frag_iface)
    .geometry_out(eevee_legacy_lightprobe_geom_frag_flat_iface)
    .push_constant(Type::INT, "Layer")
    .geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3);

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_probe_filter_glossy_no_geom)
    .additional_info("eevee_legacy_lightprobe_vert_no_geom")
    .fragment_source("lightprobe_filter_glossy_frag.glsl")
    .sampler(0, ImageType::FLOAT_CUBE, "probeHdr")
    .push_constant(Type::FLOAT, "probe_roughness")
    .push_constant(Type::FLOAT, "texelSize")
    .push_constant(Type::FLOAT, "lodFactor")
    .push_constant(Type::FLOAT, "lodMax")
    .push_constant(Type::FLOAT, "paddingSize")
    .push_constant(Type::FLOAT, "intensityFac")
    .push_constant(Type::FLOAT, "fireflyFactor")
    .push_constant(Type::FLOAT, "sampleCount")
    .fragment_out(0, Type::VEC4, "FragColor")
    .metal_backend_only(true)
    .do_static_compilation(true)
    .auto_resource_location(true);
#endif

GPU_SHADER_CREATE_INFO(eevee_legacy_probe_filter_glossy)
    .additional_info("eevee_legacy_lightprobe_vert")
    .additional_info("eevee_legacy_lightprobe_geom")
    .fragment_source("lightprobe_filter_glossy_frag.glsl")
    .sampler(0, ImageType::FLOAT_CUBE, "probeHdr")
    .push_constant(Type::FLOAT, "probe_roughness")
    .push_constant(Type::FLOAT, "texelSize")
    .push_constant(Type::FLOAT, "lodFactor")
    .push_constant(Type::FLOAT, "lodMax")
    .push_constant(Type::FLOAT, "paddingSize")
    .push_constant(Type::FLOAT, "intensityFac")
    .push_constant(Type::FLOAT, "fireflyFactor")
    .push_constant(Type::FLOAT, "sampleCount")
    .fragment_out(0, Type::VEC4, "FragColor")
    .do_static_compilation(true)
    .auto_resource_location(true);

/* EEVEE_shaders_effect_downsample_cube_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_effect_downsample_cube)
    .additional_info("eevee_legacy_lightprobe_vert")
    .additional_info("eevee_legacy_lightprobe_geom")
    .fragment_source("effect_downsample_cube_frag.glsl")
    .sampler(0, ImageType::FLOAT_CUBE, "source")
    .push_constant(Type::FLOAT, "texelSize")
    .fragment_out(0, Type::VEC4, "FragColor")
    .do_static_compilation(true)
    .auto_resource_location(true);

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_effect_downsample_cube_no_geom)
    .additional_info("eevee_legacy_lightprobe_vert_no_geom")
    .fragment_source("effect_downsample_cube_frag.glsl")
    .sampler(0, ImageType::FLOAT_CUBE, "source")
    .push_constant(Type::FLOAT, "texelSize")
    .fragment_out(0, Type::VEC4, "FragColor")
    .metal_backend_only(true)
    .do_static_compilation(true)
    .auto_resource_location(true);
#endif

/* EEVEE_shaders_probe_filter_diffuse_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_probe_filter_diffuse)
    .additional_info("eevee_legacy_irradiance_lib")
    .additional_info("draw_fullscreen")
    .fragment_source("lightprobe_filter_diffuse_frag.glsl")
    .sampler(0, ImageType::FLOAT_CUBE, "probeHdr")
    .push_constant(Type::INT, "probeSize")
    .push_constant(Type::FLOAT, "lodFactor")
    .push_constant(Type::FLOAT, "lodMax")
    .push_constant(Type::FLOAT, "intensityFac")
    .push_constant(Type::FLOAT, "sampleCount")
    .fragment_out(0, Type::VEC4, "FragColor")
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_probe_filter_diffuse_sh_l2)
    .define("IRRADIANCE_SH_L2")
    .additional_info("eevee_legacy_probe_filter_diffuse")
    .do_static_compilation(true)
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_probe_filter_diffuse_hl2)
    .define("IRRADIANCE_HL2")
    .additional_info("eevee_legacy_probe_filter_diffuse")
    .do_static_compilation(true)
    .auto_resource_location(true);

/* EEVEE_shaders_probe_filter_visibility_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_probe_filter_visibility)
    .define("IRRADIANCE_HL2")
    .additional_info("eevee_legacy_irradiance_lib")
    .additional_info("draw_fullscreen")
    .fragment_source("lightprobe_filter_visibility_frag.glsl")
    .sampler(0, ImageType::FLOAT_CUBE, "probeDepth")
    .push_constant(Type::INT, "outputSize")
    .push_constant(Type::FLOAT, "lodFactor")
    .push_constant(Type::FLOAT, "storedTexelSize")
    .push_constant(Type::FLOAT, "lodMax")
    .push_constant(Type::FLOAT, "nearClip")
    .push_constant(Type::FLOAT, "farClip")
    .push_constant(Type::FLOAT, "visibilityRange")
    .push_constant(Type::FLOAT, "visibilityBlur")
    .push_constant(Type::FLOAT, "sampleCount")
    .fragment_out(0, Type::VEC4, "FragColor")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_probe_grid_fill_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_probe_grid_fill)
    .additional_info("draw_fullscreen")
    .fragment_source("lightprobe_grid_fill_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D_ARRAY, "irradianceGrid")
    .fragment_out(0, Type::VEC4, "FragColor")
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_probe_grid_fill_sh_l2)
    .define("IRRADIANCE_SH_L2")
    .additional_info("eevee_legacy_probe_grid_fill")
    .do_static_compilation(true)
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_probe_grid_fill_hl2)
    .define("IRRADIANCE_HL2")
    .additional_info("eevee_legacy_probe_grid_fill")
    .do_static_compilation(true)
    .auto_resource_location(true);

/* EEVEE_shaders_probe_planar_display_sh_get */
GPU_SHADER_INTERFACE_INFO(legacy_probe_planar_iface, "")
    .smooth(Type::VEC3, "worldPosition")
    .flat(Type::INT, "probeIdx");

GPU_SHADER_CREATE_INFO(eevee_legacy_probe_planar_display)
    .sampler(0, ImageType::FLOAT_2D_ARRAY, "probePlanars")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::INT, "probe_id")
    .vertex_in(2, Type::MAT4, "probe_mat")
    .vertex_out(legacy_probe_planar_iface)
    .vertex_source("lightprobe_planar_display_vert.glsl")
    .fragment_source("lightprobe_planar_display_frag.glsl")
    .additional_info("draw_view")
    .fragment_out(0, Type::VEC4, "FragColor")
    .do_static_compilation(true)
    .auto_resource_location(true);

/* EEVEE_shaders_studiolight_probe_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_studiolight_probe)
    .additional_info("draw_resource_id_varying")
    .additional_info("eevee_legacy_lightprobe_lib")
    .additional_info("eevee_legacy_surface_lib_lookdev")
    .vertex_in(0, Type::VEC2, "pos")
    .sampler(0, ImageType::FLOAT_2D, "studioLight")
    .push_constant(Type::FLOAT, "backgroundAlpha")
    .push_constant(Type::MAT3, "StudioLightMatrix")
    .push_constant(Type::FLOAT, "studioLightIntensity")
    .push_constant(Type::FLOAT, "studioLightBlur")
    .fragment_out(0, Type::VEC4, "FragColor")
    .vertex_source("background_vert.glsl")
    .fragment_source("lookdev_world_frag.glsl")
    .do_static_compilation(true)
    .auto_resource_location(true);

/* EEVEE_shaders_studiolight_background_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_studiolight_background)
    .define("LOOKDEV_BG")
    .additional_info("eevee_legacy_studiolight_probe")
    .do_static_compilation(true)
    .auto_resource_location(true);

/* EEVEE_shaders_probe_planar_downsample_sh_get */

GPU_SHADER_INTERFACE_INFO(eevee_legacy_probe_planar_downsample_vert_geom_iface,
                          "lightprobe_vert_iface")
    .smooth(Type::VEC2, "vPos");
GPU_SHADER_INTERFACE_INFO(eevee_legacy_probe_planar_downsample_vert_geom_flat_iface,
                          "lightprobe_vert_iface_flat")
    .flat(Type::INT, "instance");

GPU_SHADER_INTERFACE_INFO(eevee_legacy_probe_planar_downsample_geom_frag_iface,
                          "lightprobe_geom_iface")
    .flat(Type::FLOAT, "layer");

GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_planar_downsample_common)
    .vertex_source("lightprobe_planar_downsample_vert.glsl")
    .fragment_source("lightprobe_planar_downsample_frag.glsl")
    .vertex_out(eevee_legacy_probe_planar_downsample_vert_geom_iface)
    .vertex_out(eevee_legacy_probe_planar_downsample_vert_geom_flat_iface)
    .sampler(0, ImageType::FLOAT_2D_ARRAY, "source")
    .push_constant(Type::FLOAT, "fireflyFactor")
    .fragment_out(0, Type::VEC4, "FragColor")
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_planar_downsample)
    .additional_info("eevee_legacy_lightprobe_planar_downsample_common")
    .geometry_source("lightprobe_planar_downsample_geom.glsl")
    .geometry_out(eevee_legacy_probe_planar_downsample_geom_frag_iface)
    .geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3)
    .do_static_compilation(true)
    .auto_resource_location(true);

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_planar_downsample_no_geom)
    .additional_info("eevee_legacy_lightprobe_planar_downsample_common")
    .vertex_out(eevee_legacy_probe_planar_downsample_geom_frag_iface)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .auto_resource_location(true);
#endif

/* EEVEE_shaders_probe_cube_display_sh_get */
GPU_SHADER_INTERFACE_INFO(eevee_legacy_lightprobe_cube_display_iface, "")
    .flat(Type::INT, "pid")
    .smooth(Type::VEC2, "quadCoord");

GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_cube_display)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_lightprobe_lib")
    .vertex_source("lightprobe_cube_display_vert.glsl")
    .fragment_source("lightprobe_cube_display_frag.glsl")
    .vertex_out(eevee_legacy_lightprobe_cube_display_iface)
    .push_constant(Type::FLOAT, "sphere_size")
    .push_constant(Type::VEC3, "screen_vecs", 2)
    .fragment_out(0, Type::VEC4, "FragColor")
    .do_static_compilation(true)
    .auto_resource_location(true);

/* EEVEE_shaders_probe_grid_display_sh_get */
GPU_SHADER_INTERFACE_INFO(eevee_legacy_lightprobe_grid_display_iface, "")
    .flat(Type::INT, "cellOffset")
    .smooth(Type::VEC2, "quadCoord");

GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_grid_display_common)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .additional_info("eevee_legacy_irradiance_lib")
    .vertex_source("lightprobe_grid_display_vert.glsl")
    .fragment_source("lightprobe_grid_display_frag.glsl")
    .vertex_out(eevee_legacy_lightprobe_grid_display_iface)
    .push_constant(Type::FLOAT, "sphere_size")
    .push_constant(Type::INT, "offset")
    .push_constant(Type::IVEC3, "grid_resolution")
    .push_constant(Type::VEC3, "corner")
    .push_constant(Type::VEC3, "increment_x")
    .push_constant(Type::VEC3, "increment_y")
    .push_constant(Type::VEC3, "increment_z")
    .push_constant(Type::VEC3, "screen_vecs", 2)
    .fragment_out(0, Type::VEC4, "FragColor")
    .do_static_compilation(true)
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_grid_display_common_sh_l2)
    .define("IRRADIANCE_SH_L2")
    .additional_info("eevee_legacy_lightprobe_grid_display_common")
    .do_static_compilation(true)
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_lightprobe_grid_display_common_hl2)
    .define("IRRADIANCE_HL2")
    .additional_info("eevee_legacy_lightprobe_grid_display_common")
    .do_static_compilation(true)
    .auto_resource_location(true);
