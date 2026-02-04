/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_raycast)
DEFINE("MAT_RAYCAST")
SAMPLER(OBJECT_ID_TEX_SLOT, usampler2D, object_id_tx)
SAMPLER(PREPASS_NORMAL_TEX_SLOT, sampler2D, prepass_normal_tx)
GPU_SHADER_CREATE_END()
