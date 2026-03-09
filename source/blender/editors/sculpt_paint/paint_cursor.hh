/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_paint.hh"
#include "BKE_paint_types.hh"

#include "ED_view3d.hh"

namespace blender::ed::sculpt_paint {
enum class PaintCursorDrawingType {
  Curve,
  Cursor2D,
  Cursor3D,
};

struct PaintCursorContext {
  ARegion *region = nullptr;
  wmWindow *win = nullptr;
  wmWindowManager *wm = nullptr;
  bScreen *screen = nullptr;
  Depsgraph *depsgraph = nullptr;
  Scene *scene = nullptr;
  Object *object = nullptr;
  Base *base = nullptr;
  UnifiedPaintSettings *ups = nullptr;
  Brush *brush = nullptr;
  Paint *paint = nullptr;
  PaintMode mode = PaintMode::Invalid;
  ViewContext vc = {};

  /* Sculpt related data. */
  Sculpt *sd = nullptr;
  SculptSession *ss = nullptr;

  /**
   * Previous active vertex index , used to determine if the preview is updated for the pose brush.
   */
  int prev_active_vert_index = -1;

  /** Whether the current tool is a brush. We can have a `Brush *` without this being true */
  bool is_brush_active = false;
  bool is_stroke_active = false;
  bool is_cursor_over_mesh = false;
  float radius = 0.0f;

  /* 3D view cursor position and normal. */
  float3 location = float3(0.0f);
  float3 scene_space_location = float3(0.0f);
  float3 normal = float3(0.0f);

  /* Cursor main colors. */
  float3 outline_col = float3(0.0f);
  float outline_alpha = 0.0f;

  /* GPU attribute for drawing. */
  uint pos = 0;

  PaintCursorDrawingType cursor_type = PaintCursorDrawingType::Cursor2D;

  /* This variable is set after drawing the overlay, not on initialization. It can't be used for
   * checking if alpha overlay is enabled before drawing it. */
  bool alpha_overlay_drawn = false;

  float zoomx = 0.0f;
  /* Coordinates in region space */
  int2 mval = int2(0);

  /* TODO: Figure out why this and mval are used interchangeably */
  float2 translation = float2(0.0f);

  float2 tilt = float2(0.0f);

  float final_radius = 0.0f;
  int pixel_radius = 0;
};

void grease_pencil_cursor_draw(PaintCursorContext &pcontext);

void mesh_cursor_update_and_init(PaintCursorContext &pcontext);
void mesh_cursor_active_draw(PaintCursorContext &pcontext);
void mesh_cursor_inactive_draw(PaintCursorContext &pcontext);
}  // namespace blender::ed::sculpt_paint
