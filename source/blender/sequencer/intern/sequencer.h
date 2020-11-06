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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Editing;
struct ImBuf;
struct Main;
struct Mask;
struct Scene;
struct Sequence;
struct StripColorBalance;
struct StripElem;

#define EARLY_NO_INPUT -1
#define EARLY_DO_EFFECT 0
#define EARLY_USE_INPUT_1 1
#define EARLY_USE_INPUT_2 2

/* **********************************************************************
 * sequencer.c
 *
 * sequencer scene functions
 * ********************************************************************** */

void BKE_sequencer_base_clipboard_pointers_free(struct ListBase *seqbase);
/* **********************************************************************
 * image_cache.c
 *
 * Sequencer memory cache management functions
 * ********************************************************************** */

struct ImBuf *BKE_sequencer_cache_get(const SeqRenderData *context,
                                      struct Sequence *seq,
                                      float timeline_frame,
                                      int type,
                                      bool skip_disk_cache);
void BKE_sequencer_cache_put(const SeqRenderData *context,
                             struct Sequence *seq,
                             float timeline_frame,
                             int type,
                             struct ImBuf *i,
                             float cost,
                             bool skip_disk_cache);
bool BKE_sequencer_cache_put_if_possible(const SeqRenderData *context,
                                         struct Sequence *seq,
                                         float timeline_frame,
                                         int type,
                                         struct ImBuf *nval,
                                         float cost,
                                         bool skip_disk_cache);
bool BKE_sequencer_cache_recycle_item(struct Scene *scene);
void BKE_sequencer_cache_free_temp_cache(struct Scene *scene, short id, int timeline_frame);
void BKE_sequencer_cache_destruct(struct Scene *scene);
void BKE_sequencer_cache_cleanup_all(struct Main *bmain);
void BKE_sequencer_cache_cleanup_sequence(struct Scene *scene,
                                          struct Sequence *seq,
                                          struct Sequence *seq_changed,
                                          int invalidate_types,
                                          bool force_seq_changed_range);
bool BKE_sequencer_cache_is_full(struct Scene *scene);

/* **********************************************************************
 * prefetch.c
 *
 * Sequencer frame prefetching
 * ********************************************************************** */

void BKE_sequencer_prefetch_start(const SeqRenderData *context, float timeline_frame, float cost);
void BKE_sequencer_prefetch_free(struct Scene *scene);
bool BKE_sequencer_prefetch_job_is_running(struct Scene *scene);
void BKE_sequencer_prefetch_get_time_range(struct Scene *scene, int *start, int *end);
SeqRenderData *BKE_sequencer_prefetch_get_original_context(const SeqRenderData *context);
struct Sequence *BKE_sequencer_prefetch_get_original_sequence(struct Sequence *seq,
                                                              struct Scene *scene);

/* **********************************************************************
 * seqeffects.c
 *
 * Sequencer effect strip management functions
 *  **********************************************************************
 */

struct SeqEffectHandle BKE_sequence_get_blend(struct Sequence *seq);
void BKE_sequence_effect_speed_rebuild_map(struct Scene *scene, struct Sequence *seq, bool force);
float BKE_sequencer_speed_effect_target_frame_get(const SeqRenderData *context,
                                                  struct Sequence *seq,
                                                  float timeline_frame,
                                                  int input);

/* **********************************************************************
 * sequencer.c
 *
 * Sequencer editing functions
 * **********************************************************************
 */

void BKE_sequence_sound_init(struct Scene *scene, struct Sequence *seq);
struct Sequence *BKE_sequence_metastrip(ListBase *seqbase /* = ed->seqbase */,
                                        struct Sequence *meta /* = NULL */,
                                        struct Sequence *seq);
void seq_free_sequence_recurse(struct Scene *scene, struct Sequence *seq, const bool do_id_user);
void seq_multiview_name(struct Scene *scene,
                        const int view_id,
                        const char *prefix,
                        const char *ext,
                        char *r_path,
                        size_t r_size);
int seq_num_files(struct Scene *scene, char views_format, const bool is_multiview);
void seq_open_anim_file(struct Scene *scene, struct Sequence *seq, bool openfile);
void seq_proxy_index_dir_set(struct anim *anim, const char *base_dir);

#ifdef __cplusplus
}
#endif
