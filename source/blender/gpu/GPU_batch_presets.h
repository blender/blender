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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* Batched geometry rendering is powered by the GPU library.
 * This file contains any additions or modifications specific to Blender.
 */

#ifndef __GPU_BATCH_PRESETS_H__
#define __GPU_BATCH_PRESETS_H__

struct rctf;
struct GPUVertFormat;

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

/* gpu_batch_presets.c */
struct GPUVertFormat *GPU_batch_preset_format_3d(void);

/* Replacement for gluSphere */
struct GPUBatch *GPU_batch_preset_sphere(int lod) ATTR_WARN_UNUSED_RESULT;
struct GPUBatch *GPU_batch_preset_sphere_wire(int lod) ATTR_WARN_UNUSED_RESULT;

void gpu_batch_presets_init(void);
void gpu_batch_presets_register(struct GPUBatch *preset_batch);
void gpu_batch_presets_reset(void);
void gpu_batch_presets_exit(void);

#endif  /* __GPU_BATCH_PRESETS_H__ */
