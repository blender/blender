/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"

#include "BKE_scene.h"

struct ReportList;
struct bContext;
struct wmWindow;

Scene *ED_scene_add(Main *bmain, bContext *C, wmWindow *win, eSceneCopyMethod method)
    ATTR_NONNULL();
/**
 * Add a new scene in the sequence editor.
 *
 * Special mode for adding a scene assigned to sequencer strip.
 */
Scene *ED_scene_sequencer_add(Main *bmain,
                              bContext *C,
                              eSceneCopyMethod method,
                              bool assign_strip);
/**
 * \note Only call outside of area/region loops.
 * \return true if successful.
 */
bool ED_scene_delete(bContext *C, Main *bmain, Scene *scene) ATTR_NONNULL();
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
 * compatible with the result of #PIL_check_seconds_timer.
 */
void ED_scene_fps_average_accumulate(struct Scene *scene, short fps_samples, double ltime)
    ATTR_NONNULL(1);
/**
 * Calculate an average (if it's not already calculated).
 * \return false on failure otherwise all values in `state` are initialized.
 */
bool ED_scene_fps_average_calc(const struct Scene *scene, SceneFPS_State *state)
    ATTR_NONNULL(1, 2);
/**
 * Clear run-time data for accumulating animation playback average times.
 */
void ED_scene_fps_average_clear(Scene *scene) ATTR_NONNULL(1);

/** \} */
