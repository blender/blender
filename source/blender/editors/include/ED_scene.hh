/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"

#include "BKE_scene.hh"

struct ReportList;
struct bContext;
struct wmWindow;
struct Strip;

Scene *ED_scene_add(Main *bmain, bContext *C, wmWindow *win, eSceneCopyMethod method)
    ATTR_NONNULL();
/**
 * Add a new scene from the sequence editor.
 */
Scene *ED_scene_sequencer_add(Main *bmain, bContext *C, eSceneCopyMethod method);
/**
 * \note Only call outside of area/region loops.
 * \return true if successful.
 */
bool ED_scene_delete(bContext *C, Main *bmain, Scene *scene) ATTR_NONNULL();
/**
 * Replace the given scene (assumed to be an active scene) by another suitable one.
 *
 * Checks if the given active scene can actually be deleted.
 *
 * Also ensures that all needed updates in WM and UI code is done.
 *
 * \param scene: The scene to be replaced, often the active scene but may be any scene.
 * \param scene_new: When non-null, this scene is used as a replacement for `scene`.
 *                   Otherwise, #BKE_scene_find_replacement() is used to find a replacement.
 * \return true if the given `scene` was successfully replaced and can safely be deleted.
 */
bool ED_scene_replace_active_for_deletion(bContext &C,
                                          Main &bmain,
                                          Scene &scene,
                                          Scene *scene_new = nullptr);
/**
 * Depsgraph updates after scene becomes active in a window.
 */
void ED_scene_change_update(Main *bmain, Scene *scene, ViewLayer *layer) ATTR_NONNULL();
bool ED_scene_view_layer_delete(Main *bmain, Scene *scene, ViewLayer *layer, ReportList *reports)
    ATTR_NONNULL(1, 2, 3);

void ED_operatortypes_scene();

/* -------------------------------------------------------------------- */
/** \name Scene FPS Management
 * \{ */

struct SceneFPS_State {
  float fps_average;
  float fps_target;
  bool fps_target_is_fractional;
};

/**
 * Update frame rate info for viewport drawing.
 * \param ltime: Time since the last update,
 * compatible with the result of #BLI_time_now_seconds.
 */
void ED_scene_fps_average_accumulate(Scene *scene, short fps_samples, double ltime)
    ATTR_NONNULL(1);
/**
 * Calculate an average (if it's not already calculated).
 * \return false on failure otherwise all values in `state` are initialized.
 */
bool ED_scene_fps_average_calc(const Scene *scene, SceneFPS_State *r_state) ATTR_NONNULL(1, 2);
/**
 * Clear run-time data for accumulating animation playback average times.
 */
void ED_scene_fps_average_clear(Scene *scene) ATTR_NONNULL(1);

/** \} */
