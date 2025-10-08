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
  STRIP_HANDLE_NONE,
  STRIP_HANDLE_LEFT,
  STRIP_HANDLE_RIGHT,
};

struct StripSelection {
  /** Closest strip in the selection to the mouse cursor. */
  Strip *strip1 = nullptr;
  /** Farthest strip in the selection from the mouse cursor. */
  Strip *strip2 = nullptr;
  /** Handle of `strip1`. */
  eStripHandle handle = STRIP_HANDLE_NONE;
};

void select_strip_single(Scene *scene, Strip *strip, bool deselect_all);
/**
 * Iterates over a scene's strips and deselects all of them.
 *
 * \param scene: scene containing strips to be deselected.
 * \return true if any strips were deselected; false otherwise.
 */
bool deselect_all_strips(const Scene *scene);

bool maskedit_mask_poll(bContext *C);
bool check_show_maskedit(SpaceSeq *sseq, Scene *scene);
bool maskedit_poll(bContext *C);

/**
 * Are we displaying the seq output (not channels or histogram).
 */
bool check_show_imbuf(const SpaceSeq &sseq);

bool check_show_strip(const SpaceSeq &sseq);
/**
 * Check if there is animation shown during playback.
 *
 * - Colors of color strips are displayed on the strip itself.
 * - Backdrop is drawn.
 */
bool has_playback_animation(const Scene *scene);

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

bool is_scene_time_sync_needed(const bContext &C);
/**
 * Returns the scene strip (if any) that should be used for the scene synchronization feature.
 * This is the top-most visible scene strip at the current time of the \a sequencer_scene.
 */
const Strip *get_scene_strip_for_time_sync(const Scene *sequence_scene);
void sync_active_scene_and_time_with_scene_strip(bContext &C);

}  // namespace blender::ed::vse
