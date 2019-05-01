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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup draw
 */

#include "BLI_polyfill_2d.h"
#include "BLI_math_color.h"

#include "DNA_meshdata_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil.h"

#include "DRW_render.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "gpencil_engine.h"

/* Helper to add stroke point to vbo */
static void gpencil_set_stroke_point(GPUVertBuf *vbo,
                                     const bGPDspoint *pt,
                                     int idx,
                                     uint pos_id,
                                     uint color_id,
                                     uint thickness_id,
                                     uint uvdata_id,
                                     short thickness,
                                     const float ink[4])
{

  float alpha = ink[3] * pt->strength;
  CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
  float col[4];
  ARRAY_SET_ITEMS(col, ink[0], ink[1], ink[2], alpha);

  GPU_vertbuf_attr_set(vbo, color_id, idx, col);

  /* transfer both values using the same shader variable */
  float uvdata[2] = {pt->uv_fac, pt->uv_rot};
  GPU_vertbuf_attr_set(vbo, uvdata_id, idx, uvdata);

  /* the thickness of the stroke must be affected by zoom, so a pixel scale is calculated */
  float thick = max_ff(pt->pressure * thickness, 1.0f);
  GPU_vertbuf_attr_set(vbo, thickness_id, idx, &thick);

  GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt->x);
}

/* Helper to add buffer_stroke point to vbo */
static void gpencil_set_buffer_stroke_point(GPUVertBuf *vbo,
                                            const bGPDspoint *pt,
                                            int idx,
                                            uint pos_id,
                                            uint color_id,
                                            uint thickness_id,
                                            uint uvdata_id,
                                            uint prev_pos_id,
                                            float ref_pt[3],
                                            short thickness,
                                            const float ink[4])
{

  float alpha = ink[3] * pt->strength;
  CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
  float col[4];
  ARRAY_SET_ITEMS(col, ink[0], ink[1], ink[2], alpha);

  GPU_vertbuf_attr_set(vbo, color_id, idx, col);

  /* transfer both values using the same shader variable */
  float uvdata[2] = {pt->uv_fac, pt->uv_rot};
  GPU_vertbuf_attr_set(vbo, uvdata_id, idx, uvdata);

  /* the thickness of the stroke must be affected by zoom, so a pixel scale is calculated */
  float thick = max_ff(pt->pressure * thickness, 1.0f);
  GPU_vertbuf_attr_set(vbo, thickness_id, idx, &thick);

  GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt->x);
  /* reference point to follow drawing path */
  GPU_vertbuf_attr_set(vbo, prev_pos_id, idx, ref_pt);
}

/* Helper to add a new fill point and texture coordinates to vertex buffer */
static void gpencil_set_fill_point(GPUVertBuf *vbo,
                                   int idx,
                                   bGPDspoint *pt,
                                   const float fcolor[4],
                                   float uv[2],
                                   uint pos_id,
                                   uint color_id,
                                   uint text_id)
{
  GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt->x);
  GPU_vertbuf_attr_set(vbo, color_id, idx, fcolor);
  GPU_vertbuf_attr_set(vbo, text_id, idx, uv);
}

static void gpencil_vbo_ensure_size(GpencilBatchCacheElem *be, int totvertex)
{
  if (be->vbo->vertex_alloc <= be->vbo_len + totvertex) {
    uint newsize = be->vbo->vertex_alloc +
                   (((totvertex / GPENCIL_VBO_BLOCK_SIZE) + 1) * GPENCIL_VBO_BLOCK_SIZE);
    GPU_vertbuf_data_resize(be->vbo, newsize);
  }
}

