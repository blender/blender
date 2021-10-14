/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct Scene;
struct SeqCollection;
struct Sequence;

int SEQ_transform_get_left_handle_frame(struct Sequence *seq);
int SEQ_transform_get_right_handle_frame(struct Sequence *seq);
void SEQ_transform_set_left_handle_frame(struct Sequence *seq, int val);
void SEQ_transform_set_right_handle_frame(struct Sequence *seq, int val);
void SEQ_transform_handle_xlimits(struct Sequence *seq, int leftflag, int rightflag);
bool SEQ_transform_sequence_can_be_translated(struct Sequence *seq);
bool SEQ_transform_single_image_check(struct Sequence *seq);
void SEQ_transform_fix_single_image_seq_offsets(struct Sequence *seq);
bool SEQ_transform_test_overlap(struct ListBase *seqbasep, struct Sequence *test);
bool SEQ_transform_test_overlap_seq_seq(struct Sequence *seq1, struct Sequence *seq2);
void SEQ_transform_translate_sequence(struct Scene *scene, struct Sequence *seq, int delta);
bool SEQ_transform_seqbase_shuffle_ex(struct ListBase *seqbasep,
                                      struct Sequence *test,
                                      struct Scene *evil_scene,
                                      int channel_delta);
bool SEQ_transform_seqbase_shuffle(struct ListBase *seqbasep,
                                   struct Sequence *test,
                                   struct Scene *evil_scene);
bool SEQ_transform_seqbase_shuffle_time(struct SeqCollection *strips_to_shuffle,
                                        struct ListBase *seqbasep,
                                        struct Scene *evil_scene,
                                        struct ListBase *markers,
                                        const bool use_sync_markers);
bool SEQ_transform_seqbase_isolated_sel_check(struct ListBase *seqbase);
void SEQ_transform_offset_after_frame(struct Scene *scene,
                                      struct ListBase *seqbase,
                                      const int delta,
                                      const int timeline_frame);

/* Image transformation. */
void SEQ_image_transform_mirror_factor_get(const struct Sequence *seq, float r_mirror[2]);
void SEQ_image_transform_origin_offset_pixelspace_get(const struct Scene *scene,
                                                      const struct Sequence *seq,
                                                      float r_origin[2]);
void SEQ_image_transform_quad_get(const struct Scene *scene,
                                  const struct Sequence *seq,
                                  bool apply_rotation,
                                  float r_quad[4][2]);
void SEQ_image_transform_final_quad_get(const struct Scene *scene,
                                        const struct Sequence *seq,
                                        float r_quad[4][2]);

void SEQ_image_preview_unit_to_px(const struct Scene *scene,
                                  const float co_src[2],
                                  float co_dst[2]);
void SEQ_image_preview_unit_from_px(const struct Scene *scene,
                                    const float co_src[2],
                                    float co_dst[2]);

#ifdef __cplusplus
}
#endif
