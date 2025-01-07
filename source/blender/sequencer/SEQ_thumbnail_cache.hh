/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#pragma once

struct bContext;
struct ImBuf;
struct rctf;
struct Strip;
struct Scene;

namespace blender::seq {

static constexpr int SEQ_THUMB_SIZE = 256;

/**
 * Get a thumbnail image for given strip `seq` at `timeline_frame`.
 *
 * The function can return null if a strip type does not have a thumbnail, a source media file is
 * not found, or the thumbnail has not been loaded yet.
 *
 * A "closest" thumbnail if there is no exact match can also be returned, e.g. for a movie strip
 * the closest frame that has a thumbnail already.
 *
 * When there is no exact match, a request to load a thumbnail will be internally added and
 * processed in the background. */
ImBuf *thumbnail_cache_get(const bContext *C,
                           Scene *scene,
                           const Strip *strip,
                           float timeline_frame);

/**
 * If total amount of resident thumbnails is too large, try to remove oldest-used ones to
 * keep the cache size in check.
 */
void thumbnail_cache_maintain_capacity(Scene *scene);

void thumbnail_cache_invalidate_strip(Scene *scene, const Strip *strip);

/**
 * Discard in-flight thumbnail loading requests that are outside of the given view (X coordinate:
 * timeline frames, Y coordinate: channels).
 */
void thumbnail_cache_discard_requests_outside(Scene *scene, const rctf &rect);

void thumbnail_cache_clear(Scene *scene);
void thumbnail_cache_destroy(Scene *scene);

bool strip_can_have_thumbnail(const Scene *scene, const Strip *strip);

}  // namespace blender::seq
