/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_sys_types.h"

/** \file
 * \ingroup freestyle
 * \brief Different natures for both vertices and edges
 */

namespace Freestyle {

/** Namespace gathering the different possible natures of 0D and 1D elements of the ViewMap */
namespace Nature {

/* XXX Why not using enums??? */
/* In order to optimize for space (enum is int) - T.K. */

typedef ushort VertexNature;
/** true for any 0D element */
static const VertexNature POINT = 0;  // 0
/** true for SVertex */
static const VertexNature S_VERTEX = (1 << 0);  // 1
/** true for ViewVertex */
static const VertexNature VIEW_VERTEX = (1 << 1);  // 2
/** true for NonTVertex */
static const VertexNature NON_T_VERTEX = (1 << 2);  // 4
/** true for TVertex */
static const VertexNature T_VERTEX = (1 << 3);  // 8
/** true for CUSP */
static const VertexNature CUSP = (1 << 4);  // 16

typedef ushort EdgeNature;
/** true for non feature edges (always false for 1D elements of the ViewMap) */
static const EdgeNature NO_FEATURE = 0;  // 0
/** true for silhouettes */
static const EdgeNature SILHOUETTE = (1 << 0);  // 1
/** true for borders */
static const EdgeNature BORDER = (1 << 1);  // 2
/** true for creases */
static const EdgeNature CREASE = (1 << 2);  // 4
/** true for ridges */
static const EdgeNature RIDGE = (1 << 3);  // 8
/** true for valleys */
static const EdgeNature VALLEY = (1 << 4);  // 16
/** true for suggestive contours */
static const EdgeNature SUGGESTIVE_CONTOUR = (1 << 5);  // 32
/** true for material boundaries */
static const EdgeNature MATERIAL_BOUNDARY = (1 << 6);  // 64
/** true for user-defined edge marks */
static const EdgeNature EDGE_MARK = (1 << 7);  // 128

}  // end of namespace Nature

} /* namespace Freestyle */
