/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 *
 * Cache source images for strips.
 * - Keyed by (strip + frame index within strip media + view ID).
 * - Caching is only done for strips that are independent of
 *   any other strips (images, movies, no-input effect strips like
 *   Text and Color).
 * - When full, cache eviction policy is to remove frames furthest
 *   from the current-frame, biasing towards removal of
 *   frames behind the current-frame.
 * - Invalidated fairly rarely, since the cached items only change
 *   when the source content changes.
 */

#pragma once

struct ImBuf;
struct Strip;
struct Scene;
struct RenderData;

namespace blender::seq {

void source_image_cache_put(const RenderData *context,
                            const Strip *strip,
                            float timeline_frame,
                            ImBuf *image);

ImBuf *source_image_cache_get(const RenderData *context, const Strip *strip, float timeline_frame);

void source_image_cache_invalidate_strip(Scene *scene, const Strip *strip);

void source_image_cache_clear(Scene *scene);
void source_image_cache_destroy(Scene *scene);

bool source_image_cache_evict(Scene *scene);

size_t source_image_cache_get_image_count(const Scene *scene);

}  // namespace blender::seq
