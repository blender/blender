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
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* call this before running any of the functions below */
void gpu_platform_init(void);
void gpu_platform_exit(void);

/* call this before running any of the functions below */
void gpu_extensions_init(void);
void gpu_extensions_exit(void);

/* gpu_debug.c */
void gpu_debug_init(void);
void gpu_debug_exit(void);

/* gpu_framebuffer.c */
void gpu_framebuffer_module_init(void);
void gpu_framebuffer_module_exit(void);

/* gpu_pbvh.c */
void gpu_pbvh_init(void);
void gpu_pbvh_exit(void);

#ifdef __cplusplus
}
#endif
