/* SPDX-FileCopyrightText: 2009-2010 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted code from NVIDIA Corporation. */

#ifndef __BVH_SORT_H__
#define __BVH_SORT_H__

#include <cstddef>

CCL_NAMESPACE_BEGIN

class BVHReference;
class BVHUnaligned;
struct Transform;

void bvh_reference_sort(int start,
                        int end,
                        BVHReference *data,
                        int dim,
                        const BVHUnaligned *unaligned_heuristic = NULL,
                        const Transform *aligned_space = NULL);

CCL_NAMESPACE_END

#endif /* __BVH_SORT_H__ */
