/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_map.hh"
#include "BLI_span.hh"

struct Sequence;
struct SeqRetimingKey;

blender::MutableSpan<SeqRetimingKey> SEQ_retiming_keys_get(const Sequence *seq);
blender::Map<SeqRetimingKey *, Sequence *> SEQ_retiming_selection_get(const struct Editing *ed);
int SEQ_retiming_keys_count(const struct Sequence *seq);
bool SEQ_retiming_is_active(const struct Sequence *seq);
void SEQ_retiming_data_ensure(struct Sequence *seq);
void SEQ_retiming_data_clear(struct Sequence *seq);
bool SEQ_retiming_is_allowed(const struct Sequence *seq);
/**
 * Add new retiming key.
 * This function always reallocates memory, so when function is used all stored pointers will
 * become invalid.
 */
SeqRetimingKey *SEQ_retiming_add_key(const struct Scene *scene,
                                     struct Sequence *seq,
                                     const int timeline_frame);
SeqRetimingKey *SEQ_retiming_add_transition(const struct Scene *scene,
                                            struct Sequence *seq,
                                            struct SeqRetimingKey *key,
                                            const int offset);
SeqRetimingKey *SEQ_retiming_add_freeze_frame(const struct Scene *scene,
                                              struct Sequence *seq,
                                              struct SeqRetimingKey *key,
                                              const int offset);
bool SEQ_retiming_is_last_key(const struct Sequence *seq, const struct SeqRetimingKey *key);
struct SeqRetimingKey *SEQ_retiming_last_key_get(const struct Sequence *seq);
void SEQ_retiming_remove_key(const struct Scene *scene,
                             struct Sequence *seq,
                             struct SeqRetimingKey *key);
void SEQ_retiming_offset_transition_key(const struct Scene *scene,
                                        const struct Sequence *seq,
                                        struct SeqRetimingKey *key,
                                        const int offset);
float SEQ_retiming_key_speed_get(const struct Sequence *seq, const struct SeqRetimingKey *key);
void SEQ_retiming_key_speed_set(const struct Scene *scene,
                                struct Sequence *seq,
                                struct SeqRetimingKey *key,
                                const float speed);
int SEQ_retiming_key_index_get(const struct Sequence *seq, const struct SeqRetimingKey *key);
SeqRetimingKey *SEQ_retiming_key_get_by_timeline_frame(const struct Scene *scene,
                                                       const struct Sequence *seq,
                                                       const int timeline_frame);
void SEQ_retiming_sound_animation_data_set(const struct Scene *scene, const struct Sequence *seq);
float SEQ_retiming_key_timeline_frame_get(const struct Scene *scene,
                                          const struct Sequence *seq,
                                          const struct SeqRetimingKey *key);
void SEQ_retiming_key_timeline_frame_set(const struct Scene *scene,
                                         struct Sequence *seq,
                                         struct SeqRetimingKey *key,
                                         const int timeline_frame);
SeqRetimingKey *SEQ_retiming_find_segment_start_key(const struct Sequence *seq,
                                                    const int frame_index);
bool SEQ_retiming_key_is_transition_type(const struct SeqRetimingKey *key);
bool SEQ_retiming_key_is_transition_start(const struct SeqRetimingKey *key);
SeqRetimingKey *SEQ_retiming_transition_start_get(struct SeqRetimingKey *key);
bool SEQ_retiming_key_is_freeze_frame(const struct SeqRetimingKey *key);
bool SEQ_retiming_selection_clear(const struct Editing *ed);
void SEQ_retiming_selection_append(struct SeqRetimingKey *key);
void SEQ_retiming_selection_remove(struct SeqRetimingKey *key);
void SEQ_retiming_remove_multiple_keys(struct Sequence *seq,
                                       blender::Vector<SeqRetimingKey *> &keys);
bool SEQ_retiming_selection_contains(const struct Editing *ed, const struct SeqRetimingKey *key);
bool SEQ_retiming_selection_has_whole_transition(const struct Editing *ed,
                                                 struct SeqRetimingKey *key);
bool SEQ_retiming_data_is_editable(const struct Sequence *seq);
