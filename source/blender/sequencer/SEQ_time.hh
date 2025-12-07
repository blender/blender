/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ListBase;
struct Scene;
struct Strip;
struct rctf;

namespace blender::seq {

/**
 * Initialize given rectangle with the Scene's timeline boundaries.
 *
 * \param scene: the Scene instance whose timeline boundaries are extracted from
 * \param rect: output parameter to be filled with timeline boundaries
 */
void timeline_init_boundbox(const Scene *scene, rctf *r_rect);
/**
 * Stretch the given rectangle to include the given strips boundaries
 *
 * \param seqbase: ListBase in which strips are located
 * \param rect: output parameter to be filled with strips' boundaries
 */
void timeline_expand_boundbox(const Scene *scene, const ListBase *seqbase, rctf *rect);
/**
 * Define boundary rectangle of sequencer timeline and fill in rect data
 *
 * \param scene: Scene in which strips are located
 * \param seqbase: ListBase in which strips are located
 * \param rect: data structure describing rectangle, that will be filled in by this function
 */
void timeline_boundbox(const Scene *scene, const ListBase *seqbase, rctf *r_rect);
/**
 * Find start or end position of next or previous strip.
 * \param scene: Video editing scene
 * \param timeline_frame: reference frame for searching
 * \param side: direction of searching, `SIDE_LEFT`, `SIDE_RIGHT` or `SIDE_BOTH`.
 * \param do_center: find closest strip center if true, otherwise finds closest handle position.
 * \param do_unselected: only find closest position of unselected strip.
 */
int time_find_next_prev_edit(Scene *scene,
                             int timeline_frame,
                             short side,
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

}  // namespace blender::seq
