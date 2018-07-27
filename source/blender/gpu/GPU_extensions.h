/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_extensions.h
 *  \ingroup gpu
 */

#ifndef __GPU_EXTENSIONS_H__
#define __GPU_EXTENSIONS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* GPU extensions support */

bool GPU_full_non_power_of_two_support(void);
bool GPU_bicubic_bump_support(void);

int GPU_max_texture_size(void);
int GPU_max_texture_layers(void);
int GPU_max_textures(void);
float GPU_max_texture_anisotropy(void);
int GPU_max_color_texture_samples(void);
int GPU_max_cube_map_size(void);
int GPU_max_ubo_binds(void);
int GPU_max_ubo_size(void);
int GPU_color_depth(void);
void GPU_get_dfdy_factors(float fac[2]);

bool GPU_mem_stats_supported(void);
void GPU_mem_stats_get(int *totalmem, int *freemem);

void GPU_code_generate_glsl_lib(void);

/* GPU Types */

typedef enum GPUDeviceType {
	GPU_DEVICE_NVIDIA =     (1 << 0),
	GPU_DEVICE_ATI =        (1 << 1),
	GPU_DEVICE_INTEL =      (1 << 2),
	GPU_DEVICE_SOFTWARE =   (1 << 3),
	GPU_DEVICE_UNKNOWN =    (1 << 4),
	GPU_DEVICE_ANY =        (0xff)
} GPUDeviceType;

typedef enum GPUOSType {
	GPU_OS_WIN =            (1 << 8),
	GPU_OS_MAC =            (1 << 9),
	GPU_OS_UNIX =           (1 << 10),
	GPU_OS_ANY =            (0xff00)
} GPUOSType;

typedef enum GPUDriverType {
	GPU_DRIVER_OFFICIAL =   (1 << 16),
	GPU_DRIVER_OPENSOURCE = (1 << 17),
	GPU_DRIVER_SOFTWARE =   (1 << 18),
	GPU_DRIVER_ANY =        (0xff0000)
} GPUDriverType;

bool GPU_type_matches(GPUDeviceType device, GPUOSType os, GPUDriverType driver);

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_EXTENSIONS_H__ */
