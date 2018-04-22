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

/* Batched geometry rendering is powered by the Gawain library.
 * This file contains any additions or modifications specific to Blender.
 */

#ifndef __GPU_BATCH_H__
#define __GPU_BATCH_H__

#include "../../../intern/gawain/gawain/gwn_batch.h"
#include "../../../intern/gawain/gawain/gwn_batch_private.h"

struct rctf;

// TODO: CMake magic to do this:
// #include "gawain/batch.h"

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#include "GPU_shader.h"

/* Extend GWN_batch_program_set to use Blender’s library of built-in shader programs. */

/* gpu_batch.c */
void GWN_batch_program_set_builtin(Gwn_Batch *batch, GPUBuiltinShader shader_id) ATTR_NONNULL(1);

Gwn_Batch *GPU_batch_tris_from_poly_2d_encoded(
        const uchar *polys_flat, uint polys_flat_len, const struct rctf *rect
        ) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
Gwn_Batch *GPU_batch_wire_from_poly_2d_encoded(
        const uchar *polys_flat, uint polys_flat_len, const struct rctf *rect
        ) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

void gpu_batch_init(void);
void gpu_batch_exit(void);

/* gpu_batch_presets.c */
/* Only use by draw manager. Use the presets function instead for interface. */
Gwn_Batch *gpu_batch_sphere(int lat_res, int lon_res) ATTR_WARN_UNUSED_RESULT;
/* Replacement for gluSphere */
Gwn_Batch *GPU_batch_preset_sphere(int lod) ATTR_WARN_UNUSED_RESULT;
Gwn_Batch *GPU_batch_preset_sphere_wire(int lod) ATTR_WARN_UNUSED_RESULT;

void gpu_batch_presets_init(void);
void gpu_batch_presets_register(Gwn_Batch *preset_batch);
void gpu_batch_presets_reset(void);
void gpu_batch_presets_exit(void);

#endif  /* __GPU_BATCH_H__ */
