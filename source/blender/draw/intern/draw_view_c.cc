/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Contains dynamic drawing using immediate mode
 */

#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "GPU_debug.hh"
#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_types.hh"

#include "BLI_math_rotation.h"

#include "BKE_global.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_screen.hh"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "draw_cache.hh"
#include "draw_view_c.hh"

#include "view3d_intern.hh"

/* ******************** region info ***************** */

void DRW_draw_region_info()
{
  GPU_debug_group_begin("RegionInfo");
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  view3d_draw_region_info(draw_ctx->evil_C, region);
  GPU_debug_group_end();
}

/* **************************** 3D Gizmo ******************************** */

void DRW_draw_gizmo_3d()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  /* draw depth culled gizmos - gizmos need to be updated *after* view matrix was set up */
  /* TODO: depth culling gizmos is not yet supported, just drawing _3D here, should
   * later become _IN_SCENE (and draw _3D separate) */
  WM_gizmomap_draw(region->runtime->gizmo_map, draw_ctx->evil_C, WM_GIZMOMAP_DRAWSTEP_3D);
}

void DRW_draw_gizmo_2d()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  WM_gizmomap_draw(region->runtime->gizmo_map, draw_ctx->evil_C, WM_GIZMOMAP_DRAWSTEP_2D);

  GPU_depth_mask(true);
}
