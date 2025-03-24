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

namespace blender::seq {

/**
 * Check if one sequence is input to the other.
 */
bool relation_is_effect_of_strip(const Strip *effect, const Strip *input);
/**
 * Function to free imbuf and anim data on changes.
 */
void relations_sequence_free_anim(Strip *strip);
bool relations_check_scene_recursion(Scene *scene, ReportList *reports);
/**
 * Check if "strip_main" (indirectly) uses strip "strip".
 */
bool relations_render_loop_check(Strip *strip_main, Strip *strip);
void relations_free_imbuf(Scene *scene, ListBase *seqbase, bool for_render);
void relations_invalidate_cache_raw(Scene *scene, Strip *strip);
void relations_invalidate_cache_preprocessed(Scene *scene, Strip *strip);
void relations_invalidate_cache_composite(Scene *scene, Strip *strip);
void relations_invalidate_dependent(Scene *scene, Strip *strip);
void relations_invalidate_scene_strips(Main *bmain, Scene *scene_target);
void relations_invalidate_movieclip_strips(Main *bmain, MovieClip *clip_target);
void relations_invalidate_cache_in_range(Scene *scene,
                                         Strip *strip,
                                         Strip *range_mask,
                                         int invalidate_types);
/**
 * Release FFmpeg handles of strips that are not currently displayed to minimize memory usage.
 */
void relations_free_all_anim_ibufs(Scene *scene, int timeline_frame);
/**
 * A debug and development function which checks whether sequences have unique UIDs.
 * Errors will be reported to the console.
 */
void relations_check_uids_unique_and_report(const Scene *scene);
/**
 * Generate new UID for the given sequence.
 */
void relations_session_uid_generate(Strip *sequence);

void cache_cleanup(Scene *scene);
void cache_iterate(
    Scene *scene,
    void *userdata,
    bool callback_init(void *userdata, size_t item_count),
    bool callback_iter(void *userdata, Strip *strip, int timeline_frame, int cache_type));
bool exists_in_seqbase(const Strip *strip, const ListBase *seqbase);

}  // namespace blender::seq
