/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Curve API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_color.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"

#include "BKE_curve.h"
#include "BKE_curves.hh"
#include "BKE_displist.h"
#include "BKE_geometry_set.hh"
#include "BKE_vfont.h"

#include "GPU_batch.h"
#include "GPU_capabilities.h"
#include "GPU_material.h"
#include "GPU_texture.h"

#include "UI_resources.h"

#include "DRW_render.h"

#include "draw_cache_inline.h"

#include "draw_cache_impl.h" /* own include */

using blender::Array;
using blender::ColorGeometry4f;
using blender::float3;
using blender::IndexRange;
using blender::OffsetIndices;
using blender::Span;

/* See: edit_curve_point_vert.glsl for duplicate includes. */
#define SELECT 1
#define ACTIVE_NURB (1 << 2)
#define BEZIER_HANDLE (1 << 3)
#define EVEN_U_BIT (1 << 4) /* Alternate this bit for every U vert. */
#define COLOR_SHIFT 5

/* Used as values of `color_id` in `edit_curve_overlay_handle_geom.glsl` */
enum {
  COLOR_NURB_ULINE_ID = TH_HANDLE_AUTOCLAMP - TH_HANDLE_FREE + 2,

  TOT_HANDLE_COL,
};

/**
 * TODO
 * - Ensure `CurveCache`, `SEQUENCER_DAG_WORKAROUND`.
 * - Check number of verts/edges to see if cache is valid.
 * - Check if 'overlay.edges' can use single attribute per edge, not 2 (for selection drawing).
 */

static void curve_batch_cache_clear(Curve *cu);

/* ---------------------------------------------------------------------- */
/* Curve Interface, direct access to basic data. */

static void curve_render_overlay_verts_edges_len_get(ListBase *lb,
                                                     int *r_vert_len,
                                                     int *r_edge_len)
{
  BLI_assert(r_vert_len || r_edge_len);
  int vert_len = 0;
  int edge_len = 0;
  LISTBASE_FOREACH (Nurb *, nu, lb) {
    if (nu->bezt) {
      vert_len += nu->pntsu * 3;
      /* 2x handles per point. */
      edge_len += 2 * nu->pntsu;
    }
    else if (nu->bp) {
      vert_len += nu->pntsu * nu->pntsv;
      /* segments between points */
      edge_len += (nu->pntsu - 1) * nu->pntsv;
      edge_len += (nu->pntsv - 1) * nu->pntsu;
    }
  }
  if (r_vert_len) {
    *r_vert_len = vert_len;
  }
  if (r_edge_len) {
    *r_edge_len = edge_len;
  }
}

static void curve_eval_render_wire_verts_edges_len_get(const blender::bke::CurvesGeometry &curves,
                                                       int *r_curve_len,
                                                       int *r_vert_len,
                                                       int *r_edge_len)
{
  const OffsetIndices points_by_curve = curves.evaluated_points_by_curve();
  const blender::VArray<bool> cyclic = curves.cyclic();

  *r_curve_len = curves.curves_num();
  *r_vert_len = points_by_curve.total_size();
  *r_edge_len = 0;
  for (const int i : curves.curves_range()) {
    *r_edge_len += blender::bke::curves::segments_num(points_by_curve[i].size(), cyclic[i]);
  }
}

static int curve_render_normal_len_get(const ListBase *lb, const CurveCache *ob_curve_cache)
{
  int normal_len = 0;
  const BevList *bl;
  const Nurb *nu;
  for (bl = (const BevList *)ob_curve_cache->bev.first, nu = (const Nurb *)lb->first; nu && bl;
       bl = bl->next, nu = nu->next)
  {
    int nr = bl->nr;
    int skip = nu->resolu / 16;
#if 0
    while (nr-- > 0) { /* accounts for empty bevel lists */
      normal_len += 1;
      nr -= skip;
    }
#else
    /* Same as loop above */
    normal_len += (nr / (skip + 1)) + ((nr % (skip + 1)) != 0);
#endif
  }
  return normal_len;
}

/* ---------------------------------------------------------------------- */
/* Curve Interface, indirect, partially cached access to complex data. */

struct CurveRenderData {
  int types;

  struct {
    int vert_len;
    int edge_len;
  } overlay;

