/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_print)
    .storage_buf(
        GPU_SHADER_PRINTF_SLOT, Qualifier::READ_WRITE, "uint", "gpu_print_buf[]", Frequency::PASS)
    .define("GPU_SHADER_PRINTF_MAX_CAPACITY", STRINGIFY(GPU_SHADER_PRINTF_MAX_CAPACITY))
    .define("GPU_PRINT");
