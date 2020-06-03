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
 * The Original Code is Copyright (C) 2020, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup draw
 */

#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_screen_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "ED_gpencil.h"
#include "GPU_batch.h"

#include "DEG_depsgraph_query.h"

#include "BLI_hash.h"
#include "BLI_polyfill_2d.h"

#include "draw_cache.h"
#include "draw_cache_impl.h"

/* ---------------------------------------------------------------------- */
typedef struct GpencilBatchCache {
  /** Instancing Data */
  GPUVertBuf *vbo;
  GPUVertBuf *vbo_col;
  /** Fill Topology */
  GPUIndexBuf *ibo;
  /** Instancing Batches */
  GPUBatch *stroke_batch;
  GPUBatch *fill_batch;
  GPUBatch *lines_batch;

  /** Edit Mode */
  GPUVertBuf *edit_vbo;
  GPUBatch *edit_lines_batch;
  GPUBatch *edit_points_batch;

  /** Cache is dirty */
  bool is_dirty;
  /** Last cache frame */
  int cache_frame;
} GpencilBatchCache;

static bool gpencil_batch_cache_valid(GpencilBatchCache *cache, bGPdata *gpd, int cfra)
{
  bool valid = true;

  if (cache == NULL) {
    return false;
  }

  if (cfra != cache->cache_frame) {
    valid = false;
  }
  else if (gpd->flag & GP_DATA_CACHE_IS_DIRTY) {
    valid = false;
  }
  else if (cache->is_dirty) {
    valid = false;
  }

  return valid;
}

static GpencilBatchCache *gpencil_batch_cache_init(Object *ob, int cfra)
{
  bGPdata *gpd = (bGPdata *)ob->data;

  GpencilBatchCache *cache = gpd->runtime.gpencil_cache;

  if (!cache) {
    cache = gpd->runtime.gpencil_cache = MEM_callocN(sizeof(*cache), __func__);
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->is_dirty = true;
  cache->cache_frame = cfra;

  return cache;
}

static void gpencil_batch_cache_clear(GpencilBatchCache *cache)
{
  if (!cache) {
    return;
  }

  GPU_BATCH_DISCARD_SAFE(cache->lines_batch);
  GPU_BATCH_DISCARD_SAFE(cache->fill_batch);
  GPU_BATCH_DISCARD_SAFE(cache->stroke_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->vbo);
  GPU_VERTBUF_DISCARD_SAFE(cache->vbo_col);
  GPU_INDEXBUF_DISCARD_SAFE(cache->ibo);

  GPU_BATCH_DISCARD_SAFE(cache->edit_lines_batch);
  GPU_BATCH_DISCARD_SAFE(cache->edit_points_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_vbo);

  cache->is_dirty = true;
}

static GpencilBatchCache *gpencil_batch_cache_get(Object *ob, int cfra)
{
  bGPdata *gpd = (bGPdata *)ob->data;

  GpencilBatchCache *cache = gpd->runtime.gpencil_cache;
  if (!gpencil_batch_cache_valid(cache, gpd, cfra)) {
    gpencil_batch_cache_clear(cache);
    return gpencil_batch_cache_init(ob, cfra);
  }
  else {
    return cache;
  }
}

void DRW_gpencil_batch_cache_dirty_tag(bGPdata *gpd)
{
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}

void DRW_gpencil_batch_cache_free(bGPdata *gpd)
{
  gpencil_batch_cache_clear(gpd->runtime.gpencil_cache);
  MEM_SAFE_FREE(gpd->runtime.gpencil_cache);
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  return;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Formats.
 * \{ */

/* MUST match the format below. */
typedef struct gpStrokeVert {
  int32_t mat, stroke_id, point_id, packed_asp_hard_rot;
  /** Position and thickness packed in the same attribute. */
  float pos[3], thickness;
  /** UV and strength packed in the same attribute. */
  float uv_fill[2], u_stroke, strength;
} gpStrokeVert;

static GPUVertFormat *gpencil_stroke_format(void)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "ma", GPU_COMP_I32, 4, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "uv", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    /* IMPORTANT: This means having only 4 attributes
     * to fit into GPU module limit of 16 attributes. */
    GPU_vertformat_multiload_enable(&format, 4);
  }
  return &format;
}

/* MUST match the format below. */
typedef struct gpEditVert {
  uint vflag;
  float weight;
} gpEditVert;

static GPUVertFormat *gpencil_edit_stroke_format(void)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "vflag", GPU_COMP_U32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  return &format;
}

