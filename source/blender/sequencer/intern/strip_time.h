/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

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

float seq_give_frame_index(struct Sequence *seq, float timeline_frame);
void seq_update_sound_bounds_recursive(struct Scene *scene, struct Sequence *metaseq);

/* Describes gap between strips in timeline. */
typedef struct GapInfo {
  int gap_start_frame; /* Start frame of the gap. */
  int gap_length;      /* Length of the gap. */
  bool gap_exists;     /* False if there are no gaps. */
} GapInfo;

/**
 * Find first gap between strips after initial_frame and describe it by filling data of r_gap_info
 *
 * \param scene: Scene in which strips are located.
 * \param seqbase: ListBase in which strips are located.
 * \param initial_frame: frame on timeline from where gaps are searched for.
 * \param r_gap_info: data structure describing gap, that will be filled in by this function.
 */
void seq_time_gap_info_get(const struct Scene *scene,
                           struct ListBase *seqbase,
                           int initial_frame,
                           struct GapInfo *r_gap_info);

#ifdef __cplusplus
}
#endif
