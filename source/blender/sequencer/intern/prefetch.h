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

struct Scene;
struct SeqRenderData;
struct Sequence;

#ifdef __cplusplus
}
#endif

void seq_prefetch_start(const struct SeqRenderData *context, float timeline_frame);
void seq_prefetch_free(struct Scene *scene);
bool seq_prefetch_job_is_running(struct Scene *scene);
void seq_prefetch_get_time_range(struct Scene *scene, int *start, int *end);
struct SeqRenderData *seq_prefetch_get_original_context(const struct SeqRenderData *context);
struct Sequence *seq_prefetch_get_original_sequence(struct Sequence *seq, struct Scene *scene);

#ifdef __cplusplus
}
#endif
