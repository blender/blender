/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_listBase.h"

#include "BLI_span.hh"

namespace blender {

/** \file
 * \ingroup sequencer
 */

struct Scene;
struct Strip;

namespace seq {

void strip_update_sound_bounds_recursive(const Scene *scene, Strip *strip_meta);

/* Describes gap between strips in timeline. */
struct GapInfo {
  int gap_start_frame; /* Start frame of the gap. */
  int gap_length;      /* Length of the gap. */
  bool gap_exists;     /* False if there are no gaps. */
};

/**
 * Find first gap between strips after initial_frame and describe it by filling data of r_gap_info
 *
 * \param scene: Scene in which strips are located.
 * \param seqbase: List in which strips are located.
 * \param initial_frame: frame on timeline from where gaps are searched for.
 * \param r_gap_info: data structure describing gap, that will be filled in by this function.
 */
void seq_time_gap_info_get(const Scene *scene,
                           ListBaseT<Strip> *seqbase,
                           int initial_frame,
                           GapInfo *r_gap_info);
void strip_time_effect_range_set(const Scene *scene, Strip *strip);
/**
 * Update strip `startdisp` and `enddisp` (n-input effects have no length to calculate these).
 */
void strip_time_update_effects_strip_range(const Scene *scene, Span<Strip *> effects);
float strip_retiming_evaluate(const Strip *strip, const float frame_index);

}  // namespace seq
}  // namespace blender
