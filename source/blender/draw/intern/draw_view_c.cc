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

/* -------------------------------------------------------------------- */
/** \name 2D Cursor
 * \{ */

static bool is_cursor_visible_2d(const DRWContextState *draw_ctx)
{
  SpaceInfo *space_data = (SpaceInfo *)draw_ctx->space_data;
  if (space_data == nullptr) {
    return false;
  }
  if (space_data->spacetype != SPACE_IMAGE) {
    return false;
  }
  SpaceImage *sima = (SpaceImage *)space_data;
  switch (sima->mode) {
    case SI_MODE_VIEW:
      return false;
      break;
    case SI_MODE_PAINT:
      return false;
      break;
    case SI_MODE_MASK:
      break;
    case SI_MODE_UV:
      break;
  }
  return (sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS) != 0;
}

/* -------------------------------------------------------------------- */
/** \name Generic Cursor
 * \{ */

void DRW_draw_cursor_2d_ex(const ARegion *region, const float cursor[2])
{
  int co[2];
  UI_view2d_view_to_region(&region->v2d, cursor[0], cursor[1], &co[0], &co[1]);

  /* Draw nice Anti Aliased cursor. */
  GPU_line_width(1.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);

  /* Draw lines */
  float original_proj[4][4];
  GPU_matrix_projection_get(original_proj);
  GPU_matrix_push();
  ED_region_pixelspace(region);
  GPU_matrix_translate_2f(co[0] + 0.5f, co[1] + 0.5f);
  GPU_matrix_scale_2f(U.widget_unit, U.widget_unit);

  blender::gpu::Batch *cursor_batch = DRW_cache_cursor_get(true);

  GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_3D_FLAT_COLOR);
  GPU_batch_set_shader(cursor_batch, shader);

  GPU_batch_draw(cursor_batch);

  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
  GPU_matrix_pop();
  GPU_matrix_projection_set(original_proj);
}

/** \} */

void DRW_draw_cursor_2d()
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ARegion *region = draw_ctx->region;

  GPU_color_mask(true, true, true, true);
  GPU_depth_mask(false);
  GPU_depth_test(GPU_DEPTH_NONE);

  if (is_cursor_visible_2d(draw_ctx)) {
    const SpaceImage *sima = (SpaceImage *)draw_ctx->space_data;
    DRW_draw_cursor_2d_ex(region, sima->cursor);
  }
}

/** \} */

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