  struct {
    int curve_len;
    int vert_len;
    int edge_len;
  } wire;

  /* edit mode normal's */
  struct {
    /* 'edge_len == len * 2'
     * 'vert_len == len * 3' */
    int len;
  } normal;

  struct {
    EditFont *edit_font;
  } text;

  /* borrow from 'Object' */
  CurveCache *ob_curve_cache;

  /* Owned by the evaluated object's geometry set (#geometry_set_eval). */
  const Curves *curve_eval;

  /* borrow from 'Curve' */
  ListBase *nurbs;

  /* edit, index in nurb list */
  int actnu;
  /* edit, index in active nurb (BPoint or BezTriple) */
  int actvert;
};

enum {
  /* Wire center-line */
  CU_DATATYPE_WIRE = 1 << 0,
  /* Edit-mode verts and optionally handles */
  CU_DATATYPE_OVERLAY = 1 << 1,
  /* Edit-mode normals */
  CU_DATATYPE_NORMAL = 1 << 2,
  /* Geometry */
  CU_DATATYPE_SURFACE = 1 << 3,
  /* Text */
  CU_DATATYPE_TEXT_SELECT = 1 << 4,
};

/**
 * \param ob_curve_cache: can be null.
 */
static CurveRenderData *curve_render_data_create(Curve *cu,
                                                 CurveCache *ob_curve_cache,
                                                 const int types)
{
  CurveRenderData *rdata = (CurveRenderData *)MEM_callocN(sizeof(*rdata), __func__);
  rdata->types = types;
  ListBase *nurbs;

  rdata->actnu = cu->actnu;
  rdata->actvert = cu->actvert;

  rdata->ob_curve_cache = ob_curve_cache;

  rdata->curve_eval = cu->curve_eval;

  if (types & CU_DATATYPE_WIRE) {
    if (rdata->curve_eval != nullptr) {
      curve_eval_render_wire_verts_edges_len_get(rdata->curve_eval->geometry.wrap(),
                                                 &rdata->wire.curve_len,
                                                 &rdata->wire.vert_len,
                                                 &rdata->wire.edge_len);
    }
  }

  if (cu->editnurb) {
    EditNurb *editnurb = cu->editnurb;
    nurbs = &editnurb->nurbs;

    if (types & CU_DATATYPE_OVERLAY) {
      curve_render_overlay_verts_edges_len_get(
          nurbs, &rdata->overlay.vert_len, &rdata->overlay.edge_len);

      rdata->actnu = cu->actnu;
      rdata->actvert = cu->actvert;
    }
    if (types & CU_DATATYPE_NORMAL) {
      rdata->normal.len = curve_render_normal_len_get(nurbs, rdata->ob_curve_cache);
    }
  }
  else {
    nurbs = &cu->nurb;
  }

  rdata->nurbs = nurbs;

  rdata->text.edit_font = cu->editfont;

  return rdata;
}

static void curve_render_data_free(CurveRenderData *rdata)
{
#if 0
  if (rdata->loose_verts) {
    MEM_freeN(rdata->loose_verts);
  }
#endif
  MEM_freeN(rdata);
}

static int curve_render_data_overlay_verts_len_get(const CurveRenderData *rdata)
{
  BLI_assert(rdata->types & CU_DATATYPE_OVERLAY);
  return rdata->overlay.vert_len;
}

static int curve_render_data_overlay_edges_len_get(const CurveRenderData *rdata)
{
  BLI_assert(rdata->types & CU_DATATYPE_OVERLAY);
  return rdata->overlay.edge_len;
}

static int curve_render_data_wire_verts_len_get(const CurveRenderData *rdata)
{
  BLI_assert(rdata->types & CU_DATATYPE_WIRE);
  return rdata->wire.vert_len;
}

static int curve_render_data_wire_edges_len_get(const CurveRenderData *rdata)
{
  BLI_assert(rdata->types & CU_DATATYPE_WIRE);
  return rdata->wire.edge_len;
}

static int curve_render_data_wire_curve_len_get(const CurveRenderData *rdata)
{
  BLI_assert(rdata->types & CU_DATATYPE_WIRE);
  return rdata->wire.curve_len;
}

static int curve_render_data_normal_len_get(const CurveRenderData *rdata)
{
  BLI_assert(rdata->types & CU_DATATYPE_NORMAL);
  return rdata->normal.len;
}

