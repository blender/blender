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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __GPU_BATCH_UTILS_H__
#define __GPU_BATCH_UTILS_H__

struct rctf;

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

/* gpu_batch_utils.c */
struct GPUBatch *GPU_batch_tris_from_poly_2d_encoded(
        const uchar *polys_flat, uint polys_flat_len, const struct rctf *rect
        ) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
struct GPUBatch *GPU_batch_wire_from_poly_2d_encoded(
        const uchar *polys_flat, uint polys_flat_len, const struct rctf *rect
        ) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/* Only use by draw manager. Use the presets function instead for interface. */
struct GPUBatch *gpu_batch_sphere(int lat_res, int lon_res) ATTR_WARN_UNUSED_RESULT;

#endif  /* __GPU_BATCH_UTILS_H__ */
