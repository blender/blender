/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup gpu
 *
 * Descriptor type used to define shader structure, resources and interfaces.
 *
 * Some rule of thumb:
 * - Do not include anything else than this file in each descriptor file.
 */

#pragma once

#include "GPU_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

void gpu_shader_create_info_init(void);
void gpu_shader_create_info_exit(void);

bool gpu_shader_create_info_compile_all(void);

/** Runtime create infos are not registered in the dictionary and cannot be searched. */
const GPUShaderCreateInfo *gpu_shader_create_info_get(const char *info_name);

#ifdef __cplusplus
}
#endif
