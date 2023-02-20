/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. */

/** \file
 * \ingroup draw
 */

#include "DNA_curve_types.h"
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
#include "BLI_math_vector_types.hh"
#include "BLI_polyfill_2d.h"

#include "draw_cache.h"
#include "draw_cache_impl.h"

#include "../engines/gpencil/gpencil_defines.h"
#include "../engines/gpencil/gpencil_shader_shared.h"

#define BEZIER_HANDLE (1 << 3)
#define COLOR_SHIFT 5

/* -------------------------------------------------------------------- */
/** \name Internal Types
 * \{ */

struct GpencilBatchCache {
  /** Instancing Data */
  GPUVertBuf *vbo;
  GPUVertBuf *vbo_col;
  /** Indices in material order, then stroke order with fill first.
   * Strokes can be individually rendered using `gps->runtime.stroke_start` and
   * `gps->runtime.fill_start`. */
  GPUIndexBuf *ibo;
  /** Batches */
  GPUBatch *geom_batch;
  /** Stroke lines only */
  GPUBatch *lines_batch;

  /** Edit Mode */
  GPUVertBuf *edit_vbo;
  GPUBatch *edit_lines_batch;
  GPUBatch *edit_points_batch;
  /** Edit Curve Mode */
  GPUVertBuf *edit_curve_vbo;
  GPUBatch *edit_curve_handles_batch;
  GPUBatch *edit_curve_points_batch;

