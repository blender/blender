/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Common utilities for mesh painting & sculpting
 */

#pragma once

#include "BLI_function_ref.hh"

#include "DEG_depsgraph.hh"

#include "DNA_object_enums.h"

namespace blender {
struct Paint;
namespace ed::sculpt_paint {
struct StrokeCache;
class PaintModeData;
}  // namespace ed::sculpt_paint
struct Brush;
struct Depsgraph;
struct Object;
struct PaintModeSettings;
struct Scene;
struct Sculpt;
}  // namespace blender

namespace blender::ed::sculpt_paint {

void mode_enter_generic(
    Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob, eObjectMode mode_flag);
void mode_exit_generic(Object &ob, eObjectMode mode_flag);

/** Main brush action callback */
using BrushActionFn = FunctionRef<void(const Depsgraph &depsgraph,
                                       const Scene &scene,
                                       const Brush &brush,
                                       Object &ob,
                                       PaintModeData *mode_data)>;

void do_symmetrical_brush_actions(const Depsgraph &depsgraph,
                                  const Scene &scene,
                                  const Paint &paint,
                                  Object &object,
                                  BrushActionFn action_fn,
                                  PaintModeData *mode_data);

/**
 * TODO: The naming is awkward, but because both features should be eventually supported in mor
 * than just sculpt mode, this is a fine compromise for now
 */
void do_symmetrical_brush_actions_with_tiling_and_feathering(const Depsgraph &depsgraph,
                                                             const Scene &scene,
                                                             const Paint &paint,
                                                             Object &object,
                                                             BrushActionFn action_fn,
                                                             PaintModeData *paint_mode_data);

}  // namespace blender::ed::sculpt_paint
