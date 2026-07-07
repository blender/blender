/* SPDX-FileCopyrightText: 2022-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"
#  include "gpu_srgb_to_framebuffer_space_infos.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(text_iface)
FLAT(float4, color_flat)
NO_PERSPECTIVE(float2, texCoord_interp)
FLAT(int, glyph_offset)
FLAT(uint, glyph_flags)
FLAT(int2, glyph_dim)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_text)
VERTEX_OUT(text_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT(int, glyph_tex_width_mask)
PUSH_CONSTANT(int, glyph_tex_width_shift)
SAMPLER_FREQ(0, sampler2D, glyph, PASS)
STORAGE_BUF(0, read, GlyphQuad, glyphs[])
TYPEDEF_SOURCE("GPU_shader_shared.hh")
VERTEX_SOURCE("gpu_shader_text_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_text_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space) DO_STATIC_COMPILATION() GPU_SHADER_CREATE_END()
