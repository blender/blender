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
 * Move \a strip from \a seqbase to \a dst_seqbase.
 */
bool edit_move_strip_to_seqbase(Scene *scene,
                                ListBaseT<Strip> *seqbase,
                                Strip *strip,
                                ListBaseT<Strip> *dst_seqbase);
/**
 * Move \a src_strip into meta-strip \a dst_stripm.
 *
 * \param r_error_str: Set on failure.
 */
bool edit_move_strip_to_meta(Scene *scene,
                             Strip *src_strip,
                             Strip *dst_stripm,
                             const char **r_error_str);
/**
 * Flag strip and its users (effects) for removal.
 */
void edit_flag_for_removal(Scene *scene, Strip *strip);
/**
 * Remove all flagged strips.
 */
void edit_remove_flagged_strips(Scene *scene, ListBaseT<Strip> *seqbase);
void edit_update_muting(Editing *ed);

enum eSplitMethod {
  SPLIT_SOFT,
  SPLIT_HARD,
};

/**
 * Split \a strip in two at \a timeline_frame.
 *
 * \param method: Soft keeps the cut content reachable by the handles, hard turns it into holds.
 * \return The new right-side strip, or null if the split failed.
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
 * Find the gap after \a initial_frame and move strips on its right side to close it.
 *
 * \param remove_all_gaps: Close every gap after \a initial_frame, not just the first.
 * \return Whether a gap was removed.
 */
bool edit_remove_gaps(Scene *scene,
                      ListBaseT<Strip> *seqbase,
                      int initial_frame,
                      bool remove_all_gaps);
void edit_strip_name_set(Scene *scene, Strip *strip, const char *new_name);

}  // namespace seq
}  // namespace blender
