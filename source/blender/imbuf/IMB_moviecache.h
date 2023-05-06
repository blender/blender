/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation */

#pragma once

/** \file
 * \ingroup imbuf
 */

#include "BLI_ghash.h"
#include "BLI_utildefines.h"

/* Cache system for movie data - now supports storing ImBufs only
 * Supposed to provide unified cache system for movie clips, sequencer and
 * other movie-related areas */

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;
struct MovieCache;

typedef void (*MovieCacheGetKeyDataFP)(void *userkey, int *framenr, int *proxy, int *render_flags);

typedef void *(*MovieCacheGetPriorityDataFP)(void *userkey);
typedef int (*MovieCacheGetItemPriorityFP)(void *last_userkey, void *priority_data);
typedef void (*MovieCachePriorityDeleterFP)(void *priority_data);

void IMB_moviecache_init(void);
void IMB_moviecache_destruct(void);

struct MovieCache *IMB_moviecache_create(const char *name,
                                         int keysize,
                                         GHashHashFP hashfp,
                                         GHashCmpFP cmpfp);
void IMB_moviecache_set_getdata_callback(struct MovieCache *cache,
                                         MovieCacheGetKeyDataFP getdatafp);
void IMB_moviecache_set_priority_callback(struct MovieCache *cache,
                                          MovieCacheGetPriorityDataFP getprioritydatafp,
                                          MovieCacheGetItemPriorityFP getitempriorityfp,
                                          MovieCachePriorityDeleterFP prioritydeleterfp);

void IMB_moviecache_put(struct MovieCache *cache, void *userkey, struct ImBuf *ibuf);
bool IMB_moviecache_put_if_possible(struct MovieCache *cache, void *userkey, struct ImBuf *ibuf);
struct ImBuf *IMB_moviecache_get(struct MovieCache *cache, void *userkey, bool *r_is_cached_empty);
void IMB_moviecache_remove(struct MovieCache *cache, void *userkey);
bool IMB_moviecache_has_frame(struct MovieCache *cache, void *userkey);
void IMB_moviecache_free(struct MovieCache *cache);

void IMB_moviecache_cleanup(struct MovieCache *cache,
                            bool(cleanup_check_cb)(struct ImBuf *ibuf,
                                                   void *userkey,
                                                   void *userdata),
                            void *userdata);

/**
 * Get segments of cached frames. Useful for debugging cache policies.
 */
void IMB_moviecache_get_cache_segments(
    struct MovieCache *cache, int proxy, int render_flags, int *r_totseg, int **r_points);

struct MovieCacheIter;
struct MovieCacheIter *IMB_moviecacheIter_new(struct MovieCache *cache);
void IMB_moviecacheIter_free(struct MovieCacheIter *iter);
bool IMB_moviecacheIter_done(struct MovieCacheIter *iter);
void IMB_moviecacheIter_step(struct MovieCacheIter *iter);
struct ImBuf *IMB_moviecacheIter_getImBuf(struct MovieCacheIter *iter);
void *IMB_moviecacheIter_getUserKey(struct MovieCacheIter *iter);

#ifdef __cplusplus
}
#endif
