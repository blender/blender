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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Curve API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"

#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_font.h"

#include "GPU_batch.h"
#include "GPU_material.h"
#include "GPU_texture.h"

#include "UI_resources.h"

#include "DRW_render.h"

#include "draw_cache_inline.h"

#include "draw_cache_impl.h" /* own include */

/* See: edit_curve_point_vert.glsl for duplicate includes. */
#define SELECT 1
#define ACTIVE_NURB 1 << 2
#define BEZIER_HANDLE 1 << 3
#define EVEN_U_BIT 1 << 4 /* Alternate this bit for every U vert. */
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
      /* 2x handles per point*/
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

static void curve_render_wire_verts_edges_len_get(const CurveCache *ob_curve_cache,
                                                  int *r_curve_len,
                                                  int *r_vert_len,
                                                  int *r_edge_len)
{
  BLI_assert(r_vert_len || r_edge_len);
  int vert_len = 0;
  int edge_len = 0;
  int curve_len = 0;
  LISTBASE_FOREACH (const BevList *, bl, &ob_curve_cache->bev) {
    if (bl->nr > 0) {
      const bool is_cyclic = bl->poly != -1;
      edge_len += (is_cyclic) ? bl->nr : bl->nr - 1;
      vert_len += bl->nr;
      curve_len += 1;
    }
  }
  LISTBASE_FOREACH (const DispList *, dl, &ob_curve_cache->disp) {
    if (ELEM(dl->type, DL_SEGM, DL_POLY)) {
      BLI_assert(dl->parts == 1);
      const bool is_cyclic = dl->type == DL_POLY;
      edge_len += (is_cyclic) ? dl->nr : dl->nr - 1;
      vert_len += dl->nr;
      curve_len += 1;
    }
  }
  if (r_vert_len) {
    *r_vert_len = vert_len;
  }
  if (r_edge_len) {
    *r_edge_len = edge_len;
  }
  if (r_curve_len) {
    *r_curve_len = curve_len;
  }
}

