/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "eevee_sampling_shared.hh"

#  define EEVEE_SAMPLING_DATA
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_sampling_data)
DEFINE("EEVEE_SAMPLING_DATA")
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_sampling_shared.hh")
STORAGE_BUF(SAMPLING_BUF_SLOT, read, SamplingData, sampling_buf)
GPU_SHADER_CREATE_END()
