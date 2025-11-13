/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include <cstddef>

#include "BLI_enum_flags.hh"

struct ListBase;
struct Main;
struct MovieClip;
struct ReportList;
struct bNodeTree;
struct Scene;
struct Strip;

namespace blender::seq {

/**
 * Check if one strip is input to the other.
 */
bool relation_is_effect_of_strip(const Strip *effect, const Strip *input);
/**
 * Function to free imbuf and anim data on changes.
 */
void relations_strip_free_anim(Strip *strip);
bool relations_check_scene_recursion(Scene *scene, ReportList *reports);
/**
 * Check if "strip_main" (indirectly) uses strip "strip".
 */
bool relations_render_loop_check(Strip *strip_main, Strip *strip);
void relations_free_imbuf(Scene *scene, ListBase *seqbase, bool for_render);

/**
 * Invalidates various caches related to a given strip:
 * - Final cached frames over the length of the strip,
 * - Intra-frame caches of the current frame,
 * - Source/raw caches of the meta strip that contains this strip, if any,
 * - Media presence cache of the strip,
 * - Rebuilds speed index map if this is a speed effect strip,
 * - Tags DEG for strip recalculation,
 * - Stops prefetching job, if any.
 */
void relations_invalidate_cache(Scene *scene, Strip *strip);

/**
 * Does everything #relations_invalidate_cache does, plus invalidates cached raw source
 * images of the strip.
 */
void relations_invalidate_cache_raw(Scene *scene, Strip *strip);
void relations_invalidate_scene_strips(const Main *bmain, const Scene *scene_target);

/**
 * Invalidates the cache for all strips that uses the given node tree as a compositor modifier.
 */
void relations_invalidate_compositor_modifiers(const Main *bmain, const bNodeTree *node_tree);

void relations_invalidate_movieclip_strips(Main *bmain, MovieClip *clip_target);
/**
 * Release FFmpeg handles of strips that are not currently displayed to minimize memory usage.
 */
void relations_free_all_anim_ibufs(Scene *scene, int timeline_frame);
/**
 * A debug and development function which checks whether strips have unique UIDs.
 * Errors will be reported to the console.
 */
void relations_check_uids_unique_and_report(const Scene *scene);
/**
 * Generate new UID for the given strip.
 */
void relations_session_uid_generate(Strip *strip);

enum class CacheCleanup {
  FinalImage = (1 << 0),
  SourceImage = (1 << 1),
  Thumbnails = (1 << 2),
  IntraFrame = (1 << 3),

  /* All cache types. */
  All = FinalImage | SourceImage | Thumbnails | IntraFrame,

  /* Typical "what gets rendered" cache types: final frame
   * cache, plus various intra-frame cached things. */
  FinalAndIntra = FinalImage | IntraFrame,
};
ENUM_OPERATORS(CacheCleanup);

void cache_cleanup(Scene *scene, CacheCleanup mode);

void cache_settings_changed(Scene *scene);
bool is_cache_full(const Scene *scene);
bool evict_caches_if_full(Scene *scene);

void source_image_cache_iterate(Scene *scene,
                                void *userdata,
                                void callback_iter(void *userdata,
                                                   const Strip *strip,
                                                   int timeline_frame));
void final_image_cache_iterate(Scene *scene,
                               void *userdata,
                               void callback_iter(void *userdata, int timeline_frame));

size_t source_image_cache_calc_memory_size(const Scene *scene);
size_t final_image_cache_calc_memory_size(const Scene *scene);

bool exists_in_seqbase(const Strip *strip, const ListBase *seqbase);

}  // namespace blender::seq