/* MUST match the format below. */
typedef struct gpColorVert {
  float vcol[4]; /* Vertex color */
  float fcol[4]; /* Fill color */
} gpColorVert;

static GPUVertFormat *gpencil_color_format(void)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "col", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "fcol", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    /* IMPORTANT: This means having only 4 attributes
     * to fit into GPU module limit of 16 attributes. */
    GPU_vertformat_multiload_enable(&format, 4);
  }
  return &format;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Buffers.
 * \{ */

typedef struct gpIterData {
  bGPdata *gpd;
  gpStrokeVert *verts;
  gpColorVert *cols;
  GPUIndexBufBuilder ibo;
  int vert_len;
  int tri_len;
} gpIterData;

static GPUVertBuf *gpencil_dummy_buffer_get(void)
{
  GPUBatch *batch = DRW_gpencil_dummy_buffer_get();
  return batch->verts[0];
}

static int gpencil_stroke_is_cyclic(const bGPDstroke *gps)
{
  return ((gps->flag & GP_STROKE_CYCLIC) != 0) && (gps->totpoints > 2);
}

BLI_INLINE int32_t pack_rotation_aspect_hardness(float rot, float asp, float hard)
{
  int32_t packed = 0;
  /* Aspect uses 9 bits */
  float asp_normalized = (asp > 1.0f) ? (1.0f / asp) : asp;
  packed |= (int32_t)unit_float_to_uchar_clamp(asp_normalized);
  /* Store if inversed in the 9th bit. */
  if (asp > 1.0f) {
    packed |= 1 << 8;
  }
  /* Rotation uses 9 bits */
  /* Rotation are in [-90°..90°] range, so we can encode the sign of the angle + the cosine
   * because the cosine will always be positive. */
  packed |= (int32_t)unit_float_to_uchar_clamp(cosf(rot)) << 9;
  /* Store sine sign in 9th bit. */
  if (rot < 0.0f) {
    packed |= 1 << 17;
  }
  /* Hardness uses 8 bits */
  packed |= (int32_t)unit_float_to_uchar_clamp(hard) << 18;
  return packed;
}

static void gpencil_buffer_add_point(gpStrokeVert *verts,
                                     gpColorVert *cols,
                                     const bGPDstroke *gps,
                                     const bGPDspoint *pt,
                                     int v,
                                     bool is_endpoint)
{
  /* Note: we use the sign of stength and thickness to pass cap flag. */
  const bool round_cap0 = (gps->caps[0] == GP_STROKE_CAP_ROUND);
  const bool round_cap1 = (gps->caps[1] == GP_STROKE_CAP_ROUND);
  gpStrokeVert *vert = &verts[v];
  gpColorVert *col = &cols[v];
  copy_v3_v3(vert->pos, &pt->x);
  copy_v2_v2(vert->uv_fill, pt->uv_fill);
  copy_v4_v4(col->vcol, pt->vert_color);
  copy_v4_v4(col->fcol, gps->vert_color_fill);

  /* Encode fill opacity defined by opacity modifier in vertex color alpha. If
   * no opacity modifier, the value will be always 1.0f. The opacity factor can be any
   * value between 0.0f and 2.0f */
  col->fcol[3] = (((int)(col->fcol[3] * 10000.0f)) * 10.0f) + gps->fill_opacity_fac;

  vert->strength = (round_cap0) ? pt->strength : -pt->strength;
  vert->u_stroke = pt->uv_fac;
  vert->stroke_id = gps->runtime.stroke_start;
  vert->point_id = v;
  vert->thickness = max_ff(0.0f, gps->thickness * pt->pressure) * (round_cap1 ? 1.0f : -1.0f);
  /* Tag endpoint material to -1 so they get discarded by vertex shader. */
  vert->mat = (is_endpoint) ? -1 : (gps->mat_nr % GP_MATERIAL_BUFFER_LEN);

  float aspect_ratio = gps->aspect_ratio[0] / max_ff(gps->aspect_ratio[1], 1e-8);

  vert->packed_asp_hard_rot = pack_rotation_aspect_hardness(
      pt->uv_rot, aspect_ratio, gps->hardeness);
}

static void gpencil_buffer_add_stroke(gpStrokeVert *verts,
                                      gpColorVert *cols,
                                      const bGPDstroke *gps)
{
  const bGPDspoint *pts = gps->points;
  int pts_len = gps->totpoints;
  bool is_cyclic = gpencil_stroke_is_cyclic(gps);
  int v = gps->runtime.stroke_start;

  /* First point for adjacency (not drawn). */
  int adj_idx = (is_cyclic) ? (pts_len - 1) : min_ii(pts_len - 1, 1);
  gpencil_buffer_add_point(verts, cols, gps, &pts[adj_idx], v++, true);

  for (int i = 0; i < pts_len; i++) {
    gpencil_buffer_add_point(verts, cols, gps, &pts[i], v++, false);
  }
  /* Draw line to first point to complete the loop for cyclic strokes. */
  if (is_cyclic) {
    gpencil_buffer_add_point(verts, cols, gps, &pts[0], v++, false);
  }
  /* Last adjacency point (not drawn). */
  adj_idx = (is_cyclic) ? 1 : max_ii(0, pts_len - 2);
  gpencil_buffer_add_point(verts, cols, gps, &pts[adj_idx], v++, true);
}

static void gpencil_buffer_add_fill(GPUIndexBufBuilder *ibo, const bGPDstroke *gps)
{
  int tri_len = gps->tot_triangles;
  int v = gps->runtime.stroke_start;
  for (int i = 0; i < tri_len; i++) {
    uint *tri = gps->triangles[i].verts;
    GPU_indexbuf_add_tri_verts(ibo, v + tri[0], v + tri[1], v + tri[2]);
  }
}

static void gpencil_stroke_iter_cb(bGPDlayer *UNUSED(gpl),
                                   bGPDframe *UNUSED(gpf),
                                   bGPDstroke *gps,
                                   void *thunk)
{
  gpIterData *iter = (gpIterData *)thunk;
  gpencil_buffer_add_stroke(iter->verts, iter->cols, gps);
  if (gps->tot_triangles > 0) {
    gpencil_buffer_add_fill(&iter->ibo, gps);
  }
}

static void gp_object_verts_count_cb(bGPDlayer *UNUSED(gpl),
                                     bGPDframe *UNUSED(gpf),
                                     bGPDstroke *gps,
                                     void *thunk)
{
  gpIterData *iter = (gpIterData *)thunk;

  /* Store first index offset */
  gps->runtime.stroke_start = iter->vert_len;
  gps->runtime.fill_start = iter->tri_len;
  iter->vert_len += gps->totpoints + 2 + gpencil_stroke_is_cyclic(gps);
  iter->tri_len += gps->tot_triangles;
}

static void gpencil_batches_ensure(Object *ob, GpencilBatchCache *cache, int cfra)
{
  bGPdata *gpd = (bGPdata *)ob->data;

  if (cache->vbo == NULL) {
    /* Should be discarded together. */
    BLI_assert(cache->vbo == NULL && cache->ibo == NULL);
    BLI_assert(cache->stroke_batch == NULL && cache->stroke_batch == NULL);
    /* TODO/PERF: Could be changed to only do it if needed.
     * For now it's simpler to assume we always need it
     * since multiple viewport could or could not need it.
     * Ideally we should have a dedicated onion skin geom batch. */
    /* IMPORTANT: Keep in sync with gpencil_edit_batches_ensure() */
    bool do_onion = true;

    /* First count how many vertices and triangles are needed for the whole object. */
    gpIterData iter = {
        .gpd = gpd,
        .verts = NULL,
        .ibo = {0},
        .vert_len = 1, /* Start at 1 for the gl_InstanceID trick to work (see vert shader). */
        .tri_len = 0,
    };
    BKE_gpencil_visible_stroke_iter(
        NULL, ob, NULL, gp_object_verts_count_cb, &iter, do_onion, cfra);

    /* Create VBOs. */
    GPUVertFormat *format = gpencil_stroke_format();
    GPUVertFormat *format_col = gpencil_color_format();
    cache->vbo = GPU_vertbuf_create_with_format(format);
    cache->vbo_col = GPU_vertbuf_create_with_format(format_col);
    /* Add extra space at the end of the buffer because of quad load. */
    GPU_vertbuf_data_alloc(cache->vbo, iter.vert_len + 2);
    GPU_vertbuf_data_alloc(cache->vbo_col, iter.vert_len + 2);
    iter.verts = (gpStrokeVert *)cache->vbo->data;
    iter.cols = (gpColorVert *)cache->vbo_col->data;
    /* Create IBO. */
    GPU_indexbuf_init(&iter.ibo, GPU_PRIM_TRIS, iter.tri_len, iter.vert_len);

    /* Fill buffers with data. */
    BKE_gpencil_visible_stroke_iter(NULL, ob, NULL, gpencil_stroke_iter_cb, &iter, do_onion, cfra);

    /* Mark last 2 verts as invalid. */
    for (int i = 0; i < 2; i++) {
      iter.verts[iter.vert_len + i].mat = -1;
    }

    /* Finish the IBO. */
    cache->ibo = GPU_indexbuf_build(&iter.ibo);

    /* Create the batches */
    cache->fill_batch = GPU_batch_create(GPU_PRIM_TRIS, cache->vbo, cache->ibo);
    GPU_batch_vertbuf_add(cache->fill_batch, cache->vbo_col);
    cache->stroke_batch = GPU_batch_create(GPU_PRIM_TRI_STRIP, gpencil_dummy_buffer_get(), NULL);
    GPU_batch_instbuf_add_ex(cache->stroke_batch, cache->vbo, 0);
    GPU_batch_instbuf_add_ex(cache->stroke_batch, cache->vbo_col, 0);

    gpd->flag &= ~GP_DATA_CACHE_IS_DIRTY;
    cache->is_dirty = false;
  }
}

GPUBatch *DRW_cache_gpencil_strokes_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);

  return cache->stroke_batch;
}