static int curve_render_normal_len_get(const ListBase *lb, const CurveCache *ob_curve_cache)
{
  int normal_len = 0;
  const BevList *bl;
  const Nurb *nu;
  for (bl = ob_curve_cache->bev.first, nu = lb->first; nu && bl; bl = bl->next, nu = nu->next) {
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

typedef struct CurveRenderData {
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

  /* borrow from 'Curve' */
  ListBase *nurbs;

  /* edit, index in nurb list */
  int actnu;
  /* edit, index in active nurb (BPoint or BezTriple) */
  int actvert;
} CurveRenderData;

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

/*
 * ob_curve_cache can be NULL, only needed for CU_DATATYPE_WIRE
 */
static CurveRenderData *curve_render_data_create(Curve *cu,
                                                 CurveCache *ob_curve_cache,
                                                 const int types)
{
  CurveRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
  rdata->types = types;
  ListBase *nurbs;

  rdata->actnu = cu->actnu;
  rdata->actvert = cu->actvert;

  rdata->ob_curve_cache = ob_curve_cache;

  if (types & CU_DATATYPE_WIRE) {
    curve_render_wire_verts_edges_len_get(rdata->ob_curve_cache,
                                          &rdata->wire.curve_len,
                                          &rdata->wire.vert_len,
                                          &rdata->wire.edge_len);
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

static void curve_cd_calc_used_gpu_layers(int *cd_layers,
                                          struct GPUMaterial **gpumat_array,
                                          int gpumat_array_len)
{
  for (int i = 0; i < gpumat_array_len; i++) {
    struct GPUMaterial *gpumat = gpumat_array[i];
    if (gpumat == NULL) {
      continue;
    }

    ListBase gpu_attrs = GPU_material_attributes(gpumat);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      const char *name = gpu_attr->name;
      int type = gpu_attr->type;

      /* Curves cannot have named layers.
       * Note: We could relax this assumption later. */
      if (name[0] != '\0') {
        continue;
      }

      if (type == CD_AUTO_FROM_NAME) {
        type = CD_MTFACE;
      }

      switch (type) {
        case CD_MTFACE:
          *cd_layers |= CD_MLOOPUV;
          break;
        case CD_TANGENT:
          *cd_layers |= CD_TANGENT;
          break;
        case CD_MCOL:
          /* Curve object don't have Color data. */
          break;
        case CD_ORCO:
          *cd_layers |= CD_ORCO;
          break;
      }
    }
  }
}

/* ---------------------------------------------------------------------- */
/* Curve GPUBatch Cache */

typedef struct CurveBatchCache {
  struct {
    GPUVertBuf *pos_nor;
    GPUVertBuf *edge_fac;
    GPUVertBuf *curves_pos;

    GPUVertBuf *loop_pos_nor;
    GPUVertBuf *loop_uv;
    GPUVertBuf *loop_tan;
  } ordered;

  struct {
    /* Curve points. Aligned with ordered.pos_nor */
    GPUVertBuf *curves_nor;
    GPUVertBuf *curves_weight; /* TODO. */
    /* Edit points (beztriples and bpoints) */
    GPUVertBuf *pos;
    GPUVertBuf *data;
  } edit;

  struct {
    GPUIndexBuf *surfaces_tris;
    GPUIndexBuf *surfaces_lines;
    GPUIndexBuf *curves_lines;
    GPUIndexBuf *edges_adj_lines;
    /* Edit mode */
    GPUIndexBuf *edit_verts;
    GPUIndexBuf *edit_lines;
  } ibo;

  struct {
    GPUBatch *surfaces;
    GPUBatch *surfaces_edges;
    GPUBatch *curves;
    /* control handles and vertices */
    GPUBatch *edit_edges;
    GPUBatch *edit_verts;
    GPUBatch *edit_normals;
    GPUBatch *edge_detection;
  } batch;

  GPUIndexBuf **surf_per_mat_tris;
  GPUBatch **surf_per_mat;
  int mat_len;
  int cd_used, cd_needed;

  /* settings to determine if cache is invalid */
  bool is_dirty;
  bool is_editmode;

  /* Valid only if edge_detection is up to date. */
  bool is_manifold;
} CurveBatchCache;

/* GPUBatch cache management. */

static bool curve_batch_cache_valid(Curve *cu)
{
  CurveBatchCache *cache = cu->batch_cache;

  if (cache == NULL) {
    return false;
  }

  if (cache->mat_len != DRW_curve_material_count_get(cu)) {
    return false;
  }

  if (cache->is_dirty) {
    return false;
  }

  if (cache->is_editmode != ((cu->editnurb != NULL) || (cu->editfont != NULL))) {
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
  CurveBatchCache *cache = cu->batch_cache;

  if (!cache) {
    cache = cu->batch_cache = MEM_callocN(sizeof(*cache), __func__);
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

  cache->cd_used = 0;
  cache->mat_len = DRW_curve_material_count_get(cu);
  cache->surf_per_mat_tris = MEM_callocN(sizeof(*cache->surf_per_mat_tris) * cache->mat_len,
                                         __func__);
  cache->surf_per_mat = MEM_callocN(sizeof(*cache->surf_per_mat) * cache->mat_len, __func__);

  cache->is_editmode = (cu->editnurb != NULL) || (cu->editfont != NULL);

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
  return cu->batch_cache;
}

void DRW_curve_batch_cache_dirty_tag(Curve *cu, int mode)
{
  CurveBatchCache *cache = cu->batch_cache;
  if (cache == NULL) {
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
  CurveBatchCache *cache = cu->batch_cache;
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

  for (int i = 0; i < cache->mat_len; i++) {
    GPU_INDEXBUF_DISCARD_SAFE(cache->surf_per_mat_tris[i]);
    GPU_BATCH_DISCARD_SAFE(cache->surf_per_mat[i]);
  }
  MEM_SAFE_FREE(cache->surf_per_mat_tris);
  MEM_SAFE_FREE(cache->surf_per_mat);
  cache->mat_len = 0;
  cache->cd_used = 0;
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
  BLI_assert(rdata->ob_curve_cache != NULL);

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

  int v_idx = 0;
  LISTBASE_FOREACH (const BevList *, bl, &rdata->ob_curve_cache->bev) {
    if (bl->nr <= 0) {
      continue;
    }
    const int i_end = v_idx + bl->nr;
    for (const BevPoint *bevp = bl->bevpoints; v_idx < i_end; v_idx++, bevp++) {
      GPU_vertbuf_attr_set(vbo_curves_pos, attr_id.pos, v_idx, bevp->vec);
    }
  }
  LISTBASE_FOREACH (const DispList *, dl, &rdata->ob_curve_cache->disp) {
    if (ELEM(dl->type, DL_SEGM, DL_POLY)) {
      for (int i = 0; i < dl->nr; v_idx++, i++) {
        GPU_vertbuf_attr_set(vbo_curves_pos, attr_id.pos, v_idx, &((float(*)[3])dl->verts)[i]);
      }
    }
  }
  BLI_assert(v_idx == vert_len);
}

static void curve_create_curves_lines(CurveRenderData *rdata, GPUIndexBuf *ibo_curve_lines)
{
  BLI_assert(rdata->ob_curve_cache != NULL);

  const int vert_len = curve_render_data_wire_verts_len_get(rdata);
  const int edge_len = curve_render_data_wire_edges_len_get(rdata);
  const int curve_len = curve_render_data_wire_curve_len_get(rdata);
  /* Count the last vertex or each strip and the primitive restart. */
  const int index_len = edge_len + curve_len * 2;

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, index_len, vert_len);

  int v_idx = 0;
  LISTBASE_FOREACH (const BevList *, bl, &rdata->ob_curve_cache->bev) {
    if (bl->nr <= 0) {
      continue;
    }
    const bool is_cyclic = bl->poly != -1;
    if (is_cyclic) {
      GPU_indexbuf_add_generic_vert(&elb, v_idx + (bl->nr - 1));
    }
    for (int i = 0; i < bl->nr; i++) {
      GPU_indexbuf_add_generic_vert(&elb, v_idx + i);
    }
    GPU_indexbuf_add_primitive_restart(&elb);
    v_idx += bl->nr;
  }
  LISTBASE_FOREACH (const DispList *, dl, &rdata->ob_curve_cache->disp) {
    if (ELEM(dl->type, DL_SEGM, DL_POLY)) {
      const bool is_cyclic = dl->type == DL_POLY;
      if (is_cyclic) {
        GPU_indexbuf_add_generic_vert(&elb, v_idx + (dl->nr - 1));
      }
      for (int i = 0; i < dl->nr; i++) {
        GPU_indexbuf_add_generic_vert(&elb, v_idx + i);
      }
      GPU_indexbuf_add_primitive_restart(&elb);
      v_idx += dl->nr;
    }
  }
  GPU_indexbuf_build_in_place(&elb, ibo_curve_lines);
}

static void curve_create_edit_curves_nor(CurveRenderData *rdata, GPUVertBuf *vbo_curves_nor)
{
  static GPUVertFormat format = {0};
  static struct {
    uint pos, nor, tan, rad;
  } attr_id;
  if (format.attr_len == 0) {
    /* initialize vertex formats */
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.rad = GPU_vertformat_attr_add(&format, "rad", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    attr_id.nor = GPU_vertformat_attr_add(
        &format, "nor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    attr_id.tan = GPU_vertformat_attr_add(
        &format, "tan", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  int verts_len_capacity = curve_render_data_normal_len_get(rdata) * 2;
  int vbo_len_used = 0;

  GPU_vertbuf_init_with_format(vbo_curves_nor, &format);
  GPU_vertbuf_data_alloc(vbo_curves_nor, verts_len_capacity);

  const BevList *bl;
  const Nurb *nu;

  for (bl = rdata->ob_curve_cache->bev.first, nu = rdata->nurbs->first; nu && bl;
       bl = bl->next, nu = nu->next) {
    const BevPoint *bevp = bl->bevpoints;
    int nr = bl->nr;
    int skip = nu->resolu / 16;

    while (nr-- > 0) { /* accounts for empty bevel lists */
      float nor[3] = {1.0f, 0.0f, 0.0f};
      mul_qt_v3(bevp->quat, nor);

      GPUPackedNormal pnor = GPU_normal_convert_i10_v3(nor);
      GPUPackedNormal ptan = GPU_normal_convert_i10_v3(bevp->dir);

      /* Only set attributes for one vertex. */
      GPU_vertbuf_attr_set(vbo_curves_nor, attr_id.pos, vbo_len_used, bevp->vec);
      GPU_vertbuf_attr_set(vbo_curves_nor, attr_id.rad, vbo_len_used, &bevp->radius);
      GPU_vertbuf_attr_set(vbo_curves_nor, attr_id.nor, vbo_len_used, &pnor);
      GPU_vertbuf_attr_set(vbo_curves_nor, attr_id.tan, vbo_len_used, &ptan);
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

static char beztriple_vflag_get(CurveRenderData *rdata,
                                char flag,
                                char col_id,
                                int v_idx,
                                int nu_id,
                                bool handle_point,
                                const bool handle_selected)
{
  char vflag = 0;
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

static char bpoint_vflag_get(CurveRenderData *rdata, char flag, int v_idx, int nu_id, int u)
{
  char vflag = 0;
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

  if (DRW_TEST_ASSIGN_VBO(vbo_pos)) {
    GPU_vertbuf_init_with_format(vbo_pos, &format_pos);
    GPU_vertbuf_data_alloc(vbo_pos, verts_len_capacity);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_data)) {
    GPU_vertbuf_init_with_format(vbo_data, &format_data);
    GPU_vertbuf_data_alloc(vbo_data, verts_len_capacity);
  }

  GPUIndexBufBuilder elb_verts, *elbp_verts = NULL;
  GPUIndexBufBuilder elb_lines, *elbp_lines = NULL;
  if (DRW_TEST_ASSIGN_IBO(ibo_edit_verts_points)) {
    elbp_verts = &elb_verts;
    GPU_indexbuf_init(elbp_verts, GPU_PRIM_POINTS, verts_len_capacity, verts_len_capacity);
  }
  if (DRW_TEST_ASSIGN_IBO(ibo_edit_lines)) {
    elbp_lines = &elb_lines;
    GPU_indexbuf_init(elbp_lines, GPU_PRIM_LINES, edges_len_capacity, verts_len_capacity);
  }

  int nu_id = 0;
  for (Nurb *nu = rdata->nurbs->first; nu; nu = nu->next, nu_id++) {
    const BezTriple *bezt = nu->bezt;
    const BPoint *bp = nu->bp;

    if (bezt) {
      for (int a = 0; a < nu->pntsu; a++, bezt++) {
        if (bezt->hide == true) {
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
          const char vflag[3] = {
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
        if (bp->hide == true) {
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
          const BPoint *bp_next_u = (u < (nu->pntsu - 1)) ? &nu->bp[a + 1] : NULL;
          const BPoint *bp_next_v = (v < (nu->pntsv - 1)) ? &nu->bp[a + nu->pntsu] : NULL;
          if (bp_next_u && (bp_next_u->hide == false)) {
            GPU_indexbuf_add_line_verts(elbp_lines, vbo_len_used, vbo_len_used + 1);
          }
          if (bp_next_v && (bp_next_v->hide == false)) {
            GPU_indexbuf_add_line_verts(elbp_lines, vbo_len_used, vbo_len_used + nu->pntsu);
          }
        }
        if (vbo_data) {
          char vflag = bpoint_vflag_get(rdata, bp->f1, a, nu_id, u);
          GPU_vertbuf_attr_set(vbo_data, attr_id.data, vbo_len_used, &vflag);
        }
        if (vbo_pos) {
          GPU_vertbuf_attr_set(vbo_pos, attr_id.pos, vbo_len_used, bp->vec);
        }
      }
    }
  }

  /* Resize & Finish */
  if (elbp_verts != NULL) {
    GPU_indexbuf_build_in_place(elbp_verts, ibo_edit_verts_points);
  }
  if (elbp_lines != NULL) {
    GPU_indexbuf_build_in_place(elbp_lines, ibo_edit_lines);
  }
  if (vbo_len_used != verts_len_capacity) {
    if (vbo_pos != NULL) {
      GPU_vertbuf_data_resize(vbo_pos, vbo_len_used);
    }
    if (vbo_data != NULL) {
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

GPUBatch *DRW_curve_batch_cache_get_triangles_with_normals(struct Curve *cu)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);
  return DRW_batch_request(&cache->batch.surfaces);
}

GPUBatch **DRW_curve_batch_cache_get_surface_shaded(struct Curve *cu,
                                                    struct GPUMaterial **gpumat_array,
                                                    uint gpumat_array_len)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);

  BLI_assert(gpumat_array_len == cache->mat_len);

  curve_cd_calc_used_gpu_layers(&cache->cd_needed, gpumat_array, gpumat_array_len);

  for (int i = 0; i < cache->mat_len; i++) {
    DRW_batch_request(&cache->surf_per_mat[i]);
  }
  return cache->surf_per_mat;
}

GPUBatch *DRW_curve_batch_cache_get_wireframes_face(Curve *cu)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);
  return DRW_batch_request(&cache->batch.surfaces_edges);
}

GPUBatch *DRW_curve_batch_cache_get_edge_detection(Curve *cu, bool *r_is_manifold)
{
  CurveBatchCache *cache = curve_batch_cache_get(cu);
  /* Even if is_manifold is not correct (not updated),
   * the default (not manifold) is just the worst case. */
  if (r_is_manifold) {
    *r_is_manifold = cache->is_manifold;
  }
  return DRW_batch_request(&cache->batch.edge_detection);
}

int DRW_curve_material_count_get(Curve *cu)
{
  return max_ii(1, cu->totcol);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grouped batch generation
 * \{ */

void DRW_curve_batch_cache_create_requested(Object *ob)
{
  BLI_assert(ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT));

  Curve *cu = ob->data;
  CurveBatchCache *cache = curve_batch_cache_get(cu);

  /* Verify that all surface batches have needed attribute layers. */
  /* TODO(fclem): We could be a bit smarter here and only do it per material. */
  if ((cache->cd_used & cache->cd_needed) != cache->cd_needed) {
    for (int i = 0; i < cache->mat_len; i++) {
      /* We can't discard batches at this point as they have been
       * referenced for drawing. Just clear them in place. */
      GPU_BATCH_CLEAR_SAFE(cache->surf_per_mat[i]);
    }

    cache->cd_used |= cache->cd_needed;
    cache->cd_needed = 0;
  }

  /* Init batches and request VBOs & IBOs */
  if (DRW_batch_requested(cache->batch.surfaces, GPU_PRIM_TRIS)) {
    DRW_vbo_request(cache->batch.surfaces, &cache->ordered.loop_pos_nor);
  }
  if (DRW_batch_requested(cache->batch.surfaces_edges, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache->batch.surfaces_edges, &cache->ibo.surfaces_lines);
    DRW_vbo_request(cache->batch.surfaces_edges, &cache->ordered.pos_nor);
    DRW_vbo_request(cache->batch.surfaces_edges, &cache->ordered.edge_fac);
  }
  if (DRW_batch_requested(cache->batch.curves, GPU_PRIM_LINE_STRIP)) {
    DRW_ibo_request(cache->batch.curves, &cache->ibo.curves_lines);
    DRW_vbo_request(cache->batch.curves, &cache->ordered.curves_pos);
  }
  if (DRW_batch_requested(cache->batch.edge_detection, GPU_PRIM_LINES_ADJ)) {
    DRW_ibo_request(cache->batch.edge_detection, &cache->ibo.edges_adj_lines);
    DRW_vbo_request(cache->batch.edge_detection, &cache->ordered.pos_nor);
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
  for (int i = 0; i < cache->mat_len; i++) {
    if (DRW_batch_requested(cache->surf_per_mat[i], GPU_PRIM_TRIS)) {
      if (cache->mat_len > 1) {
        DRW_ibo_request(cache->surf_per_mat[i], &cache->surf_per_mat_tris[i]);
      }
      if (cache->cd_used & CD_MLOOPUV) {
        DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_uv);
      }
      if (cache->cd_used & CD_TANGENT) {
        DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_tan);
      }
      DRW_vbo_request(cache->surf_per_mat[i], &cache->ordered.loop_pos_nor);
    }
  }

#ifdef DRW_DEBUG_MESH_CACHE_REQUEST
  printf("-- %s %s --\n", __func__, ob->id.name + 2);
#endif

  /* Generate MeshRenderData flags */
  int mr_flag = 0;
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.pos_nor, CU_DATATYPE_SURFACE);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.edge_fac, CU_DATATYPE_SURFACE);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.curves_pos, CU_DATATYPE_WIRE);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.loop_pos_nor, CU_DATATYPE_SURFACE);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.loop_uv, CU_DATATYPE_SURFACE);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->ordered.loop_tan, CU_DATATYPE_SURFACE);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.surfaces_tris, CU_DATATYPE_SURFACE);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.surfaces_lines, CU_DATATYPE_SURFACE);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.curves_lines, CU_DATATYPE_WIRE);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.edges_adj_lines, CU_DATATYPE_SURFACE);

  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->edit.pos, CU_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->edit.data, CU_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->edit.curves_nor, CU_DATATYPE_NORMAL);
  DRW_ADD_FLAG_FROM_VBO_REQUEST(mr_flag, cache->edit.curves_weight, CU_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.edit_verts, CU_DATATYPE_OVERLAY);
  DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->ibo.edit_lines, CU_DATATYPE_OVERLAY);

  for (int i = 0; i < cache->mat_len; i++) {
    DRW_ADD_FLAG_FROM_IBO_REQUEST(mr_flag, cache->surf_per_mat_tris[i], CU_DATATYPE_SURFACE);
  }

