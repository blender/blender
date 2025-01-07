/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct Scene;
struct SeqRenderData;
struct Strip;

/**
 * Start or resume prefetching.
 */
void seq_prefetch_start(const SeqRenderData *context, float timeline_frame);
void seq_prefetch_free(Scene *scene);
bool seq_prefetch_job_is_running(Scene *scene);
void seq_prefetch_get_time_range(Scene *scene, int *r_start, int *r_end);
/**
 * For cache context swapping.
 */
SeqRenderData *seq_prefetch_get_original_context(const SeqRenderData *context);
/**
 * For cache context swapping.
 */
Strip *seq_prefetch_get_original_sequence(Strip *strip, Scene *scene);