/* ---------------------------------------------------------------------- */
/* Curve GPUBatch Cache */

struct CurveBatchCache {
  struct {
    GPUVertBuf *curves_pos;
    GPUVertBuf *attr_viewer;
  } ordered;

  struct {
    GPUVertBuf *curves_nor;
    /* Edit points (beztriples and bpoints) */
    GPUVertBuf *pos;
    GPUVertBuf *data;
  } edit;

  struct {
    GPUIndexBuf *curves_lines;
    /* Edit mode */
    GPUIndexBuf *edit_verts;
    GPUIndexBuf *edit_lines;
  } ibo;

  struct {
    GPUBatch *curves;
    GPUBatch *curves_viewer_attribute;
    /* control handles and vertices */
    GPUBatch *edit_edges;
    GPUBatch *edit_verts;
    GPUBatch *edit_normals;
  } batch;

  /* settings to determine if cache is invalid */
  bool is_dirty;
  bool is_editmode;
};

/* GPUBatch cache management. */

static bool curve_batch_cache_valid(Curve *cu)
{
  CurveBatchCache *cache = (CurveBatchCache *)cu->batch_cache;

  if (cache == nullptr) {
    return false;
  }

  if (cache->is_dirty) {
    return false;
  }

  if (cache->is_editmode != ((cu->editnurb != nullptr) || (cu->editfont != nullptr))) {
    return false;
  }

  if (cache->is_editmode) {
    if (cu->editfont) {
      /* TODO */
    }
  }

  return true;
}

static void curve_batch_cache_init(Curve *cu)
{
  CurveBatchCache *cache = (CurveBatchCache *)cu->batch_cache;

  if (!cache) {
    cache = (CurveBatchCache *)MEM_callocN(sizeof(*cache), __func__);
    cu->batch_cache = cache;
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

#if 0
  ListBase *nurbs;
  if (cu->editnurb) {
    EditNurb *editnurb = cu->editnurb;
    nurbs = &editnurb->nurbs;
  }
  else {
    nurbs = &cu->nurb;
  }
#endif

  cache->is_editmode = (cu->editnurb != nullptr) || (cu->editfont != nullptr);

  cache->is_dirty = false;
}

void DRW_curve_batch_cache_validate(Curve *cu)
{
  if (!curve_batch_cache_valid(cu)) {
    curve_batch_cache_clear(cu);
    curve_batch_cache_init(cu);
  }
}

static CurveBatchCache *curve_batch_cache_get(Curve *cu)
{
  return (CurveBatchCache *)cu->batch_cache;
}

void DRW_curve_batch_cache_dirty_tag(Curve *cu, int mode)
{
  CurveBatchCache *cache = (CurveBatchCache *)cu->batch_cache;
  if (cache == nullptr) {
    return;
  }
  switch (mode) {
    case BKE_CURVE_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    case BKE_CURVE_BATCH_DIRTY_SELECT:
      GPU_VERTBUF_DISCARD_SAFE(cache->edit.data);

      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_edges);
      GPU_BATCH_DISCARD_SAFE(cache->batch.edit_verts);
      break;
    default:
      BLI_assert(0);
  }
}

static void curve_batch_cache_clear(Curve *cu)
{
  CurveBatchCache *cache = (CurveBatchCache *)cu->batch_cache;
  if (!cache) {
    return;
  }

  for (int i = 0; i < sizeof(cache->ordered) / sizeof(void *); i++) {
    GPUVertBuf **vbo = (GPUVertBuf **)&cache->ordered;
    GPU_VERTBUF_DISCARD_SAFE(vbo[i]);
  }
  for (int i = 0; i < sizeof(cache->edit) / sizeof(void *); i++) {
    GPUVertBuf **vbo = (GPUVertBuf **)&cache->edit;
    GPU_VERTBUF_DISCARD_SAFE(vbo[i]);
  }
  for (int i = 0; i < sizeof(cache->ibo) / sizeof(void *); i++) {
    GPUIndexBuf **ibo = (GPUIndexBuf **)&cache->ibo;
    GPU_INDEXBUF_DISCARD_SAFE(ibo[i]);
  }
  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); i++) {
    GPUBatch **batch = (GPUBatch **)&cache->batch;
    GPU_BATCH_DISCARD_SAFE(batch[i]);
  }
}