#ifdef DRW_DEBUG_MESH_CACHE_REQUEST
  printf("  mr_flag %d\n\n", mr_flag);
#endif

  CurveRenderData *rdata = curve_render_data_create(cu, ob->runtime.curve_cache, mr_flag);

  /* DispLists */
  ListBase *lb = &rdata->ob_curve_cache->disp;

  /* Generate VBOs */
  if (DRW_vbo_requested(cache->ordered.pos_nor)) {
    DRW_displist_vertbuf_create_pos_and_nor(lb, cache->ordered.pos_nor);
  }
  if (DRW_vbo_requested(cache->ordered.edge_fac)) {
    DRW_displist_vertbuf_create_wiredata(lb, cache->ordered.edge_fac);
  }
  if (DRW_vbo_requested(cache->ordered.curves_pos)) {
    curve_create_curves_pos(rdata, cache->ordered.curves_pos);
  }

  if (DRW_vbo_requested(cache->ordered.loop_pos_nor) ||
      DRW_vbo_requested(cache->ordered.loop_uv) || DRW_vbo_requested(cache->ordered.loop_tan)) {
    DRW_displist_vertbuf_create_loop_pos_and_nor_and_uv_and_tan(
        lb, cache->ordered.loop_pos_nor, cache->ordered.loop_uv, cache->ordered.loop_tan);
  }

  if (DRW_ibo_requested(cache->surf_per_mat_tris[0])) {
    DRW_displist_indexbuf_create_triangles_loop_split_by_material(
        lb, cache->surf_per_mat_tris, cache->mat_len);
  }

  if (DRW_ibo_requested(cache->ibo.curves_lines)) {
    curve_create_curves_lines(rdata, cache->ibo.curves_lines);
  }
  if (DRW_ibo_requested(cache->ibo.surfaces_tris)) {
    DRW_displist_indexbuf_create_triangles_in_order(lb, cache->ibo.surfaces_tris);
  }
  if (DRW_ibo_requested(cache->ibo.surfaces_lines)) {
    DRW_displist_indexbuf_create_lines_in_order(lb, cache->ibo.surfaces_lines);
  }
  if (DRW_ibo_requested(cache->ibo.edges_adj_lines)) {
    DRW_displist_indexbuf_create_edges_adjacency_lines(
        lb, cache->ibo.edges_adj_lines, &cache->is_manifold);
  }

  if (DRW_vbo_requested(cache->edit.pos) || DRW_vbo_requested(cache->edit.data) ||
      DRW_ibo_requested(cache->ibo.edit_verts) || DRW_ibo_requested(cache->ibo.edit_lines)) {
    curve_create_edit_data_and_handles(
        rdata, cache->edit.pos, cache->edit.data, cache->ibo.edit_verts, cache->ibo.edit_lines);
  }
  if (DRW_vbo_requested(cache->edit.curves_nor)) {
    curve_create_edit_curves_nor(rdata, cache->edit.curves_nor);
  }

  curve_render_data_free(rdata);

#ifdef DEBUG
  /* Make sure all requested batches have been setup. */
  for (int i = 0; i < sizeof(cache->batch) / sizeof(void *); i++) {
    BLI_assert(!DRW_batch_requested(((GPUBatch **)&cache->batch)[i], 0));
  }
#endif
}

/** \} */
