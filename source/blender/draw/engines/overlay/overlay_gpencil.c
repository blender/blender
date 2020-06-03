/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2020, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BKE_gpencil.h"

#include "UI_resources.h"

#include "DNA_gpencil_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_view3d.h"

#include "overlay_private.h"

#include "draw_common.h"
#include "draw_manager_text.h"

void OVERLAY_edit_gpencil_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  struct GPUShader *sh;
  DRWShadingGroup *grp;

  /* Default: Display nothing. */
  pd->edit_gpencil_points_grp = NULL;
  pd->edit_gpencil_wires_grp = NULL;
  psl->edit_gpencil_ps = NULL;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  Object *ob = draw_ctx->obact;
  bGPdata *gpd = ob ? (bGPdata *)ob->data : NULL;
  Scene *scene = draw_ctx->scene;
  ToolSettings *ts = scene->toolsettings;

  if (gpd == NULL || ob->type != OB_GPENCIL) {
    return;
  }

  /* For sculpt show only if mask mode, and only points if not stroke mode. */
  const bool use_sculpt_mask = (GPENCIL_SCULPT_MODE(gpd) &&
                                GPENCIL_ANY_SCULPT_MASK(ts->gpencil_selectmode_sculpt));
  const bool show_sculpt_points = (GPENCIL_SCULPT_MODE(gpd) &&
                                   (ts->gpencil_selectmode_sculpt &
                                    (GP_SCULPT_MASK_SELECTMODE_POINT |
                                     GP_SCULPT_MASK_SELECTMODE_SEGMENT)));

  /* For vertex paint show only if mask mode, and only points if not stroke mode. */
  bool use_vertex_mask = (GPENCIL_VERTEX_MODE(gpd) &&
                          GPENCIL_ANY_VERTEX_MASK(ts->gpencil_selectmode_vertex));
  const bool show_vertex_points = (GPENCIL_VERTEX_MODE(gpd) &&
                                   (ts->gpencil_selectmode_vertex &
                                    (GP_VERTEX_MASK_SELECTMODE_POINT |
                                     GP_VERTEX_MASK_SELECTMODE_SEGMENT)));

  /* If Sculpt or Vertex mode and the mask is disabled, the select must be hidden. */
  const bool hide_select = ((GPENCIL_SCULPT_MODE(gpd) && !use_sculpt_mask) ||
                            (GPENCIL_VERTEX_MODE(gpd) && !use_vertex_mask));

  const bool do_multiedit = GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const bool show_multi_edit_lines = (v3d->gp_flag & V3D_GP_SHOW_MULTIEDIT_LINES) != 0;

  const bool show_lines = (v3d->gp_flag & V3D_GP_SHOW_EDIT_LINES) || show_multi_edit_lines;

  const bool hide_lines = !GPENCIL_EDIT_MODE(gpd) && !GPENCIL_WEIGHT_MODE(gpd) &&
                          !use_sculpt_mask && !use_vertex_mask && !show_lines;

  /* Special case when vertex paint and multiedit lines. */
  if (do_multiedit && show_multi_edit_lines && GPENCIL_VERTEX_MODE(gpd)) {
    use_vertex_mask = true;
  }

  const bool is_weight_paint = (gpd) && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE);

  /* Show Edit points if:
   *  Edit mode: Not in Stroke selection mode
   *  Sculpt mode: If use Mask and not Stroke mode
   *  Weight mode: Always
   *  Vertex mode: If use Mask and not Stroke mode
   */
  const bool show_points = show_sculpt_points || is_weight_paint || show_vertex_points ||
                           (GPENCIL_EDIT_MODE(gpd) &&
                            (ts->gpencil_selectmode_edit != GP_SELECTMODE_STROKE));

  if ((!GPENCIL_VERTEX_MODE(gpd) && !GPENCIL_PAINT_MODE(gpd)) || use_vertex_mask) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->edit_gpencil_ps, state | pd->clipping_state);

    if (show_lines && !hide_lines) {
      sh = OVERLAY_shader_edit_gpencil_wire();
      pd->edit_gpencil_wires_grp = grp = DRW_shgroup_create(sh, psl->edit_gpencil_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_bool_copy(grp, "doMultiframe", show_multi_edit_lines);
      DRW_shgroup_uniform_bool_copy(grp, "doWeightColor", is_weight_paint);
      DRW_shgroup_uniform_bool_copy(grp, "hideSelect", hide_select);
      DRW_shgroup_uniform_float_copy(grp, "gpEditOpacity", v3d->vertex_opacity);
      DRW_shgroup_uniform_texture(grp, "weightTex", G_draw.weight_ramp);
    }

    if (show_points && !hide_select) {
      sh = OVERLAY_shader_edit_gpencil_point();
      pd->edit_gpencil_points_grp = grp = DRW_shgroup_create(sh, psl->edit_gpencil_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_bool_copy(grp, "doMultiframe", do_multiedit);
      DRW_shgroup_uniform_bool_copy(grp, "doWeightColor", is_weight_paint);
      DRW_shgroup_uniform_float_copy(grp, "gpEditOpacity", v3d->vertex_opacity);
      DRW_shgroup_uniform_texture(grp, "weightTex", G_draw.weight_ramp);
    }
  }

  /* control points for primitives and speed guide */
  const bool is_cppoint = (gpd->runtime.tot_cp_points > 0);
  const bool is_speed_guide = (ts->gp_sculpt.guide.use_guide &&
                               (draw_ctx->object_mode == OB_MODE_PAINT_GPENCIL));
  const bool is_show_gizmo = (((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) &&
                              ((v3d->gizmo_flag & V3D_GIZMO_HIDE_TOOL) == 0));

  if ((is_cppoint || is_speed_guide) && (is_show_gizmo)) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->edit_gpencil_gizmos_ps, state);

    sh = OVERLAY_shader_edit_gpencil_guide_point();
    grp = DRW_shgroup_create(sh, psl->edit_gpencil_gizmos_ps);

    if (gpd->runtime.cp_points != NULL) {
      for (int i = 0; i < gpd->runtime.tot_cp_points; i++) {
        bGPDcontrolpoint *cp = &gpd->runtime.cp_points[i];
        grp = DRW_shgroup_create_sub(grp);
        DRW_shgroup_uniform_vec3_copy(grp, "pPosition", &cp->x);
        DRW_shgroup_uniform_float_copy(grp, "pSize", cp->size * 0.8f * G_draw.block.sizePixel);
        DRW_shgroup_uniform_vec4_copy(grp, "pColor", cp->color);
        DRW_shgroup_call_procedural_points(grp, NULL, 1);
      }
    }

    if (ts->gp_sculpt.guide.use_guide) {
      float color[4];
      if (ts->gp_sculpt.guide.reference_point == GP_GUIDE_REF_CUSTOM) {
        UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
        DRW_shgroup_uniform_vec3_copy(grp, "pPosition", ts->gp_sculpt.guide.location);
      }
      else if (ts->gp_sculpt.guide.reference_point == GP_GUIDE_REF_OBJECT &&
               ts->gp_sculpt.guide.reference_object != NULL) {
        UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color);
        DRW_shgroup_uniform_vec3_copy(grp, "pPosition", ts->gp_sculpt.guide.reference_object->loc);
      }
      else {
        UI_GetThemeColor4fv(TH_REDALERT, color);
        DRW_shgroup_uniform_vec3_copy(grp, "pPosition", scene->cursor.location);
      }
      DRW_shgroup_uniform_vec4_copy(grp, "pColor", color);
      DRW_shgroup_uniform_float_copy(grp, "pSize", 8.0f * G_draw.block.sizePixel);
      DRW_shgroup_call_procedural_points(grp, NULL, 1);
    }
  }
}

