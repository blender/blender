/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __BVH_METAL_H__
#define __BVH_METAL_H__

#ifdef WITH_METAL

#  include "bvh/bvh.h"

CCL_NAMESPACE_BEGIN

BVH *bvh_metal_create(const BVHParams &params,
                      const vector<Geometry *> &geometry,
                      const vector<Object *> &objects,
                      Device *device);

CCL_NAMESPACE_END

#endif /* WITH_METAL */

#endif /* __BVH_METAL_H__ */
