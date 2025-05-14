/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct Scene;
struct Strip;

namespace blender::seq {

struct RenderData;

/**
 * Start or resume prefetching.
 */
void seq_prefetch_start(const RenderData *context, float timeline_frame);
void seq_prefetch_free(Scene *scene);
bool seq_prefetch_job_is_running(Scene *scene);
void seq_prefetch_get_time_range(Scene *scene, int *r_start, int *r_end);

Scene *prefetch_get_original_scene(const RenderData *context);
Scene *prefetch_get_original_scene_and_strip(const RenderData *context, const Strip *&strip);

}  // namespace blender::seq