GPUBatch *DRW_cache_gpencil_fills_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);

  return cache->fill_batch;
}

static void gp_lines_indices_cb(bGPDlayer *UNUSED(gpl),
                                bGPDframe *UNUSED(gpf),
                                bGPDstroke *gps,
                                void *thunk)
{
  gpIterData *iter = (gpIterData *)thunk;
  int pts_len = gps->totpoints + gpencil_stroke_is_cyclic(gps);

  int start = gps->runtime.stroke_start + 1;
  int end = start + pts_len;
  for (int i = start; i < end; i++) {
    GPU_indexbuf_add_generic_vert(&iter->ibo, i);
  }
  GPU_indexbuf_add_primitive_restart(&iter->ibo);
}

GPUBatch *DRW_cache_gpencil_face_wireframe_get(Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  int cfra = DEG_get_ctime(draw_ctx->depsgraph);

  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);

  if (cache->lines_batch == NULL) {
    GPUVertBuf *vbo = cache->vbo;

    gpIterData iter = {
        .gpd = ob->data,
        .ibo = {0},
    };

    GPU_indexbuf_init_ex(&iter.ibo, GPU_PRIM_LINE_STRIP, vbo->vertex_len, vbo->vertex_len);

    /* IMPORTANT: Keep in sync with gpencil_edit_batches_ensure() */
    bool do_onion = true;
    BKE_gpencil_visible_stroke_iter(NULL, ob, NULL, gp_lines_indices_cb, &iter, do_onion, cfra);

    GPUIndexBuf *ibo = GPU_indexbuf_build(&iter.ibo);

    cache->lines_batch = GPU_batch_create_ex(GPU_PRIM_LINE_STRIP, vbo, ibo, GPU_BATCH_OWNS_INDEX);
  }
  return cache->lines_batch;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Sbuffer stroke batches.
 * \{ */

bGPDstroke *DRW_cache_gpencil_sbuffer_stroke_data_get(Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  Brush *brush = gpd->runtime.sbuffer_brush;
  /* Convert the sbuffer to a bGPDstroke. */
  if (gpd->runtime.sbuffer_gps == NULL) {
    bGPDstroke *gps = MEM_callocN(sizeof(*gps), "bGPDstroke sbuffer");
    gps->totpoints = gpd->runtime.sbuffer_used;
    gps->mat_nr = max_ii(0, gpd->runtime.matid - 1);
    gps->flag = gpd->runtime.sbuffer_sflag;
    gps->thickness = brush->size;
    gps->hardeness = brush->gpencil_settings->hardeness;
    copy_v2_v2(gps->aspect_ratio, brush->gpencil_settings->aspect_ratio);

    /* Reduce slightly the opacity of fill to make easy fill areas while drawing. */
    gps->fill_opacity_fac = 0.8f;

    gps->tot_triangles = max_ii(0, gpd->runtime.sbuffer_used - 2);
    gps->caps[0] = gps->caps[1] = GP_STROKE_CAP_ROUND;
    gps->runtime.stroke_start = 1; /* Add one for the adjacency index. */
    copy_v4_v4(gps->vert_color_fill, gpd->runtime.vert_color_fill);
    gpd->runtime.sbuffer_gps = gps;
  }
  return gpd->runtime.sbuffer_gps;
}

static void gpencil_sbuffer_stroke_ensure(bGPdata *gpd, bool do_stroke, bool do_fill)
{
  tGPspoint *tpoints = gpd->runtime.sbuffer;
  bGPDstroke *gps = gpd->runtime.sbuffer_gps;
  int vert_len = gpd->runtime.sbuffer_used;

  /* DRW_cache_gpencil_sbuffer_stroke_data_get need to have been called previously. */
  BLI_assert(gps != NULL);

  if (do_stroke && (gpd->runtime.sbuffer_stroke_batch == NULL)) {
    gps->points = MEM_mallocN(vert_len * sizeof(*gps->points), __func__);

    const DRWContextState *draw_ctx = DRW_context_state_get();
    Scene *scene = draw_ctx->scene;
    ARegion *region = draw_ctx->region;
    Object *ob = draw_ctx->obact;

    BLI_assert(ob && (ob->type == OB_GPENCIL));

    /* Get origin to reproject points. */
    float origin[3];
    ToolSettings *ts = scene->toolsettings;
    ED_gpencil_drawing_reference_get(scene, ob, ts->gpencil_v3d_align, origin);

    for (int i = 0; i < vert_len; i++) {
      ED_gpencil_tpoint_to_point(region, origin, &tpoints[i], &gps->points[i]);
      mul_m4_v3(ob->imat, &gps->points[i].x);
      bGPDspoint *pt = &gps->points[i];
      copy_v4_v4(pt->vert_color, tpoints[i].vert_color);
    }
    /* Calc uv data along the stroke. */
    BKE_gpencil_stroke_uv_update(gps);

    /* Create VBO. */
    GPUVertFormat *format = gpencil_stroke_format();
    GPUVertFormat *format_color = gpencil_color_format();
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(format);
    GPUVertBuf *vbo_col = GPU_vertbuf_create_with_format(format_color);
    /* Add extra space at the end (and start) of the buffer because of quad load and cyclic. */
    GPU_vertbuf_data_alloc(vbo, 1 + vert_len + 1 + 2);
    GPU_vertbuf_data_alloc(vbo_col, 1 + vert_len + 1 + 2);
    gpStrokeVert *verts = (gpStrokeVert *)vbo->data;
    gpColorVert *cols = (gpColorVert *)vbo_col->data;

    /* Fill buffers with data. */
    gpencil_buffer_add_stroke(verts, cols, gps);

    GPUBatch *batch = GPU_batch_create(GPU_PRIM_TRI_STRIP, gpencil_dummy_buffer_get(), NULL);
    GPU_batch_instbuf_add_ex(batch, vbo, true);
    GPU_batch_instbuf_add_ex(batch, vbo_col, true);

    gpd->runtime.sbuffer_stroke_batch = batch;

    MEM_freeN(gps->points);
  }

  if (do_fill && (gpd->runtime.sbuffer_fill_batch == NULL)) {
    /* Create IBO. */
    GPUIndexBufBuilder ibo_builder;
    GPU_indexbuf_init(&ibo_builder, GPU_PRIM_TRIS, gps->tot_triangles, vert_len);

    if (gps->tot_triangles > 0) {
      float(*tpoints2d)[2] = MEM_mallocN(sizeof(*tpoints2d) * vert_len, __func__);
      /* Triangulate in 2D. */
      for (int i = 0; i < vert_len; i++) {
        copy_v2_v2(tpoints2d[i], &tpoints[i].x);
      }
      /* Compute directly inside the IBO data buffer. */
      /* OPTI: This is a bottleneck if the stroke is very long. */
      BLI_polyfill_calc(tpoints2d, (uint)vert_len, 0, (uint(*)[3])ibo_builder.data);
      /* Add stroke start offset. */
      for (int i = 0; i < gps->tot_triangles * 3; i++) {
        ibo_builder.data[i] += gps->runtime.stroke_start;
      }
      /* HACK since we didn't use the builder API to avoid another malloc and copy,
       * we need to set the number of indices manually. */
      ibo_builder.index_len = gps->tot_triangles * 3;

      MEM_freeN(tpoints2d);
    }

    GPUIndexBuf *ibo = GPU_indexbuf_build(&ibo_builder);
    GPUVertBuf *vbo = gpd->runtime.sbuffer_stroke_batch->inst[0];
    GPUVertBuf *vbo_col = gpd->runtime.sbuffer_stroke_batch->inst[1];

    GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_TRIS, vbo, ibo, GPU_BATCH_OWNS_INDEX);
    GPU_batch_vertbuf_add(batch, vbo_col);

    gpd->runtime.sbuffer_fill_batch = batch;
  }
}

