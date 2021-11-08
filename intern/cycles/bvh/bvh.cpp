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

#include "bvh/bvh.h"

#include "bvh/bvh2.h"
#include "bvh/embree.h"
#include "bvh/multi.h"
#include "bvh/optix.h"

#include "util/log.h"
#include "util/progress.h"

CCL_NAMESPACE_BEGIN

/* BVH Parameters. */

const char *bvh_layout_name(BVHLayout layout)
{
  switch (layout) {
    case BVH_LAYOUT_NONE:
      return "NONE";
    case BVH_LAYOUT_BVH2:
      return "BVH2";
    case BVH_LAYOUT_EMBREE:
      return "EMBREE";
    case BVH_LAYOUT_OPTIX:
      return "OPTIX";
    case BVH_LAYOUT_MULTI_OPTIX:
    case BVH_LAYOUT_MULTI_OPTIX_EMBREE:
      return "MULTI";
    case BVH_LAYOUT_ALL:
      return "ALL";
  }
  LOG(DFATAL) << "Unsupported BVH layout was passed.";
  return "";
}

BVHLayout BVHParams::best_bvh_layout(BVHLayout requested_layout, BVHLayoutMask supported_layouts)
{
  const BVHLayoutMask requested_layout_mask = (BVHLayoutMask)requested_layout;
  /* Check whether requested layout is supported, if so -- no need to do
   * any extra computation.
   */
  if (supported_layouts & requested_layout_mask) {
    return requested_layout;
  }
  /* Some bit magic to get widest supported BVH layout. */
  /* This is a mask of supported BVH layouts which are narrower than the
   * requested one.
   */
  BVHLayoutMask allowed_layouts_mask = (supported_layouts & (requested_layout_mask - 1));
  /* If the requested layout is not supported, choose from the supported layouts instead. */
  if (allowed_layouts_mask == 0) {
    allowed_layouts_mask = supported_layouts;
  }
  /* We get widest from allowed ones and convert mask to actual layout. */
  const BVHLayoutMask widest_allowed_layout_mask = __bsr((uint32_t)allowed_layouts_mask);
  return (BVHLayout)(1 << widest_allowed_layout_mask);
}

/* BVH */

BVH::BVH(const BVHParams &params_,
         const vector<Geometry *> &geometry_,
         const vector<Object *> &objects_)
    : params(params_), geometry(geometry_), objects(objects_)
{
}

BVH *BVH::create(const BVHParams &params,
                 const vector<Geometry *> &geometry,
                 const vector<Object *> &objects,
                 Device *device)
{
  switch (params.bvh_layout) {
    case BVH_LAYOUT_BVH2:
      return new BVH2(params, geometry, objects);
    case BVH_LAYOUT_EMBREE:
#ifdef WITH_EMBREE
      return new BVHEmbree(params, geometry, objects);
#else
      break;
#endif
    case BVH_LAYOUT_OPTIX:
#ifdef WITH_OPTIX
      return new BVHOptiX(params, geometry, objects, device);
#else
      (void)device;
      break;
#endif
    case BVH_LAYOUT_MULTI_OPTIX:
    case BVH_LAYOUT_MULTI_OPTIX_EMBREE:
      return new BVHMulti(params, geometry, objects);
    case BVH_LAYOUT_NONE:
    case BVH_LAYOUT_ALL:
      break;
  }
  LOG(DFATAL) << "Requested unsupported BVH layout.";
  return NULL;
}

CCL_NAMESPACE_END
