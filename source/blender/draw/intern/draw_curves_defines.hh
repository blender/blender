/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * List of defines that are shared with the GPUShaderCreateInfos. We do this to avoid
 * dragging larger headers into the createInfo pipeline which would cause problems.
 */

#pragma once

/** Curves evaluation. */
#define CURVES_PER_THREADGROUP 64

#define POINTS_BY_CURVES_SLOT 0
#define CURVE_TYPE_SLOT 1
#define CURVE_RESOLUTION_SLOT 2
#define EVALUATED_POINT_SLOT 3
#define CURVE_CYCLIC_SLOT 4

#define HANDLES_POS_LEFT_SLOT 5
#define HANDLES_POS_RIGHT_SLOT 6
#define BEZIER_OFFSETS_SLOT 7

/* Nurbs (alias of other buffers). */
#define CURVES_ORDER_SLOT CURVE_RESOLUTION_SLOT
#define BASIS_CACHE_SLOT HANDLES_POS_LEFT_SLOT
#define CONTROL_WEIGHTS_SLOT HANDLES_POS_RIGHT_SLOT
#define BASIS_CACHE_OFFSET_SLOT BEZIER_OFFSETS_SLOT

/* Position evaluation. */
#define POINT_POSITIONS_SLOT 8
#define POINT_RADII_SLOT 9
#define EVALUATED_POS_RAD_SLOT 10

/* Attribute evaluation. */
#define POINT_ATTR_SLOT 8
#define EVALUATED_ATTR_SLOT 9

/* Intercept evaluation. */
#define EVALUATED_TIME_SLOT 8
#define CURVES_LENGTH_SLOT 9
