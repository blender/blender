/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "SEQ_render.hh" /* Needed for #eSeqTaskId. */

struct ImBuf;
struct Main;
struct Scene;
struct SeqCache;
struct SeqRenderData;
struct Sequence;

struct SeqCacheKey {
  SeqCache *cache_owner;
  void *userkey;
  SeqCacheKey *link_prev; /* Used for linking intermediate items to final frame. */
  SeqCacheKey *link_next; /* Used for linking intermediate items to final frame. */
  Sequence *seq;
  SeqRenderData context;
  float frame_index;    /* Usually same as timeline_frame. Mapped to media for RAW entries. */
  float timeline_frame; /* Only for reference - used for freeing when cache is full. */
  float cost;           /* In short: render time(s) divided by playback frame duration(s) */
  bool is_temp_cache;   /* this cache entry will be freed before rendering next frame */
  /* ID of task for assigning temp cache entries to particular task(thread, etc.) */
  eSeqTaskId task_id;
  int type;
};

ImBuf *seq_cache_get(const SeqRenderData *context, Sequence *seq, float timeline_frame, int type);
void seq_cache_put(
    const SeqRenderData *context, Sequence *seq, float timeline_frame, int type, ImBuf *i);
void seq_cache_thumbnail_put(const SeqRenderData *context,
                             Sequence *seq,
                             float timeline_frame,
                             ImBuf *i,
                             const rctf *view_area);
bool seq_cache_put_if_possible(
    const SeqRenderData *context, Sequence *seq, float timeline_frame, int type, ImBuf *ibuf);
/**
 * Find only "base" keys.
 * Sources(other types) for a frame must be freed all at once.
 */
bool seq_cache_recycle_item(Scene *scene);
void seq_cache_free_temp_cache(Scene *scene, short id, int timeline_frame);
void seq_cache_destruct(Scene *scene);
void seq_cache_cleanup_all(Main *bmain);
void seq_cache_cleanup_sequence(Scene *scene,
                                Sequence *seq,
                                Sequence *seq_changed,
                                int invalidate_types,
                                bool force_seq_changed_range);
void seq_cache_thumbnail_cleanup(Scene *scene, rctf *r_view_area_safe);
bool seq_cache_is_full();
float seq_cache_frame_index_to_timeline_frame(Sequence *seq, float frame_index);
