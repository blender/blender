/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"

#  define USE_WORLD_CLIP_PLANES
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_clip_planes)
UNIFORM_BUF_FREQ(1, GPUClipPlanes, clipPlanes, PASS)
BUILTINS(BuiltinBits::CLIP_DISTANCES)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
DEFINE("USE_WORLD_CLIP_PLANES")
GPU_SHADER_CREATE_END()
