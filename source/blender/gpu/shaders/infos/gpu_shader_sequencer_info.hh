/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "GPU_shader_shared.hh"
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_seq_strip_iface)
NO_PERSPECTIVE(VEC2, co_interp)
FLAT(UINT, strip_id)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_sequencer_strips)
VERTEX_OUT(gpu_seq_strip_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
UNIFORM_BUF(0, SeqStripDrawData, strip_data[GPU_SEQ_STRIP_DRAW_DATA_LEN])
UNIFORM_BUF(1, SeqContextDrawData, context_data)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_sequencer_strips_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_sequencer_strips_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(gpu_seq_thumb_iface)
NO_PERSPECTIVE(VEC2, pos_interp)
NO_PERSPECTIVE(VEC2, texCoord_interp)
FLAT(UINT, thumb_id)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_sequencer_thumbs)
VERTEX_OUT(gpu_seq_thumb_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
UNIFORM_BUF(0, SeqStripThumbData, thumb_data[GPU_SEQ_STRIP_DRAW_DATA_LEN])
UNIFORM_BUF(1, SeqContextDrawData, context_data)
SAMPLER(0, FLOAT_2D, image)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_sequencer_thumbs_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_sequencer_thumbs_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
