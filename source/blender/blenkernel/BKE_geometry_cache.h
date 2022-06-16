/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct GeometryCache *BKE_geometry_cache_new();
void BKE_geometry_cache_free(struct GeometryCache *cache);

void BKE_geometry_cache_append(struct GeometryCache *cache,
                               int cfra,
                               const struct GeometrySet *geometry_set);

#ifdef __cplusplus
}
#endif
