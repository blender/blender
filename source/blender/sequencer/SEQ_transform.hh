/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "BLI_array.hh"
#include "BLI_bounds_types.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

namespace blender {

enum eSeqOverlapMode : int;
enum eSeqRippleFlag : int;

struct Scene;
struct Strip;
struct SeqTimelineChannel;

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
void transform_shuffle_vertical(ListBaseT<Strip> *seqbasep,
                                Span<Strip *> strips,
                                Scene *scene,
                                int channel_delta = 1);

void transform_handle_overlap(Scene *scene,
                              ListBaseT<Strip> *seqbasep,
                              Span<Strip *> source_strips,
                              bool use_sync_markers,
                              Span<Strip *> time_dependent_strips = {});
/**
 * As above, but resolve the overlap with an overridden \a overlap_mode and \a ripple_flag instead
 * of the sequencer scene's tool settings.
 */
void transform_handle_overlap(Scene *scene,
                              ListBaseT<Strip> *seqbasep,
                              Span<Strip *> source_strips,
                              bool use_sync_markers,
                              eSeqOverlapMode overlap_mode,
                              eSeqRippleFlag ripple_flag,
                              Span<Strip *> time_dependent_strips = {});
/**
 * Move strips and markers (if not locked) that start after \a timeline_frame by \a delta frames.
 */
void transform_strips_after_frame(Scene *scene,
                                  ListBaseT<Strip> *seqbase,
                                  int timeline_frame,
                                  int delta);

/**
 * Check if `strip` can be moved.
 * This function also checks `SeqTimelineChannel` flag.
 */
bool transform_is_locked(const ListBaseT<SeqTimelineChannel> *channels, const Strip *strip);

/**
 *  Returns the extents of the strip along channels and frames.
 */
rcti strip_int_bounds_get(const Scene *scene, const Strip *strip);

struct RippleRange {
  int start;
  int end;
  Set<int> channels;
};

/**
 * Break up \a strips into "ranges" (rather than calculate an entire bounding box from all the
 * strips under consideration). This lets us simultaneously ripple separated parts of the timeline
 * if the user edits strips that are far apart. Returned ranges are sorted by start frame.
 */
Vector<RippleRange> transform_ripple_ranges_get(const Scene *scene, Span<Strip *> strips);
/**
 * Whether \a strip should be rippled by some transformation on a channel in \a range.
 */
bool transform_strip_is_on_rippled_channel(const RippleRange &range,
                                           const Strip *strip,
                                           bool all_channels);

/* Image transformation. */

/**
 * Get per-axis mirror factors for a \a strip image.
 * \return float2 where each component is 1.0f (normal) or -1.0f (mirrored).
 */
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
 * \return int2 with (width, height) in view-space pixels
 */
int2 image_transform_box_size_get(const Scene *scene, const Strip *strip);

/**
 * Get 4 corner points of strip image. Corner vectors are in viewport space.
 * Indices correspond to following corners (assuming no rotation):
 * 3--0
 * |  |
 * 2--1
 */
Array<float2> image_transform_quad_get(const Scene *scene, const Strip *strip);

float2 image_preview_unit_to_px(const Scene *scene, float2 co_src);
float2 image_preview_unit_from_px(const Scene *scene, float2 co_src);

/**
 * Get viewport axis-aligned bounding box from multiple strips.
 */
Bounds<float2> image_transform_bounding_box_from_strips_get(Scene *scene, Span<Strip *> strips);

}  // namespace seq
}  // namespace blender
