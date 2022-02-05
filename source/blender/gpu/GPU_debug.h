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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Helpers for GPU / drawing debugging.
 */

#pragma once

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GPU_DEBUG_SHADER_COMPILATION_GROUP "Shader Compilation"

void GPU_debug_group_begin(const char *name);
void GPU_debug_group_end(void);
/**
 * Return a formatted string showing the current group hierarchy in this format:
 * "Group1 > Group 2 > Group3 > ... > GroupN : "
 */
void GPU_debug_get_groups_names(int name_buf_len, char *r_name_buf);
/**
 * Return true if inside a debug group with the same name.
 */
bool GPU_debug_group_match(const char *ref);

#ifdef __cplusplus
}
#endif
