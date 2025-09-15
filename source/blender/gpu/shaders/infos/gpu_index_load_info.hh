/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"

#  define GPU_INDEX_LOAD
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_index_buffer_load)
PUSH_CONSTANT(bool, gpu_index_no_buffer)
PUSH_CONSTANT(bool, gpu_index_16bit)
PUSH_CONSTANT(int, gpu_index_base_index)
STORAGE_BUF_FREQ(GPU_SSBO_INDEX_BUF_SLOT, read, uint, gpu_index_buf[], GEOMETRY)
DEFINE("GPU_INDEX_LOAD")
GPU_SHADER_CREATE_END()
