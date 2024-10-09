/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>

#include "BKE_gpencil_update_cache_legacy.h"

#include "BLI_dlrbTree.h"
#include "BLI_listbase.h"

#include "DNA_gpencil_legacy_types.h"

#include "MEM_guardedalloc.h"

static GPencilUpdateCache *update_cache_alloc(int index, int flag, void *data)
{
  GPencilUpdateCache *new_cache = static_cast<GPencilUpdateCache *>(
      MEM_callocN(sizeof(GPencilUpdateCache), __func__));
  new_cache->children = BLI_dlrbTree_new();
  new_cache->flag = flag;
  new_cache->index = index;
  new_cache->data = data;

  return new_cache;
}

static void cache_node_free(void *node);

static void update_cache_free(GPencilUpdateCache *cache)
{
  BLI_dlrbTree_free(cache->children, cache_node_free);
  MEM_SAFE_FREE(cache->children);
  MEM_freeN(cache);
}

static void cache_node_free(void *node)
{
  GPencilUpdateCache *cache = ((GPencilUpdateCacheNode *)node)->cache;
  if (cache != nullptr) {
    update_cache_free(cache);
  }
  MEM_freeN(node);
}

static void gpencil_traverse_update_cache_ex(GPencilUpdateCache *parent_cache,
                                             GPencilUpdateCacheTraverseSettings *ts,
                                             int depth,
                                             void *user_data)
{
  if (BLI_listbase_is_empty((ListBase *)parent_cache->children)) {
    return;
  }

  LISTBASE_FOREACH (GPencilUpdateCacheNode *, cache_node, parent_cache->children) {
    GPencilUpdateCache *cache = cache_node->cache;

    GPencilUpdateCacheIter_Cb cb = ts->update_cache_cb[depth];
    if (cb != nullptr) {
      bool skip = cb(cache, user_data);
      if (skip) {
        continue;
      }
    }

    gpencil_traverse_update_cache_ex(cache, ts, depth + 1, user_data);
  }
}

/* -------------------------------------------------------------------- */
/** \name Update Cache API
 *
 * \{ */

GPencilUpdateCache *BKE_gpencil_create_update_cache(void *data, bool full_copy)
{
  return update_cache_alloc(
      0, full_copy ? GP_UPDATE_NODE_FULL_COPY : GP_UPDATE_NODE_LIGHT_COPY, data);
}

void BKE_gpencil_traverse_update_cache(GPencilUpdateCache *cache,
                                       GPencilUpdateCacheTraverseSettings *ts,
                                       void *user_data)
{
  gpencil_traverse_update_cache_ex(cache, ts, 0, user_data);
}

void BKE_gpencil_free_update_cache(bGPdata *gpd)
{
  GPencilUpdateCache *gpd_cache = gpd->runtime.update_cache;
  if (gpd_cache) {
    update_cache_free(gpd_cache);
    gpd->runtime.update_cache = nullptr;
  }
}

/** \} */
