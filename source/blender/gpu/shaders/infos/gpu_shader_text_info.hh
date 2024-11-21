/* SPDX-FileCopyrightText: 2022-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "GPU_shader_shared.hh"
#  include "gpu_srgb_to_framebuffer_space_info.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(text_iface)
FLAT(VEC4, color_flat)
NO_PERSPECTIVE(VEC2, texCoord_interp)
FLAT(INT, glyph_offset)
FLAT(UINT, glyph_flags)
FLAT(IVEC2, glyph_dim)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_text)
VERTEX_IN(0, VEC4, pos)
VERTEX_IN(1, VEC4, col)
VERTEX_IN(2, IVEC2, glyph_size)
VERTEX_IN(3, INT, offset)
VERTEX_IN(4, UINT, flags)
VERTEX_OUT(text_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(INT, glyph_tex_width_mask)
PUSH_CONSTANT(INT, glyph_tex_width_shift)
SAMPLER_FREQ(0, FLOAT_2D, glyph, PASS)
VERTEX_SOURCE("gpu_shader_text_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_text_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space) DO_STATIC_COMPILATION() GPU_SHADER_CREATE_END()
