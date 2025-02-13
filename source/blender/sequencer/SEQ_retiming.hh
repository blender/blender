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
struct Strip;
struct SeqRetimingKey;

blender::MutableSpan<SeqRetimingKey> SEQ_retiming_keys_get(const Strip *strip);
blender::Map<SeqRetimingKey *, Strip *> SEQ_retiming_selection_get(const Editing *ed);
int SEQ_retiming_keys_count(const Strip *strip);
bool SEQ_retiming_is_active(const Strip *strip);
void SEQ_retiming_data_ensure(Strip *strip);
void SEQ_retiming_data_clear(Strip *strip);
void SEQ_retiming_reset(Scene *scene, Strip *strip);
bool SEQ_retiming_is_allowed(const Strip *strip);
/**
 * Add new retiming key.
 * This function always reallocates memory, so when function is used all stored pointers will
 * become invalid.
 */
SeqRetimingKey *SEQ_retiming_add_key(const Scene *scene, Strip *strip, int timeline_frame);
SeqRetimingKey *SEQ_retiming_add_transition(const Scene *scene,
                                            Strip *strip,
                                            SeqRetimingKey *key,
                                            float offset);
SeqRetimingKey *SEQ_retiming_add_freeze_frame(const Scene *scene,
                                              Strip *strip,
                                              SeqRetimingKey *key,
                                              const int offset);
bool SEQ_retiming_is_last_key(const Strip *strip, const SeqRetimingKey *key);
SeqRetimingKey *SEQ_retiming_last_key_get(const Strip *strip);
void SEQ_retiming_remove_key(Strip *strip, SeqRetimingKey *key);
void SEQ_retiming_transition_key_frame_set(const Scene *scene,
                                           const Strip *strip,
                                           SeqRetimingKey *key,
                                           int timeline_frame);
float SEQ_retiming_key_speed_get(const Strip *strip, const SeqRetimingKey *key);
void SEQ_retiming_key_speed_set(
    const Scene *scene, Strip *strip, SeqRetimingKey *key, float speed, bool keep_retiming);
int SEQ_retiming_key_index_get(const Strip *strip, const SeqRetimingKey *key);
SeqRetimingKey *SEQ_retiming_key_get_by_timeline_frame(const Scene *scene,
                                                       const Strip *strip,
                                                       int timeline_frame);
void SEQ_retiming_sound_animation_data_set(const Scene *scene, const Strip *strip);
int SEQ_retiming_key_timeline_frame_get(const Scene *scene,
                                        const Strip *strip,
                                        const SeqRetimingKey *key);
void SEQ_retiming_key_timeline_frame_set(const Scene *scene,
                                         Strip *strip,
                                         SeqRetimingKey *key,
                                         int timeline_frame);
SeqRetimingKey *SEQ_retiming_find_segment_start_key(const Strip *strip, float frame_index);
bool SEQ_retiming_key_is_transition_type(const SeqRetimingKey *key);
bool SEQ_retiming_key_is_transition_start(const SeqRetimingKey *key);
SeqRetimingKey *SEQ_retiming_transition_start_get(SeqRetimingKey *key);
bool SEQ_retiming_key_is_freeze_frame(const SeqRetimingKey *key);
bool SEQ_retiming_selection_clear(const Editing *ed);
void SEQ_retiming_selection_append(SeqRetimingKey *key);
void SEQ_retiming_selection_remove(SeqRetimingKey *key);
void SEQ_retiming_selection_copy(SeqRetimingKey *dst, const SeqRetimingKey *src);
void SEQ_retiming_remove_multiple_keys(Strip *strip,
                                       blender::Vector<SeqRetimingKey *> &keys_to_remove);
bool SEQ_retiming_selection_contains(const Editing *ed, const SeqRetimingKey *key);
bool SEQ_retiming_selection_has_whole_transition(const Editing *ed, SeqRetimingKey *key);
bool SEQ_retiming_data_is_editable(const Strip *strip);
