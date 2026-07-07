/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "eevee_light_shared.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_light_data)
TYPEDEF_SOURCE("eevee_light_shared.hh")
STORAGE_BUF(LIGHT_CULL_BUF_SLOT, read, LightCullingData, light_cull_buf)
STORAGE_BUF(LIGHT_BUF_SLOT, read, LightData, light_buf[])
STORAGE_BUF(LIGHT_ZBIN_BUF_SLOT, read, uint, light_zbin_buf[])
STORAGE_BUF(LIGHT_TILE_BUF_SLOT, read, uint, light_tile_buf[])
GPU_SHADER_CREATE_END()
