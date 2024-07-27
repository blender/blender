/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "SEQ_effects.hh"

struct Scene;
struct Sequence;

SeqEffectHandle seq_effect_get_sequence_blend(Sequence *seq);
/**
 * Build frame map when speed in mode #SEQ_SPEED_MULTIPLY is animated.
 * This is, because `target_frame` value is integrated over time.
 */
void seq_effect_speed_rebuild_map(Scene *scene, Sequence *seq);
/**
 * Override timeline_frame when rendering speed effect input.
 */
float seq_speed_effect_target_frame_get(Scene *scene,
                                        Sequence *seq_speed,
                                        float timeline_frame,
                                        int input);
