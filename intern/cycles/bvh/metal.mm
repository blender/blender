/*
 * Copyright 2021 Blender Foundation
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

#ifdef WITH_METAL

#  include "device/metal/bvh.h"

CCL_NAMESPACE_BEGIN

BVH *bvh_metal_create(const BVHParams &params,
                      const vector<Geometry *> &geometry,
                      const vector<Object *> &objects,
                      Device *device)
{
  return new BVHMetal(params, geometry, objects, device);
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
