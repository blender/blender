/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

#include "BLI_ghash.h"
#include "BLI_utildefines.h"

/* Cache system for movie data - now supports storing ImBufs only
 * Supposed to provide unified cache system for movie clips, sequencer and
 * other movie-related areas */

struct ImBuf;
struct MovieCache;

using MovieCacheGetKeyDataFP = void (*)(void *userkey,
                                        int *framenr,
                                        int *proxy,
                                        int *render_flags);

using MovieCacheGetPriorityDataFP = void *(*)(void *userkey);
using MovieCacheGetItemPriorityFP = int (*)(void *last_userkey, void *priority_data);
using MovieCachePriorityDeleterFP = void (*)(void *priority_data);

void IMB_moviecache_init(void);
void IMB_moviecache_destruct(void);

MovieCache *IMB_moviecache_create(const char *name,
                                  int keysize,
                                  GHashHashFP hashfp,
                                  GHashCmpFP cmpfp);
void IMB_moviecache_set_getdata_callback(MovieCache *cache, MovieCacheGetKeyDataFP getdatafp);
void IMB_moviecache_set_priority_callback(MovieCache *cache,
                                          MovieCacheGetPriorityDataFP getprioritydatafp,
                                          MovieCacheGetItemPriorityFP getitempriorityfp,
                                          MovieCachePriorityDeleterFP prioritydeleterfp);

void IMB_moviecache_put(MovieCache *cache, void *userkey, ImBuf *ibuf);
bool IMB_moviecache_put_if_possible(MovieCache *cache, void *userkey, ImBuf *ibuf);
ImBuf *IMB_moviecache_get(MovieCache *cache, void *userkey, bool *r_is_cached_empty);
void IMB_moviecache_remove(MovieCache *cache, void *userkey);
bool IMB_moviecache_has_frame(MovieCache *cache, void *userkey);
void IMB_moviecache_free(MovieCache *cache);

void IMB_moviecache_cleanup(MovieCache *cache,
                            bool(cleanup_check_cb)(ImBuf *ibuf, void *userkey, void *userdata),
                            void *userdata);

/**
 * Get segments of cached frames. Useful for debugging cache policies.
 */
void IMB_moviecache_get_cache_segments(
    MovieCache *cache, int proxy, int render_flags, int *r_totseg, int **r_points);

struct MovieCacheIter;
MovieCacheIter *IMB_moviecacheIter_new(MovieCache *cache);
void IMB_moviecacheIter_free(MovieCacheIter *iter);
bool IMB_moviecacheIter_done(MovieCacheIter *iter);
void IMB_moviecacheIter_step(MovieCacheIter *iter);
ImBuf *IMB_moviecacheIter_getImBuf(MovieCacheIter *iter);
void *IMB_moviecacheIter_getUserKey(MovieCacheIter *iter);
