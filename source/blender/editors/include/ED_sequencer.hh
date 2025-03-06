/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_vector_set.hh"

struct Scene;
struct Strip;
struct SpaceSeq;
struct bContext;
struct View2D;

namespace blender::ed::vse {

enum eStripHandle {
  SEQ_HANDLE_NONE,
  SEQ_HANDLE_LEFT,
  SEQ_HANDLE_RIGHT,
  SEQ_HANDLE_BOTH,
};

struct StripSelection {
  Strip *seq1 = nullptr;
  Strip *seq2 = nullptr;
  eStripHandle handle = SEQ_HANDLE_NONE;
};

void select_sequence_single(Scene *scene, Strip *strip, bool deselect_all);
/**
 * Iterates over a scene's sequences and deselects all of them.
 *
 * \param scene: scene containing sequences to be deselected.
 * \return true if any sequences were deselected; false otherwise.
 */
bool deselect_all_strips(Scene *scene);

bool maskedit_mask_poll(bContext *C);
bool check_show_maskedit(SpaceSeq *sseq, Scene *scene);
bool maskedit_poll(bContext *C);

/**
 * Are we displaying the seq output (not channels or histogram).
 */
bool check_show_imbuf(SpaceSeq *sseq);

bool check_show_strip(SpaceSeq *sseq);
/**
 * Check if there is animation shown during playback.
 *
 * - Colors of color strips are displayed on the strip itself.
 * - Backdrop is drawn.
 */
bool has_playback_animation(const SpaceSeq *sseq, const Scene *scene);

void ED_operatormacros_sequencer();

Strip *special_preview_get();
void special_preview_set(bContext *C, const int mval[2]);
void special_preview_clear();
bool sequencer_retiming_mode_is_active(const bContext *C);
/**
 * Returns collection with selected strips presented to user. If operation is done in preview,
 * collection is limited to selected presented strips, that can produce image output at current
 * frame.
 *
 * \param C: context
 * \return collection of strips (`Strip`)
 */
blender::VectorSet<Strip *> selected_strips_from_context(bContext *C);
StripSelection pick_strip_and_handle(const struct Scene *scene,
                                     const View2D *v2d,
                                     float mouse_co[2]);
bool can_select_handle(const Scene *scene, const Strip *strip, const View2D *v2d);
bool handle_is_selected(const Strip *strip, eStripHandle handle);

}  // namespace blender::ed::vse
