/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "SEQ_render.hh" /* Needed for #eSeqTaskId. */

struct ImBuf;
struct Scene;
struct SeqCache;
struct SeqRenderData;
struct Strip;

struct SeqCacheKey {
  SeqCache *cache_owner;
  void *userkey;
  SeqCacheKey *link_prev; /* Used for linking intermediate items to final frame. */
  SeqCacheKey *link_next; /* Used for linking intermediate items to final frame. */
  Strip *strip;
  SeqRenderData context;
  float frame_index;    /* Usually same as timeline_frame. Mapped to media for RAW entries. */
  float timeline_frame; /* Only for reference - used for freeing when cache is full. */
  float cost;           /* In short: render time(s) divided by playback frame duration(s) */
  bool is_temp_cache;   /* this cache entry will be freed before rendering next frame */
  /* ID of task for assigning temp cache entries to particular task(thread, etc.) */
  eSeqTaskId task_id;
  int type;
};

ImBuf *seq_cache_get(const SeqRenderData *context, Strip *strip, float timeline_frame, int type);
void seq_cache_put(
    const SeqRenderData *context, Strip *strip, float timeline_frame, int type, ImBuf *i);
bool seq_cache_put_if_possible(
    const SeqRenderData *context, Strip *strip, float timeline_frame, int type, ImBuf *ibuf);
/**
 * Find only "base" keys.
 * Sources(other types) for a frame must be freed all at once.
 */
bool seq_cache_recycle_item(Scene *scene);
void seq_cache_free_temp_cache(Scene *scene, short id, int timeline_frame);
void seq_cache_destruct(Scene *scene);
void seq_cache_cleanup_sequence(Scene *scene,
                                Strip *strip,
                                Strip *strip_changed,
                                int invalidate_types,
                                bool force_seq_changed_range);
bool seq_cache_is_full();
float seq_cache_frame_index_to_timeline_frame(Strip *strip, float frame_index);
