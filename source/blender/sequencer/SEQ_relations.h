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

struct ListBase;
struct Main;
struct MovieClip;
struct ReportList;
struct Scene;
struct Sequence;

void SEQ_relations_sequence_free_anim(struct Sequence *seq);
void SEQ_relations_update_changed_seq_and_deps(struct Scene *scene,
                                               struct Sequence *changed_seq,
                                               int len_change,
                                               int ibuf_change);
bool SEQ_relations_check_scene_recursion(struct Scene *scene, struct ReportList *reports);
bool SEQ_relations_render_loop_check(struct Sequence *seq_main, struct Sequence *seq);
void SEQ_relations_free_imbuf(struct Scene *scene, struct ListBase *seqbasep, bool for_render);
void SEQ_relations_invalidate_cache_raw(struct Scene *scene, struct Sequence *seq);
void SEQ_relations_invalidate_cache_preprocessed(struct Scene *scene, struct Sequence *seq);
void SEQ_relations_invalidate_cache_composite(struct Scene *scene, struct Sequence *seq);
void SEQ_relations_invalidate_dependent(struct Scene *scene, struct Sequence *seq);
void SEQ_relations_invalidate_scene_strips(struct Main *bmain, struct Scene *scene_target);
void SEQ_relations_invalidate_movieclip_strips(struct Main *bmain, struct MovieClip *clip_target);
void SEQ_relations_invalidate_cache_in_range(struct Scene *scene,
                                             struct Sequence *seq,
                                             struct Sequence *range_mask,
                                             int invalidate_types);
void SEQ_relations_free_all_anim_ibufs(struct Scene *scene, int timeline_frame);
/* A debug and development function which checks whether sequences have unique UUIDs.
 * Errors will be reported to the console. */
void SEQ_relations_check_uuids_unique_and_report(const struct Scene *scene);
/* Generate new UUID for the given sequence. */
void SEQ_relations_session_uuid_generate(struct Sequence *sequence);

void SEQ_cache_cleanup(struct Scene *scene);
void SEQ_cache_iterate(
    struct Scene *scene,
    void *userdata,
    bool callback_init(void *userdata, size_t item_count),
    bool callback_iter(void *userdata, struct Sequence *seq, int timeline_frame, int cache_type));
#ifdef __cplusplus
}
#endif
