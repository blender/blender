/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ListBase;
struct Scene;
struct Sequence;

bool SEQ_transform_sequence_can_be_translated(Sequence *seq);
/**
 * Used so we can do a quick check for single image seq
 * since they work a bit differently to normal image seq's (during transform).
 */
bool SEQ_transform_single_image_check(Sequence *seq);
bool SEQ_transform_test_overlap(const Scene *scene, ListBase *seqbasep, Sequence *test);
bool SEQ_transform_test_overlap_seq_seq(const Scene *scene, Sequence *seq1, Sequence *seq2);
void SEQ_transform_translate_sequence(Scene *evil_scene, Sequence *seq, int delta);
/**
 * \return 0 if there weren't enough space.
 */
bool SEQ_transform_seqbase_shuffle_ex(ListBase *seqbasep,
                                      Sequence *test,
                                      Scene *evil_scene,
                                      int channel_delta);
bool SEQ_transform_seqbase_shuffle(ListBase *seqbasep, Sequence *test, Scene *evil_scene);
bool SEQ_transform_seqbase_shuffle_time(blender::Span<Sequence *> strips_to_shuffle,
                                        blender::Span<Sequence *> time_dependent_strips,
                                        ListBase *seqbasep,
                                        Scene *evil_scene,
                                        ListBase *markers,
                                        bool use_sync_markers);
bool SEQ_transform_seqbase_shuffle_time(blender::Span<Sequence *> strips_to_shuffle,
                                        ListBase *seqbasep,
                                        Scene *evil_scene,
                                        ListBase *markers,
                                        bool use_sync_markers);

void SEQ_transform_handle_overlap(Scene *scene,
                                  ListBase *seqbasep,
                                  blender::Span<Sequence *> transformed_strips,
                                  blender::Span<Sequence *> time_dependent_strips,
                                  bool use_sync_markers);
void SEQ_transform_handle_overlap(Scene *scene,
                                  ListBase *seqbasep,
                                  blender::Span<Sequence *> transformed_strips,
                                  bool use_sync_markers);
/**
 * Check if the selected seq's reference unselected seq's.
 */
bool SEQ_transform_seqbase_isolated_sel_check(ListBase *seqbase);
/**
 * Move strips and markers (if not locked) that start after timeline_frame by delta frames
 *
 * \param scene: Scene in which strips are located
 * \param seqbase: ListBase in which strips are located
 * \param delta: offset in frames to be applied
 * \param timeline_frame: frame on timeline from where strips are moved
 */
void SEQ_transform_offset_after_frame(Scene *scene,
                                      ListBase *seqbase,
                                      int delta,
                                      int timeline_frame);

/**
 * Check if `seq` can be moved.
 * This function also checks `SeqTimelineChannel` flag.
 */
bool SEQ_transform_is_locked(ListBase *channels, const Sequence *seq);

/* Image transformation. */

void SEQ_image_transform_mirror_factor_get(const Sequence *seq, float r_mirror[2]);
/**
 * Get strip transform origin offset from image center
 * NOTE: This function does not apply axis mirror.
 *
 * \param scene: Scene in which strips are located
 * \param seq: Sequence to calculate image transform origin
 * \param r_origin: return value
 */
void SEQ_image_transform_origin_offset_pixelspace_get(const Scene *scene,
                                                      const Sequence *seq,
                                                      float r_origin[2]);
/**
 * Get 4 corner points of strip image, optionally without rotation component applied.
 * Corner vectors are in viewport space.
 *
 * \param scene: Scene in which strips are located
 * \param seq: Sequence to calculate transformed image quad
 * \param apply_rotation: Apply sequence rotation transform to the quad
 * \param r_quad: array of 4 2D vectors
 */
void SEQ_image_transform_quad_get(const Scene *scene,
                                  const Sequence *seq,
                                  bool apply_rotation,
                                  float r_quad[4][2]);
/**
 * Get 4 corner points of strip image. Corner vectors are in viewport space.
 *
 * \param scene: Scene in which strips are located
 * \param seq: Sequence to calculate transformed image quad
 * \param r_quad: array of 4 2D vectors
 */
void SEQ_image_transform_final_quad_get(const Scene *scene,
                                        const Sequence *seq,
                                        float r_quad[4][2]);

void SEQ_image_preview_unit_to_px(const Scene *scene, const float co_src[2], float co_dst[2]);
void SEQ_image_preview_unit_from_px(const Scene *scene, const float co_src[2], float co_dst[2]);

/**
 * Get viewport axis aligned bounding box from a collection of sequences.
 * The collection must have one or more strips
 *
 * \param scene: Scene in which strips are located
 * \param strips: Collection of strips to get the bounding box from
 * \param apply_rotation: Include sequence rotation transform in the bounding box calculation
 * \param r_min: Minimum x and y values
 * \param r_max: Maximum x and y values
 */
void SEQ_image_transform_bounding_box_from_collection(Scene *scene,
                                                      blender::Span<Sequence *> strips,
                                                      bool apply_rotation,
                                                      float r_min[2],
                                                      float r_max[2]);
