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
struct Sequence;

int SEQ_transform_get_left_handle_frame(struct Sequence *seq, bool metaclip);
int SEQ_transform_get_right_handle_frame(struct Sequence *seq, bool metaclip);
void SEQ_transform_set_left_handle_frame(struct Sequence *seq, int val);
void SEQ_transform_set_right_handle_frame(struct Sequence *seq, int val);
void SEQ_transform_handle_xlimits(struct Sequence *seq, int leftflag, int rightflag);
bool SEQ_transform_sequence_can_be_translated(struct Sequence *seq);
bool SEQ_transform_single_image_check(struct Sequence *seq);
void SEQ_transform_fix_single_image_seq_offsets(struct Sequence *seq);
bool SEQ_transform_test_overlap(struct ListBase *seqbasep, struct Sequence *test);
void SEQ_transform_translate_sequence(struct Scene *scene, struct Sequence *seq, int delta);
bool SEQ_transform_seqbase_shuffle_ex(struct ListBase *seqbasep,
                                      struct Sequence *test,
                                      struct Scene *evil_scene,
                                      int channel_delta);
bool SEQ_transform_seqbase_shuffle(struct ListBase *seqbasep,
                                   struct Sequence *test,
                                   struct Scene *evil_scene);
bool SEQ_transform_seqbase_shuffle_time(struct ListBase *seqbasep,
                                        struct Scene *evil_scene,
                                        struct ListBase *markers,
                                        const bool use_sync_markers);
bool SEQ_transform_seqbase_isolated_sel_check(struct ListBase *seqbase);
void SEQ_transform_offset_after_frame(struct Scene *scene,
                                      struct ListBase *seqbase,
                                      const int delta,
                                      const int timeline_frame);

#ifdef __cplusplus
}
#endif
