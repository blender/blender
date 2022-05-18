/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct Scene;
struct Sequence;
struct rctf;

/**
 * Initialize given rectangle with the Scene's timeline boundaries.
 *
 * \param scene: the Scene instance whose timeline boundaries are extracted from
 * \param rect: output parameter to be filled with timeline boundaries
 */
void SEQ_timeline_init_boundbox(const struct Scene *scene, struct rctf *rect);
/**
 * Stretch the given rectangle to include the given strips boundaries
 *
 * \param seqbase: ListBase in which strips are located
 * \param rect: output parameter to be filled with strips' boundaries
 */
void SEQ_timeline_expand_boundbox(const struct ListBase *seqbase, struct rctf *rect);
/**
 * Define boundary rectangle of sequencer timeline and fill in rect data
 *
 * \param scene: Scene in which strips are located
 * \param seqbase: ListBase in which strips are located
 * \param rect: data structure describing rectangle, that will be filled in by this function
 */
void SEQ_timeline_boundbox(const struct Scene *scene,
                           const struct ListBase *seqbase,
                           struct rctf *rect);
float SEQ_time_sequence_get_fps(struct Scene *scene, struct Sequence *seq);
int SEQ_time_find_next_prev_edit(struct Scene *scene,
                                 int timeline_frame,
                                 short side,
                                 bool do_skip_mute,
                                 bool do_center,
                                 bool do_unselected);
void SEQ_time_update_sequence(struct Scene *scene, struct ListBase *seqbase, struct Sequence *seq);
void SEQ_time_update_recursive(struct Scene *scene, struct Sequence *changed_seq);
/**
 * Test if strip intersects with timeline frame.
 * \note This checks if strip would be rendered at this frame. For rendering it is assumed, that
 * timeline frame has width of 1 frame and therefore ends at timeline_frame + 1
 *
 * \param seq: Sequence to be checked
 * \param timeline_frame: absolute frame position
 * \return true if strip intersects with timeline frame.
 */
bool SEQ_time_strip_intersects_frame(const struct Sequence *seq, int timeline_frame);
void SEQ_time_update_meta_strip_range(struct Scene *scene, struct Sequence *seq_meta);
bool SEQ_time_has_still_frames(const struct Sequence *seq);
bool SEQ_time_has_left_still_frames(const struct Sequence *seq);
bool SEQ_time_has_right_still_frames(const struct Sequence *seq);
int SEQ_time_left_handle_frame_get(struct Sequence *seq);
int SEQ_time_right_handle_frame_get(struct Sequence *seq);
void SEQ_time_left_handle_frame_set(struct Sequence *seq, int val);
void SEQ_time_right_handle_frame_set(struct Sequence *seq, int val);

#ifdef __cplusplus
}
#endif
