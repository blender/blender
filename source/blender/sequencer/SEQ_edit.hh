/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_listBase.h"

namespace blender {

/** \file
 * \ingroup sequencer
 */

struct Editing;
struct Main;
struct Scene;
struct Strip;

namespace seq {

bool edit_strip_swap(Scene *scene, Strip *strip_a, Strip *strip_b, const char **r_error_str);
/**
 * Move strip to seqbase.
 *
 * \param scene: Scene containing the editing
 * \param seqbase: seqbase where `strip` is located
 * \param strip: Strip to move
 * \param dst_seqbase: Target seqbase
 */
bool edit_move_strip_to_seqbase(Scene *scene,
                                ListBaseT<Strip> *seqbase,
                                Strip *strip,
                                ListBaseT<Strip> *dst_seqbase);
/**
 * Move strip to meta-strip.
 *
 * \param scene: Scene containing the editing
 * \param src_strip: Strip to move
 * \param dst_stripm: Target meta-strip
 * \param r_error_str: Error message
 */
bool edit_move_strip_to_meta(Scene *scene,
                             Strip *src_strip,
                             Strip *dst_stripm,
                             const char **r_error_str);
/**
 * Flag strip and its users (effects) for removal.
 */
void edit_flag_for_removal(Scene *scene, ListBaseT<Strip> *seqbase, Strip *strip);
/**
 * Remove all flagged strips, return true if strip is removed.
 */
void edit_remove_flagged_strips(Scene *scene, ListBaseT<Strip> *seqbase);
void edit_update_muting(Editing *ed);

enum eSplitMethod {
  SPLIT_SOFT,
  SPLIT_HARD,
};

/**
 * Split Strip at timeline_frame in two.
 *
 * \param strip: Strip to be split
 * \param timeline_frame: frame at which strip is split.
 * \param method: affects type of offset to be applied to resize Strip
 * \return The newly created strip. This is always the Strip on the right side.
 */
Strip *edit_strip_split(Main *bmain,
                        Scene *scene,
                        ListBaseT<Strip> *seqbase,
                        Strip *strip,
                        int timeline_frame,
                        eSplitMethod method,
                        bool ignore_connections,
                        const char **r_error);
/**
 * Find gap after initial_frame and move strips on right side to close the gap
 *
 * \param scene: Scene in which strips are located
 * \param seqbase: List in which strips are located
 * \param initial_frame: frame on timeline from where gaps are searched for
 * \param remove_all_gaps: remove all gaps instead of one gap
 * \return true if gap is removed, otherwise false
 */
bool edit_remove_gaps(Scene *scene,
                      ListBaseT<Strip> *seqbase,
                      int initial_frame,
                      bool remove_all_gaps);
void edit_strip_name_set(Scene *scene, Strip *strip, const char *new_name);

}  // namespace seq
}  // namespace blender