  /** Cache is dirty */
  bool is_dirty;
  /** Last cache frame */
  int cache_frame;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static bool gpencil_batch_cache_valid(GpencilBatchCache *cache, bGPdata *gpd, int cfra)
{
  bool valid = true;

  if (cache == nullptr) {
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
    cache = gpd->runtime.gpencil_cache = (GpencilBatchCache *)MEM_callocN(sizeof(*cache),
                                                                          __func__);
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
  GPU_BATCH_DISCARD_SAFE(cache->geom_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->vbo);
  GPU_VERTBUF_DISCARD_SAFE(cache->vbo_col);
  GPU_INDEXBUF_DISCARD_SAFE(cache->ibo);

  GPU_BATCH_DISCARD_SAFE(cache->edit_lines_batch);
  GPU_BATCH_DISCARD_SAFE(cache->edit_points_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_vbo);

  GPU_BATCH_DISCARD_SAFE(cache->edit_curve_handles_batch);
  GPU_BATCH_DISCARD_SAFE(cache->edit_curve_points_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_curve_vbo);

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

  return cache;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BKE Callbacks
 * \{ */

void DRW_gpencil_batch_cache_dirty_tag(bGPdata *gpd)
{
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}

void DRW_gpencil_batch_cache_free(bGPdata *gpd)
{
  gpencil_batch_cache_clear(gpd->runtime.gpencil_cache);
  MEM_SAFE_FREE(gpd->runtime.gpencil_cache);
  gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Formats
 * \{ */

/* MUST match the format below. */
struct gpStrokeVert {
  /** Position and thickness packed in the same attribute. */
  float pos[3], thickness;
  /** Material Index, Stroke Index, Point Index, Packed aspect + hardness + rotation. */
  int32_t mat, stroke_id, point_id, packed_asp_hard_rot;
  /** UV and strength packed in the same attribute. */
  float uv_fill[2], u_stroke, strength;
};

static GPUVertFormat *gpencil_stroke_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "ma", GPU_COMP_I32, 4, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "uv", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  return &format;
}

/* MUST match the format below. */
struct gpEditVert {
  uint vflag;
  float weight;
};

static GPUVertFormat *gpencil_edit_stroke_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "vflag", GPU_COMP_U32, 1, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  return &format;
}

/* MUST match the format below. */
struct gpEditCurveVert {
  float pos[3];
  uint32_t data;
};

static GPUVertFormat *gpencil_edit_curve_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* initialize vertex formats */
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }
  return &format;
}

/* MUST match the format below. */
struct gpColorVert {
  float vcol[4]; /* Vertex color */
  float fcol[4]; /* Fill color */
};

static GPUVertFormat *gpencil_color_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "col", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "fcol", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  return &format;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Buffers
 * \{ */

struct gpIterData {
  bGPdata *gpd;
  gpStrokeVert *verts;
  gpColorVert *cols;
  GPUIndexBufBuilder ibo;
  int vert_len;
  int tri_len;
  int curve_len;
};

static GPUVertBuf *gpencil_dummy_buffer_get()
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
  packed |= int32_t(unit_float_to_uchar_clamp(asp_normalized));
  /* Store if inversed in the 9th bit. */
  if (asp > 1.0f) {
    packed |= 1 << 8;
  }
  /* Rotation uses 9 bits */
  /* Rotation are in [-90°..90°] range, so we can encode the sign of the angle + the cosine
   * because the cosine will always be positive. */
  packed |= int32_t(unit_float_to_uchar_clamp(cosf(rot))) << 9;
  /* Store sine sign in 9th bit. */
  if (rot < 0.0f) {
    packed |= 1 << 17;
  }
  /* Hardness uses 8 bits */
  packed |= int32_t(unit_float_to_uchar_clamp(hard)) << 18;
  return packed;
}

static void gpencil_buffer_add_point(GPUIndexBufBuilder *ibo,
                                     gpStrokeVert *verts,
                                     gpColorVert *cols,
                                     const bGPDstroke *gps,
                                     const bGPDspoint *pt,
                                     int v,
                                     bool is_endpoint)
{
  /* NOTE: we use the sign of strength and thickness to pass cap flag. */
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
  col->fcol[3] = (int(col->fcol[3] * 10000.0f) * 10.0f) + gps->fill_opacity_fac;

  vert->strength = (round_cap0) ? pt->strength : -pt->strength;
  vert->u_stroke = pt->uv_fac;
  vert->stroke_id = gps->runtime.vertex_start;
  vert->point_id = v;
  vert->thickness = max_ff(0.0f, gps->thickness * pt->pressure) * (round_cap1 ? 1.0f : -1.0f);
  /* Tag endpoint material to -1 so they get discarded by vertex shader. */
  vert->mat = (is_endpoint) ? -1 : (gps->mat_nr % GPENCIL_MATERIAL_BUFFER_LEN);

  float aspect_ratio = gps->aspect_ratio[0] / max_ff(gps->aspect_ratio[1], 1e-8);

  vert->packed_asp_hard_rot = pack_rotation_aspect_hardness(
      pt->uv_rot, aspect_ratio, gps->hardeness);

  if (!is_endpoint) {
    /* Issue a Quad per point. */
    /* The attribute loading uses a different shader and will undo this bit packing. */
    int v_mat = (v << GP_VERTEX_ID_SHIFT) | GP_IS_STROKE_VERTEX_BIT;
    GPU_indexbuf_add_tri_verts(ibo, v_mat + 0, v_mat + 1, v_mat + 2);
    GPU_indexbuf_add_tri_verts(ibo, v_mat + 2, v_mat + 1, v_mat + 3);
  }
}

static void gpencil_buffer_add_stroke(GPUIndexBufBuilder *ibo,
                                      gpStrokeVert *verts,
                                      gpColorVert *cols,
                                      const bGPDstroke *gps)
{
  const bGPDspoint *pts = gps->points;
  int pts_len = gps->totpoints;
  bool is_cyclic = gpencil_stroke_is_cyclic(gps);
  int v = gps->runtime.vertex_start;

  /* First point for adjacency (not drawn). */
  int adj_idx = (is_cyclic) ? (pts_len - 1) : min_ii(pts_len - 1, 1);
  gpencil_buffer_add_point(ibo, verts, cols, gps, &pts[adj_idx], v++, true);

  for (int i = 0; i < pts_len; i++) {
    gpencil_buffer_add_point(ibo, verts, cols, gps, &pts[i], v++, false);
  }
  /* Draw line to first point to complete the loop for cyclic strokes. */
  if (is_cyclic) {
    gpencil_buffer_add_point(ibo, verts, cols, gps, &pts[0], v, false);
    /* UV factor needs to be adjusted for the last point to not be equal to the UV factor of the
     * first point. It should be the factor of the last point plus the distance from the last point
     * to the first.
     */
    gpStrokeVert *vert = &verts[v];
    vert->u_stroke = verts[v - 1].u_stroke + len_v3v3(&pts[pts_len - 1].x, &pts[0].x);
    v++;
  }
  /* Last adjacency point (not drawn). */
  adj_idx = (is_cyclic) ? 1 : max_ii(0, pts_len - 2);
  gpencil_buffer_add_point(ibo, verts, cols, gps, &pts[adj_idx], v++, true);
}

static void gpencil_buffer_add_fill(GPUIndexBufBuilder *ibo, const bGPDstroke *gps)
{
  int tri_len = gps->tot_triangles;
  int v = gps->runtime.vertex_start + 1;
  for (int i = 0; i < tri_len; i++) {
    uint *tri = gps->triangles[i].verts;
    /* The attribute loading uses a different shader and will undo this bit packing. */
    GPU_indexbuf_add_tri_verts(ibo,
                               (v + tri[0]) << GP_VERTEX_ID_SHIFT,
                               (v + tri[1]) << GP_VERTEX_ID_SHIFT,
                               (v + tri[2]) << GP_VERTEX_ID_SHIFT);
  }
}

static void gpencil_stroke_iter_cb(bGPDlayer * /*gpl*/,
                                   bGPDframe * /*gpf*/,
                                   bGPDstroke *gps,
                                   void *thunk)
{
  gpIterData *iter = (gpIterData *)thunk;
  if (gps->tot_triangles > 0) {
    gpencil_buffer_add_fill(&iter->ibo, gps);
  }
  gpencil_buffer_add_stroke(&iter->ibo, iter->verts, iter->cols, gps);
}

static void gpencil_object_verts_count_cb(bGPDlayer * /*gpl*/,
                                          bGPDframe * /*gpf*/,
                                          bGPDstroke *gps,
                                          void *thunk)
{
  gpIterData *iter = (gpIterData *)thunk;
  int stroke_vert_len = gps->totpoints + gpencil_stroke_is_cyclic(gps);
  gps->runtime.vertex_start = iter->vert_len;
  /* Add additional padding at the start and end. */
  iter->vert_len += 1 + stroke_vert_len + 1;
  /* Store first index offset. */
  gps->runtime.fill_start = iter->tri_len;
  iter->tri_len += gps->tot_triangles;
  gps->runtime.stroke_start = iter->tri_len;
  iter->tri_len += stroke_vert_len * 2;
}

static void gpencil_batches_ensure(Object *ob, GpencilBatchCache *cache, int cfra)
{
  bGPdata *gpd = (bGPdata *)ob->data;

  if (cache->vbo == nullptr) {
    /* Should be discarded together. */
    BLI_assert(cache->vbo == nullptr && cache->ibo == nullptr);
    BLI_assert(cache->geom_batch == nullptr);
    /* TODO/PERF: Could be changed to only do it if needed.
     * For now it's simpler to assume we always need it
     * since multiple viewport could or could not need it.
     * Ideally we should have a dedicated onion skin geom batch. */
    /* IMPORTANT: Keep in sync with gpencil_edit_batches_ensure() */
    bool do_onion = true;

    /* First count how many vertices and triangles are needed for the whole object. */
    gpIterData iter = {};
    iter.gpd = gpd;
    iter.verts = nullptr;
    iter.ibo = {0};
    iter.vert_len = 0;
    iter.tri_len = 0;
    iter.curve_len = 0;
    BKE_gpencil_visible_stroke_advanced_iter(
        nullptr, ob, nullptr, gpencil_object_verts_count_cb, &iter, do_onion, cfra);

    GPUUsageType vbo_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
    /* Create VBOs. */
    GPUVertFormat *format = gpencil_stroke_format();
    GPUVertFormat *format_col = gpencil_color_format();
    cache->vbo = GPU_vertbuf_create_with_format_ex(format, vbo_flag);
    cache->vbo_col = GPU_vertbuf_create_with_format_ex(format_col, vbo_flag);
    /* Add extra space at the end of the buffer because of quad load. */
    GPU_vertbuf_data_alloc(cache->vbo, iter.vert_len + 2);
    GPU_vertbuf_data_alloc(cache->vbo_col, iter.vert_len + 2);
    iter.verts = (gpStrokeVert *)GPU_vertbuf_get_data(cache->vbo);
    iter.cols = (gpColorVert *)GPU_vertbuf_get_data(cache->vbo_col);
    /* Create IBO. */
    GPU_indexbuf_init(&iter.ibo, GPU_PRIM_TRIS, iter.tri_len, 0xFFFFFFFFu);

    /* Fill buffers with data. */
    BKE_gpencil_visible_stroke_advanced_iter(
        nullptr, ob, nullptr, gpencil_stroke_iter_cb, &iter, do_onion, cfra);

    /* Mark last 2 verts as invalid. */
    for (int i = 0; i < 2; i++) {
      iter.verts[iter.vert_len + i].mat = -1;
    }
    /* Also mark first vert as invalid. */
    iter.verts[0].mat = -1;

    /* Finish the IBO. */
    cache->ibo = GPU_indexbuf_build(&iter.ibo);
    /* Create the batches */
    cache->geom_batch = GPU_batch_create(GPU_PRIM_TRIS, cache->vbo, cache->ibo);
    /* Allow creation of buffer texture. */
    GPU_vertbuf_use(cache->vbo);
    GPU_vertbuf_use(cache->vbo_col);

    gpd->flag &= ~GP_DATA_CACHE_IS_DIRTY;
    cache->is_dirty = false;
  }
}

GPUBatch *DRW_cache_gpencil_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);

