/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "SEQ_render.h" /* Needed for #eSeqTaskId. */

struct ImBuf;
struct Main;
struct Scene;
struct SeqRenderData;
struct Sequence;

typedef struct SeqCacheKey {
  struct SeqCache *cache_owner;
  void *userkey;
  struct SeqCacheKey *link_prev; /* Used for linking intermediate items to final frame. */
  struct SeqCacheKey *link_next; /* Used for linking intermediate items to final frame. */
  struct Sequence *seq;
  struct SeqRenderData context;
  float frame_index;    /* Usually same as timeline_frame. Mapped to media for RAW entries. */
  float timeline_frame; /* Only for reference - used for freeing when cache is full. */
  float cost;           /* In short: render time(s) divided by playback frame duration(s) */
  bool is_temp_cache;   /* this cache entry will be freed before rendering next frame */
  /* ID of task for assigning temp cache entries to particular task(thread, etc.) */
  eSeqTaskId task_id;
  int type;
} SeqCacheKey;

struct ImBuf *seq_cache_get(const struct SeqRenderData *context,
                            struct Sequence *seq,
                            float timeline_frame,
                            int type);
void seq_cache_put(const struct SeqRenderData *context,
                   struct Sequence *seq,
                   float timeline_frame,
                   int type,
                   struct ImBuf *i);
void seq_cache_thumbnail_put(const struct SeqRenderData *context,
                             struct Sequence *seq,
                             float timeline_frame,
                             struct ImBuf *i,
                             const struct rctf *view_area);
bool seq_cache_put_if_possible(const struct SeqRenderData *context,
                               struct Sequence *seq,
                               float timeline_frame,
                               int type,
                               struct ImBuf *nval);
/**
 * Find only "base" keys.
 * Sources(other types) for a frame must be freed all at once.
 */
bool seq_cache_recycle_item(struct Scene *scene);
void seq_cache_free_temp_cache(struct Scene *scene, short id, int timeline_frame);
void seq_cache_destruct(struct Scene *scene);
void seq_cache_cleanup_all(struct Main *bmain);
void seq_cache_cleanup_sequence(struct Scene *scene,
                                struct Sequence *seq,
                                struct Sequence *seq_changed,
                                int invalidate_types,
                                bool force_seq_changed_range);
void seq_cache_thumbnail_cleanup(Scene *scene, rctf *view_area);
bool seq_cache_is_full(void);
float seq_cache_frame_index_to_timeline_frame(struct Sequence *seq, float frame_index);

#ifdef __cplusplus
}
#endif
