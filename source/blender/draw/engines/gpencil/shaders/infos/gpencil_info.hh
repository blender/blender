/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "gpu_glsl_cpp_stubs.hh"

#  define GP_LIGHT

#  include "gpencil_shader_shared.h"

#  include "draw_fullscreen_info.hh"
#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"

#  define SMAA_GLSL_3
#  define SMAA_STAGE 1
#  define SMAA_PRESET_HIGH
#  define SMAA_NO_DISCARD
#  define SMAA_RT_METRICS viewportMetrics
#  define SMAA_LUMA_WEIGHT float4(1.0, 1.0, 1.0, 1.0)
#endif

#include "gpu_shader_create_info.hh"

#include "gpencil_defines.h"

/* -------------------------------------------------------------------- */
/** \name GPencil Object rendering
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_geometry_iface, gp_interp)
SMOOTH(VEC4, color_mul)
SMOOTH(VEC4, color_add)
SMOOTH(VEC3, pos)
SMOOTH(VEC2, uv)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp)
GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_geometry_flat_iface, gp_interp_flat)
FLAT(VEC2, aspect)
FLAT(VEC4, sspos)
FLAT(UINT, mat_flag)
FLAT(FLOAT, depth)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_flat)
GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_geometry_noperspective_iface, gp_interp_noperspective)
NO_PERSPECTIVE(VEC2, thickness)
NO_PERSPECTIVE(FLOAT, hardness)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_noperspective)

GPU_SHADER_CREATE_INFO(gpencil_geometry)
DO_STATIC_COMPILATION()
DEFINE("GP_LIGHT")
TYPEDEF_SOURCE("gpencil_defines.h")
SAMPLER(2, FLOAT_2D, gpFillTexture)
SAMPLER(3, FLOAT_2D, gpStrokeTexture)
SAMPLER(4, DEPTH_2D, gpSceneDepthTexture)
SAMPLER(5, FLOAT_2D, gpMaskTexture)
UNIFORM_BUF_FREQ(4, gpMaterial, gp_materials[GPENCIL_MATERIAL_BUFFER_LEN], BATCH)
UNIFORM_BUF_FREQ(3, gpLight, gp_lights[GPENCIL_LIGHT_BUFFER_LEN], BATCH)
PUSH_CONSTANT(VEC2, viewportSize)
/* Per Object */
PUSH_CONSTANT(VEC3, gpNormal)
PUSH_CONSTANT(BOOL, gpStrokeOrder3d)
PUSH_CONSTANT(INT, gpMaterialOffset)
/* Per Layer */
PUSH_CONSTANT(FLOAT, gpVertexColorOpacity)
PUSH_CONSTANT(VEC4, gpLayerTint)
PUSH_CONSTANT(FLOAT, gpLayerOpacity)
PUSH_CONSTANT(FLOAT, gpStrokeIndexOffset)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, revealColor)
VERTEX_OUT(gpencil_geometry_iface)
VERTEX_OUT(gpencil_geometry_flat_iface)
VERTEX_OUT(gpencil_geometry_noperspective_iface)
VERTEX_SOURCE("gpencil_vert.glsl")
FRAGMENT_SOURCE("gpencil_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_gpencil_new)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Full-Screen Shaders
 * \{ */

GPU_SHADER_CREATE_INFO(gpencil_layer_blend)
DO_STATIC_COMPILATION()
SAMPLER(0, FLOAT_2D, colorBuf)
SAMPLER(1, FLOAT_2D, revealBuf)
SAMPLER(2, FLOAT_2D, maskBuf)
PUSH_CONSTANT(INT, blendMode)
PUSH_CONSTANT(FLOAT, blendOpacity)
/* Reminder: This is considered SRC color in blend equations.
 * Same operation on all buffers. */
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, fragRevealage)
FRAGMENT_SOURCE("gpencil_layer_blend_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_mask_invert)
DO_STATIC_COMPILATION()
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, fragRevealage)
FRAGMENT_SOURCE("gpencil_mask_invert_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_depth_merge)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(MAT4, gpModelMatrix)
PUSH_CONSTANT(BOOL, strokeOrder3d)
SAMPLER(0, DEPTH_2D, depthBuf)
VERTEX_SOURCE("gpencil_depth_merge_vert.glsl")
FRAGMENT_SOURCE("gpencil_depth_merge_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Anti-Aliasing
 * \{ */

GPU_SHADER_INTERFACE_INFO(gpencil_antialiasing_iface)
SMOOTH(VEC2, uvs)
SMOOTH(VEC2, pixcoord)
SMOOTH(VEC4, offset[3])
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing)
DEFINE("SMAA_GLSL_3")
DEFINE_VALUE("SMAA_RT_METRICS", "viewportMetrics")
DEFINE("SMAA_PRESET_HIGH")
DEFINE_VALUE("SMAA_LUMA_WEIGHT", "float4(lumaWeight, lumaWeight, lumaWeight, 0.0)")
DEFINE("SMAA_NO_DISCARD")
VERTEX_OUT(gpencil_antialiasing_iface)
PUSH_CONSTANT(VEC4, viewportMetrics)
PUSH_CONSTANT(FLOAT, lumaWeight)
VERTEX_SOURCE("gpencil_antialiasing_vert.glsl")
FRAGMENT_SOURCE("gpencil_antialiasing_frag.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_0)
DEFINE_VALUE("SMAA_STAGE", "0")
SAMPLER(0, FLOAT_2D, colorTex)
SAMPLER(1, FLOAT_2D, revealTex)
FRAGMENT_OUT(0, VEC2, out_edges)
ADDITIONAL_INFO(gpencil_antialiasing)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_1)
DEFINE_VALUE("SMAA_STAGE", "1")
SAMPLER(0, FLOAT_2D, edgesTex)
SAMPLER(1, FLOAT_2D, areaTex)
SAMPLER(2, FLOAT_2D, searchTex)
FRAGMENT_OUT(0, VEC4, out_weights)
ADDITIONAL_INFO(gpencil_antialiasing)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_2)
DEFINE_VALUE("SMAA_STAGE", "2")
SAMPLER(0, FLOAT_2D, colorTex)
SAMPLER(1, FLOAT_2D, revealTex)
SAMPLER(2, FLOAT_2D, blendTex)
PUSH_CONSTANT(FLOAT, mixFactor)
PUSH_CONSTANT(FLOAT, taaAccumulatedWeight)
PUSH_CONSTANT(BOOL, doAntiAliasing)
PUSH_CONSTANT(BOOL, onlyAlpha)
/* Reminder: Blending func is `fragRevealage * DST + fragColor`. */
FRAGMENT_OUT_DUAL(0, VEC4, out_color, SRC_0)
FRAGMENT_OUT_DUAL(0, VEC4, out_reveal, SRC_1)
ADDITIONAL_INFO(gpencil_antialiasing)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
