/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_METAL

#  include "bvh/bvh.h"

#  include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

unique_ptr<BVH> bvh_metal_create(const BVHParams &params,
                                 const vector<Geometry *> &geometry,
                                 const vector<Object *> &objects,
                                 Device *device);

CCL_NAMESPACE_END

#endif /* WITH_METAL */
