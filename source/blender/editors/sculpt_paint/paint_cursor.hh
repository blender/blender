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
  ARegion *region;
  wmWindow *win;
  wmWindowManager *wm;
  bScreen *screen;
  Depsgraph *depsgraph;
  Scene *scene;
  Object *object;
  Base *base;
  UnifiedPaintSettings *ups;
  Brush *brush;
  Paint *paint;
  PaintMode mode;
  ViewContext vc;

  /* Sculpt related data. */
  Sculpt *sd;
  SculptSession *ss;

  /**
   * Previous active vertex index , used to determine if the preview is updated for the pose brush.
   */
  int prev_active_vert_index;

  /** Whether the current tool is a brush. We can have a `Brush *` without this being true */
  bool is_brush_active;
  bool is_stroke_active;
  bool is_cursor_over_mesh;
  float radius;

  /* 3D view cursor position and normal. */
  float3 location;
  float3 scene_space_location;
  float3 normal;

  /* Cursor main colors. */
  float3 outline_col;
  float outline_alpha;

  /* GPU attribute for drawing. */
  uint pos;

  PaintCursorDrawingType cursor_type;

  /* This variable is set after drawing the overlay, not on initialization. It can't be used for
   * checking if alpha overlay is enabled before drawing it. */
  bool alpha_overlay_drawn;

  float zoomx;
  /* Coordinates in region space */
  int2 mval;

  /* TODO: Figure out why this and mval are used interchangeably */
  float2 translation;

  float2 tilt;

  float final_radius;
  int pixel_radius;
};

void grease_pencil_cursor_draw(PaintCursorContext &pcontext);

void mesh_cursor_update_and_init(PaintCursorContext &pcontext);
void mesh_cursor_active_draw(PaintCursorContext &pcontext);
void mesh_cursor_inactive_draw(PaintCursorContext &pcontext);
}  // namespace blender::ed::sculpt_paint
