/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "gpu_shader_compat.hh"

#  define GP_LIGHT

#  include "gpencil_shader_shared.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "gpu_shader_fullscreen_infos.hh"

#  define SMAA_GLSL_3
#  define SMAA_STAGE 1
#  define SMAA_PRESET_HIGH
#  define SMAA_NO_DISCARD
#  define SMAA_RT_METRICS viewport_metrics
#  define SMAA_LUMA_WEIGHT float4(1.0f, 1.0f, 1.0f, 1.0f)
#endif

#include "gpu_shader_create_info.hh"

#include "gpencil_defines.hh"

/* -------------------------------------------------------------------- */
/** \name GPencil Object rendering
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_geometry_iface, gp_interp)
SMOOTH(float4, color_mul)
SMOOTH(float4, color_add)
SMOOTH(float3, pos)
SMOOTH(float2, uv)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp)
GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_geometry_flat_iface, gp_interp_flat)
FLAT(float2, aspect)
FLAT(float4, sspos)
FLAT(float4, sspos_adj)
FLAT(uint, mat_flag)
FLAT(float, depth)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_flat)
GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_geometry_noperspective_iface, gp_interp_noperspective)
NO_PERSPECTIVE(float4, thickness)
NO_PERSPECTIVE(float, hardness)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_noperspective)

GPU_SHADER_CREATE_INFO(gpencil_geometry)
DO_STATIC_COMPILATION()
DEFINE("GP_LIGHT")
TYPEDEF_SOURCE("gpencil_defines.hh")
SAMPLER(2, sampler2D, gp_fill_tx)
SAMPLER(3, sampler2D, gp_stroke_tx)
SAMPLER(4, sampler2DDepth, gp_scene_depth_tx)
SAMPLER(5, sampler2D, gp_mask_tx)
UNIFORM_BUF_FREQ(4, gpMaterial, gp_materials[GPENCIL_MATERIAL_BUFFER_LEN], BATCH)
UNIFORM_BUF_FREQ(3, gpLight, gp_lights[GPENCIL_LIGHT_BUFFER_LEN], BATCH)
PUSH_CONSTANT(float2, viewport_size)
/* Per Object */
PUSH_CONSTANT(float3, gp_normal)
PUSH_CONSTANT(bool, gp_stroke_order3d)
PUSH_CONSTANT(int, gp_material_offset)
/* Per Layer */
PUSH_CONSTANT(float, gp_vertex_color_opacity)
PUSH_CONSTANT(float4, gp_layer_tint)
PUSH_CONSTANT(float, gp_layer_opacity)
PUSH_CONSTANT(float, gp_stroke_index_offset)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, revealColor)
VERTEX_OUT(gpencil_geometry_iface)
VERTEX_OUT(gpencil_geometry_flat_iface)
VERTEX_OUT(gpencil_geometry_noperspective_iface)
VERTEX_SOURCE("gpencil_vert.glsl")
FRAGMENT_SOURCE("gpencil_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_gpencil)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Full-Screen Shaders
 * \{ */

GPU_SHADER_CREATE_INFO(gpencil_layer_blend)
DO_STATIC_COMPILATION()
SAMPLER(0, sampler2D, color_buf)
SAMPLER(1, sampler2D, reveal_buf)
SAMPLER(2, sampler2D, mask_buf)
PUSH_CONSTANT(int, blend_mode)
PUSH_CONSTANT(float, blend_opacity)
/* Reminder: This is considered SRC color in blend equations.
 * Same operation on all buffers. */
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, fragRevealage)
FRAGMENT_SOURCE("gpencil_layer_blend_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_mask_invert)
DO_STATIC_COMPILATION()
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, fragRevealage)
FRAGMENT_SOURCE("gpencil_mask_invert_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_depth_merge)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(float4x4, gp_model_matrix)
PUSH_CONSTANT(bool, stroke_order3d)
SAMPLER(0, sampler2DDepth, depth_buf)
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
SMOOTH(float2, uvs)
SMOOTH(float2, pixcoord)
SMOOTH(float4, offset0)
SMOOTH(float4, offset1)
SMOOTH(float4, offset2)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing)
DEFINE("SMAA_GLSL_3")
DEFINE_VALUE("SMAA_RT_METRICS", "viewport_metrics")
DEFINE("SMAA_PRESET_HIGH")
DEFINE_VALUE("SMAA_LUMA_WEIGHT", "float4(luma_weight, luma_weight, luma_weight, 0.0f)")
DEFINE("SMAA_NO_DISCARD")
VERTEX_OUT(gpencil_antialiasing_iface)
PUSH_CONSTANT(float4, viewport_metrics)
PUSH_CONSTANT(float, luma_weight)
VERTEX_SOURCE("gpencil_antialiasing_vert.glsl")
FRAGMENT_SOURCE("gpencil_antialiasing_frag.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_0)
DEFINE_VALUE("SMAA_STAGE", "0")
SAMPLER(0, sampler2D, color_tx)
SAMPLER(1, sampler2D, reveal_tx)
FRAGMENT_OUT(0, float2, out_edges)
ADDITIONAL_INFO(gpencil_antialiasing)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_1)
DEFINE_VALUE("SMAA_STAGE", "1")
SAMPLER(0, sampler2D, edges_tx)
SAMPLER(1, sampler2D, area_tx)
SAMPLER(2, sampler2D, search_tx)
FRAGMENT_OUT(0, float4, out_weights)
ADDITIONAL_INFO(gpencil_antialiasing)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_stage_2)
DEFINE_VALUE("SMAA_STAGE", "2")
SAMPLER(0, sampler2D, color_tx)
SAMPLER(1, sampler2D, reveal_tx)
SAMPLER(2, sampler2D, blend_tx)
PUSH_CONSTANT(float, mix_factor)
PUSH_CONSTANT(float, taa_accumulated_weight)
PUSH_CONSTANT(bool, do_anti_aliasing)
PUSH_CONSTANT(bool, only_alpha)
/* Reminder: Blending func is `fragRevealage * DST + frag_color`. */
FRAGMENT_OUT_DUAL(0, float4, out_color, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, out_reveal, SRC_1)
ADDITIONAL_INFO(gpencil_antialiasing)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_antialiasing_accumulation)
IMAGE(0, GPENCIL_RENDER_FORMAT, read, image2D, src_img)
IMAGE(1, GPENCIL_ACCUM_FORMAT, read_write, image2D, dst_img)
PUSH_CONSTANT(float, weight_src)
PUSH_CONSTANT(float, weight_dst)
FRAGMENT_SOURCE("gpencil_antialiasing_accumulation_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
