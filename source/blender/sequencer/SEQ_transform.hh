/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_span.hh"

struct ListBase;
struct Scene;
struct Strip;

namespace blender::seq {

bool transform_strip_can_be_translated(const Strip *strip);
/**
 * Checks whether the strip functions as a single static display,
 * which means it has only one unique frame of content and does not draw holds.
 * This includes non-sequence image strips and all effect strips with no inputs (e.g. color, text).
 */
bool transform_single_image_check(const Strip *strip);
bool transform_test_overlap(const Scene *scene, ListBase *seqbasep, Strip *test);
bool transform_test_overlap(const Scene *scene, Strip *strip1, Strip *strip2);
void transform_translate_strip(Scene *evil_scene, Strip *strip, int delta);
/**
 * \return 0 if there weren't enough space.
 */
bool transform_seqbase_shuffle_ex(ListBase *seqbasep,
                                  Strip *test,
                                  Scene *evil_scene,
                                  int channel_delta);
bool transform_seqbase_shuffle(ListBase *seqbasep, Strip *test, Scene *evil_scene);
bool transform_seqbase_shuffle_time(Span<Strip *> strips_to_shuffle,
                                    Span<Strip *> time_dependent_strips,
                                    ListBase *seqbasep,
                                    Scene *evil_scene,
                                    ListBase *markers,
                                    bool use_sync_markers);
bool transform_seqbase_shuffle_time(Span<Strip *> strips_to_shuffle,
                                    ListBase *seqbasep,
                                    Scene *evil_scene,
                                    ListBase *markers,
                                    bool use_sync_markers);

void transform_handle_overlap(Scene *scene,
                              ListBase *seqbasep,
                              Span<Strip *> transformed_strips,
                              Span<Strip *> time_dependent_strips,
                              bool use_sync_markers);
void transform_handle_overlap(Scene *scene,
                              ListBase *seqbasep,
                              Span<Strip *> transformed_strips,
                              bool use_sync_markers);
/**
 * Set strip channel. This value is clamped to valid values.
 */
void strip_channel_set(Strip *strip, int channel);
/**
 * Move strips and markers (if not locked) that start after timeline_frame by delta frames
 *
 * \param scene: Scene in which strips are located
 * \param seqbase: ListBase in which strips are located
 * \param delta: offset in frames to be applied
 * \param timeline_frame: frame on timeline from where strips are moved
 */
void transform_offset_after_frame(Scene *scene, ListBase *seqbase, int delta, int timeline_frame);

/**
 * Check if `strip` can be moved.
 * This function also checks `SeqTimelineChannel` flag.
 */
bool transform_is_locked(ListBase *channels, const Strip *strip);

/* Image transformation. */

float2 image_transform_mirror_factor_get(const Strip *strip);
/**
 * Get strip transform origin offset from image center
 * NOTE: This function does not apply axis mirror.
 *
 * \param scene: Scene in which strips are located
 * \param strip: Strip to calculate image transform origin
 */
float2 image_transform_origin_offset_pixelspace_get(const Scene *scene, const Strip *strip);

/**
 * Get strip transform origin relative value. This function is mainly needed to
 * recalculate text strip origin position.
 *
 * \param render_size: Size of image canvas in pixels
 * \param strip: Strip to calculate origin for
 */
float2 image_transform_origin_get(const Scene *scene, const Strip *strip);

/**
 * Get size of the image, which is produced by strip without any transformation.
 *
 * \param render_size: Size of image canvas in pixels
 * \param strip: Strip to calculate origin for
 */
float2 transform_image_raw_size_get(const Scene *scene, const Strip *strip);

/**
 * Get 4 corner points of strip image, optionally without rotation component applied.
 * Corner vectors are in viewport space.
 *
 * \param scene: Scene in which strips are located
 * \param strip: Strip to calculate transformed image quad
 * \param apply_rotation: Apply strip rotation transform to the quad
 * \return array of 4 2D vectors
 */
Array<float2> image_transform_quad_get(const Scene *scene,
                                       const Strip *strip,
                                       bool apply_rotation);
/**
 * Get 4 corner points of strip image. Corner vectors are in viewport space.
 * Indices correspond to following corners (assuming no rotation):
 * 3--0
 * |  |
 * 2--1
 *
 * \param scene: Scene in which strips are located
 * \param strip: Strip to calculate transformed image quad
 * \return array of 4 2D vectors
 */
Array<float2> image_transform_final_quad_get(const Scene *scene, const Strip *strip);

float2 image_preview_unit_to_px(const Scene *scene, float2 co_src);
float2 image_preview_unit_from_px(const Scene *scene, float2 co_src);

/**
 * Get viewport axis aligned bounding box from a collection of sequences.
 * The collection must have one or more strips
 *
 * \param scene: Scene in which strips are located
 * \param strips: Collection of strips to get the bounding box from
 * \param apply_rotation: Include strip rotation transform in the bounding box calculation
 * \param r_min: Minimum x and y values
 * \param r_max: Maximum x and y values
 */
Bounds<float2> image_transform_bounding_box_from_collection(Scene *scene,
                                                            Span<Strip *> strips,
                                                            bool apply_rotation);

/**
 * Get strip image transformation matrix. Pivot point is set to correspond with viewport coordinate
 * system
 *
 * \param scene: Scene in which strips are located
 * \param strip: Strip that is used to construct the matrix
 */
float3x3 image_transform_matrix_get(const Scene *scene, const Strip *strip);

}  // namespace blender::seq
