/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include <cstddef>

struct ListBase;
struct Main;
struct MovieClip;
struct ReportList;
struct Scene;
struct Strip;

/**
 * Check if one sequence is input to the other.
 */
bool SEQ_relation_is_effect_of_strip(const Strip *effect, const Strip *input);
/**
 * Function to free imbuf and anim data on changes.
 */
void SEQ_relations_sequence_free_anim(Strip *strip);
bool SEQ_relations_check_scene_recursion(Scene *scene, ReportList *reports);
/**
 * Check if "strip_main" (indirectly) uses strip "strip".
 */
bool SEQ_relations_render_loop_check(Strip *strip_main, Strip *strip);
void SEQ_relations_free_imbuf(Scene *scene, ListBase *seqbase, bool for_render);
void SEQ_relations_invalidate_cache_raw(Scene *scene, Strip *strip);
void SEQ_relations_invalidate_cache_preprocessed(Scene *scene, Strip *strip);
void SEQ_relations_invalidate_cache_composite(Scene *scene, Strip *strip);
void SEQ_relations_invalidate_dependent(Scene *scene, Strip *strip);
void SEQ_relations_invalidate_scene_strips(Main *bmain, Scene *scene_target);
void SEQ_relations_invalidate_movieclip_strips(Main *bmain, MovieClip *clip_target);
void SEQ_relations_invalidate_cache_in_range(Scene *scene,
                                             Strip *strip,
                                             Strip *range_mask,
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
void SEQ_relations_session_uid_generate(Strip *sequence);

void SEQ_cache_cleanup(Scene *scene);
void SEQ_cache_iterate(
    Scene *scene,
    void *userdata,
    bool callback_init(void *userdata, size_t item_count),
    bool callback_iter(void *userdata, Strip *strip, int timeline_frame, int cache_type));
/**
 * Return immediate parent meta of sequence.
 */
Strip *SEQ_find_metastrip_by_sequence(ListBase *seqbase /* = ed->seqbase */,
                                      Strip *meta /* = NULL */,
                                      Strip *strip);
bool SEQ_exists_in_seqbase(const Strip *strip, const ListBase *seqbase);
