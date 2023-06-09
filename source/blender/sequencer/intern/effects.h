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

struct SeqEffectHandle seq_effect_get_sequence_blend(struct Sequence *seq);
/**
 * Build frame map when speed in mode #SEQ_SPEED_MULTIPLY is animated.
 * This is, because `target_frame` value is integrated over time.
 */
void seq_effect_speed_rebuild_map(struct Scene *scene, struct Sequence *seq);
/**
 * Override timeline_frame when rendering speed effect input.
 */
float seq_speed_effect_target_frame_get(struct Scene *scene,
                                        struct Sequence *seq,
                                        float timeline_frame,
                                        int input);

#ifdef __cplusplus
}
#endif
