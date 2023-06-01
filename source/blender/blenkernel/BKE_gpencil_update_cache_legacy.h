/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_sys_types.h" /* for bool */

struct DLRBT_Tree;
struct GPencilUpdateCache;
struct bGPDframe;
struct bGPDlayer;
struct bGPDstroke;
struct bGPdata;

/** #GPencilUpdateCache.flag */
typedef enum eGPUpdateCacheNodeFlag {
  /* Node is a placeholder (e.g. when only an index is needed). */
  GP_UPDATE_NODE_NO_COPY = 0,
  /* Copy only element, not the content. */
  GP_UPDATE_NODE_LIGHT_COPY = 1,
  /* Copy the element as well as all of its content. */
  GP_UPDATE_NODE_FULL_COPY = 2,
} eGPUpdateCacheNodeFlag;

/**
 * Cache for what needs to be updated after bGPdata was modified.
 *
 * Every node holds information about one element that was changed:
 * - The index of where that element is in the linked-list.
 * - The pointer to the original element in bGPdata.
 *
 * Additionally, nodes also hold other nodes that are one "level" below them.
 * E.g. a node that represents a change on a bGPDframe could contain a set of
 * nodes that represent a change on bGPDstrokes.
 * These nodes are stored in a red-black tree so that they are sorted by their
 * index to make sure they can be processed in the correct order.
 */
typedef struct GPencilUpdateCache {
  /* Mapping from index to a GPencilUpdateCache struct. */
  struct DLRBT_Tree *children;
  /* eGPUpdateCacheNodeFlag */
  int flag;
  /* Index of the element in the linked-list. */
  int index;
  /* Pointer to one of bGPdata, bGPDLayer, bGPDFrame, bGPDStroke. */
  void *data;
} GPencilUpdateCache;

/* Node structure in the DLRBT_Tree for GPencilUpdateCache mapping. */
typedef struct GPencilUpdateCacheNode {
  /* DLRB tree capabilities. */
  struct GPencilUpdateCacheNode *next, *prev;
  struct GPencilUpdateCacheNode *left, *right;
  struct GPencilUpdateCacheNode *parent;
  char tree_col;

  char _pad[7];
  /* Content of DLRB tree node. */
  GPencilUpdateCache *cache;
} GPencilUpdateCacheNode;

/**
 * Callback that is called in BKE_gpencil_traverse_update_cache at each level. If the callback
 * returns true, then the children will not be iterated over and instead continue.
 * \param cache: The cache at this level.
 * \param user_data: Pointer to the user_data passed to BKE_gpencil_traverse_update_cache.
 * \returns true, if iterating over the children of \a cache should be skipped, false if not.
 */
typedef bool (*GPencilUpdateCacheIter_Cb)(GPencilUpdateCache *cache, void *user_data);

typedef struct GPencilUpdateCacheTraverseSettings {
  /* Callbacks for the update cache traversal. Callback with index 0 is for layers, 1 for frames
   * and 2 for strokes. */
  GPencilUpdateCacheIter_Cb update_cache_cb[3];
} GPencilUpdateCacheTraverseSettings;

/**
 * Allocates a new GPencilUpdateCache and populates it.
 * \param data: A data pointer to populate the initial cache with.
 * \param full_copy: If true, will mark this update cache as a full copy
 * (GP_UPDATE_NODE_FULL_COPY). If false, it will be marked as a struct copy
 * (GP_UPDATE_NODE_LIGHT_COPY).
 */
GPencilUpdateCache *BKE_gpencil_create_update_cache(void *data, bool full_copy);

/**
 * Traverses an update cache and executes callbacks at each level.
 * \param cache: The update cache to traverse.
 * \param ts: The traversal settings. This stores the callbacks that are called at each level.
 * \param user_data: Custom data passed to each callback.
 */
void BKE_gpencil_traverse_update_cache(GPencilUpdateCache *cache,
                                       GPencilUpdateCacheTraverseSettings *ts,
                                       void *user_data);

/**
 * Tags an element (bGPdata, bGPDlayer, bGPDframe, or bGPDstroke) and all of its containing data to
 * be updated in the next update-on-write operation.
 *
 * The function assumes that when a parameter is NULL all of the following parameters are NULL too.
 * E.g. in order to tag a layer (gpl), the parameters would *have* to be (gpd, gpl, NULL, NULL).
 */
void BKE_gpencil_tag_full_update(struct bGPdata *gpd,
                                 struct bGPDlayer *gpl,
                                 struct bGPDframe *gpf,
                                 struct bGPDstroke *gps);

/**
 * Tags an element (bGPdata, bGPDlayer, bGPDframe, or bGPDstroke) to be updated in the next
 * update-on-write operation. This function will not update any of the containing data, only the
 * struct itself.
 *
 * The function assumes that when a parameter is NULL all of the following parameters are NULL too.
 * E.g. in order to tag a layer (gpl), the parameters would *have* to be (gpd, gpl, NULL, NULL).
 */
void BKE_gpencil_tag_light_update(struct bGPdata *gpd,
                                  struct bGPDlayer *gpl,
                                  struct bGPDframe *gpf,
                                  struct bGPDstroke *gps);

/**
 * Frees the GPencilUpdateCache on the gpd->runtime. This will not free the data that the cache
 * node might point to. It assumes that the cache does not own the data.
 */
void BKE_gpencil_free_update_cache(struct bGPdata *gpd);

#ifdef __cplusplus
}
#endif
