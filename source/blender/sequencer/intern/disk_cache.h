/*
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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup sequencer
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;
struct Main;
struct Scene;
struct SeqCacheKey;
struct SeqDiskCache;
struct Sequence;

struct SeqDiskCache *seq_disk_cache_create(struct Main *bmain, struct Scene *scene);
void seq_disk_cache_free(struct SeqDiskCache *disk_cache);
bool seq_disk_cache_is_enabled(struct Main *bmain);
struct ImBuf *seq_disk_cache_read_file(struct SeqDiskCache *disk_cache, struct SeqCacheKey *key);
bool seq_disk_cache_write_file(struct SeqDiskCache *disk_cache,
                               struct SeqCacheKey *key,
                               struct ImBuf *ibuf);
bool seq_disk_cache_enforce_limits(struct SeqDiskCache *disk_cache);
void seq_disk_cache_invalidate(struct SeqDiskCache *disk_cache,
                               struct Scene *scene,
                               struct Sequence *seq,
                               struct Sequence *seq_changed,
                               int invalidate_types);
#ifdef __cplusplus
}
#endif
