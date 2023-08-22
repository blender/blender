/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"

#include "BKE_scene.h"

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
