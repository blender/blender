/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 *
 * Cached intermediate images used while rendering one sequencer frame.
 * - For each strip, "preprocessed" (strip source, possibly
 *   transformed, with modifiers applied) and "composite" (result of
 *   blending this strip with image underneath) images are cached.
 * - Whenever going to a different frame, the cached content of previous
 *   frame is cleared.
 * - Primary reason for having this cache at all, is when the whole frame
 *   is a complex stack of things, and you want to tweak settings of one
 *   of the involved strips. You don't want to be re-calculating all the
 *   strips that are "below" your tweaked strip, for better interactivity.
 */

#pragma once

struct ImBuf;
struct Strip;
struct Scene;

namespace blender::seq {

ImBuf *intra_frame_cache_get_preprocessed(Scene *scene, const Strip *strip);
ImBuf *intra_frame_cache_get_composite(Scene *scene, const Strip *strip);
void intra_frame_cache_put_preprocessed(Scene *scene, const Strip *strip, ImBuf *image);
void intra_frame_cache_put_composite(Scene *scene, const Strip *strip, ImBuf *image);

void intra_frame_cache_destroy(Scene *scene);

void intra_frame_cache_invalidate(Scene *scene, const Strip *strip);
void intra_frame_cache_invalidate(Scene *scene);

void intra_frame_cache_set_cur_frame(
    Scene *scene, float frame, int view_id, int width, int height);

}  // namespace blender::seq
