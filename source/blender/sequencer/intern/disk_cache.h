/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
