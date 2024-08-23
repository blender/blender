/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ListBase;
struct Main;
struct Scene;
struct Sequence;

bool SEQ_edit_sequence_swap(Scene *scene,
                            Sequence *seq_a,
                            Sequence *seq_b,
                            const char **r_error_str);
/**
 * Move sequence to seqbase.
 *
 * \param scene: Scene containing the editing
 * \param seqbase: seqbase where `seq` is located
 * \param seq: Sequence to move
 * \param dst_seqbase: Target seqbase
 */
bool SEQ_edit_move_strip_to_seqbase(Scene *scene,
                                    ListBase *seqbase,
                                    Sequence *seq,
                                    ListBase *dst_seqbase);
/**
 * Move sequence to meta sequence.
 *
 * \param scene: Scene containing the editing
 * \param src_seq: Sequence to move
 * \param dst_seqm: Target Meta sequence
 * \param r_error_str: Error message
 */
bool SEQ_edit_move_strip_to_meta(Scene *scene,
                                 Sequence *src_seq,
                                 Sequence *dst_seqm,
                                 const char **r_error_str);
/**
 * Flag seq and its users (effects) for removal.
 */
void SEQ_edit_flag_for_removal(Scene *scene, ListBase *seqbase, Sequence *seq);
/**
 * Remove all flagged sequences, return true if sequence is removed.
 */
void SEQ_edit_remove_flagged_sequences(Scene *scene, ListBase *seqbase);
void SEQ_edit_update_muting(Editing *ed);

enum eSeqSplitMethod {
  SEQ_SPLIT_SOFT,
  SEQ_SPLIT_HARD,
};

/**
 * Split Sequence at timeline_frame in two.
 *
 * \param bmain: Main in which Sequence is located
 * \param scene: Scene in which Sequence is located
 * \param seqbase: ListBase in which Sequence is located
 * \param seq: Sequence to be split
 * \param timeline_frame: frame at which seq is split.
 * \param method: affects type of offset to be applied to resize Sequence
 * \return The newly created sequence strip. This is always Sequence on right side.
 */
Sequence *SEQ_edit_strip_split(Main *bmain,
                               Scene *scene,
                               ListBase *seqbase,
                               Sequence *seq,
                               int timeline_frame,
                               eSeqSplitMethod method,
                               const char **r_error);
/**
 * Find gap after initial_frame and move strips on right side to close the gap
 *
 * \param scene: Scene in which strips are located
 * \param seqbase: ListBase in which strips are located
 * \param initial_frame: frame on timeline from where gaps are searched for
 * \param remove_all_gaps: remove all gaps instead of one gap
 * \return true if gap is removed, otherwise false
 */
bool SEQ_edit_remove_gaps(Scene *scene,
                          ListBase *seqbase,
                          int initial_frame,
                          bool remove_all_gaps);
void SEQ_edit_sequence_name_set(Scene *scene, Sequence *seq, const char *new_name);
