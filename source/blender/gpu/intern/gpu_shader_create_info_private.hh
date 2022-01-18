/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

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

const GPUShaderCreateInfo *gpu_shader_create_info_get(const char *info_name);

#ifdef __cplusplus
}
#endif
