/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

#include "gpencil_defines.h"

/* -------------------------------------------------------------------- */
/** \name GPencil Object rendering
 * \{ */

GPU_SHADER_INTERFACE_INFO(gpencil_geometry_iface, "gp_interp")
    .smooth(Type::VEC4, "color_mul")
    .smooth(Type::VEC4, "color_add")
    .smooth(Type::VEC3, "pos")
    .smooth(Type::VEC2, "uv")
    .no_perspective(Type::VEC2, "thickness")
    .no_perspective(Type::FLOAT, "hardness")
    .flat(Type::VEC2, "aspect")
    .flat(Type::VEC4, "sspos")
    .flat(Type::UINT, "mat_flag")
    .flat(Type::FLOAT, "depth");

GPU_SHADER_CREATE_INFO(gpencil_geometry)
    .do_static_compilation(true)
    .define("GP_LIGHT")
    .typedef_source("gpencil_defines.h")
    .sampler(2, ImageType::FLOAT_2D, "gpFillTexture")
    .sampler(3, ImageType::FLOAT_2D, "gpStrokeTexture")
    .sampler(4, ImageType::DEPTH_2D, "gpSceneDepthTexture")
    .sampler(5, ImageType::FLOAT_2D, "gpMaskTexture")
    .uniform_buf(4, "gpMaterial", "gp_materials[GPENCIL_MATERIAL_BUFFER_LEN]", Frequency::BATCH)
    .uniform_buf(3, "gpLight", "gp_lights[GPENCIL_LIGHT_BUFFER_LEN]", Frequency::BATCH)
    .push_constant(Type::VEC2, "viewportSize")
    /* Per Object */
    .push_constant(Type::VEC3, "gpNormal")
    .push_constant(Type::BOOL, "gpStrokeOrder3d")
    .push_constant(Type::INT, "gpMaterialOffset")
    /* Per Layer */
    .push_constant(Type::FLOAT, "gpVertexColorOpacity")
    .push_constant(Type::VEC4, "gpLayerTint")
    .push_constant(Type::FLOAT, "gpLayerOpacity")
    .push_constant(Type::FLOAT, "gpStrokeIndexOffset")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "revealColor")
    .vertex_out(gpencil_geometry_iface)
    .vertex_source("gpencil_vert.glsl")
    .fragment_source("gpencil_frag.glsl")
    .depth_write(DepthWrite::ANY)
    .additional_info("draw_gpencil");

