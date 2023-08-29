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
void SEQ_timeline_expand_boundbox(const struct Scene *scene,
                                  const struct ListBase *seqbase,
                                  struct rctf *rect);
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
/**
 * Get FPS rate of source media. Movie, scene and movie-clip strips are supported.
 * Returns 0 for unsupported strip or if media can't be loaded.
 */
float SEQ_time_sequence_get_fps(struct Scene *scene, struct Sequence *seq);
/**
 * Find start or end position of next or previous strip.
 * \param scene: Video editing scene
 * \param timeline_frame: reference frame for searching
 * \param side: direction of searching, `SEQ_SIDE_LEFT`, `SEQ_SIDE_RIGHT` or `SEQ_SIDE_BOTH`.
 * \param do_center: find closest strip center if true, otherwise finds closest handle position.
 * \param do_unselected: only find closest position of unselected strip.
 */
int SEQ_time_find_next_prev_edit(struct Scene *scene,
                                 int timeline_frame,
                                 short side,
                                 bool do_skip_mute,
                                 bool do_center,
                                 bool do_unselected);
/**
 * Test if strip intersects with timeline frame.
 * \note This checks if strip would be rendered at this frame. For rendering it is assumed, that
 * timeline frame has width of 1 frame and therefore ends at timeline_frame + 1
 *
 * \param seq: Sequence to be checked
 * \param timeline_frame: absolute frame position
 * \return true if strip intersects with timeline frame.
 */
bool SEQ_time_strip_intersects_frame(const struct Scene *scene,
                                     const struct Sequence *seq,
                                     int timeline_frame);
/* Convert timeline frame so strip frame index. */
float SEQ_give_frame_index(const struct Scene *scene, struct Sequence *seq, float timeline_frame);
/**
 * Returns true if strip has frames without content to render.
 */
bool SEQ_time_has_still_frames(const struct Scene *scene, const struct Sequence *seq);
/**
 * Returns true if at beginning of strip there is no content to be rendered.
 */
bool SEQ_time_has_left_still_frames(const struct Scene *scene, const struct Sequence *seq);
/**
 * Returns true if at end of strip there is no content to be rendered.
 */
bool SEQ_time_has_right_still_frames(const struct Scene *scene, const struct Sequence *seq);
/**
 * Get timeline frame where strip boundary starts.
 */
int SEQ_time_left_handle_frame_get(const struct Scene *scene, const struct Sequence *seq);
/**
 * Get timeline frame where strip boundary ends.
 */
int SEQ_time_right_handle_frame_get(const struct Scene *scene, const struct Sequence *seq);
/**
 * Set frame where strip boundary starts. This function moves only handle, content is not moved.
 */
void SEQ_time_left_handle_frame_set(const struct Scene *scene,
                                    struct Sequence *seq,
                                    int timeline_frame);
/**
 * Set frame where strip boundary ends.
 * This function moves only handle, content is not moved.
 */
void SEQ_time_right_handle_frame_set(const struct Scene *scene,
                                     struct Sequence *seq,
                                     int timeline_frame);
/**
 * Get number of frames (in timeline) that can be rendered.
 * This can change depending on scene FPS or strip speed factor.
 */
int SEQ_time_strip_length_get(const struct Scene *scene, const struct Sequence *seq);
/**
 * Set strip playback speed.
 * Strip length is affected by changing speed factor.
 */
void SEQ_time_speed_factor_set(const struct Scene *scene,
                               struct Sequence *seq,
                               const float speed_factor);
/**
 * Get timeline frame where strip content starts.
 */
float SEQ_time_start_frame_get(const struct Sequence *seq);
/**
 * Get timeline frame where strip content ends.
 */
float SEQ_time_content_end_frame_get(const struct Scene *scene, const struct Sequence *seq);
/**
 * Set frame where strip content starts.
 * This function will also move strip handles.
 */
void SEQ_time_start_frame_set(const struct Scene *scene, struct Sequence *seq, int timeline_frame);
/**
 * Update meta strip content start and end, update sound playback range.
 * To be used after any contained strip length or position has changed.
 *
 * \note this function is currently only used internally and in versioning code.
 */
void SEQ_time_update_meta_strip_range(const struct Scene *scene, struct Sequence *seq_meta);
#ifdef __cplusplus
}
#endif
