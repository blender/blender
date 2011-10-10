/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_MOVIECACHE_H
#define BKE_MOVIECACHE_H

/** \file BKE_moviecache.h
 *  \ingroup bke
 *  \author Sergey Sharybin
 */

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

/* Cache system for movie data - now supports stroting ImBufs only
   Supposed to provide unified cache system for movie clips, sequencer and
   other movie-related areas */

struct ImBuf;
struct MovieCache;

typedef void (*MovieCacheGetKeyDataFP) (void *userkey, int *framenr, int *proxy, int *render_flags);

void BKE_moviecache_init(void);
void BKE_moviecache_destruct(void);

struct MovieCache *BKE_moviecache_create(int keysize, GHashHashFP hashfp, GHashCmpFP cmpfp, MovieCacheGetKeyDataFP getdatafp);
void BKE_moviecache_put(struct MovieCache *cache, void *userkey, struct ImBuf *ibuf);
struct ImBuf* BKE_moviecache_get(struct MovieCache *cache, void *userkey);
void BKE_moviecache_free(struct MovieCache *cache);
void BKE_moviecache_get_cache_segments(struct MovieCache *cache, int proxy, int render_flags, int *totseg_r, int **points_r);

#endif