void DRW_curve_batch_cache_free(Curve *cu)
{
  curve_batch_cache_clear(cu);
  MEM_SAFE_FREE(cu->batch_cache);
}

/* -------------------------------------------------------------------- */
/** \name Private Curve Cache API
 * \{ */

/* GPUBatch cache usage. */
static void curve_create_curves_pos(CurveRenderData *rdata, GPUVertBuf *vbo_curves_pos)
{
  if (rdata->curve_eval == nullptr) {
    return;
  }

  static GPUVertFormat format = {0};
  static struct {
    uint pos;
  } attr_id;
  if (format.attr_len == 0) {
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  const int vert_len = curve_render_data_wire_verts_len_get(rdata);
  GPU_vertbuf_init_with_format(vbo_curves_pos, &format);
  GPU_vertbuf_data_alloc(vbo_curves_pos, vert_len);

  const blender::bke::CurvesGeometry &curves = rdata->curve_eval->geometry.wrap();
  const Span<float3> positions = curves.evaluated_positions();
  GPU_vertbuf_attr_fill(vbo_curves_pos, attr_id.pos, positions.data());
}

static void curve_create_attribute(CurveRenderData *rdata, GPUVertBuf *vbo_attr)
{
  using namespace blender;
  if (rdata->curve_eval == nullptr) {
    return;
  }

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "attribute_value", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  const int vert_len = curve_render_data_wire_verts_len_get(rdata);
  GPU_vertbuf_init_with_format(vbo_attr, &format);
  GPU_vertbuf_data_alloc(vbo_attr, vert_len);

  const bke::CurvesGeometry &curves = rdata->curve_eval->geometry.wrap();
  curves.ensure_can_interpolate_to_evaluated();
  const VArraySpan colors = *curves.attributes().lookup<ColorGeometry4f>(".viewer",
                                                                         ATTR_DOMAIN_POINT);
  ColorGeometry4f *vbo_data = static_cast<ColorGeometry4f *>(GPU_vertbuf_get_data(vbo_attr));
  curves.interpolate_to_evaluated(colors, MutableSpan<ColorGeometry4f>{vbo_data, vert_len});
}

static void curve_create_curves_lines(CurveRenderData *rdata, GPUIndexBuf *ibo_curve_lines)
{
  using namespace blender;
  if (rdata->curve_eval == nullptr) {
    return;
  }

  const int vert_len = curve_render_data_wire_verts_len_get(rdata);
  const int edge_len = curve_render_data_wire_edges_len_get(rdata);
  const int curve_len = curve_render_data_wire_curve_len_get(rdata);
  /* Count the last vertex or each strip and the primitive restart. */
  const int index_len = edge_len + curve_len * 2;

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, index_len, vert_len);

  const bke::CurvesGeometry &curves = rdata->curve_eval->geometry.wrap();
  const OffsetIndices points_by_curve = curves.evaluated_points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();

  for (const int i : curves.curves_range()) {
    const IndexRange points = points_by_curve[i];
    if (cyclic[i] && points.size() > 1) {
      GPU_indexbuf_add_generic_vert(&elb, points.last());
    }
    for (const int i_point : points) {
      GPU_indexbuf_add_generic_vert(&elb, i_point);
    }
    GPU_indexbuf_add_primitive_restart(&elb);
  }

  GPU_indexbuf_build_in_place(&elb, ibo_curve_lines);
}

