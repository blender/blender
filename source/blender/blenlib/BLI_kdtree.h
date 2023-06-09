/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