void OVERLAY_gpencil_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  struct GPUShader *sh;
  DRWShadingGroup *grp;

  /* Default: Display nothing. */
  psl->gpencil_canvas_ps = NULL;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  Object *ob = draw_ctx->obact;
  bGPdata *gpd = ob ? (bGPdata *)ob->data : NULL;
  Scene *scene = draw_ctx->scene;
  ToolSettings *ts = scene->toolsettings;
  const View3DCursor *cursor = &scene->cursor;

  if (gpd == NULL || ob->type != OB_GPENCIL) {
    return;
  }

  const bool show_overlays = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
  const bool show_grid = (v3d->gp_flag & V3D_GP_SHOW_GRID) != 0 &&
                         ((ts->gpencil_v3d_align &
                           (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE)) == 0);
  const bool grid_xray = (v3d->gp_flag & V3D_GP_SHOW_GRID_XRAY);

  if (show_grid && show_overlays) {
    const char *grid_unit = NULL;
    float mat[4][4];
    float col_grid[4];
    float size[2];

    /* set color */
    copy_v3_v3(col_grid, gpd->grid.color);
    col_grid[3] = max_ff(v3d->overlay.gpencil_grid_opacity, 0.01f);

    copy_m4_m4(mat, ob->obmat);

    float viewinv[4][4];
    /* Set the grid in the selected axis */
    switch (ts->gp_sculpt.lock_axis) {
      case GP_LOCKAXIS_X:
        swap_v4_v4(mat[0], mat[2]);
        break;
      case GP_LOCKAXIS_Y:
        swap_v4_v4(mat[1], mat[2]);
        break;
      case GP_LOCKAXIS_Z:
        /* Default. */
        break;
      case GP_LOCKAXIS_CURSOR:
        loc_eul_size_to_mat4(mat, cursor->location, cursor->rotation_euler, (float[3]){1, 1, 1});
        break;
      case GP_LOCKAXIS_VIEW:
        /* view aligned */
        DRW_view_viewmat_get(NULL, viewinv, true);
        copy_v3_v3(mat[0], viewinv[0]);
        copy_v3_v3(mat[1], viewinv[1]);
        break;
    }

    /* Move the grid to the right location depending of the align type.
     * This is required only for 3D Cursor or Origin. */
    if (ts->gpencil_v3d_align & GP_PROJECT_CURSOR) {
      copy_v3_v3(mat[3], cursor->location);
    }
    else if (ts->gpencil_v3d_align & GP_PROJECT_VIEWSPACE) {
      copy_v3_v3(mat[3], ob->obmat[3]);
    }

    translate_m4(mat, gpd->grid.offset[0], gpd->grid.offset[1], 0.0f);
    mul_v2_v2fl(size, gpd->grid.scale, 2.0f * ED_scene_grid_scale(scene, &grid_unit));
    rescale_m4(mat, (float[3]){size[0], size[1], 0.0f});

    const int gridlines = (gpd->grid.lines <= 0) ? 1 : gpd->grid.lines;
    int line_ct = gridlines * 4 + 2;

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;
    state |= (grid_xray) ? DRW_STATE_DEPTH_ALWAYS : DRW_STATE_DEPTH_LESS_EQUAL;

    DRW_PASS_CREATE(psl->gpencil_canvas_ps, state);

    sh = OVERLAY_shader_gpencil_canvas();
    grp = DRW_shgroup_create(sh, psl->gpencil_canvas_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_vec4_copy(grp, "color", col_grid);
    DRW_shgroup_uniform_vec3_copy(grp, "xAxis", mat[0]);
    DRW_shgroup_uniform_vec3_copy(grp, "yAxis", mat[1]);
    DRW_shgroup_uniform_vec3_copy(grp, "origin", mat[3]);
    DRW_shgroup_uniform_int_copy(grp, "halfLineCount", line_ct / 2);
    DRW_shgroup_call_procedural_lines(grp, NULL, line_ct);
  }
}