static void curve_create_edit_curves_nor(CurveRenderData *rdata,
                                         GPUVertBuf *vbo_curves_nor,
                                         const Scene *scene)
{
  const bool do_hq_normals = (scene->r.perf_flag & SCE_PERF_HQ_NORMALS) != 0 ||
                             GPU_use_hq_normals_workaround();

  static GPUVertFormat format = {0};
  static GPUVertFormat format_hq = {0};
  static struct {
    uint pos, nor, tan, rad;
    uint pos_hq, nor_hq, tan_hq, rad_hq;
  } attr_id;
  if (format.attr_len == 0) {
    /* initialize vertex formats */
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.rad = GPU_vertformat_attr_add(&format, "rad", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    attr_id.nor = GPU_vertformat_attr_add(
        &format, "nor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    attr_id.tan = GPU_vertformat_attr_add(
        &format, "tan", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);

    attr_id.pos_hq = GPU_vertformat_attr_add(&format_hq, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.rad_hq = GPU_vertformat_attr_add(&format_hq, "rad", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    attr_id.nor_hq = GPU_vertformat_attr_add(
        &format_hq, "nor", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    attr_id.tan_hq = GPU_vertformat_attr_add(
        &format_hq, "tan", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  const GPUVertFormat *format_ptr = do_hq_normals ? &format_hq : &format;

  int verts_len_capacity = curve_render_data_normal_len_get(rdata) * 2;
  int vbo_len_used = 0;

  GPU_vertbuf_init_with_format(vbo_curves_nor, format_ptr);
  GPU_vertbuf_data_alloc(vbo_curves_nor, verts_len_capacity);

  const BevList *bl;
  const Nurb *nu;

  const uint pos_id = do_hq_normals ? attr_id.pos_hq : attr_id.pos;
  const uint nor_id = do_hq_normals ? attr_id.nor_hq : attr_id.nor;
  const uint tan_id = do_hq_normals ? attr_id.tan_hq : attr_id.tan;
  const uint rad_id = do_hq_normals ? attr_id.rad_hq : attr_id.rad;

  for (bl = (const BevList *)rdata->ob_curve_cache->bev.first,
      nu = (const Nurb *)rdata->nurbs->first;
       nu && bl;
       bl = bl->next, nu = nu->next)
  {
    const BevPoint *bevp = bl->bevpoints;
    int nr = bl->nr;
    int skip = nu->resolu / 16;

    while (nr-- > 0) { /* accounts for empty bevel lists */
      float nor[3] = {1.0f, 0.0f, 0.0f};
      mul_qt_v3(bevp->quat, nor);

      GPUNormal pnor;
      GPUNormal ptan;
      GPU_normal_convert_v3(&pnor, nor, do_hq_normals);
      GPU_normal_convert_v3(&ptan, bevp->dir, do_hq_normals);
      /* Only set attributes for one vertex. */
      GPU_vertbuf_attr_set(vbo_curves_nor, pos_id, vbo_len_used, bevp->vec);
      GPU_vertbuf_attr_set(vbo_curves_nor, rad_id, vbo_len_used, &bevp->radius);
      GPU_vertbuf_attr_set(vbo_curves_nor, nor_id, vbo_len_used, &pnor);
      GPU_vertbuf_attr_set(vbo_curves_nor, tan_id, vbo_len_used, &ptan);
      vbo_len_used++;

      /* Skip the other vertex (it does not need to be offsetted). */
      GPU_vertbuf_attr_set(vbo_curves_nor, attr_id.pos, vbo_len_used, bevp->vec);
      vbo_len_used++;

      bevp += skip + 1;
      nr -= skip;
    }
  }
  BLI_assert(vbo_len_used == verts_len_capacity);
}

static uint8_t beztriple_vflag_get(CurveRenderData *rdata,
                                   uint8_t flag,
                                   uint8_t col_id,
                                   int v_idx,
                                   int nu_id,
                                   bool handle_point,
                                   const bool handle_selected)
{
  uint8_t vflag = 0;
  SET_FLAG_FROM_TEST(vflag, (flag & SELECT), VFLAG_VERT_SELECTED);
  SET_FLAG_FROM_TEST(vflag, (v_idx == rdata->actvert && nu_id == rdata->actnu), VFLAG_VERT_ACTIVE);
  SET_FLAG_FROM_TEST(vflag, (nu_id == rdata->actnu), ACTIVE_NURB);
  SET_FLAG_FROM_TEST(vflag, handle_point, BEZIER_HANDLE);
  SET_FLAG_FROM_TEST(vflag, handle_selected, VFLAG_VERT_SELECTED_BEZT_HANDLE);
  /* Setting flags that overlap with will cause the color id not to work properly. */
  BLI_assert((vflag >> COLOR_SHIFT) == 0);
  /* handle color id */
  vflag |= col_id << COLOR_SHIFT;
  return vflag;
}

static uint8_t bpoint_vflag_get(CurveRenderData *rdata, uint8_t flag, int v_idx, int nu_id, int u)
{
  uint8_t vflag = 0;
  SET_FLAG_FROM_TEST(vflag, (flag & SELECT), VFLAG_VERT_SELECTED);
  SET_FLAG_FROM_TEST(vflag, (v_idx == rdata->actvert && nu_id == rdata->actnu), VFLAG_VERT_ACTIVE);
  SET_FLAG_FROM_TEST(vflag, (nu_id == rdata->actnu), ACTIVE_NURB);
  SET_FLAG_FROM_TEST(vflag, ((u % 2) == 0), EVEN_U_BIT);
  /* Setting flags that overlap with will cause the color id not to work properly. */
  BLI_assert((vflag >> COLOR_SHIFT) == 0);
  vflag |= COLOR_NURB_ULINE_ID << COLOR_SHIFT;
  return vflag;
}

static void curve_create_edit_data_and_handles(CurveRenderData *rdata,
                                               GPUVertBuf *vbo_pos,
                                               GPUVertBuf *vbo_data,
                                               GPUIndexBuf *ibo_edit_verts_points,
                                               GPUIndexBuf *ibo_edit_lines)
{
  static GPUVertFormat format_pos = {0};
  static GPUVertFormat format_data = {0};
  static struct {
    uint pos, data;
  } attr_id;
  if (format_pos.attr_len == 0) {
    /* initialize vertex formats */
    attr_id.pos = GPU_vertformat_attr_add(&format_pos, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.data = GPU_vertformat_attr_add(&format_data, "data", GPU_COMP_U8, 1, GPU_FETCH_INT);
  }

  int verts_len_capacity = curve_render_data_overlay_verts_len_get(rdata);
  int edges_len_capacity = curve_render_data_overlay_edges_len_get(rdata) * 2;
  int vbo_len_used = 0;

#define DRW_TEST_ASSIGN_VBO(v) (v = (DRW_vbo_requested(v) ? (v) : nullptr))
#define DRW_TEST_ASSIGN_IBO(v) (v = (DRW_ibo_requested(v) ? (v) : nullptr))

  if (DRW_TEST_ASSIGN_VBO(vbo_pos)) {
    GPU_vertbuf_init_with_format(vbo_pos, &format_pos);
    GPU_vertbuf_data_alloc(vbo_pos, verts_len_capacity);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_data)) {
    GPU_vertbuf_init_with_format(vbo_data, &format_data);
    GPU_vertbuf_data_alloc(vbo_data, verts_len_capacity);
  }

  GPUIndexBufBuilder elb_verts, *elbp_verts = nullptr;
  GPUIndexBufBuilder elb_lines, *elbp_lines = nullptr;
  if (DRW_TEST_ASSIGN_IBO(ibo_edit_verts_points)) {
    elbp_verts = &elb_verts;
    GPU_indexbuf_init(elbp_verts, GPU_PRIM_POINTS, verts_len_capacity, verts_len_capacity);
  }
  if (DRW_TEST_ASSIGN_IBO(ibo_edit_lines)) {
    elbp_lines = &elb_lines;
    GPU_indexbuf_init(elbp_lines, GPU_PRIM_LINES, edges_len_capacity, verts_len_capacity);
  }

#undef DRW_TEST_ASSIGN_VBO
#undef DRW_TEST_ASSIGN_IBO

  int nu_id = 0;
  for (Nurb *nu = (Nurb *)rdata->nurbs->first; nu; nu = nu->next, nu_id++) {
    const BezTriple *bezt = nu->bezt;
    const BPoint *bp = nu->bp;

    if (bezt) {
      for (int a = 0; a < nu->pntsu; a++, bezt++) {
        if (bezt->hide != 0) {
          continue;
        }
        const bool handle_selected = BEZT_ISSEL_ANY(bezt);

        if (elbp_verts) {
          GPU_indexbuf_add_point_vert(elbp_verts, vbo_len_used + 0);
          GPU_indexbuf_add_point_vert(elbp_verts, vbo_len_used + 1);
          GPU_indexbuf_add_point_vert(elbp_verts, vbo_len_used + 2);
        }
        if (elbp_lines) {
          GPU_indexbuf_add_line_verts(elbp_lines, vbo_len_used + 1, vbo_len_used + 0);
          GPU_indexbuf_add_line_verts(elbp_lines, vbo_len_used + 1, vbo_len_used + 2);
        }
        if (vbo_data) {
          const uint8_t vflag[3] = {
              beztriple_vflag_get(rdata, bezt->f1, bezt->h1, a, nu_id, true, handle_selected),
              beztriple_vflag_get(rdata, bezt->f2, bezt->h1, a, nu_id, false, handle_selected),
              beztriple_vflag_get(rdata, bezt->f3, bezt->h2, a, nu_id, true, handle_selected),
          };
          for (int j = 0; j < 3; j++) {
            GPU_vertbuf_attr_set(vbo_data, attr_id.data, vbo_len_used + j, &vflag[j]);
          }
        }
        if (vbo_pos) {
          for (int j = 0; j < 3; j++) {
            GPU_vertbuf_attr_set(vbo_pos, attr_id.pos, vbo_len_used + j, bezt->vec[j]);
          }
        }
        vbo_len_used += 3;
      }
    }
    else if (bp) {
      int pt_len = nu->pntsu * nu->pntsv;
      for (int a = 0; a < pt_len; a++, bp++, vbo_len_used += 1) {
        if (bp->hide != 0) {
          continue;
        }
        int u = (a % nu->pntsu);
        int v = (a / nu->pntsu);
        /* Use indexed rendering for bezier.
         * Specify all points and use indices to hide/show. */
        if (elbp_verts) {
          GPU_indexbuf_add_point_vert(elbp_verts, vbo_len_used);
        }
        if (elbp_lines) {
          const BPoint *bp_next_u = (u < (nu->pntsu - 1)) ? &nu->bp[a + 1] : nullptr;
          const BPoint *bp_next_v = (v < (nu->pntsv - 1)) ? &nu->bp[a + nu->pntsu] : nullptr;
          if (bp_next_u && (bp_next_u->hide == false)) {
            GPU_indexbuf_add_line_verts(elbp_lines, vbo_len_used, vbo_len_used + 1);
          }
          if (bp_next_v && (bp_next_v->hide == false)) {
            GPU_indexbuf_add_line_verts(elbp_lines, vbo_len_used, vbo_len_used + nu->pntsu);
          }
        }
        if (vbo_data) {
          uint8_t vflag = bpoint_vflag_get(rdata, bp->f1, a, nu_id, u);
          GPU_vertbuf_attr_set(vbo_data, attr_id.data, vbo_len_used, &vflag);
        }
        if (vbo_pos) {
          GPU_vertbuf_attr_set(vbo_pos, attr_id.pos, vbo_len_used, bp->vec);
        }
      }
    }
  }

  /* Resize & Finish */
  if (elbp_verts != nullptr) {
    GPU_indexbuf_build_in_place(elbp_verts, ibo_edit_verts_points);
  }
  if (elbp_lines != nullptr) {
    GPU_indexbuf_build_in_place(elbp_lines, ibo_edit_lines);
  }
  if (vbo_len_used != verts_len_capacity) {
    if (vbo_pos != nullptr) {
      GPU_vertbuf_data_resize(vbo_pos, vbo_len_used);
    }
    if (vbo_data != nullptr) {
      GPU_vertbuf_data_resize(vbo_data, vbo_len_used);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Object/Curve API
 * \{ */

GPUBatch *DRW_curve_batch_cache_get_wire_edge(Curve *cu)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);
  return DRW_batch_request(&cache->batch.curves);
}

GPUBatch *DRW_curve_batch_cache_get_wire_edge_viewer_attribute(Curve *cu)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);
  return DRW_batch_request(&cache->batch.curves_viewer_attribute);
}

GPUBatch *DRW_curve_batch_cache_get_normal_edge(Curve *cu)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);
  return DRW_batch_request(&cache->batch.edit_normals);
}

GPUBatch *DRW_curve_batch_cache_get_edit_edges(Curve *cu)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);
  return DRW_batch_request(&cache->batch.edit_edges);
}

GPUBatch *DRW_curve_batch_cache_get_edit_verts(Curve *cu)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);
  return DRW_batch_request(&cache->batch.edit_verts);
}

