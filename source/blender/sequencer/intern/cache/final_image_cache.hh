/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 *
 * Cache of final rendered frames.
 * - Keyed by (timeline frame, view_id).
 * - When full, cache eviction policy is to remove frames furthest
 *   from the current-frame, biasing towards removal of
 *   frames behind the current-frame.
 * - Invalidated fairly often while editing, basically whenever any
 *   strip overlapping that frame changes.
 */

#pragma once

struct ImBuf;
struct Strip;
struct Scene;

namespace blender::seq {

void final_image_cache_put(Scene *scene,
                           const ListBase *seqbasep,
                           float timeline_frame,
                           int view_id,
                           int display_channel,
                           ImBuf *image);

ImBuf *final_image_cache_get(Scene *scene,
                             const ListBase *seqbasep,
                             float timeline_frame,
                             int view_id,
                             int display_channel);

void final_image_cache_invalidate_frame_range(Scene *scene,
                                              const float timeline_frame_start,
                                              const float timeline_frame_end);

void final_image_cache_clear(Scene *scene);
void final_image_cache_destroy(Scene *scene);

bool final_image_cache_evict(Scene *scene);

size_t final_image_cache_get_image_count(const Scene *scene);

}  // namespace blender::seq
