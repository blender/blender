/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation. */

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
void SEQ_retiming_data_ensure(struct Sequence *seq);
void SEQ_retiming_data_clear(struct Sequence *seq);
bool SEQ_retiming_is_allowed(const struct Sequence *seq);

/**
 * Add new retiming handle.
 * This function always reallocates memory, so when function is used all stored pointers will
 * become invalid.
 */
struct SeqRetimingHandle *SEQ_retiming_add_handle(struct Scene *scene,
                                                  struct Sequence *seq,
                                                  const int timeline_frame);
struct SeqRetimingHandle *SEQ_retiming_last_handle_get(const struct Sequence *seq);
void SEQ_retiming_remove_handle(struct Sequence *seq, struct SeqRetimingHandle *handle);
void SEQ_retiming_offset_handle(const struct Scene *scene,
                                struct Sequence *seq,
                                struct SeqRetimingHandle *handle,
                                const int offset);
float SEQ_retiming_handle_speed_get(const struct Sequence *seq,
                                    const struct SeqRetimingHandle *handle);
int SEQ_retiming_handle_index_get(const struct Sequence *seq,
                                  const struct SeqRetimingHandle *handle);
void SEQ_retiming_sound_animation_data_set(const struct Scene *scene, const struct Sequence *seq);
float SEQ_retiming_handle_timeline_frame_get(const struct Scene *scene,
                                             const struct Sequence *seq,
                                             const struct SeqRetimingHandle *handle);
#ifdef __cplusplus
}
#endif