int DRW_curve_material_count_get(Curve *cu)
{
  return max_ii(1, cu->totcol);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grouped batch generation
 * \{ */

void DRW_curve_batch_cache_create_requested(Object *ob, const Scene *scene)
{
  BLI_assert(ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF, OB_FONT));

  Curve *cu = (Curve *)ob->data;
  CurveBatchCache *cache = curve_batch_cache_get(cu);

  /* Init batches and request VBOs & IBOs */
  if (DRW_batch_requested(cache->batch.curves, GPU_PRIM_LINE_STRIP)) {
    DRW_ibo_request(cache->batch.curves, &cache->ibo.curves_lines);
    DRW_vbo_request(cache->batch.curves, &cache->ordered.curves_pos);
  }
  if (DRW_batch_requested(cache->batch.curves_viewer_attribute, GPU_PRIM_LINE_STRIP)) {
    DRW_ibo_request(cache->batch.curves_viewer_attribute, &cache->ibo.curves_lines);
    DRW_vbo_request(cache->batch.curves_viewer_attribute, &cache->ordered.curves_pos);
    DRW_vbo_request(cache->batch.curves_viewer_attribute, &cache->ordered.attr_viewer);
  }

  /* Edit mode */
  if (DRW_batch_requested(cache->batch.edit_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.edit_edges, &cache->ibo.edit_lines);
    DRW_vbo_request(cache->batch.edit_edges, &cache->edit.pos);
    DRW_vbo_request(cache->batch.edit_edges, &cache->edit.data);
  }
  if (DRW_batch_requested(cache->batch.edit_verts, GPU_PRIM_POINTS)) {
    DRW_ibo_request(cache->batch.edit_verts, &cache->ibo.edit_verts);
    DRW_vbo_request(cache->batch.edit_verts, &cache->edit.pos);
    DRW_vbo_request(cache->batch.edit_verts, &cache->edit.data);
  }
  if (DRW_batch_requested(cache->batch.edit_normals, GPU_PRIM_LINES)) {
    DRW_vbo_request(cache->batch.edit_normals, &cache->edit.curves_nor);
  }

#ifdef DRW_DEBUG_MESH_CACHE_REQUEST
  printf("-- %s %s --\n", __func__, ob->id.name + 2);
#endif

  /* Generate MeshRenderData flags */
  int mr_flag = 0;
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.curves_pos, CU_DATATYPE_WIRE);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(
      mr_flag, cache->ordered.attr_viewer, CU_DATATYPE_WIRE | CU_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.curves_lines, CU_DATATYPE_WIRE);

  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->edit.pos, CU_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->edit.data, CU_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->edit.curves_nor, CU_DATATYPE_NORMAL);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.edit_verts, CU_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.edit_lines, CU_DATATYPE_OVERLAY);

