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
 * Shader source dependency builder that make possible to support #include directive inside the
 * shader files.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void gpu_shader_dependency_init(void);

void gpu_shader_dependency_exit(void);

/* User must free the resulting string using free. */
char *gpu_shader_dependency_get_resolved_source(const char *shader_source_name,
                                                uint32_t *builtins);
char *gpu_shader_dependency_get_source(const char *shader_source_name);

#ifdef __cplusplus
}
#endif
