/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/**
 * Check if one sequence is input to the other.
 */
bool SEQ_relation_is_effect_of_strip(const struct Sequence *effect, const struct Sequence *input);
/**
 * Function to free imbuf and anim data on changes.
 */
void SEQ_relations_sequence_free_anim(struct Sequence *seq);
bool SEQ_relations_check_scene_recursion(struct Scene *scene, struct ReportList *reports);
/**
 * Check if "seq_main" (indirectly) uses strip "seq".
 */
bool SEQ_relations_render_loop_check(struct Sequence *seq_main, struct Sequence *seq);
void SEQ_relations_free_imbuf(struct Scene *scene, struct ListBase *seqbase, bool for_render);
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
/**
 * Release FFmpeg handles of strips that are not currently displayed to minimize memory usage.
 */
void SEQ_relations_free_all_anim_ibufs(struct Scene *scene, int timeline_frame);
/**
 * A debug and development function which checks whether sequences have unique UUIDs.
 * Errors will be reported to the console.
 */
void SEQ_relations_check_uuids_unique_and_report(const struct Scene *scene);
/**
 * Generate new UUID for the given sequence.
 */
void SEQ_relations_session_uuid_generate(struct Sequence *sequence);

void SEQ_cache_cleanup(struct Scene *scene);
void SEQ_cache_iterate(
    struct Scene *scene,
    void *userdata,
    bool callback_init(void *userdata, size_t item_count),
    bool callback_iter(void *userdata, struct Sequence *seq, int timeline_frame, int cache_type));
/**
 * Return immediate parent meta of sequence.
 */
struct Sequence *SEQ_find_metastrip_by_sequence(ListBase *seqbase /* = ed->seqbase */,
                                                struct Sequence *meta /* = NULL */,
                                                struct Sequence *seq);
bool SEQ_exists_in_seqbase(const struct Sequence *seq, const struct ListBase *seqbase);

#ifdef __cplusplus
}
#endif
