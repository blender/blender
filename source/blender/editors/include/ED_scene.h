/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_compiler_attrs.h"

#include "BKE_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Scene *ED_scene_add(struct Main *bmain,
                           struct bContext *C,
                           struct wmWindow *win,
                           eSceneCopyMethod method) ATTR_NONNULL();
/**
 * Add a new scene in the sequence editor.
 *
 * Special mode for adding a scene assigned to sequencer strip.
 */
struct Scene *ED_scene_sequencer_add(struct Main *bmain,
                                     struct bContext *C,
                                     eSceneCopyMethod method,
                                     bool assign_strip);
/**
 * \note Only call outside of area/region loops.
 * \return true if successful.
 */
bool ED_scene_delete(struct bContext *C, struct Main *bmain, struct Scene *scene) ATTR_NONNULL();
/**
 * Depsgraph updates after scene becomes active in a window.
 */
void ED_scene_change_update(struct Main *bmain, struct Scene *scene, struct ViewLayer *layer)
    ATTR_NONNULL();
bool ED_scene_view_layer_delete(struct Main *bmain,
                                struct Scene *scene,
                                struct ViewLayer *layer,
                                struct ReportList *reports) ATTR_NONNULL(1, 2, 3);

void ED_operatortypes_scene(void);

#ifdef __cplusplus
}
#endif
