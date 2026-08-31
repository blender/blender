/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Common utilities for mesh painting & sculpting
 */

#pragma once

#include "BKE_paint_bvh.hh"

#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"
#include "BLI_math_vector_types.hh"

#include "DEG_depsgraph.hh"

#include "DNA_object_enums.h"

namespace blender {
struct ViewContext;
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

/**
 * Initialize common `StrokeCache` values that do not change over the course of the stroke
 */
void stroke_cache_common_init(
    ViewContext &vc, const Paint &paint, const Brush &brush, Object &object, float2 mval);

/**
 * Query the BVH based on the current brush being used.
 *
 * TODO: Merge this implementation with #pbvh_gather_generic
 */
IndexMask gather_brush_nodes(const Object &ob,
                             const Brush &brush,
                             IndexMaskMemory &memory,
                             FunctionRef<bool(const bke::pbvh::Node &)> node_ignore_fn);

}  // namespace blender::ed::sculpt_paint