GPUBatch *DRW_cache_gpencil_sbuffer_stroke_get(Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  gpencil_sbuffer_stroke_ensure(gpd, true, false);

  return gpd->runtime.sbuffer_stroke_batch;
}

GPUBatch *DRW_cache_gpencil_sbuffer_fill_get(Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  /* Fill batch also need stroke batch to be created (vbo is shared). */
  gpencil_sbuffer_stroke_ensure(gpd, true, true);

  return gpd->runtime.sbuffer_fill_batch;
}

/* Sbuffer batches are temporary. We need to clear it after drawing */
void DRW_cache_gpencil_sbuffer_clear(Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  MEM_SAFE_FREE(gpd->runtime.sbuffer_gps);
  GPU_BATCH_DISCARD_SAFE(gpd->runtime.sbuffer_fill_batch);
  GPU_BATCH_DISCARD_SAFE(gpd->runtime.sbuffer_stroke_batch);
}

/** \} */

/* ---------------------------------------------------------------------- */
/* Edit GPencil Batches */

#define GP_EDIT_POINT_SELECTED (1 << 0)
#define GP_EDIT_STROKE_SELECTED (1 << 1)
#define GP_EDIT_MULTIFRAME (1 << 2)
#define GP_EDIT_STROKE_START (1 << 3)
#define GP_EDIT_STROKE_END (1 << 4)

