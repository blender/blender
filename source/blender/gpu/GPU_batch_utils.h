/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rctf;

/* `gpu_batch_utils.cc` */

/**
 * Creates triangles from a byte-array of polygons.
 *
 * See 'make_shape_2d_from_blend.py' utility to create data to pass to this function.
 *
 * \param polys_flat: Pairs of X, Y coordinates (repeating to signify closing the polygon).
 * \param polys_flat_len: Length of the array (must be an even number).
 * \param rect: Optional region to map the byte 0..255 coords to. When not set use -1..1.
 */
struct GPUBatch *GPU_batch_tris_from_poly_2d_encoded(
    const uchar *polys_flat, uint polys_flat_len, const struct rctf *rect) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
struct GPUBatch *GPU_batch_wire_from_poly_2d_encoded(
    const uchar *polys_flat, uint polys_flat_len, const struct rctf *rect) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);

/**
 * Replacement for #gluSphere.
 *
 * \note Only use by draw manager. Use the presets function instead for interface.
 */
struct GPUBatch *gpu_batch_sphere(int lat_res, int lon_res) ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif
