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
struct SeqRenderData;
struct Sequence;

#ifdef __cplusplus
}
#endif

/**
 * Start or resume prefetching.
 */
void seq_prefetch_start(const struct SeqRenderData *context, float timeline_frame);
void seq_prefetch_free(struct Scene *scene);
bool seq_prefetch_job_is_running(struct Scene *scene);
void seq_prefetch_get_time_range(struct Scene *scene, int *start, int *end);
/**
 * For cache context swapping.
 */
struct SeqRenderData *seq_prefetch_get_original_context(const struct SeqRenderData *context);
/**
 * For cache context swapping.
 */
struct Sequence *seq_prefetch_get_original_sequence(struct Sequence *seq, struct Scene *scene);

#ifdef __cplusplus
}
#endif