typedef struct gpEditIterData {
  gpEditVert *verts;
  int vgindex;
} gpEditIterData;

static uint32_t gpencil_point_edit_flag(const bool layer_lock,
                                        const bGPDspoint *pt,
                                        int v,
                                        int v_len)
{
  uint32_t sflag = 0;
  SET_FLAG_FROM_TEST(sflag, (!layer_lock) && pt->flag & GP_SPOINT_SELECT, GP_EDIT_POINT_SELECTED);
  SET_FLAG_FROM_TEST(sflag, v == 0, GP_EDIT_STROKE_START);
  SET_FLAG_FROM_TEST(sflag, v == (v_len - 1), GP_EDIT_STROKE_END);
  return sflag;
}

static float gpencil_point_edit_weight(const MDeformVert *dvert, int v, int vgindex)
{
  return (dvert && dvert[v].dw) ? BKE_defvert_find_weight(&dvert[v], vgindex) : -1.0f;
}

static void gpencil_edit_stroke_iter_cb(bGPDlayer *gpl,
                                        bGPDframe *gpf,
                                        bGPDstroke *gps,
                                        void *thunk)
{
  gpEditIterData *iter = (gpEditIterData *)thunk;
  const int v_len = gps->totpoints;
  const int v = gps->runtime.stroke_start + 1;
  MDeformVert *dvert = ((iter->vgindex > -1) && gps->dvert) ? gps->dvert : NULL;
  gpEditVert *vert_ptr = iter->verts + v;

  const bool layer_lock = (gpl->flag & GP_LAYER_LOCKED);
  uint32_t sflag = 0;
  SET_FLAG_FROM_TEST(
      sflag, (!layer_lock) && gps->flag & GP_STROKE_SELECT, GP_EDIT_STROKE_SELECTED);
  SET_FLAG_FROM_TEST(sflag, gpf->runtime.onion_id != 0.0f, GP_EDIT_MULTIFRAME);

  for (int i = 0; i < v_len; i++) {
    vert_ptr->vflag = sflag | gpencil_point_edit_flag(layer_lock, &gps->points[i], i, v_len);
    vert_ptr->weight = gpencil_point_edit_weight(dvert, i, iter->vgindex);
    vert_ptr++;
  }
  /* Draw line to first point to complete the loop for cyclic strokes. */
  vert_ptr->vflag = sflag | gpencil_point_edit_flag(layer_lock, &gps->points[0], 0, v_len);
  vert_ptr->weight = gpencil_point_edit_weight(dvert, 0, iter->vgindex);
}

