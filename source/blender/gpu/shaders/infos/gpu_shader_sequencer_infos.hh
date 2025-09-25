/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"
#  include "gpu_shader_fullscreen_infos.hh"
#endif

#include "gpu_interface_infos.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_seq_strip_iface)
NO_PERSPECTIVE(float2, co_interp)
FLAT(uint, strip_id)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_sequencer_strips)
VERTEX_OUT(gpu_seq_strip_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
UNIFORM_BUF(0, SeqStripDrawData, strip_data[GPU_SEQ_STRIP_DRAW_DATA_LEN])
UNIFORM_BUF(1, SeqContextDrawData, context_data)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_sequencer_strips_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_sequencer_strips_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(gpu_seq_thumb_iface)
NO_PERSPECTIVE(float2, pos_interp)
NO_PERSPECTIVE(float2, texCoord_interp)
FLAT(uint, thumb_id)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_sequencer_thumbs)
VERTEX_OUT(gpu_seq_thumb_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
UNIFORM_BUF(0, SeqStripThumbData, thumb_data[GPU_SEQ_STRIP_DRAW_DATA_LEN])
UNIFORM_BUF(1, SeqContextDrawData, context_data)
SAMPLER(0, sampler2D, image)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_sequencer_thumbs_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_sequencer_thumbs_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(gpu_seq_scope_iface)
SMOOTH(float4, finalColor)
SMOOTH(float2, radii)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_sequencer_scope_raster)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float3, luma_coeffs)
PUSH_CONSTANT(float, scope_point_size)
PUSH_CONSTANT(bool, img_premultiplied)
PUSH_CONSTANT(int, image_width)
PUSH_CONSTANT(int, image_height)
PUSH_CONSTANT(int, scope_mode)
PUSH_CONSTANT(int, view_width)
PUSH_CONSTANT(int, view_height)
SAMPLER(0, sampler2D, image)
STORAGE_BUF(0, read_write, SeqScopeRasterData, raster_buf[])
TYPEDEF_SOURCE("GPU_shader_shared.hh")
COMPUTE_SOURCE("gpu_shader_sequencer_scope_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_sequencer_scope_resolve)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(int, view_width)
PUSH_CONSTANT(int, view_height)
PUSH_CONSTANT(float, alpha_exponent)
STORAGE_BUF(0, read, SeqScopeRasterData, raster_buf[])
TYPEDEF_SOURCE("GPU_shader_shared.hh")
FRAGMENT_SOURCE("gpu_shader_sequencer_scope_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_sequencer_zebra)
VERTEX_IN(0, float2, pos)
VERTEX_IN(1, float2, texCoord)
VERTEX_OUT(smooth_tex_coord_interp_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(float, zebra_limit)
PUSH_CONSTANT(bool, img_premultiplied)
SAMPLER(0, sampler2D, image)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_2D_image_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_sequencer_zebra_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