/* create batch geometry data for points stroke shader */
void DRW_gpencil_get_point_geom(GpencilBatchCacheElem *be,
                                bGPDstroke *gps,
                                short thickness,
                                const float ink[4])
{
  int totvertex = gps->totpoints;
  if (be->vbo == NULL) {
    be->pos_id = GPU_vertformat_attr_add(&be->format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    be->color_id = GPU_vertformat_attr_add(&be->format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    be->thickness_id = GPU_vertformat_attr_add(
        &be->format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    be->uvdata_id = GPU_vertformat_attr_add(
        &be->format, "uvdata", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    be->prev_pos_id = GPU_vertformat_attr_add(
        &be->format, "prev_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    be->vbo = GPU_vertbuf_create_with_format(&be->format);
    GPU_vertbuf_data_alloc(be->vbo, be->tot_vertex);
    be->vbo_len = 0;
  }
  gpencil_vbo_ensure_size(be, totvertex);

  /* draw stroke curve */
  const bGPDspoint *pt = gps->points;
  float alpha;
  float col[4];

  for (int i = 0; i < gps->totpoints; i++, pt++) {
    /* set point */
    alpha = ink[3] * pt->strength;
    CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
    ARRAY_SET_ITEMS(col, ink[0], ink[1], ink[2], alpha);

    float thick = max_ff(pt->pressure * thickness, 1.0f);

    GPU_vertbuf_attr_set(be->vbo, be->color_id, be->vbo_len, col);
    GPU_vertbuf_attr_set(be->vbo, be->thickness_id, be->vbo_len, &thick);

    /* transfer both values using the same shader variable */
    float uvdata[2] = {pt->uv_fac, pt->uv_rot};
    GPU_vertbuf_attr_set(be->vbo, be->uvdata_id, be->vbo_len, uvdata);

    GPU_vertbuf_attr_set(be->vbo, be->pos_id, be->vbo_len, &pt->x);

    /* use previous point to determine stroke direction */
    bGPDspoint *pt2 = NULL;
    float fpt[3];
    if (i == 0) {
      if (gps->totpoints > 1) {
        /* extrapolate a point before first point */
        pt2 = &gps->points[1];
        interp_v3_v3v3(fpt, &pt2->x, &pt->x, 1.5f);
        GPU_vertbuf_attr_set(be->vbo, be->prev_pos_id, be->vbo_len, fpt);
      }
      else {
        /* add small offset to get a vector */
        copy_v3_v3(fpt, &pt->x);
        fpt[0] += 0.00001f;
        fpt[1] += 0.00001f;
        GPU_vertbuf_attr_set(be->vbo, be->prev_pos_id, be->vbo_len, fpt);
      }
    }
    else {
      pt2 = &gps->points[i - 1];
      GPU_vertbuf_attr_set(be->vbo, be->prev_pos_id, be->vbo_len, &pt2->x);
    }

    be->vbo_len++;
  }
}

/* create batch geometry data for stroke shader */
void DRW_gpencil_get_stroke_geom(struct GpencilBatchCacheElem *be,
                                 bGPDstroke *gps,
                                 short thickness,
                                 const float ink[4])
{
  bGPDspoint *points = gps->points;
  int totpoints = gps->totpoints;
  /* if cyclic needs more vertex */
  int cyclic_add = (gps->flag & GP_STROKE_CYCLIC) ? 1 : 0;
  int totvertex = totpoints + cyclic_add + 2;

  if (be->vbo == NULL) {
    be->pos_id = GPU_vertformat_attr_add(&be->format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    be->color_id = GPU_vertformat_attr_add(&be->format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    be->thickness_id = GPU_vertformat_attr_add(
        &be->format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    be->uvdata_id = GPU_vertformat_attr_add(
        &be->format, "uvdata", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    be->vbo = GPU_vertbuf_create_with_format(&be->format);
    GPU_vertbuf_data_alloc(be->vbo, be->tot_vertex);
    be->vbo_len = 0;
  }
  gpencil_vbo_ensure_size(be, totvertex);

  /* draw stroke curve */
  const bGPDspoint *pt = points;
  for (int i = 0; i < totpoints; i++, pt++) {
    /* first point for adjacency (not drawn) */
    if (i == 0) {
      if (gps->flag & GP_STROKE_CYCLIC && totpoints > 2) {
        gpencil_set_stroke_point(be->vbo,
                                 &points[totpoints - 1],
                                 be->vbo_len,
                                 be->pos_id,
                                 be->color_id,
                                 be->thickness_id,
                                 be->uvdata_id,
                                 thickness,
                                 ink);
        be->vbo_len++;
      }
      else {
        gpencil_set_stroke_point(be->vbo,
                                 &points[1],
                                 be->vbo_len,
                                 be->pos_id,
                                 be->color_id,
                                 be->thickness_id,
                                 be->uvdata_id,
                                 thickness,
                                 ink);
        be->vbo_len++;
      }
    }
    /* set point */
    gpencil_set_stroke_point(be->vbo,
                             pt,
                             be->vbo_len,
                             be->pos_id,
                             be->color_id,
                             be->thickness_id,
                             be->uvdata_id,
                             thickness,
                             ink);
    be->vbo_len++;
  }

  if (gps->flag & GP_STROKE_CYCLIC && totpoints > 2) {
    /* draw line to first point to complete the cycle */
    gpencil_set_stroke_point(be->vbo,
                             &points[0],
                             be->vbo_len,
                             be->pos_id,
                             be->color_id,
                             be->thickness_id,
                             be->uvdata_id,
                             thickness,
                             ink);
    be->vbo_len++;
    /* now add adjacency point (not drawn) */
    gpencil_set_stroke_point(be->vbo,
                             &points[1],
                             be->vbo_len,
                             be->pos_id,
                             be->color_id,
                             be->thickness_id,
                             be->uvdata_id,
                             thickness,
                             ink);
    be->vbo_len++;
  }
  /* last adjacency point (not drawn) */
  else {
    gpencil_set_stroke_point(be->vbo,
                             &points[totpoints - 2],
                             be->vbo_len,
                             be->pos_id,
                             be->color_id,
                             be->thickness_id,
                             be->uvdata_id,
                             thickness,
                             ink);
    be->vbo_len++;
  }
}

/* create batch geometry data for stroke shader */
void DRW_gpencil_get_fill_geom(struct GpencilBatchCacheElem *be,
                               Object *ob,
                               bGPDstroke *gps,
                               const float color[4])
{
  BLI_assert(gps->totpoints >= 3);

  /* Calculate triangles cache for filling area (must be done only after changes) */
  if ((gps->flag & GP_STROKE_RECALC_GEOMETRY) || (gps->tot_triangles == 0) ||
      (gps->triangles == NULL)) {
    DRW_gpencil_triangulate_stroke_fill(ob, gps);
  }

  BLI_assert(gps->tot_triangles >= 1);
  int totvertex = gps->tot_triangles * 3;

  if (be->vbo == NULL) {
    be->pos_id = GPU_vertformat_attr_add(&be->format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    be->color_id = GPU_vertformat_attr_add(&be->format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    be->uvdata_id = GPU_vertformat_attr_add(
        &be->format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    be->vbo = GPU_vertbuf_create_with_format(&be->format);
    GPU_vertbuf_data_alloc(be->vbo, be->tot_vertex);
    be->vbo_len = 0;
  }
  gpencil_vbo_ensure_size(be, totvertex);

  /* Draw all triangles for filling the polygon (cache must be calculated before) */
  bGPDtriangle *stroke_triangle = gps->triangles;
  for (int i = 0; i < gps->tot_triangles; i++, stroke_triangle++) {
    for (int j = 0; j < 3; j++) {
      gpencil_set_fill_point(be->vbo,
                             be->vbo_len,
                             &gps->points[stroke_triangle->verts[j]],
                             color,
                             stroke_triangle->uv[j],
                             be->pos_id,
                             be->color_id,
                             be->uvdata_id);
      be->vbo_len++;
    }
  }
}

/* create batch geometry data for current buffer stroke shader */
GPUBatch *DRW_gpencil_get_buffer_stroke_geom(bGPdata *gpd, short thickness)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  ARegion *ar = draw_ctx->ar;
  RegionView3D *rv3d = draw_ctx->rv3d;
  ToolSettings *ts = scene->toolsettings;
  Object *ob = draw_ctx->obact;

  tGPspoint *points = gpd->runtime.sbuffer;
  int totpoints = gpd->runtime.sbuffer_size;
  /* if cyclic needs more vertex */
  int cyclic_add = (gpd->runtime.sbuffer_sflag & GP_STROKE_CYCLIC) ? 1 : 0;
  int totvertex = totpoints + cyclic_add + 2;

  static GPUVertFormat format = {0};
  static uint pos_id, color_id, thickness_id, uvdata_id;
  if (format.attr_len == 0) {
    pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    thickness_id = GPU_vertformat_attr_add(&format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    uvdata_id = GPU_vertformat_attr_add(&format, "uvdata", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  }

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, totvertex);

  /* draw stroke curve */
  const tGPspoint *tpt = points;
  bGPDspoint pt, pt2, pt3;
  int idx = 0;

  /* get origin to reproject point */
  float origin[3];
  bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
  ED_gp_get_drawing_reference(scene, ob, gpl, ts->gpencil_v3d_align, origin);

  for (int i = 0; i < totpoints; i++, tpt++) {
    ED_gpencil_tpoint_to_point(ar, origin, tpt, &pt);
    ED_gp_project_point_to_plane(scene, ob, rv3d, origin, ts->gp_sculpt.lock_axis - 1, &pt);

    /* first point for adjacency (not drawn) */
    if (i == 0) {
      if (gpd->runtime.sbuffer_sflag & GP_STROKE_CYCLIC && totpoints > 2) {
        ED_gpencil_tpoint_to_point(ar, origin, &points[totpoints - 1], &pt2);
        gpencil_set_stroke_point(vbo,
                                 &pt2,
                                 idx,
                                 pos_id,
                                 color_id,
                                 thickness_id,
                                 uvdata_id,
                                 thickness,
                                 gpd->runtime.scolor);
        idx++;
      }
      else {
        ED_gpencil_tpoint_to_point(ar, origin, &points[1], &pt2);
        gpencil_set_stroke_point(vbo,
                                 &pt2,
                                 idx,
                                 pos_id,
                                 color_id,
                                 thickness_id,
                                 uvdata_id,
                                 thickness,
                                 gpd->runtime.scolor);
        idx++;
      }
    }

    /* set point */
    gpencil_set_stroke_point(
        vbo, &pt, idx, pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
    idx++;
  }

  /* last adjacency point (not drawn) */
  if (gpd->runtime.sbuffer_sflag & GP_STROKE_CYCLIC && totpoints > 2) {
    /* draw line to first point to complete the cycle */
    ED_gpencil_tpoint_to_point(ar, origin, &points[0], &pt2);
    gpencil_set_stroke_point(
        vbo, &pt2, idx, pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
    idx++;
    /* now add adjacency point (not drawn) */
    ED_gpencil_tpoint_to_point(ar, origin, &points[1], &pt3);
    gpencil_set_stroke_point(
        vbo, &pt3, idx, pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
    idx++;
  }
  /* last adjacency point (not drawn) */
  else {
    ED_gpencil_tpoint_to_point(ar, origin, &points[totpoints - 2], &pt2);
    gpencil_set_stroke_point(
        vbo, &pt2, idx, pos_id, color_id, thickness_id, uvdata_id, thickness, gpd->runtime.scolor);
    idx++;
  }

  return GPU_batch_create_ex(GPU_PRIM_LINE_STRIP_ADJ, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* create batch geometry data for current buffer point shader */
GPUBatch *DRW_gpencil_get_buffer_point_geom(bGPdata *gpd, short thickness)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  ARegion *ar = draw_ctx->ar;
  RegionView3D *rv3d = draw_ctx->rv3d;
  ToolSettings *ts = scene->toolsettings;
  Object *ob = draw_ctx->obact;

  tGPspoint *points = gpd->runtime.sbuffer;
  int totpoints = gpd->runtime.sbuffer_size;

  static GPUVertFormat format = {0};
  static uint pos_id, color_id, thickness_id, uvdata_id, prev_pos_id;
  if (format.attr_len == 0) {
    pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    thickness_id = GPU_vertformat_attr_add(&format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    uvdata_id = GPU_vertformat_attr_add(&format, "uvdata", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    prev_pos_id = GPU_vertformat_attr_add(&format, "prev_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, totpoints);

  /* draw stroke curve */
  const tGPspoint *tpt = points;
  bGPDspoint pt;
  int idx = 0;

  /* get origin to reproject point */
  float origin[3];
  bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
  ED_gp_get_drawing_reference(scene, ob, gpl, ts->gpencil_v3d_align, origin);

  for (int i = 0; i < totpoints; i++, tpt++) {
    ED_gpencil_tpoint_to_point(ar, origin, tpt, &pt);
    ED_gp_project_point_to_plane(scene, ob, rv3d, origin, ts->gp_sculpt.lock_axis - 1, &pt);

    /* use previous point to determine stroke direction (drawing path) */
    bGPDspoint pt2;
    float ref_pt[3];

    if (i == 0) {
      if (totpoints > 1) {
        /* extrapolate a point before first point */
        tGPspoint *tpt2 = &points[1];
        ED_gpencil_tpoint_to_point(ar, origin, tpt2, &pt2);
        ED_gp_project_point_to_plane(scene, ob, rv3d, origin, ts->gp_sculpt.lock_axis - 1, &pt2);

        interp_v3_v3v3(ref_pt, &pt2.x, &pt.x, 1.5f);
      }
      else {
        copy_v3_v3(ref_pt, &pt.x);
      }
    }
    else {
      tGPspoint *tpt2 = &points[i - 1];
      ED_gpencil_tpoint_to_point(ar, origin, tpt2, &pt2);
      ED_gp_project_point_to_plane(scene, ob, rv3d, origin, ts->gp_sculpt.lock_axis - 1, &pt2);

      copy_v3_v3(ref_pt, &pt2.x);
    }

    /* set point */
    gpencil_set_buffer_stroke_point(vbo,
                                    &pt,
                                    idx,
                                    pos_id,
                                    color_id,
                                    thickness_id,
                                    uvdata_id,
                                    prev_pos_id,
                                    ref_pt,
                                    thickness,
                                    gpd->runtime.scolor);
    idx++;
  }

  return GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* create batch geometry data for current buffer control point shader */
GPUBatch *DRW_gpencil_get_buffer_ctrlpoint_geom(bGPdata *gpd)
{
  bGPDcontrolpoint *cps = gpd->runtime.cp_points;
  int totpoints = gpd->runtime.tot_cp_points;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  ToolSettings *ts = scene->toolsettings;

  if (ts->gp_sculpt.guide.use_guide) {
    totpoints++;
  }

  static GPUVertFormat format = {0};
  static uint pos_id, color_id, size_id;
  if (format.attr_len == 0) {
    pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    size_id = GPU_vertformat_attr_add(&format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, totpoints);

  int idx = 0;
  for (int i = 0; i < gpd->runtime.tot_cp_points; i++) {
    bGPDcontrolpoint *cp = &cps[i];

    GPU_vertbuf_attr_set(vbo, color_id, idx, cp->color);

    /* scale size */
    float size = cp->size * 0.8f;
    GPU_vertbuf_attr_set(vbo, size_id, idx, &size);

    GPU_vertbuf_attr_set(vbo, pos_id, idx, &cp->x);
    idx++;
  }

  if (ts->gp_sculpt.guide.use_guide) {
    float size = 10 * 0.8f;
    float color[4];
    float position[3];
    if (ts->gp_sculpt.guide.reference_point == GP_GUIDE_REF_CUSTOM) {
      UI_GetThemeColor4fv(TH_GIZMO_PRIMARY, color);
      copy_v3_v3(position, ts->gp_sculpt.guide.location);
    }
    else if (ts->gp_sculpt.guide.reference_point == GP_GUIDE_REF_OBJECT &&
             ts->gp_sculpt.guide.reference_object != NULL) {
      UI_GetThemeColor4fv(TH_GIZMO_SECONDARY, color);
      copy_v3_v3(position, ts->gp_sculpt.guide.reference_object->loc);
    }
    else {
      UI_GetThemeColor4fv(TH_REDALERT, color);
      copy_v3_v3(position, scene->cursor.location);
    }
    GPU_vertbuf_attr_set(vbo, pos_id, idx, position);
    GPU_vertbuf_attr_set(vbo, size_id, idx, &size);
    GPU_vertbuf_attr_set(vbo, color_id, idx, color);
  }

  return GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* create batch geometry data for current buffer fill shader */
GPUBatch *DRW_gpencil_get_buffer_fill_geom(bGPdata *gpd)
{
  if (gpd == NULL) {
    return NULL;
  }

  const tGPspoint *points = gpd->runtime.sbuffer;
  int totpoints = gpd->runtime.sbuffer_size;
  if (totpoints < 3) {
    return NULL;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  ARegion *ar = draw_ctx->ar;
  ToolSettings *ts = scene->toolsettings;
  Object *ob = draw_ctx->obact;

  /* get origin to reproject point */
  float origin[3];
  bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
  ED_gp_get_drawing_reference(scene, ob, gpl, ts->gpencil_v3d_align, origin);

  int tot_triangles = totpoints - 2;
  /* allocate memory for temporary areas */
  uint(*tmp_triangles)[3] = MEM_mallocN(sizeof(*tmp_triangles) * tot_triangles, __func__);
  float(*points2d)[2] = MEM_mallocN(sizeof(*points2d) * totpoints, __func__);

  /* Convert points to array and triangulate
   * Here a cache is not used because while drawing the information changes all the time, so the
   * cache would be recalculated constantly, so it is better to do direct calculation for each
   * function call
   */
  for (int i = 0; i < totpoints; i++) {
    const tGPspoint *pt = &points[i];
    points2d[i][0] = pt->x;
    points2d[i][1] = pt->y;
  }
  BLI_polyfill_calc(points2d, (uint)totpoints, 0, tmp_triangles);

  static GPUVertFormat format = {0};
  static uint pos_id, color_id;
  if (format.attr_len == 0) {
    pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);

  /* draw triangulation data */
  if (tot_triangles > 0) {
    GPU_vertbuf_data_alloc(vbo, tot_triangles * 3);

    const tGPspoint *tpt;
    bGPDspoint pt;

    int idx = 0;
    for (int i = 0; i < tot_triangles; i++) {
      for (int j = 0; j < 3; j++) {
        tpt = &points[tmp_triangles[i][j]];
        ED_gpencil_tpoint_to_point(ar, origin, tpt, &pt);
        GPU_vertbuf_attr_set(vbo, pos_id, idx, &pt.x);
        GPU_vertbuf_attr_set(vbo, color_id, idx, gpd->runtime.sfill);
        idx++;
      }
    }
  }

  /* clear memory */
  if (tmp_triangles) {
    MEM_freeN(tmp_triangles);
  }
  if (points2d) {
    MEM_freeN(points2d);
  }

  return GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, NULL, GPU_BATCH_OWNS_VBO);
}

/* Draw selected verts for strokes being edited */
void DRW_gpencil_get_edit_geom(struct GpencilBatchCacheElem *be,
                               bGPDstroke *gps,
                               float alpha,
                               short dflag)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Object *ob = draw_ctx->obact;
  bGPdata *gpd = ob->data;
  const bool is_weight_paint = (gpd) && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE);

  int vgindex = ob->actdef - 1;
  if (!BLI_findlink(&ob->defbase, vgindex)) {
    vgindex = -1;
  }

  /* Get size of verts:
   * - The selected state needs to be larger than the unselected state so that
   *   they stand out more.
   * - We use the theme setting for size of the unselected verts
   */
  float bsize = UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
  float vsize;
  if ((int)bsize > 8) {
    vsize = 10.0f;
    bsize = 8.0f;
  }
  else {
    vsize = bsize + 2;
  }

  /* for now, we assume that the base color of the points is not too close to the real color */
  float selectColor[4];
  UI_GetThemeColor3fv(TH_GP_VERTEX_SELECT, selectColor);
  selectColor[3] = alpha;

  float unselectColor[4];
  UI_GetThemeColor3fv(TH_GP_VERTEX, unselectColor);
  unselectColor[3] = alpha;

  if (be->vbo == NULL) {
    be->pos_id = GPU_vertformat_attr_add(&be->format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    be->color_id = GPU_vertformat_attr_add(&be->format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    be->thickness_id = GPU_vertformat_attr_add(
        &be->format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);

    be->vbo = GPU_vertbuf_create_with_format(&be->format);
    GPU_vertbuf_data_alloc(be->vbo, be->tot_vertex);
    be->vbo_len = 0;
  }
  gpencil_vbo_ensure_size(be, gps->totpoints);

  /* Draw start and end point differently if enabled stroke direction hint */
  bool show_direction_hint = (dflag & GP_DATA_SHOW_DIRECTION) && (gps->totpoints > 1);

  /* Draw all the stroke points (selected or not) */
  bGPDspoint *pt = gps->points;
  MDeformVert *dvert = gps->dvert;

  float fcolor[4];
  float fsize = 0;
  for (int i = 0; i < gps->totpoints; i++, pt++) {
    /* weight paint */
    if (is_weight_paint) {
      float weight = (dvert && dvert->dw && (vgindex > -1)) ? defvert_find_weight(dvert, vgindex) :
                                                              0.0f;
      float hue = 2.0f * (1.0f - weight) / 3.0f;
      hsv_to_rgb(hue, 1.0f, 1.0f, &selectColor[0], &selectColor[1], &selectColor[2]);
      selectColor[3] = 1.0f;
      copy_v4_v4(fcolor, selectColor);
      fsize = vsize;
    }
    else {
      if (show_direction_hint && i == 0) {
        /* start point in green bigger */
        ARRAY_SET_ITEMS(fcolor, 0.0f, 1.0f, 0.0f, 1.0f);
        fsize = vsize + 4;
      }
      else if (show_direction_hint && (i == gps->totpoints - 1)) {
        /* end point in red smaller */
        ARRAY_SET_ITEMS(fcolor, 1.0f, 0.0f, 0.0f, 1.0f);
        fsize = vsize + 1;
      }
      else if (pt->flag & GP_SPOINT_SELECT) {
        copy_v4_v4(fcolor, selectColor);
        fsize = vsize;
      }
      else {
        copy_v4_v4(fcolor, unselectColor);
        fsize = bsize;
      }
    }

    GPU_vertbuf_attr_set(be->vbo, be->color_id, be->vbo_len, fcolor);
    GPU_vertbuf_attr_set(be->vbo, be->thickness_id, be->vbo_len, &fsize);
    GPU_vertbuf_attr_set(be->vbo, be->pos_id, be->vbo_len, &pt->x);
    be->vbo_len++;
    if (gps->dvert != NULL) {
      dvert++;
    }
  }
}

/* Draw lines for strokes being edited */
void DRW_gpencil_get_edlin_geom(struct GpencilBatchCacheElem *be,
                                bGPDstroke *gps,
                                float alpha,
                                short UNUSED(dflag))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Object *ob = draw_ctx->obact;
  bGPdata *gpd = ob->data;
  const bool is_weight_paint = (gpd) && (gpd->flag & GP_DATA_STROKE_WEIGHTMODE);

  int vgindex = ob->actdef - 1;
  if (!BLI_findlink(&ob->defbase, vgindex)) {
    vgindex = -1;
  }

  float selectColor[4];
  UI_GetThemeColor3fv(TH_GP_VERTEX_SELECT, selectColor);
  selectColor[3] = alpha;
  float linecolor[4];
  copy_v4_v4(linecolor, gpd->line_color);

  if (be->vbo == NULL) {
    be->pos_id = GPU_vertformat_attr_add(&be->format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    be->color_id = GPU_vertformat_attr_add(&be->format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

    be->vbo = GPU_vertbuf_create_with_format(&be->format);
    GPU_vertbuf_data_alloc(be->vbo, be->tot_vertex);
    be->vbo_len = 0;
  }
  gpencil_vbo_ensure_size(be, gps->totpoints);

  /* Draw all the stroke lines (selected or not) */
  bGPDspoint *pt = gps->points;
  MDeformVert *dvert = gps->dvert;

  float fcolor[4];
  for (int i = 0; i < gps->totpoints; i++, pt++) {
    /* weight paint */
    if (is_weight_paint) {
      float weight = (dvert && dvert->dw && (vgindex > -1)) ? defvert_find_weight(dvert, vgindex) :
                                                              0.0f;
      float hue = 2.0f * (1.0f - weight) / 3.0f;
      hsv_to_rgb(hue, 1.0f, 1.0f, &selectColor[0], &selectColor[1], &selectColor[2]);
      selectColor[3] = 1.0f;
      copy_v4_v4(fcolor, selectColor);
    }
    else {
      if (pt->flag & GP_SPOINT_SELECT) {
        copy_v4_v4(fcolor, selectColor);
      }
      else {
        copy_v4_v4(fcolor, linecolor);
      }
    }

    GPU_vertbuf_attr_set(be->vbo, be->color_id, be->vbo_len, fcolor);
    GPU_vertbuf_attr_set(be->vbo, be->pos_id, be->vbo_len, &pt->x);
    be->vbo_len++;

    if (gps->dvert != NULL) {
      dvert++;
    }
  }
}

static void set_grid_point(GPUVertBuf *vbo,
                           int idx,
                           float col_grid[4],
                           uint pos_id,
                           uint color_id,
                           float v1,
                           float v2,
                           const int axis)
{
  GPU_vertbuf_attr_set(vbo, color_id, idx, col_grid);

  float pos[3];
  /* Set the grid in the selected axis */
  switch (axis) {
    case GP_LOCKAXIS_X: {
      ARRAY_SET_ITEMS(pos, 0.0f, v1, v2);
      break;
    }
    case GP_LOCKAXIS_Y: {
      ARRAY_SET_ITEMS(pos, v1, 0.0f, v2);
      break;
    }
    case GP_LOCKAXIS_Z:
    default: /* view aligned */
    {
      ARRAY_SET_ITEMS(pos, v1, v2, 0.0f);
      break;
    }
  }

  GPU_vertbuf_attr_set(vbo, pos_id, idx, pos);
}

/* Draw grid lines */
GPUBatch *DRW_gpencil_get_grid(Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;
  ToolSettings *ts = scene->toolsettings;
  View3D *v3d = draw_ctx->v3d;
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool do_center = (gpd->grid.lines <= 0) ? false : true;

  float col_grid[4];

  /* verify we have something to draw and valid values */
  if (gpd->grid.scale[0] == 0.0f) {
    gpd->grid.scale[0] = 1.0f;
  }
  if (gpd->grid.scale[1] == 0.0f) {
    gpd->grid.scale[1] = 1.0f;
  }

  if (v3d->overlay.gpencil_grid_opacity < 0.1f) {
    v3d->overlay.gpencil_grid_opacity = 0.1f;
  }

  /* set color */
  copy_v3_v3(col_grid, gpd->grid.color);
  col_grid[3] = v3d->overlay.gpencil_grid_opacity;

  const int axis = ts->gp_sculpt.lock_axis;

  const char *grid_unit = NULL;
  const int gridlines = (gpd->grid.lines <= 0) ? 1 : gpd->grid.lines;
  const float grid_w = gpd->grid.scale[0] * ED_scene_grid_scale(scene, &grid_unit);
  const float grid_h = gpd->grid.scale[1] * ED_scene_grid_scale(scene, &grid_unit);
  const float space_w = (grid_w / gridlines);
  const float space_h = (grid_h / gridlines);
  const float offset[2] = {gpd->grid.offset[0], gpd->grid.offset[1]};

  const uint vertex_len = 2 * (gridlines * 4 + 2);

  static GPUVertFormat format = {0};
  static uint pos_id, color_id;
  if (format.attr_len == 0) {
    pos_id = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    color_id = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, vertex_len);

  int idx = 0;

  for (int a = 1; a <= gridlines; a++) {
    const float line_w = a * space_w;
    const float line_h = a * space_h;

    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, -grid_w + offset[0], -line_h + offset[1], axis);
    idx++;
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, +grid_w + offset[0], -line_h + offset[1], axis);
    idx++;
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, -grid_w + offset[0], +line_h + offset[1], axis);
    idx++;
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, +grid_w + offset[0], +line_h + offset[1], axis);
    idx++;

    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, -line_w + offset[0], -grid_h + offset[1], axis);
    idx++;
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, -line_w + offset[0], +grid_h + offset[1], axis);
    idx++;
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, +line_w + offset[0], -grid_h + offset[1], axis);
    idx++;
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, +line_w + offset[0], +grid_h + offset[1], axis);
    idx++;
  }
  /* center lines */
  if (do_center) {
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, -grid_w + offset[0], 0.0f + offset[1], axis);
    idx++;
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, +grid_w + offset[0], 0.0f + offset[1], axis);
    idx++;

    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, 0.0f + offset[0], -grid_h + offset[1], axis);
    idx++;
    set_grid_point(
        vbo, idx, col_grid, pos_id, color_id, 0.0f + offset[0], +grid_h + offset[1], axis);
    idx++;
  }
  return GPU_batch_create_ex(GPU_PRIM_LINES, vbo, NULL, GPU_BATCH_OWNS_VBO);
}
