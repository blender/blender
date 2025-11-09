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
 * Get FPS rate of source media. Movie, scene and movie-clip strips are supported.
 * Returns 0 for unsupported strip or if media can't be loaded.
 */
float time_strip_fps_get(Scene *scene, Strip *strip);
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
/**
 * Test if strip intersects with timeline frame.
 * \note This checks if strip would be rendered at this frame. For rendering it is assumed, that
 * timeline frame has width of 1 frame and therefore ends at timeline_frame + 1
 *
 * \param strip: Strip to be checked
 * \param timeline_frame: absolute frame position
 * \return true if strip intersects with timeline frame.
 */
bool time_strip_intersects_frame(const Scene *scene, const Strip *strip, int timeline_frame);
/* Convert timeline frame so strip frame index. */
float give_frame_index(const Scene *scene, const Strip *strip, float timeline_frame);
/**
 * Returns true if strip has frames without content to render.
 */
bool time_has_still_frames(const Scene *scene, const Strip *strip);
/**
 * Returns true if at beginning of strip there is no content to be rendered.
 */
bool time_has_left_still_frames(const Scene *scene, const Strip *strip);
/**
 * Returns true if at end of strip there is no content to be rendered.
 */
bool time_has_right_still_frames(const Scene *scene, const Strip *strip);
/**
 * Get timeline frame where strip boundary starts.
 */
int time_left_handle_frame_get(const Scene *scene, const Strip *strip);
/**
 * Get timeline frame where strip boundary ends.
 */
int time_right_handle_frame_get(const Scene *scene, const Strip *strip);
/**
 * Set frame where strip boundary starts. This function moves only handle, content is not moved.
 */
void time_left_handle_frame_set(const Scene *scene, Strip *strip, int timeline_frame);
/**
 * Set frame where strip boundary ends.
 * This function moves only handle, content is not moved.
 */
void time_right_handle_frame_set(const Scene *scene, Strip *strip, int timeline_frame);
/**
 * This function has same effect as calling @time_right_handle_frame_set and
 * @time_right_handle_frame_set. If both handles are to be set after strip length changes, it is
 * recommended to use this function as the order of setting handles is important. See #131731.
 */
void time_handles_frame_set(const Scene *scene,
                            Strip *strip,
                            int left_handle_timeline_frame,
                            int right_handle_timeline_frame);
/**
 * Get number of frames (in timeline) that can be rendered.
 * This can change depending on scene FPS or strip speed factor.
 */
int time_strip_length_get(const Scene *scene, const Strip *strip);
/**
 * Get timeline frame where strip content starts.
 */
float time_start_frame_get(const Strip *strip);
/**
 * Get timeline frame where strip content ends.
 */
float time_content_end_frame_get(const Scene *scene, const Strip *strip);
/**
 * Set frame where strip content starts.
 * This function will also move strip handles.
 */
void time_start_frame_set(const Scene *scene, Strip *strip, int timeline_frame);
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
/**
 * Get difference between scene and movie strip frame-rate.
 * Returns 1.0f for all other strip types.
 */
float time_media_playback_rate_factor_get(const Strip *strip, float scene_fps);
/**
 * Get the sound offset (if any) and round it to the nearest integer.
 * This is mostly used in places where subframe data is not allowed (like re-timing key positions).
 * Returns zero if strip is not a sound strip or if there is no offset.
 */
int time_get_rounded_sound_offset(const Strip *strip, float frames_per_second);

}  // namespace blender::seq
