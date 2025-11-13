/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "eevee_uniform_shared.hh"

#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_global_ubo)
TYPEDEF_SOURCE("eevee_uniform_shared.hh")
UNIFORM_BUF(UNIFORM_BUF_SLOT, UniformData, uniform_buf)
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_clip_plane_iface, clip_interp)
SMOOTH(float, clip_distance)
GPU_SHADER_NAMED_INTERFACE_END(clip_interp)

GPU_SHADER_CREATE_INFO(eevee_clip_plane)
VERTEX_OUT(eevee_clip_plane_iface)
TYPEDEF_SOURCE("eevee_uniform_shared.hh")
UNIFORM_BUF(CLIP_PLANE_BUF, ClipPlaneData, clip_plane)
DEFINE("MAT_CLIP_PLANE")
GPU_SHADER_CREATE_END()
