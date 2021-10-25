/*
 * Adapted from code copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
