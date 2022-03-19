/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Fullscreen shaders
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
