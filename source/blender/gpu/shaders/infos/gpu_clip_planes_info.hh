/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_clip_planes)
UNIFORM_BUF_FREQ(1, GPUClipPlanes, clipPlanes, PASS)
TYPEDEF_SOURCE("GPU_shader_shared.hh")
DEFINE("USE_WORLD_CLIP_PLANES")
GPU_SHADER_CREATE_END()
