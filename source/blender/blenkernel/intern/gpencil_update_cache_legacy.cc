/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <stdio.h>

#include "BKE_gpencil_update_cache_legacy.h"

#include "BLI_dlrbTree.h"
#include "BLI_listbase.h"

#include "BKE_gpencil_legacy.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_userdef_types.h"

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

static short cache_node_compare(void *node, void *data)
{
  int index_a = ((GPencilUpdateCacheNode *)node)->cache->index;
  int index_b = ((GPencilUpdateCache *)data)->index;
  if (index_a == index_b) {
    return 0;
  }
  return index_a < index_b ? 1 : -1;
}

static DLRBT_Node *cache_node_alloc(void *data)
{
  GPencilUpdateCacheNode *new_node = static_cast<GPencilUpdateCacheNode *>(
      MEM_callocN(sizeof(GPencilUpdateCacheNode), __func__));
  new_node->cache = ((GPencilUpdateCache *)data);
  return (DLRBT_Node *)new_node;
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

static void cache_node_update(void *node, void *data)
{
  GPencilUpdateCache *update_cache = ((GPencilUpdateCacheNode *)node)->cache;
  GPencilUpdateCache *new_update_cache = (GPencilUpdateCache *)data;

  /* If the new cache is already "covered" by the current cache, just free it and return. */
  if (new_update_cache->flag <= update_cache->flag) {
    update_cache_free(new_update_cache);
    return;
  }

  update_cache->data = new_update_cache->data;
  update_cache->flag = new_update_cache->flag;

  /* In case the new cache does a full update, remove its children since they will be all
   * updated by this cache. */
  if (new_update_cache->flag == GP_UPDATE_NODE_FULL_COPY) {
    BLI_dlrbTree_free(update_cache->children, cache_node_free);
  }

  update_cache_free(new_update_cache);
}

static void update_cache_node_create_ex(GPencilUpdateCache *root_cache,
                                        void *data,
                                        int gpl_index,
                                        int gpf_index,
                                        int gps_index,
                                        bool full_copy)
{
  if (root_cache->flag == GP_UPDATE_NODE_FULL_COPY) {
    /* Entire data-block has to be recalculated, e.g. nothing else needs to be added to the cache.
     */
    return;
  }

  const int node_flag = full_copy ? GP_UPDATE_NODE_FULL_COPY : GP_UPDATE_NODE_LIGHT_COPY;

  if (gpl_index == -1) {
    root_cache->data = (bGPdata *)data;
    root_cache->flag = node_flag;
    if (full_copy) {
      /* Entire data-block has to be recalculated, remove all caches of "lower" elements. */
      BLI_dlrbTree_free(root_cache->children, cache_node_free);
    }
    return;
  }

  const bool is_layer_update_node = (gpf_index == -1);
  /* If the data pointer in #GPencilUpdateCache is nullptr, this element is not actually cached
   * and does not need to be updated, but we do need the index to find elements that are in
   * levels below. E.g. if a stroke needs to be updated, the frame it is in would not hold a
   * pointer to it's data. */
  GPencilUpdateCache *gpl_cache = update_cache_alloc(
      gpl_index,
      is_layer_update_node ? node_flag : GP_UPDATE_NODE_NO_COPY,
      is_layer_update_node ? (bGPDlayer *)data : nullptr);
  GPencilUpdateCacheNode *gpl_node = (GPencilUpdateCacheNode *)BLI_dlrbTree_add(
      root_cache->children, cache_node_compare, cache_node_alloc, cache_node_update, gpl_cache);

  BLI_dlrbTree_linkedlist_sync(root_cache->children);
  if (gpl_node->cache->flag == GP_UPDATE_NODE_FULL_COPY || is_layer_update_node) {
    return;
  }

  const bool is_frame_update_node = (gps_index == -1);
  GPencilUpdateCache *gpf_cache = update_cache_alloc(
      gpf_index,
      is_frame_update_node ? node_flag : GP_UPDATE_NODE_NO_COPY,
      is_frame_update_node ? (bGPDframe *)data : nullptr);
  GPencilUpdateCacheNode *gpf_node = (GPencilUpdateCacheNode *)BLI_dlrbTree_add(
      gpl_node->cache->children,
      cache_node_compare,
      cache_node_alloc,
      cache_node_update,
      gpf_cache);

  BLI_dlrbTree_linkedlist_sync(gpl_node->cache->children);
  if (gpf_node->cache->flag == GP_UPDATE_NODE_FULL_COPY || is_frame_update_node) {
    return;
  }

  GPencilUpdateCache *gps_cache = update_cache_alloc(gps_index, node_flag, (bGPDstroke *)data);
  BLI_dlrbTree_add(gpf_node->cache->children,
                   cache_node_compare,
                   cache_node_alloc,
                   cache_node_update,
                   gps_cache);

  BLI_dlrbTree_linkedlist_sync(gpf_node->cache->children);
}

static void update_cache_node_create(
    bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps, bool full_copy)
{
  if (gpd == nullptr) {
    return;
  }

  GPencilUpdateCache *root_cache = gpd->runtime.update_cache;
  if (root_cache == nullptr) {
    gpd->runtime.update_cache = update_cache_alloc(0, GP_UPDATE_NODE_NO_COPY, nullptr);
    root_cache = gpd->runtime.update_cache;
  }

  if (root_cache->flag == GP_UPDATE_NODE_FULL_COPY) {
    /* Entire data-block has to be recalculated, e.g. nothing else needs to be added to the cache.
     */
    return;
  }

  const int gpl_index = (gpl != nullptr) ? BLI_findindex(&gpd->layers, gpl) : -1;
  const int gpf_index = (gpl != nullptr && gpf != nullptr) ? BLI_findindex(&gpl->frames, gpf) : -1;
  const int gps_index = (gpf != nullptr && gps != nullptr) ? BLI_findindex(&gpf->strokes, gps) :
                                                             -1;

  void *data = gps;
  if (!data) {
    data = gpf;
  }
  if (!data) {
    data = gpl;
  }
  if (!data) {
    data = gpd;
  }

  update_cache_node_create_ex(root_cache, data, gpl_index, gpf_index, gps_index, full_copy);
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

void BKE_gpencil_tag_full_update(bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps)
{
  update_cache_node_create(gpd, gpl, gpf, gps, true);
}

void BKE_gpencil_tag_light_update(bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps)
{
  update_cache_node_create(gpd, gpl, gpf, gps, false);
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
