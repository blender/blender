/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 *
 * Additionally, this holds GPU textures representing the current frame.
 * This is to avoid the same GPU texture needing to get re-created if there
 * are multiple preview areas open (e.g. with scopes).
 */

#pragma once

struct Scene;

namespace blender::gpu {
class Texture;
}

namespace blender::seq {

gpu::Texture *preview_cache_get_gpu_texture(Scene *scene, int timeline_frame, int display_channel);
void preview_cache_set_gpu_texture(Scene *scene,
                                   int timeline_frame,
                                   int display_channel,
                                   gpu::Texture *texture);
gpu::Texture *preview_cache_get_gpu_display_texture(Scene *scene,
                                                    int timeline_frame,
                                                    int display_channel);
void preview_cache_set_gpu_display_texture(Scene *scene,
                                           int timeline_frame,
                                           int display_channel,
                                           gpu::Texture *texture);

void preview_cache_invalidate(Scene *scene);
void preview_cache_destroy(Scene *scene);

}  // namespace blender::seq
