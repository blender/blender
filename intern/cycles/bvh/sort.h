/* SPDX-FileCopyrightText: 2009-2010 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted code from NVIDIA Corporation. */

#pragma once

CCL_NAMESPACE_BEGIN

class BVHReference;
class BVHUnaligned;
struct Transform;

void bvh_reference_sort(const int start,
                        const int end,
                        BVHReference *data,
                        const int dim,
                        const BVHUnaligned *unaligned_heuristic = nullptr,
                        const Transform *aligned_space = nullptr);

CCL_NAMESPACE_END
