/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/guarded_allocator.h"
#include "util/stats.h"

CCL_NAMESPACE_BEGIN

static Stats global_stats(Stats::static_init);

/* Internal API. */

void util_guarded_mem_alloc(size_t n)
{
  global_stats.mem_alloc(n);
}

void util_guarded_mem_free(size_t n)
{
  global_stats.mem_free(n);
}

/* Public API. */

size_t util_guarded_get_mem_used()
{
  return global_stats.mem_used;
}

size_t util_guarded_get_mem_peak()
{
  return global_stats.mem_peak;
}

CCL_NAMESPACE_END
