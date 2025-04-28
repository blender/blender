/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Contains dynamic drawing using immediate mode
 */

#include "DNA_screen_types.h"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "GPU_debug.hh"
#include "GPU_state.hh"

#include "UI_view2d.hh"

#include "WM_types.hh"

#include "BKE_paint.hh"
#include "BKE_screen.hh"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "draw_view_c.hh"

#include "view3d_intern.hh"

/* ******************** region info ***************** */

void DRW_draw_region_info(const bContext *C, ARegion *region)
{
  GPU_debug_group_begin("RegionInfo");
  view3d_draw_region_info(C, region);
  GPU_debug_group_end();
}

/* **************************** 3D Gizmo ******************************** */

void DRW_draw_gizmo_3d(const bContext *C, ARegion *region)
{
  /* draw depth culled gizmos - gizmos need to be updated *after* view matrix was set up */
  /* TODO: depth culling gizmos is not yet supported, just drawing _3D here, should
   * later become _IN_SCENE (and draw _3D separate) */
  WM_gizmomap_draw(region->runtime->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_3D);
}

void DRW_draw_gizmo_2d(const bContext *C, ARegion *region)
{
  WM_gizmomap_draw(region->runtime->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);
  GPU_depth_mask(true);
}
