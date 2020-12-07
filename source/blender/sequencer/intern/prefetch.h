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

struct ImBuf;
struct Main;
struct Scene;
struct Sequence;
struct SeqRenderData;

#ifdef __cplusplus
}
#endif

void BKE_sequencer_prefetch_start(const struct SeqRenderData *context,
                                  float timeline_frame,
                                  float cost);
void BKE_sequencer_prefetch_free(struct Scene *scene);
bool BKE_sequencer_prefetch_job_is_running(struct Scene *scene);
void BKE_sequencer_prefetch_get_time_range(struct Scene *scene, int *start, int *end);
struct SeqRenderData *BKE_sequencer_prefetch_get_original_context(
    const struct SeqRenderData *context);
struct Sequence *BKE_sequencer_prefetch_get_original_sequence(struct Sequence *seq,
                                                              struct Scene *scene);

#ifdef __cplusplus
}
#endif