  return cache->geom_batch;
}

GPUVertBuf *DRW_cache_gpencil_position_buffer_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);

  return cache->vbo;
}

GPUVertBuf *DRW_cache_gpencil_color_buffer_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);

  return cache->vbo_col;
}

static void gpencil_lines_indices_cb(bGPDlayer * /*gpl*/,
                                     bGPDframe * /*gpf*/,
                                     bGPDstroke *gps,
                                     void *thunk)
{
  gpIterData *iter = (gpIterData *)thunk;
  int pts_len = gps->totpoints + gpencil_stroke_is_cyclic(gps);

  int start = gps->runtime.vertex_start + 1;
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

  if (cache->lines_batch == nullptr) {
    GPUVertBuf *vbo = cache->vbo;

    gpIterData iter = {};
    iter.gpd = (bGPdata *)ob->data;
    iter.ibo = {0};

    uint vert_len = GPU_vertbuf_get_vertex_len(vbo);
    GPU_indexbuf_init_ex(&iter.ibo, GPU_PRIM_LINE_STRIP, vert_len, vert_len);

    /* IMPORTANT: Keep in sync with gpencil_edit_batches_ensure() */
    bool do_onion = true;
    BKE_gpencil_visible_stroke_advanced_iter(
        nullptr, ob, nullptr, gpencil_lines_indices_cb, &iter, do_onion, cfra);

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
  if (gpd->runtime.sbuffer_gps == nullptr) {
    bGPDstroke *gps = (bGPDstroke *)MEM_callocN(sizeof(*gps), "bGPDstroke sbuffer");
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
    gps->runtime.vertex_start = 0;
    gps->runtime.fill_start = 0;
    gps->runtime.stroke_start = 0;
    copy_v4_v4(gps->vert_color_fill, gpd->runtime.vert_color_fill);
    /* Caps. */
    gps->caps[0] = gps->caps[1] = short(brush->gpencil_settings->caps_type);

    gpd->runtime.sbuffer_gps = gps;
  }
  return gpd->runtime.sbuffer_gps;
}

static void gpencil_sbuffer_stroke_ensure(bGPdata *gpd, bool do_fill)
{
  tGPspoint *tpoints = (tGPspoint *)gpd->runtime.sbuffer;
  bGPDstroke *gps = gpd->runtime.sbuffer_gps;
  int vert_len = gpd->runtime.sbuffer_used;

  /* DRW_cache_gpencil_sbuffer_stroke_data_get need to have been called previously. */
  BLI_assert(gps != nullptr);

  if (gpd->runtime.sbuffer_batch == nullptr) {
    gps->points = (bGPDspoint *)MEM_mallocN(vert_len * sizeof(*gps->points), __func__);

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
      mul_m4_v3(ob->world_to_object, &gps->points[i].x);
      bGPDspoint *pt = &gps->points[i];
      copy_v4_v4(pt->vert_color, tpoints[i].vert_color);
    }
    /* Calc uv data along the stroke. */
    BKE_gpencil_stroke_uv_update(gps);

    int tri_len = gps->tot_triangles + (gps->totpoints + gpencil_stroke_is_cyclic(gps)) * 2;
    /* Create IBO. */
    GPUIndexBufBuilder ibo_builder;
    GPU_indexbuf_init(&ibo_builder, GPU_PRIM_TRIS, tri_len, 0xFFFFFFFFu);
    /* Create VBO. */
    GPUUsageType vbo_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
    GPUVertFormat *format = gpencil_stroke_format();
    GPUVertFormat *format_color = gpencil_color_format();
    GPUVertBuf *vbo = GPU_vertbuf_create_with_format_ex(format, vbo_flag);
    GPUVertBuf *vbo_col = GPU_vertbuf_create_with_format_ex(format_color, vbo_flag);
    /* Add extra space at the start and end the buffer because of quad load and cyclic. */
    GPU_vertbuf_data_alloc(vbo, 1 + vert_len + 1 + 2);
    GPU_vertbuf_data_alloc(vbo_col, 1 + vert_len + 1 + 2);
    gpStrokeVert *verts = (gpStrokeVert *)GPU_vertbuf_get_data(vbo);
    gpColorVert *cols = (gpColorVert *)GPU_vertbuf_get_data(vbo_col);

    /* Create fill indices. */
    if (do_fill && gps->tot_triangles > 0) {
      float(*tpoints2d)[2] = (float(*)[2])MEM_mallocN(sizeof(*tpoints2d) * vert_len, __func__);
      /* Triangulate in 2D. */
      for (int i = 0; i < vert_len; i++) {
        copy_v2_v2(tpoints2d[i], tpoints[i].m_xy);
      }
      /* Compute directly inside the IBO data buffer. */
      /* OPTI: This is a bottleneck if the stroke is very long. */
      BLI_polyfill_calc(tpoints2d, uint(vert_len), 0, (uint(*)[3])ibo_builder.data);
      /* Add stroke start offset and shift. */
      for (int i = 0; i < gps->tot_triangles * 3; i++) {
        ibo_builder.data[i] = (ibo_builder.data[i] + 1) << GP_VERTEX_ID_SHIFT;
      }
      /* HACK since we didn't use the builder API to avoid another malloc and copy,
       * we need to set the number of indices manually. */
      ibo_builder.index_len = gps->tot_triangles * 3;
      ibo_builder.index_min = 0;
      /* For this case, do not allow index compaction to avoid yet another preprocessing step. */
      ibo_builder.index_max = 0xFFFFFFFFu - 1u;

      gps->runtime.stroke_start = gps->tot_triangles;

      MEM_freeN(tpoints2d);
    }

    /* Fill buffers with data. */
    gpencil_buffer_add_stroke(&ibo_builder, verts, cols, gps);

    GPUBatch *batch = GPU_batch_create_ex(GPU_PRIM_TRIS,
                                          gpencil_dummy_buffer_get(),
                                          GPU_indexbuf_build(&ibo_builder),
                                          GPU_BATCH_OWNS_INDEX);

    gpd->runtime.sbuffer_position_buf = vbo;
    gpd->runtime.sbuffer_color_buf = vbo_col;
    gpd->runtime.sbuffer_batch = batch;

    MEM_freeN(gps->points);
  }
}