static void OVERLAY_edit_gpencil_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  bGPdata *gpd = (bGPdata *)ob->data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;

  /* Overlay is only for active object. */
  if (ob != draw_ctx->obact) {
    return;
  }

  if (pd->edit_gpencil_wires_grp) {
    DRWShadingGroup *grp = DRW_shgroup_create_sub(pd->edit_gpencil_wires_grp);
    DRW_shgroup_uniform_vec4_copy(grp, "gpEditColor", gpd->line_color);

    struct GPUBatch *geom = DRW_cache_gpencil_edit_lines_get(ob, pd->cfra);
    DRW_shgroup_call_no_cull(pd->edit_gpencil_wires_grp, geom, ob);
  }

  if (pd->edit_gpencil_points_grp) {
    const bool show_direction = (v3d->gp_flag & V3D_GP_SHOW_STROKE_DIRECTION) != 0;

    DRWShadingGroup *grp = DRW_shgroup_create_sub(pd->edit_gpencil_points_grp);
    DRW_shgroup_uniform_float_copy(grp, "doStrokeEndpoints", show_direction);

    struct GPUBatch *geom = DRW_cache_gpencil_edit_points_get(ob, pd->cfra);
    DRW_shgroup_call_no_cull(grp, geom, ob);
  }
}

static void overlay_gpencil_draw_stroke_color_name(bGPDlayer *UNUSED(gpl),
                                                   bGPDframe *UNUSED(gpf),
                                                   bGPDstroke *gps,
                                                   void *thunk)
{
  Object *ob = (Object *)thunk;
  Material *ma = BKE_object_material_get(ob, gps->mat_nr + 1);
  if (ma == NULL) {
    return;
  }
  MaterialGPencilStyle *gp_style = ma->gp_style;
  /* skip stroke if it doesn't have any valid data */
  if ((gps->points == NULL) || (gps->totpoints < 1) || (gp_style == NULL)) {
    return;
  }
  /* check if the color is visible */
  if (gp_style->flag & GP_MATERIAL_HIDE) {
    return;
  }
  /* only if selected */
  if (gps->flag & GP_STROKE_SELECT) {
    for (int i = 0; i < gps->totpoints; i++) {
      bGPDspoint *pt = &gps->points[i];
      /* Draw name at the first selected point. */
      if (pt->flag & GP_SPOINT_SELECT) {
        const DRWContextState *draw_ctx = DRW_context_state_get();
        ViewLayer *view_layer = draw_ctx->view_layer;

        int theme_id = DRW_object_wire_theme_get(ob, view_layer, NULL);
        uchar color[4];
        UI_GetThemeColor4ubv(theme_id, color);

        float fpt[3];
        mul_v3_m4v3(fpt, ob->obmat, &pt->x);

        struct DRWTextStore *dt = DRW_text_cache_ensure();
        DRW_text_cache_add(dt,
                           fpt,
                           ma->id.name + 2,
                           strlen(ma->id.name + 2),
                           10,
                           0,
                           DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                           color);
        break;
      }
    }
  }
}

