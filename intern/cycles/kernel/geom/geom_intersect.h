/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Common utilities for various geometry type intersections. */

#pragma once

#include "kernel/globals.h"
#include "kernel/sample/lcg.h"

CCL_NAMESPACE_BEGIN

/* For an intersection with the given distance isect_t from the ray origin increase the number
 * of hits (when needed) and return an index within local_isect->hits where intersection is to
 * be stored. If the return value -1 then the intersection is to be ignored (nothing is to be
 * written to the local_isect->hits and intersection test function is to return false. */
#ifdef __BVH_LOCAL__
ccl_device_forceinline int local_intersect_get_record_index(
    ccl_private LocalIntersection *local_isect,
    const float isect_t,
    ccl_private uint *lcg_state,
    const int max_hits)
{
  if (lcg_state) {
    /* Record up to max_hits intersections. */
    for (int i = min(max_hits, local_isect->num_hits) - 1; i >= 0; --i) {
      if (local_isect->hits[i].t == isect_t) {
        return -1;
      }
    }

    local_isect->num_hits++;

    int hit;
    if (local_isect->num_hits <= max_hits) {
      hit = local_isect->num_hits - 1;
    }
    else {
      /* Reservoir sampling: if we are at the maximum number of hits, randomly replace element or
       * skip it. */
      hit = lcg_step_uint(lcg_state) % local_isect->num_hits;
      if (hit >= max_hits) {
        return -1;
      }
    }
    return hit;
  }

  /* Record closest intersection only. */
  if (local_isect->num_hits && isect_t > local_isect->hits[0].t) {
    return -1;
  }
  local_isect->num_hits = 1;
  return 0;
}
#endif /* __BVH_LOCAL__ */

CCL_NAMESPACE_END
