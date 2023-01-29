/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* DOF Lib */
GPU_SHADER_CREATE_INFO(eevee_legacy_dof_lib)
    .additional_info("draw_view")
    .push_constant(Type::VEC4, "cocParams");

/* EEVEE_shaders_depth_of_field_bokeh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_bokeh)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_bokeh_frag.glsl")
    .push_constant(Type::FLOAT, "bokehSides")
    .push_constant(Type::FLOAT, "bokehRotation")
    .push_constant(Type::VEC2, "bokehAnisotropyInv")
    .fragment_out(0, Type::VEC2, "outGatherLut")
    .fragment_out(1, Type::FLOAT, "outScatterLut")
    .fragment_out(2, Type::FLOAT, "outResolveLut")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_setup_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_setup)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_setup_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::DEPTH_2D, "depthBuffer")
    .push_constant(Type::FLOAT, "bokehMaxSize")
    .fragment_out(0, Type::VEC4, "outColor")
    .fragment_out(1, Type::VEC2, "outCoc")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_flatten_tiles_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_flatten_tiles)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_flatten_tiles_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "halfResCocBuffer")
    .fragment_out(0, Type::VEC4, "outFgCoc")
    .fragment_out(1, Type::VEC3, "outBgCoc")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_dilate_tiles_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_dilate_tiles_common)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_dilate_tiles_frag.glsl")
    .push_constant(Type::INT, "ringCount")
    .push_constant(Type::INT, "ringWidthMultiplier")
    .push_constant(Type::BOOL, "dilateSlightFocus")
    .sampler(0, ImageType::FLOAT_2D, "cocTilesFgBuffer")
    .sampler(1, ImageType::FLOAT_2D, "cocTilesBgBuffer")
    .fragment_out(0, Type::VEC4, "outFgCoc")
    .fragment_out(1, Type::VEC3, "outBgCoc")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_dilate_tiles_MINMAX)
    .define("DILATE_MODE_MIN_MAX")
    .additional_info("eevee_legacy_depth_of_field_dilate_tiles_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_dilate_tiles_MINABS)
    .define("DILATE_MODE_MIN_ABS")
    .additional_info("eevee_legacy_depth_of_field_dilate_tiles_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_downsample_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_downsample)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_downsample_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::FLOAT_2D, "cocBuffer")
    .fragment_out(0, Type::VEC4, "outColor")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_reduce_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_reduce_common)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_reduce_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::FLOAT_2D, "cocBuffer")
    .sampler(2, ImageType::FLOAT_2D, "downsampledBuffer")
    .push_constant(Type::VEC2, "bokehAnisotropy")
    .push_constant(Type::FLOAT, "scatterColorThreshold")
    .push_constant(Type::FLOAT, "scatterCocThreshold")
    .push_constant(Type::FLOAT, "scatterColorNeighborMax")
    .push_constant(Type::FLOAT, "colorNeighborClamping")
    .fragment_out(0, Type::VEC4, "outColor")
    .fragment_out(1, Type::FLOAT, "outCoc")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_reduce_COPY_PASS)
    .define("COPY_PASS")
    .fragment_out(2, Type::VEC3, "outScatterColor")
    .additional_info("eevee_legacy_depth_of_field_reduce_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_reduce_REDUCE_PASS)
    .define("REDUCE_PASS")
    .additional_info("eevee_legacy_depth_of_field_reduce_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_gather_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_gather_common)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_gather_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::FLOAT_2D, "cocBuffer")
    .sampler(2, ImageType::FLOAT_2D, "colorBufferBilinear")
    .sampler(3, ImageType::FLOAT_2D, "cocTilesFgBuffer")
    .sampler(4, ImageType::FLOAT_2D, "cocTilesBgBuffer")
    .sampler(5, ImageType::FLOAT_2D, "bokehLut")
    .push_constant(Type::VEC2, "gatherInputUvCorrection")
    .push_constant(Type::VEC2, "gatherOutputTexelSize")
    .push_constant(Type::VEC2, "bokehAnisotropy")
    .fragment_out(0, Type::VEC4, "outColor")
    .fragment_out(1, Type::FLOAT, "outWeight")
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_gather_bokeh).define("DOF_BOKEH_TEXTURE");

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_gather_FOREGROUND)
    .define("DOF_FOREGROUND_PASS")
    .additional_info("eevee_legacy_depth_of_field_gather_common")
    .fragment_out(2, Type::VEC2, "outOcclusion") /* NOT DOF_HOLEFILL_PASS */
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_gather_BACKGROUND)
    .define("DOF_BACKGROUND_PASS")
    .additional_info("eevee_legacy_depth_of_field_gather_common")
    .fragment_out(2, Type::VEC2, "outOcclusion") /* NOT DOF_HOLEFILL_PASS */
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_gather_HOLEFILL)
    .define("DOF_BACKGROUND_PASS")
    .define("DOF_HOLEFILL_PASS")
    .additional_info("eevee_legacy_depth_of_field_gather_common")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_gather_FOREGROUND_BOKEH)
    .additional_info("eevee_legacy_depth_of_field_gather_bokeh")
    .additional_info("eevee_legacy_depth_of_field_gather_FOREGROUND")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_gather_BACKGROUND_BOKEH)
    .additional_info("eevee_legacy_depth_of_field_gather_bokeh")
    .additional_info("eevee_legacy_depth_of_field_gather_BACKGROUND")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_gather_HOLEFILL_BOKEH)
    .additional_info("eevee_legacy_depth_of_field_gather_bokeh")
    .additional_info("eevee_legacy_depth_of_field_gather_HOLEFILL")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_filter_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_filter)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_filter_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::FLOAT_2D, "weightBuffer")
    .fragment_out(0, Type::VEC4, "outColor")
    .fragment_out(1, Type::FLOAT, "outWeight")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_scatter_get */
