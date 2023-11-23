/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ImBuf;
struct Main;
struct Scene;
struct SeqCacheKey;
struct SeqDiskCache;
struct Sequence;

SeqDiskCache *seq_disk_cache_create(Main *bmain, Scene *scene);
void seq_disk_cache_free(SeqDiskCache *disk_cache);
bool seq_disk_cache_is_enabled(Main *bmain);
ImBuf *seq_disk_cache_read_file(SeqDiskCache *disk_cache, SeqCacheKey *key);
bool seq_disk_cache_write_file(SeqDiskCache *disk_cache, SeqCacheKey *key, ImBuf *ibuf);
bool seq_disk_cache_enforce_limits(SeqDiskCache *disk_cache);
void seq_disk_cache_invalidate(SeqDiskCache *disk_cache,
                               Scene *scene,
                               Sequence *seq,
                               Sequence *seq_changed,
                               int invalidate_types);