GPU_SHADER_CREATE_INFO(gpencil_geometry_next)
    .do_static_compilation(true)
    .define("GP_LIGHT")
    .typedef_source("gpencil_defines.h")
    .sampler(GPENCIL_SCENE_DEPTH_TEX_SLOT, ImageType::DEPTH_2D, "gpSceneDepthTexture")
    .sampler(GPENCIL_MASK_TEX_SLOT, ImageType::FLOAT_2D, "gpMaskTexture")
    .sampler(GPENCIL_FILL_TEX_SLOT, ImageType::FLOAT_2D, "gpFillTexture")
    .sampler(GPENCIL_STROKE_TEX_SLOT, ImageType::FLOAT_2D, "gpStrokeTexture")
    .storage_buf(GPENCIL_OBJECT_SLOT, Qualifier::READ, "gpObject", "gp_object[]")
    .storage_buf(GPENCIL_LAYER_SLOT, Qualifier::READ, "gpLayer", "gp_layer[]")
    .storage_buf(GPENCIL_MATERIAL_SLOT, Qualifier::READ, "gpMaterial", "gp_materials[]")
    .storage_buf(GPENCIL_LIGHT_SLOT, Qualifier::READ, "gpLight", "gp_lights[]")
    .uniform_buf(GPENCIL_SCENE_SLOT, "gpScene", "gp_scene")
    /* Per Scene */
    .define("viewportSize", "gp_scene.render_size")
    /* Per Object */
    .define("gpNormal", "gp_object[resource_id].normal")
    .define("gpStrokeOrder3d", "gp_object[resource_id].stroke_order3d")
    .define("gpMaterialOffset", "gp_object[resource_id].material_offset")
    /* Per Layer */
    .define("layer_id", "gp_object[resource_id].layer_offset") /* TODO */
    .define("gpVertexColorOpacity", "gp_layer[layer_id].vertex_color_opacity")
    .define("gpLayerTint", "gp_layer[layer_id].tint")
    .define("gpLayerOpacity", "gp_layer[layer_id].opacity")
    .define("gpStrokeIndexOffset", "gp_layer[layer_id].stroke_index_offset")
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "revealColor")
    .vertex_out(gpencil_geometry_iface)
    .vertex_source("grease_pencil_vert.glsl")
    .fragment_source("gpencil_frag.glsl")
    .additional_info("draw_gpencil_new")
    .depth_write(DepthWrite::ANY);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Full-Screen Shaders
 * \{ */

GPU_SHADER_CREATE_INFO(gpencil_layer_blend)
    .do_static_compilation(true)
    .sampler(0, ImageType::FLOAT_2D, "colorBuf")
    .sampler(1, ImageType::FLOAT_2D, "revealBuf")
    .sampler(2, ImageType::FLOAT_2D, "maskBuf")
    .push_constant(Type::INT, "blendMode")
    .push_constant(Type::FLOAT, "blendOpacity")
    /* Reminder: This is considered SRC color in blend equations.
     * Same operation on all buffers. */
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "fragRevealage")
    .fragment_source("gpencil_layer_blend_frag.glsl")
    .additional_info("draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_mask_invert)
    .do_static_compilation(true)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "fragRevealage")
    .fragment_source("gpencil_mask_invert_frag.glsl")
    .additional_info("draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_depth_merge)
    .do_static_compilation(true)
    .push_constant(Type::VEC4, "gpModelMatrix", 4)
    .push_constant(Type::BOOL, "strokeOrder3d")
    .sampler(0, ImageType::DEPTH_2D, "depthBuf")
    .vertex_source("gpencil_depth_merge_vert.glsl")
    .fragment_source("gpencil_depth_merge_frag.glsl")
    .depth_write(DepthWrite::ANY)
    .additional_info("draw_view");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Anti-Aliasing
 * \{ */

GPU_SHADER_INTERFACE_INFO(gpencil_antialiasing_iface, "")
    .smooth(Type::VEC2, "uvs")
    .smooth(Type::VEC2, "pixcoord")
    .smooth(Type::VEC4, "offset[3]");

GPU_SHADER_CREATE_INFO(gpencil_antialiasing)
    .define("SMAA_GLSL_3")
    .define("SMAA_RT_METRICS", "viewportMetrics")
    .define("SMAA_PRESET_HIGH")
    .define("SMAA_LUMA_WEIGHT", "float4(lumaWeight, lumaWeight, lumaWeight, 0.0)")
    .define("SMAA_NO_DISCARD")
    .vertex_out(gpencil_antialiasing_iface)
    .push_constant(Type::VEC4, "viewportMetrics")
    .push_constant(Type::FLOAT, "lumaWeight")
    .vertex_source("gpencil_antialiasing_vert.glsl")
    .fragment_source("gpencil_antialiasing_frag.glsl");

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_0)
    .define("SMAA_STAGE", "0")
    .sampler(0, ImageType::FLOAT_2D, "colorTex")
    .sampler(1, ImageType::FLOAT_2D, "revealTex")
    .fragment_out(0, Type::VEC2, "out_edges")
    .additional_info("gpencil_antialiasing")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_1)
    .define("SMAA_STAGE", "1")
    .sampler(0, ImageType::FLOAT_2D, "edgesTex")
    .sampler(1, ImageType::FLOAT_2D, "areaTex")
    .sampler(2, ImageType::FLOAT_2D, "searchTex")
    .fragment_out(0, Type::VEC4, "out_weights")
    .additional_info("gpencil_antialiasing")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_2)
    .define("SMAA_STAGE", "2")
    .sampler(0, ImageType::FLOAT_2D, "colorTex")
    .sampler(1, ImageType::FLOAT_2D, "revealTex")
    .sampler(2, ImageType::FLOAT_2D, "blendTex")
    .push_constant(Type::FLOAT, "mixFactor")
    .push_constant(Type::FLOAT, "taaAccumulatedWeight")
    .push_constant(Type::BOOL, "doAntiAliasing")
    .push_constant(Type::BOOL, "onlyAlpha")
    /* Reminder: Blending func is `fragRevealage * DST + fragColor`. */
    .fragment_out(0, Type::VEC4, "out_color", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_reveal", DualBlend::SRC_1)
    .additional_info("gpencil_antialiasing")
    .do_static_compilation(true);

/** \} */