#ifdef DRW_DEBUG_MESH_CACHE_REQUEST
  printf("  mr_flag %d\n\n", mr_flag);
#endif

  CurveRenderData *rdata = curve_render_data_create(cu, ob->runtime.curve_cache, mr_flag);

  /* Generate VBOs */
  if (DRW_vbo_requested(cache->ordered.curves_pos)) {
    curve_create_curves_pos(rdata, cache->ordered.curves_pos);
  }
  if (DRW_vbo_requested(cache->ordered.attr_viewer)) {
    curve_create_attribute(rdata, cache->ordered.attr_viewer);
  }
  if (DRW_ibo_requested(cache->ibo.curves_lines)) {
    curve_create_curves_lines(rdata, cache->ibo.curves_lines);
  }
  if (DRW_vbo_requested(cache->edit.pos) || DRW_vbo_requested(cache->edit.data) ||
      DRW_ibo_requested(cache->ibo.edit_verts) || DRW_ibo_requested(cache->ibo.edit_lines))
  {
    curve_create_edit_data_and_handles(
        rdata, cache->edit.pos, cache->edit.data, cache->ibo.edit_verts, cache->ibo.edit_lines);
  }
  if (DRW_vbo_requested(cache->edit.curves_nor)) {
    curve_create_edit_curves_nor(rdata, cache->edit.curves_nor, scene);
  }

  curve_render_data_free(rdata);

#ifdef DEBUG
  /* Make sure all requested batches have been setup. */
  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); i++) {
    BLI_assert(!DRW_batch_requested(((GPUBatch **)&cache->batch)[i], (GPUPrimType)0));
  }
#endif
}

/** \} */
