/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_vector_set.hh"

struct Scene;
struct Sequence;
struct SpaceSeq;
struct bContext;

void ED_sequencer_select_sequence_single(Scene *scene, Sequence *seq, bool deselect_all);
/**
 * Iterates over a scene's sequences and deselects all of them.
 *
 * \param scene: scene containing sequences to be deselected.
 * \return true if any sequences were deselected; false otherwise.
 */
bool ED_sequencer_deselect_all(Scene *scene);

bool ED_space_sequencer_maskedit_mask_poll(bContext *C);
bool ED_space_sequencer_check_show_maskedit(SpaceSeq *sseq, Scene *scene);
bool ED_space_sequencer_maskedit_poll(bContext *C);

/**
 * Are we displaying the seq output (not channels or histogram).
 */
bool ED_space_sequencer_check_show_imbuf(SpaceSeq *sseq);

bool ED_space_sequencer_check_show_strip(SpaceSeq *sseq);
/**
 * Check if there is animation shown during playback.
 *
 * - Colors of color strips are displayed on the strip itself.
 * - Backdrop is drawn.
 */
bool ED_space_sequencer_has_playback_animation(const SpaceSeq *sseq, const Scene *scene);

void ED_operatormacros_sequencer();

Sequence *ED_sequencer_special_preview_get();
void ED_sequencer_special_preview_set(bContext *C, const int mval[2]);
void ED_sequencer_special_preview_clear();
bool sequencer_retiming_mode_is_active(const bContext *C);

/**
 * Returns collection with selected strips presented to user. If operation is done in preview,
 * collection is limited to selected presented strips, that can produce image output at current
 * frame.
 *
 * \param C: context
 * \return collection of strips (`Sequence`)
 */
blender::VectorSet<Sequence *> ED_sequencer_selected_strips_from_context(bContext *C);
bool ED_sequencer_can_select_handle(const Sequence *seq);
