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

struct ImBuf;
struct Main;
struct Scene;
struct SeqRenderData;
struct Sequence;

#ifdef __cplusplus
}
#endif

struct ImBuf *BKE_sequencer_cache_get(const struct SeqRenderData *context,
                                      struct Sequence *seq,
                                      float timeline_frame,
                                      int type,
                                      bool skip_disk_cache);
void BKE_sequencer_cache_put(const struct SeqRenderData *context,
                             struct Sequence *seq,
                             float timeline_frame,
                             int type,
                             struct ImBuf *i,
                             float cost,
                             bool skip_disk_cache);
bool BKE_sequencer_cache_put_if_possible(const struct SeqRenderData *context,
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

#ifdef __cplusplus
}
#endif
