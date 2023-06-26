/* SPDX-FileCopyrightText: 2004 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Scene;
struct Sequence;
struct SeqRetimingHandle;

int SEQ_retiming_handles_count(const struct Sequence *seq);
bool SEQ_retiming_is_active(const struct Sequence *seq);
void SEQ_retiming_data_ensure(const struct Scene *scene, struct Sequence *seq);
void SEQ_retiming_data_clear(struct Sequence *seq);
bool SEQ_retiming_is_allowed(const struct Sequence *seq);

/**
 * Add new retiming handle.
 * This function always reallocates memory, so when function is used all stored pointers will
 * become invalid.
 */
struct SeqRetimingHandle *SEQ_retiming_add_handle(const struct Scene *scene,
                                                  struct Sequence *seq,
                                                  const int timeline_frame);
SeqRetimingHandle *SEQ_retiming_add_transition(const struct Scene *scene,
                                               struct Sequence *seq,
                                               struct SeqRetimingHandle *handle,
                                               const int offset);
SeqRetimingHandle *SEQ_retiming_add_freeze_frame(const struct Scene *scene,
                                                 struct Sequence *seq,
                                                 struct SeqRetimingHandle *handle,
                                                 const int offset);
struct SeqRetimingHandle *SEQ_retiming_last_handle_get(const struct Sequence *seq);
void SEQ_retiming_remove_handle(const struct Scene *scene,
                                struct Sequence *seq,
                                struct SeqRetimingHandle *handle);
void SEQ_retiming_offset_handle(const struct Scene *scene,
                                struct Sequence *seq,
                                struct SeqRetimingHandle *handle,
                                const int offset);
float SEQ_retiming_handle_speed_get(const struct Sequence *seq,
                                    const struct SeqRetimingHandle *handle);
void SEQ_retiming_handle_speed_set(const struct Scene *scene,
                                   struct Sequence *seq,
                                   struct SeqRetimingHandle *handle,
                                   const float speed);
int SEQ_retiming_handle_index_get(const struct Sequence *seq,
                                  const struct SeqRetimingHandle *handle);
void SEQ_retiming_sound_animation_data_set(const struct Scene *scene, const struct Sequence *seq);
float SEQ_retiming_handle_timeline_frame_get(const struct Scene *scene,
                                             const struct Sequence *seq,
                                             const struct SeqRetimingHandle *handle);
const SeqRetimingHandle *SEQ_retiming_find_segment_start_handle(const struct Sequence *seq,
                                                                const int frame_index);
bool SEQ_retiming_handle_is_transition_type(const struct SeqRetimingHandle *handle);
bool SEQ_retiming_handle_is_freeze_frame(const struct SeqRetimingHandle *handle);
#ifdef __cplusplus
}
#endif