GPUBatch *DRW_cache_gpencil_sbuffer_get(Object *ob, bool show_fill)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  /* Fill batch also need stroke batch to be created (vbo is shared). */
  gpencil_sbuffer_stroke_ensure(gpd, show_fill);

  return gpd->runtime.sbuffer_batch;
}

GPUVertBuf *DRW_cache_gpencil_sbuffer_position_buffer_get(Object *ob, bool show_fill)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  /* Fill batch also need stroke batch to be created (vbo is shared). */
  gpencil_sbuffer_stroke_ensure(gpd, show_fill);

  return gpd->runtime.sbuffer_position_buf;
}

GPUVertBuf *DRW_cache_gpencil_sbuffer_color_buffer_get(Object *ob, bool show_fill)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  /* Fill batch also need stroke batch to be created (vbo is shared). */
  gpencil_sbuffer_stroke_ensure(gpd, show_fill);

  return gpd->runtime.sbuffer_color_buf;
}

void DRW_cache_gpencil_sbuffer_clear(Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  MEM_SAFE_FREE(gpd->runtime.sbuffer_gps);
  GPU_BATCH_DISCARD_SAFE(gpd->runtime.sbuffer_batch);
  GPU_VERTBUF_DISCARD_SAFE(gpd->runtime.sbuffer_position_buf);
  GPU_VERTBUF_DISCARD_SAFE(gpd->runtime.sbuffer_color_buf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit GPencil Batches
 * \{ */

#define GP_EDIT_POINT_SELECTED (1 << 0)
#define GP_EDIT_STROKE_SELECTED (1 << 1)
#define GP_EDIT_MULTIFRAME (1 << 2)
#define GP_EDIT_STROKE_START (1 << 3)
#define GP_EDIT_STROKE_END (1 << 4)
#define GP_EDIT_POINT_DIMMED (1 << 5)

struct gpEditIterData {
  gpEditVert *verts;
  int vgindex;
};

struct gpEditCurveIterData {
  gpEditCurveVert *verts;
  int vgindex;
};

static uint32_t gpencil_point_edit_flag(const bool layer_lock,
                                        const bGPDspoint *pt,
                                        int v,
                                        int v_len)
{
  uint32_t sflag = 0;
  SET_FLAG_FROM_TEST(sflag, (!layer_lock) && pt->flag & GP_SPOINT_SELECT, GP_EDIT_POINT_SELECTED);
  SET_FLAG_FROM_TEST(sflag, v == 0, GP_EDIT_STROKE_START);
  SET_FLAG_FROM_TEST(sflag, v == (v_len - 1), GP_EDIT_STROKE_END);
  SET_FLAG_FROM_TEST(sflag, pt->runtime.pt_orig == nullptr, GP_EDIT_POINT_DIMMED);
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
  const int v = gps->runtime.vertex_start + 1;
  MDeformVert *dvert = ((iter->vgindex > -1) && gps->dvert) ? gps->dvert : nullptr;
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

  if (gpencil_stroke_is_cyclic(gps)) {
    /* Draw line to first point to complete the loop for cyclic strokes. */
    vert_ptr->vflag = sflag | gpencil_point_edit_flag(layer_lock, &gps->points[0], 0, v_len);
    vert_ptr->weight = gpencil_point_edit_weight(dvert, 0, iter->vgindex);
  }
}

static void gpencil_edit_curve_stroke_count_cb(bGPDlayer *gpl,
                                               bGPDframe * /*gpf*/,
                                               bGPDstroke *gps,
                                               void *thunk)
{
  if (gpl->flag & GP_LAYER_LOCKED) {
    return;
  }

  gpIterData *iter = (gpIterData *)thunk;

  if (gps->editcurve == nullptr) {
    return;
  }

  /* Store first index offset */
  gps->runtime.curve_start = iter->curve_len;
  iter->curve_len += gps->editcurve->tot_curve_points * 4;
}

static uint32_t gpencil_beztriple_vflag_get(char flag,
                                            char col_id,
                                            bool handle_point,
                                            const bool handle_selected)
{
  uint32_t vflag = 0;
  SET_FLAG_FROM_TEST(vflag, (flag & SELECT), VFLAG_VERT_SELECTED);
  SET_FLAG_FROM_TEST(vflag, handle_point, BEZIER_HANDLE);
  SET_FLAG_FROM_TEST(vflag, handle_selected, VFLAG_VERT_SELECTED_BEZT_HANDLE);
  vflag |= VFLAG_VERT_GPENCIL_BEZT_HANDLE;

  /* Handle color id. */
  vflag |= col_id << COLOR_SHIFT;
  return vflag;
}

static void gpencil_edit_curve_stroke_iter_cb(bGPDlayer *gpl,
                                              bGPDframe * /*gpf*/,
                                              bGPDstroke *gps,
                                              void *thunk)
{
  if (gpl->flag & GP_LAYER_LOCKED) {
    return;
  }

  if (gps->editcurve == nullptr) {
    return;
  }
  bGPDcurve *editcurve = gps->editcurve;
  gpEditCurveIterData *iter = (gpEditCurveIterData *)thunk;
  const int v = gps->runtime.curve_start;
  gpEditCurveVert *vert_ptr = iter->verts + v;
  /* Hide points when the curve is unselected. Passing the control point
   * as handle produces the point shader skip it if you are not in ALL mode. */
  const bool hide = !(editcurve->flag & GP_CURVE_SELECT);

  for (int i = 0; i < editcurve->tot_curve_points; i++) {
    BezTriple *bezt = &editcurve->curve_points[i].bezt;
    const bool handle_selected = BEZT_ISSEL_ANY(bezt);
    const uint32_t vflag[3] = {
        gpencil_beztriple_vflag_get(bezt->f1, bezt->h1, true, handle_selected),
        gpencil_beztriple_vflag_get(bezt->f2, bezt->h1, hide, handle_selected),
        gpencil_beztriple_vflag_get(bezt->f3, bezt->h2, true, handle_selected),
    };

    /* First segment. */
    mul_v3_m4v3(vert_ptr->pos, gpl->layer_mat, bezt->vec[0]);
    vert_ptr->data = vflag[0];
    vert_ptr++;

    mul_v3_m4v3(vert_ptr->pos, gpl->layer_mat, bezt->vec[1]);
    vert_ptr->data = vflag[1];
    vert_ptr++;

    /* Second segment. */
    mul_v3_m4v3(vert_ptr->pos, gpl->layer_mat, bezt->vec[1]);
    vert_ptr->data = vflag[1];
    vert_ptr++;

    mul_v3_m4v3(vert_ptr->pos, gpl->layer_mat, bezt->vec[2]);
    vert_ptr->data = vflag[2];
    vert_ptr++;
  }
}

static void gpencil_edit_batches_ensure(Object *ob, GpencilBatchCache *cache, int cfra)
{
  bGPdata *gpd = (bGPdata *)ob->data;

  if (cache->edit_vbo == nullptr) {
    /* TODO/PERF: Could be changed to only do it if needed.
     * For now it's simpler to assume we always need it
     * since multiple viewport could or could not need it.
     * Ideally we should have a dedicated onion skin geom batch. */
    /* IMPORTANT: Keep in sync with gpencil_batches_ensure() */
    bool do_onion = true;

    /* Vertex counting has already been done for cache->vbo. */
    BLI_assert(cache->vbo);
    int vert_len = GPU_vertbuf_get_vertex_len(cache->vbo);

    gpEditIterData iter;
    iter.vgindex = gpd->vertex_group_active_index - 1;
    if (!BLI_findlink(&gpd->vertex_group_names, iter.vgindex)) {
      iter.vgindex = -1;
    }

    /* Create VBO. */
    GPUVertFormat *format = gpencil_edit_stroke_format();
    cache->edit_vbo = GPU_vertbuf_create_with_format(format);
    /* Add extra space at the end of the buffer because of quad load. */
    GPU_vertbuf_data_alloc(cache->edit_vbo, vert_len);
    iter.verts = (gpEditVert *)GPU_vertbuf_get_data(cache->edit_vbo);

    /* Fill buffers with data. */
    BKE_gpencil_visible_stroke_advanced_iter(
        nullptr, ob, nullptr, gpencil_edit_stroke_iter_cb, &iter, do_onion, cfra);

    /* Create the batches */
    cache->edit_points_batch = GPU_batch_create(GPU_PRIM_POINTS, cache->vbo, nullptr);
    GPU_batch_vertbuf_add(cache->edit_points_batch, cache->edit_vbo, false);

    cache->edit_lines_batch = GPU_batch_create(GPU_PRIM_LINE_STRIP, cache->vbo, nullptr);
    GPU_batch_vertbuf_add(cache->edit_lines_batch, cache->edit_vbo, false);
  }

  /* Curve Handles and Points for Editing. */
  if (cache->edit_curve_vbo == nullptr) {
    gpIterData iterdata = {};
    iterdata.gpd = gpd;
    iterdata.verts = nullptr;
    iterdata.ibo = {0};
    iterdata.vert_len = 0;
    iterdata.tri_len = 0;
    iterdata.curve_len = 0;

    /* Create VBO. */
    GPUVertFormat *format = gpencil_edit_curve_format();
    cache->edit_curve_vbo = GPU_vertbuf_create_with_format(format);

    /* Count data. */
    BKE_gpencil_visible_stroke_advanced_iter(
        nullptr, ob, nullptr, gpencil_edit_curve_stroke_count_cb, &iterdata, false, cfra);

    gpEditCurveIterData iter;
    int vert_len = iterdata.curve_len;
    if (vert_len > 0) {

      GPU_vertbuf_data_alloc(cache->edit_curve_vbo, vert_len);
      iter.verts = (gpEditCurveVert *)GPU_vertbuf_get_data(cache->edit_curve_vbo);

      /* Fill buffers with data. */
      BKE_gpencil_visible_stroke_advanced_iter(
          nullptr, ob, nullptr, gpencil_edit_curve_stroke_iter_cb, &iter, false, cfra);

      cache->edit_curve_handles_batch = GPU_batch_create(
          GPU_PRIM_LINES, cache->edit_curve_vbo, nullptr);
      GPU_batch_vertbuf_add(cache->edit_curve_handles_batch, cache->edit_curve_vbo, false);

      cache->edit_curve_points_batch = GPU_batch_create(
          GPU_PRIM_POINTS, cache->edit_curve_vbo, nullptr);
      GPU_batch_vertbuf_add(cache->edit_curve_points_batch, cache->edit_curve_vbo, false);
    }

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

GPUBatch *DRW_cache_gpencil_edit_curve_handles_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);
  gpencil_edit_batches_ensure(ob, cache, cfra);

  return cache->edit_curve_handles_batch;
}

GPUBatch *DRW_cache_gpencil_edit_curve_points_get(Object *ob, int cfra)
{
  GpencilBatchCache *cache = gpencil_batch_cache_get(ob, cfra);
  gpencil_batches_ensure(ob, cache, cfra);
  gpencil_edit_batches_ensure(ob, cache, cfra);

  return cache->edit_curve_points_batch;
}

int DRW_gpencil_material_count_get(bGPdata *gpd)
{
  return max_ii(1, gpd->totcol);
}

/** \} */
