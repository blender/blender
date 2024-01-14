/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_map.hh"
#include "BLI_span.hh"

struct Editing;
struct Scene;
struct Sequence;
struct SeqRetimingKey;

blender::MutableSpan<SeqRetimingKey> SEQ_retiming_keys_get(const Sequence *seq);
blender::Map<SeqRetimingKey *, Sequence *> SEQ_retiming_selection_get(const Editing *ed);
int SEQ_retiming_keys_count(const Sequence *seq);
bool SEQ_retiming_is_active(const Sequence *seq);
void SEQ_retiming_data_ensure(Sequence *seq);
void SEQ_retiming_data_clear(Sequence *seq);
bool SEQ_retiming_is_allowed(const Sequence *seq);
/**
 * Add new retiming key.
 * This function always reallocates memory, so when function is used all stored pointers will
 * become invalid.
 */
SeqRetimingKey *SEQ_retiming_add_key(const Scene *scene, Sequence *seq, int timeline_frame);
SeqRetimingKey *SEQ_retiming_add_transition(const Scene *scene,
                                            Sequence *seq,
                                            SeqRetimingKey *key,
                                            const int offset);
SeqRetimingKey *SEQ_retiming_add_freeze_frame(const Scene *scene,
                                              Sequence *seq,
                                              SeqRetimingKey *key,
                                              const int offset);
bool SEQ_retiming_is_last_key(const Sequence *seq, const SeqRetimingKey *key);
SeqRetimingKey *SEQ_retiming_last_key_get(const Sequence *seq);
void SEQ_retiming_remove_key(const Scene *scene, Sequence *seq, SeqRetimingKey *key);
void SEQ_retiming_transition_key_frame_set(const Scene *scene,
                                           const Sequence *seq,
                                           SeqRetimingKey *key,
                                           int timeline_frame);
float SEQ_retiming_key_speed_get(const Sequence *seq, const SeqRetimingKey *key);
void SEQ_retiming_key_speed_set(
    const Scene *scene, Sequence *seq, SeqRetimingKey *key, float speed, bool keep_retiming);
int SEQ_retiming_key_index_get(const Sequence *seq, const SeqRetimingKey *key);
SeqRetimingKey *SEQ_retiming_key_get_by_timeline_frame(const Scene *scene,
                                                       const Sequence *seq,
                                                       int timeline_frame);
void SEQ_retiming_sound_animation_data_set(const Scene *scene, const Sequence *seq);
int SEQ_retiming_key_timeline_frame_get(const Scene *scene,
                                        const Sequence *seq,
                                        const SeqRetimingKey *key);
void SEQ_retiming_key_timeline_frame_set(const Scene *scene,
                                         Sequence *seq,
                                         SeqRetimingKey *key,
                                         int timeline_frame);
SeqRetimingKey *SEQ_retiming_find_segment_start_key(const Sequence *seq, int frame_index);
bool SEQ_retiming_key_is_transition_type(const SeqRetimingKey *key);
bool SEQ_retiming_key_is_transition_start(const SeqRetimingKey *key);
SeqRetimingKey *SEQ_retiming_transition_start_get(SeqRetimingKey *key);
bool SEQ_retiming_key_is_freeze_frame(const SeqRetimingKey *key);
bool SEQ_retiming_selection_clear(const Editing *ed);
void SEQ_retiming_selection_append(SeqRetimingKey *key);
void SEQ_retiming_selection_remove(SeqRetimingKey *key);
void SEQ_retiming_remove_multiple_keys(Sequence *seq, blender::Vector<SeqRetimingKey *> &keys);
bool SEQ_retiming_selection_contains(const Editing *ed, const SeqRetimingKey *key);
bool SEQ_retiming_selection_has_whole_transition(const Editing *ed, SeqRetimingKey *key);
bool SEQ_retiming_data_is_editable(const Sequence *seq);
