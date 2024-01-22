/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ListBase;
struct Main;
struct MovieClip;
struct ReportList;
struct Scene;
struct Sequence;

/**
 * Check if one sequence is input to the other.
 */
bool SEQ_relation_is_effect_of_strip(const Sequence *effect, const Sequence *input);
/**
 * Function to free imbuf and anim data on changes.
 */
void SEQ_relations_sequence_free_anim(Sequence *seq);
bool SEQ_relations_check_scene_recursion(Scene *scene, ReportList *reports);
/**
 * Check if "seq_main" (indirectly) uses strip "seq".
 */
bool SEQ_relations_render_loop_check(Sequence *seq_main, Sequence *seq);
void SEQ_relations_free_imbuf(Scene *scene, ListBase *seqbase, bool for_render);
void SEQ_relations_invalidate_cache_raw(Scene *scene, Sequence *seq);
void SEQ_relations_invalidate_cache_preprocessed(Scene *scene, Sequence *seq);
void SEQ_relations_invalidate_cache_composite(Scene *scene, Sequence *seq);
void SEQ_relations_invalidate_dependent(Scene *scene, Sequence *seq);
void SEQ_relations_invalidate_scene_strips(Main *bmain, Scene *scene_target);
void SEQ_relations_invalidate_movieclip_strips(Main *bmain, MovieClip *clip_target);
void SEQ_relations_invalidate_cache_in_range(Scene *scene,
                                             Sequence *seq,
                                             Sequence *range_mask,
                                             int invalidate_types);
/**
 * Release FFmpeg handles of strips that are not currently displayed to minimize memory usage.
 */
void SEQ_relations_free_all_anim_ibufs(Scene *scene, int timeline_frame);
/**
 * A debug and development function which checks whether sequences have unique UIDs.
 * Errors will be reported to the console.
 */
void SEQ_relations_check_uids_unique_and_report(const Scene *scene);
/**
 * Generate new UID for the given sequence.
 */
void SEQ_relations_session_uid_generate(Sequence *sequence);

void SEQ_cache_cleanup(Scene *scene);
void SEQ_cache_iterate(
    Scene *scene,
    void *userdata,
    bool callback_init(void *userdata, size_t item_count),
    bool callback_iter(void *userdata, Sequence *seq, int timeline_frame, int cache_type));
/**
 * Return immediate parent meta of sequence.
 */
Sequence *SEQ_find_metastrip_by_sequence(ListBase *seqbase /* = ed->seqbase */,
                                         Sequence *meta /* = NULL */,
                                         Sequence *seq);
bool SEQ_exists_in_seqbase(const Sequence *seq, const ListBase *seqbase);
