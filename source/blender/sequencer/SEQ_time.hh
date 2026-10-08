/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"

namespace blender {

struct Scene;
struct Strip;
struct rctf;

namespace seq {

enum class Side : int;

/**
 * Initialize \a r_rect with the scene's timeline boundaries.
 */
void timeline_init_boundbox(const Scene *scene, rctf *r_rect);
/**
 * Stretch \a rect to include the boundaries of the strips in \a seqbase.
 */
void timeline_expand_boundbox(const Scene *scene, const ListBaseT<Strip> *seqbase, rctf *rect);
/**
 * Fill \a r_rect with the scene's timeline boundaries, stretched to include the strips in
 * \a seqbase.
 */
void timeline_boundbox(const Scene *scene, const ListBaseT<Strip> *seqbase, rctf *r_rect);
/**
 * Find start or end position of the next or previous strip from \a timeline_frame.
 *
 * \param do_center: Find the closest strip center instead of handle.
 * \param do_unselected: Only consider unselected strips.
 */
int time_find_next_prev_edit(Scene *scene,
                             int timeline_frame,
                             Side side,
                             bool do_skip_mute,
                             bool do_center,
                             bool do_unselected);
/* Convert timeline frame so strip frame index. */
float give_frame_index(const Scene *scene, const Strip *strip, float timeline_frame);
/**
 * Update meta strip content start and end, update sound playback range.
 * To be used after any contained strip length or position has changed.
 *
 * \note this function is currently only used internally and in versioning code.
 */
void time_update_meta_strip_range(const Scene *scene, Strip *strip_meta);
/**
 * Move contents of a strip without moving the strip handles.
 */
void time_slip_strip(
    const Scene *scene, Strip *strip, int frame_delta, float subframe_delta, bool slip_keyframes);

}  // namespace seq
}  // namespace blender
