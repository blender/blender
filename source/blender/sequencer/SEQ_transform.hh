/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_span.hh"

namespace blender {

struct Scene;
struct Strip;
struct SeqTimelineChannel;
struct TimeMarker;

namespace seq {

bool transform_strip_can_be_translated(const Strip *strip);
/**
 * Checks whether the strip functions as a single static display,
 * which means it has only one unique frame of content and does not draw holds.
 * This includes non-sequence image strips and all effect strips with no inputs (e.g. color, text).
 */
bool transform_single_image_check(const Strip *strip);
bool transform_test_overlap(const Scene *scene, ListBaseT<Strip> *seqbasep, Strip *test);
bool transform_test_overlap(const Scene *scene, Strip *strip1, Strip *strip2);
void transform_translate_strip(Scene *evil_scene, Strip *strip, int delta);
/**
 * \return 0 if there weren't enough space.
 */
bool transform_seqbase_shuffle_ex(ListBaseT<Strip> *seqbasep,
                                  Strip *test,
                                  Scene *evil_scene,
                                  int channel_delta);
bool transform_seqbase_shuffle(ListBaseT<Strip> *seqbasep, Strip *test, Scene *evil_scene);
bool transform_seqbase_shuffle_time(Span<Strip *> strips_to_shuffle,
                                    Span<Strip *> time_dependent_strips,
                                    ListBaseT<Strip> *seqbasep,
                                    Scene *evil_scene,
                                    ListBaseT<TimeMarker> *markers,
                                    bool use_sync_markers);
bool transform_seqbase_shuffle_time(Span<Strip *> strips_to_shuffle,
                                    ListBaseT<Strip> *seqbasep,
                                    Scene *evil_scene,
                                    ListBaseT<TimeMarker> *markers,
                                    bool use_sync_markers);

void transform_handle_overlap(Scene *scene,
                              ListBaseT<Strip> *seqbasep,
                              Span<Strip *> transformed_strips,
                              Span<Strip *> time_dependent_strips,
                              bool use_sync_markers);
void transform_handle_overlap(Scene *scene,
                              ListBaseT<Strip> *seqbasep,
                              Span<Strip *> transformed_strips,
                              bool use_sync_markers);
/**
 * Move strips and markers (if not locked) that start after timeline_frame by delta frames
 *
 * \param scene: Scene in which strips are located
 * \param seqbase: List in which strips are located
 * \param delta: offset in frames to be applied
 * \param timeline_frame: frame on timeline from where strips are moved
 */
void transform_offset_after_frame(Scene *scene,
                                  ListBaseT<Strip> *seqbase,
                                  int delta,
                                  int timeline_frame);

/**
 * Check if `strip` can be moved.
 * This function also checks `SeqTimelineChannel` flag.
 */
bool transform_is_locked(const ListBaseT<SeqTimelineChannel> *channels, const Strip *strip);

/* Image transformation. */

/**
 * Get per-axis mirror factors for a \a strip image.
 * \return float2 where each component is 1.0f (normal) or -1.0f (mirrored). */
float2 image_transform_mirror_factor_get(const Strip *strip);

/**
 * Get the \a strip origin as a fraction of its rendered image. This origin can be anywhere, but
 * (0,0) corresponds to the bottom left of the image, and (1,1) the top right.
 *
 * NOTE: #StripTransform::origin is stored relative to the strip box
 * (#image_transform_box_size_get), which for text strips is smaller than their rendered image.
 * This function properly converts it to be relative to the rendered image for the render pipeline
 * to use. Being a fraction, it is independent of proxy render size.
 */
float2 image_transform_origin_get(const Scene *scene, const Strip *strip);

/**
 * Get the \a strip origin's offset in view-space pixels from the preview's center, including axis
 * mirror and viewport pixel aspect.
 */
float2 image_transform_origin_preview_offset_get(const Scene *scene, const Strip *strip);

/**
 * Get \a strip image transformation matrix relative to its origin in view-space, including axis
 * mirror and viewport pixel aspect.
 */
float3x3 image_transform_matrix_get(const Scene *scene, const Strip *strip);

/**
 * Get the size of the drawn \a strip quad before any cropping, scaling, or transformation.
 * This is the size of the rendered `ImBuf` for every type but text strips, where it is the tighter
 * bounding box of the text glyphs.
 *
 * For the fully-processed quad, see #image_transform_quad_get.
 * For the bounding box of the quad, see #image_transform_bounding_box_from_strips_get.
 *
 * \return float2 with (width, height) in view-space pixels
 */
float2 image_transform_box_size_get(const Scene *scene, const Strip *strip);

/**
 * Get 4 corner points of strip image. Corner vectors are in viewport space.
 * Indices correspond to following corners (assuming no rotation):
 * 3--0
 * |  |
 * 2--1
 *
 * \param strip: Strip to calculate transformed image quad
 * \return array of four 2D points
 */
Array<float2> image_transform_quad_get(const Scene *scene, const Strip *strip);

float2 image_preview_unit_to_px(const Scene *scene, float2 co_src);
float2 image_preview_unit_from_px(const Scene *scene, float2 co_src);

/**
 * Get viewport axis aligned bounding box from multiple strips.
 * \param strips: Span of strips to calculate the bounding box for
 */
Bounds<float2> image_transform_bounding_box_from_strips_get(Scene *scene, Span<Strip *> strips);

}  // namespace seq
}  // namespace blender
