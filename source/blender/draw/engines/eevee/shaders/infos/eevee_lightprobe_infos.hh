/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Common public resources to use the light-probes.
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "eevee_lightprobe_shared.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_surfel_common)
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
STORAGE_BUF(SURFEL_BUF_SLOT, read_write, Surfel, surfel_buf[])
STORAGE_BUF(CAPTURE_BUF_SLOT, read, CaptureInfoData, capture_info_buf)
GPU_SHADER_CREATE_END()
