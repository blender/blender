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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 * \ingroup bli
 * \brief A KD-tree for nearest neighbor search.
 */

/* 1D version */
#define KD_DIMS 1
#define KDTREE_PREFIX_ID BLI_kdtree_1d
#define KDTree KDTree_1d
#define KDTreeNearest KDTreeNearest_1d
#include "BLI_kdtree_impl.h"
#undef KD_DIMS
#undef KDTree
#undef KDTreeNearest
#undef KDTREE_PREFIX_ID

/* 2D version */
#define KD_DIMS 2
#define KDTREE_PREFIX_ID BLI_kdtree_2d
#define KDTree KDTree_2d
#define KDTreeNearest KDTreeNearest_2d
#include "BLI_kdtree_impl.h"
#undef KD_DIMS
#undef KDTree
#undef KDTreeNearest
#undef KDTREE_PREFIX_ID

/* 3D version */
#define KD_DIMS 3
#define KDTREE_PREFIX_ID BLI_kdtree_3d
#define KDTree KDTree_3d
#define KDTreeNearest KDTreeNearest_3d
#include "BLI_kdtree_impl.h"
#undef KD_DIMS
#undef KDTree
#undef KDTreeNearest
#undef KDTREE_PREFIX_ID

/* 4D version */
#define KD_DIMS 4
#define KDTREE_PREFIX_ID BLI_kdtree_4d
#define KDTree KDTree_4d
#define KDTreeNearest KDTreeNearest_4d
#include "BLI_kdtree_impl.h"
#undef KD_DIMS
#undef KDTree
#undef KDTreeNearest
#undef KDTREE_PREFIX_ID

#ifdef __cplusplus
}
#endif