GPU_SHADER_INTERFACE_INFO(eevee_legacy_dof_scatter_iface, "")
    .flat(Type::VEC4, "color1")
    .flat(Type::VEC4, "color2")
    .flat(Type::VEC4, "color3")
    .flat(Type::VEC4, "color4")
    .flat(Type::VEC4, "weights")
    .flat(Type::VEC4, "cocs")
    .flat(Type::VEC2, "spritepos")
    .flat(Type::FLOAT, "spritesize");

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_scatter_common)
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_dof_lib")
    .vertex_source("effect_dof_scatter_vert.glsl")
    .fragment_source("effect_dof_scatter_frag.glsl")
    .vertex_out(eevee_legacy_dof_scatter_iface)
    .push_constant(Type::VEC2, "targetTexelSize")
    .push_constant(Type::INT, "spritePerRow")
    .push_constant(Type::VEC2, "bokehAnisotropy")
    .push_constant(Type::VEC2, "bokehAnisotropyInv")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::FLOAT_2D, "cocBuffer")
    .sampler(2, ImageType::FLOAT_2D, "occlusionBuffer")
    .sampler(3, ImageType::FLOAT_2D, "bokehLut")
    .fragment_out(0, Type::VEC4, "fragColor")
    .auto_resource_location(true)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_scatter_bokeh).define("DOF_BOKEH_TEXTURE");

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_scatter_FOREGROUND)
    .define("DOF_FOREGROUND_PASS")
    .additional_info("eevee_legacy_depth_of_field_scatter_common")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_scatter_BACKGROUND)
    .define("DOF_BACKGROUND_PASS")
    .additional_info("eevee_legacy_depth_of_field_scatter_common")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_scatter_FOREGROUND_BOKEH)
    .additional_info("eevee_legacy_depth_of_field_scatter_bokeh")
    .additional_info("eevee_legacy_depth_of_field_scatter_FOREGROUND")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_scatter_BACKGROUND_BOKEH)
    .additional_info("eevee_legacy_depth_of_field_scatter_bokeh")
    .additional_info("eevee_legacy_depth_of_field_scatter_BACKGROUND")
    .do_static_compilation(true);

/* EEVEE_shaders_depth_of_field_resolve_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_resolve_common)
    .define("DOF_RESOLVE_PASS")
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("eevee_legacy_dof_lib")
    .fragment_source("effect_dof_resolve_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "fullResColorBuffer")
    .sampler(1, ImageType::DEPTH_2D, "fullResDepthBuffer")
    .sampler(2, ImageType::FLOAT_2D, "bgColorBuffer")
    .sampler(3, ImageType::FLOAT_2D, "bgWeightBuffer")
    .sampler(4, ImageType::FLOAT_2D, "bgTileBuffer")
    .sampler(5, ImageType::FLOAT_2D, "fgColorBuffer")
    .sampler(6, ImageType::FLOAT_2D, "fgWeightBuffer")
    .sampler(7, ImageType::FLOAT_2D, "fgTileBuffer")
    .sampler(8, ImageType::FLOAT_2D, "holefillColorBuffer")
    .sampler(9, ImageType::FLOAT_2D, "holefillWeightBuffer")
    .sampler(10, ImageType::FLOAT_2D, "bokehLut")
    .push_constant(Type::FLOAT, "bokehMaxSize")
    .fragment_out(0, Type::VEC4, "fragColor")
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_resolve_bokeh).define("DOF_BOKEH_TEXTURE");

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_resolve_HQ)
    .define("DOF_SLIGHT_FOCUS_DENSITY", "4")
    .additional_info("eevee_legacy_depth_of_field_resolve_common")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_resolve_LQ)
    .define("DOF_SLIGHT_FOCUS_DENSITY", "2")
    .additional_info("eevee_legacy_depth_of_field_resolve_common")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_resolve_HQ_BOKEH)
    .additional_info("eevee_legacy_depth_of_field_resolve_HQ")
    .additional_info("eevee_legacy_depth_of_field_resolve_bokeh")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_depth_of_field_resolve_LQ_BOKEH)
    .additional_info("eevee_legacy_depth_of_field_resolve_LQ")
    .additional_info("eevee_legacy_depth_of_field_resolve_bokeh")
    .do_static_compilation(true);