static void OVERLAY_gpencil_color_names(Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  int cfra = DEG_get_ctime(draw_ctx->depsgraph);

  BKE_gpencil_visible_stroke_iter(
      NULL, ob, NULL, overlay_gpencil_draw_stroke_color_name, ob, false, cfra);
}

void OVERLAY_gpencil_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;

  bGPdata *gpd = (bGPdata *)ob->data;
  if (gpd == NULL) {
    return;
  }

  if (GPENCIL_ANY_MODE(gpd)) {
    OVERLAY_edit_gpencil_cache_populate(vedata, ob);
  }

  /* don't show object extras in set's */
  if ((ob->base_flag & (BASE_FROM_SET | BASE_FROM_DUPLI)) == 0) {
    if ((v3d->gp_flag & V3D_GP_SHOW_MATERIAL_NAME) && (ob->mode == OB_MODE_EDIT_GPENCIL) &&
        DRW_state_show_text()) {
      OVERLAY_gpencil_color_names(ob);
    }
  }
}

void OVERLAY_gpencil_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->gpencil_canvas_ps) {
    DRW_draw_pass(psl->gpencil_canvas_ps);
  }
}

void OVERLAY_edit_gpencil_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->edit_gpencil_gizmos_ps) {
    DRW_draw_pass(psl->edit_gpencil_gizmos_ps);
  }

  if (psl->edit_gpencil_ps) {
    DRW_draw_pass(psl->edit_gpencil_ps);
  }
}
