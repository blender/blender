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
struct Main;
struct Scene;
struct Sequence;

int SEQ_edit_sequence_swap(struct Sequence *seq_a, struct Sequence *seq_b, const char **error_str);
bool SEQ_edit_move_strip_to_meta(struct Scene *scene,
                                 struct Sequence *src_seq,
                                 struct Sequence *dst_seqm,
                                 const char **error_str);
void SEQ_edit_flag_for_removal(struct Scene *scene,
                               struct ListBase *seqbase,
                               struct Sequence *seq);
void SEQ_edit_remove_flagged_sequences(struct Scene *scene, struct ListBase *seqbase);
void SEQ_edit_update_muting(struct Editing *ed);

typedef enum eSeqSplitMethod {
  SEQ_SPLIT_SOFT,
  SEQ_SPLIT_HARD,
} eSeqSplitMethod;

struct Sequence *SEQ_edit_strip_split(struct Main *bmain,
                                      struct Scene *scene,
                                      struct ListBase *seqbase,
                                      struct Sequence *seq,
                                      const int timeline_frame,
                                      const eSeqSplitMethod method);
bool SEQ_edit_remove_gaps(struct Scene *scene,
                          struct ListBase *seqbase,
                          const int initial_frame,
                          const bool remove_all_gaps);
#ifdef __cplusplus
}
#endif
