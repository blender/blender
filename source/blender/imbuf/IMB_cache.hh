/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

#include "BLI_ghash.h"
struct ImBufCacheIter;
namespace blender {

/* Cache system for movie data - now supports storing ImBufs only
 * Supposed to provide unified cache system for movie clips, sequencer and
 * other movie-related areas */

struct ImBuf;
struct ImBufCache;

using ImBufCacheGetKeyDataFP = void (*)(void *userkey,
                                        int *framenr,
                                        int *proxy,
                                        int *render_flags);

using ImBufCacheGetPriorityDataFP = void *(*)(void *userkey);
using ImBufCacheGetItemPriorityFP = int (*)(void *last_userkey, void *priority_data);
using ImBufCachePriorityDeleterFP = void (*)(void *priority_data);

void IMB_cache_init();
void IMB_cache_destruct();

ImBufCache *IMB_cache_create(const char *name, int keysize, GHashHashFP hashfp, GHashCmpFP cmpfp);
void IMB_cache_set_getdata_callback(ImBufCache *cache, ImBufCacheGetKeyDataFP getdatafp);
void IMB_cache_set_priority_callback(ImBufCache *cache,
                                     ImBufCacheGetPriorityDataFP getprioritydatafp,
                                     ImBufCacheGetItemPriorityFP getitempriorityfp,
                                     ImBufCachePriorityDeleterFP prioritydeleterfp);

void IMB_cache_put(ImBufCache *cache, void *userkey, ImBuf *ibuf);
bool IMB_cache_put_if_possible(ImBufCache *cache, void *userkey, ImBuf *ibuf);
ImBuf *IMB_cache_get(ImBufCache *cache, void *userkey, bool *r_is_cached_empty);
void IMB_cache_remove(ImBufCache *cache, void *userkey);
bool IMB_cache_has_frame(ImBufCache *cache, void *userkey);
void IMB_cache_free(ImBufCache *cache);

void IMB_cache_cleanup(ImBufCache *cache,
                       bool(cleanup_check_cb)(ImBuf *ibuf, void *userkey, void *userdata),
                       void *userdata);

/**
 * Get segments of cached frames. Useful for debugging cache policies.
 */
void IMB_cache_get_cache_segments(
    ImBufCache *cache, int proxy, int render_flags, int *r_totseg, int **r_points);

ImBufCacheIter *IMB_cacheIter_new(ImBufCache *cache);
void IMB_cacheIter_free(ImBufCacheIter *iter);
bool IMB_cacheIter_done(ImBufCacheIter *iter);
void IMB_cacheIter_step(ImBufCacheIter *iter);
ImBuf *IMB_cacheIter_getImBuf(ImBufCacheIter *iter);
void *IMB_cacheIter_getUserKey(ImBufCacheIter *iter);

}  // namespace blender