static void gpencil_edit_batches_ensure(Object *ob, GpencilBatchCache *cache, int cfra)
{
  bGPdata *gpd = (bGPdata *)ob->data;

  if (cache->edit_vbo == NULL) {
    /* TODO/PERF: Could be changed to only do it if needed.
     * For now it's simpler to assume we always need it
     * since multiple viewport could or could not need it.
     * Ideally we should have a dedicated onion skin geom batch. */
    /* IMPORTANT: Keep in sync with gpencil_batches_ensure() */
    bool do_onion = true;

    /* Vertex counting has already been done for cache->vbo. */
    BLI_assert(cache->vbo);
    int vert_len = cache->vbo->vertex_len;

    gpEditIterData iter;
    iter.vgindex = ob->actdef - 1;
    if (!BLI_findlink(&ob->defbase, iter.vgindex)) {
      iter.vgindex = -1;
    }

    /* Create VBO. */
    GPUVertFormat *format = gpencil_edit_stroke_format();
    cache->edit_vbo = GPU_vertbuf_create_with_format(format);
    /* Add extra space at the end of the buffer because of quad load. */
    GPU_vertbuf_data_alloc(cache->edit_vbo, vert_len);
    iter.verts = (gpEditVert *)cache->edit_vbo->data;

    /* Fill buffers with data. */
    BKE_gpencil_visible_stroke_iter(
        NULL, ob, NULL, gpencil_edit_stroke_iter_cb, &iter, do_onion, cfra);

    /* Create the batches */
    cache->edit_points_batch = GPU_batch_create(GPU_PRIM_POINTS, cache->vbo, NULL);
    GPU_batch_vertbuf_add(cache->edit_points_batch, cache->edit_vbo);

    cache->edit_lines_batch = GPU_batch_create(GPU_PRIM_LINE_STRIP, cache->vbo, NULL);
    GPU_batch_vertbuf_add(cache->edit_lines_batch, cache->edit_vbo);

    gpd->flag &= ~GP_DATA_CACHE_IS_DIRTY;
    cache->is_dirty = false;
  }
}

GPUBatch *DRW_cache_gpencil_edit_lines_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);
  gpencil_edit_batches_ensure(ob, cache, cfra);

  return cache->edit_lines_batch;
}

GPUBatch *DRW_cache_gpencil_edit_points_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);
  gpencil_edit_batches_ensure(ob, cache, cfra);

  return cache->edit_points_batch;
}

/** \} */
