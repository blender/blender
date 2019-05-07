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
 * \brief Lattice API for render engines
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_userdef_types.h"

#include "BKE_lattice.h"
#include "BKE_deform.h"
#include "BKE_colorband.h"

#include "GPU_batch.h"

#include "draw_cache_impl.h" /* own include */

#define SELECT 1

/**
 * TODO
 * - 'DispList' is currently not used
 *   (we could avoid using since it will be removed)
 */

static void lattice_batch_cache_clear(Lattice *lt);

/* ---------------------------------------------------------------------- */
/* Lattice Interface, direct access to basic data. */

static int vert_len_calc(int u, int v, int w)
{
  if (u <= 0 || v <= 0 || w <= 0) {
    return 0;
  }
  return u * v * w;
}

static int edge_len_calc(int u, int v, int w)
{
  if (u <= 0 || v <= 0 || w <= 0) {
    return 0;
  }
  return (((((u - 1) * v) + ((v - 1) * u)) * w) + ((w - 1) * (u * v)));
}

static int lattice_render_verts_len_get(Lattice *lt)
{
  if (lt->editlatt) {
    lt = lt->editlatt->latt;
  }

  const int u = lt->pntsu;
  const int v = lt->pntsv;
  const int w = lt->pntsw;

  if ((lt->flag & LT_OUTSIDE) == 0) {
    return vert_len_calc(u, v, w);
  }
  else {
    /* TODO remove internal coords */
    return vert_len_calc(u, v, w);
  }
}

static int lattice_render_edges_len_get(Lattice *lt)
{
  if (lt->editlatt) {
    lt = lt->editlatt->latt;
  }

  const int u = lt->pntsu;
  const int v = lt->pntsv;
  const int w = lt->pntsw;

  if ((lt->flag & LT_OUTSIDE) == 0) {
    return edge_len_calc(u, v, w);
  }
  else {
    /* TODO remove internal coords */
    return edge_len_calc(u, v, w);
  }
}

/* ---------------------------------------------------------------------- */
/* Lattice Interface, indirect, partially cached access to complex data. */

typedef struct LatticeRenderData {
  int types;

  int vert_len;
  int edge_len;

  struct {
    int u_len, v_len, w_len;
  } dims;
  bool show_only_outside;

  struct EditLatt *edit_latt;
  BPoint *bp;

  int actbp;

  struct MDeformVert *dvert;
} LatticeRenderData;

enum {
  LR_DATATYPE_VERT = 1 << 0,
  LR_DATATYPE_EDGE = 1 << 1,
  LR_DATATYPE_OVERLAY = 1 << 2,
};

static LatticeRenderData *lattice_render_data_create(Lattice *lt, const int types)
{
  LatticeRenderData *rdata = MEM_callocN(sizeof(*rdata), __func__);
  rdata->types = types;

  if (lt->editlatt) {
    EditLatt *editlatt = lt->editlatt;
    lt = editlatt->latt;

    rdata->edit_latt = editlatt;

    rdata->dvert = lt->dvert;

    if (types & (LR_DATATYPE_VERT)) {
      rdata->vert_len = lattice_render_verts_len_get(lt);
    }
    if (types & (LR_DATATYPE_EDGE)) {
      rdata->edge_len = lattice_render_edges_len_get(lt);
    }
    if (types & LR_DATATYPE_OVERLAY) {
      rdata->actbp = lt->actbp;
    }
  }
  else {
    rdata->dvert = NULL;

    if (types & (LR_DATATYPE_VERT)) {
      rdata->vert_len = lattice_render_verts_len_get(lt);
    }
    if (types & (LR_DATATYPE_EDGE)) {
      rdata->edge_len = lattice_render_edges_len_get(lt);
      /*no edge data */
    }
  }

  rdata->bp = lt->def;

  rdata->dims.u_len = lt->pntsu;
  rdata->dims.v_len = lt->pntsv;
  rdata->dims.w_len = lt->pntsw;

  rdata->show_only_outside = (lt->flag & LT_OUTSIDE) != 0;
  rdata->actbp = lt->actbp;

  return rdata;
}

static void lattice_render_data_free(LatticeRenderData *rdata)
{
#if 0
  if (rdata->loose_verts) {
    MEM_freeN(rdata->loose_verts);
  }
#endif
  MEM_freeN(rdata);
}

static int lattice_render_data_verts_len_get(const LatticeRenderData *rdata)
{
  BLI_assert(rdata->types & LR_DATATYPE_VERT);
  return rdata->vert_len;
}

static int lattice_render_data_edges_len_get(const LatticeRenderData *rdata)
{
  BLI_assert(rdata->types & LR_DATATYPE_EDGE);
  return rdata->edge_len;
}

static const BPoint *lattice_render_data_vert_bpoint(const LatticeRenderData *rdata,
                                                     const int vert_idx)
{
  BLI_assert(rdata->types & LR_DATATYPE_VERT);
  return &rdata->bp[vert_idx];
}

/* TODO, move into shader? */
static void rgb_from_weight(float r_rgb[3], const float weight)
{
  const float blend = ((weight / 2.0f) + 0.5f);

  if (weight <= 0.25f) { /* blue->cyan */
    r_rgb[0] = 0.0f;
    r_rgb[1] = blend * weight * 4.0f;
    r_rgb[2] = blend;
  }
  else if (weight <= 0.50f) { /* cyan->green */
    r_rgb[0] = 0.0f;
    r_rgb[1] = blend;
    r_rgb[2] = blend * (1.0f - ((weight - 0.25f) * 4.0f));
  }
  else if (weight <= 0.75f) { /* green->yellow */
    r_rgb[0] = blend * ((weight - 0.50f) * 4.0f);
    r_rgb[1] = blend;
    r_rgb[2] = 0.0f;
  }
  else if (weight <= 1.0f) { /* yellow->red */
    r_rgb[0] = blend;
    r_rgb[1] = blend * (1.0f - ((weight - 0.75f) * 4.0f));
    r_rgb[2] = 0.0f;
  }
  else {
    /* exceptional value, unclamped or nan,
     * avoid uninitialized memory use */
    r_rgb[0] = 1.0f;
    r_rgb[1] = 0.0f;
    r_rgb[2] = 1.0f;
  }
}

static void lattice_render_data_weight_col_get(const LatticeRenderData *rdata,
                                               const int vert_idx,
                                               const int actdef,
                                               float r_col[4])
{
  if (actdef > -1) {
    float weight = defvert_find_weight(rdata->dvert + vert_idx, actdef);

    if (U.flag & USER_CUSTOM_RANGE) {
      BKE_colorband_evaluate(&U.coba_weight, weight, r_col);
    }
    else {
      rgb_from_weight(r_col, weight);
    }

    r_col[3] = 1.0f;
  }
  else {
    zero_v4(r_col);
  }
}

/* ---------------------------------------------------------------------- */
/* Lattice GPUBatch Cache */

typedef struct LatticeBatchCache {
  GPUVertBuf *pos;
  GPUIndexBuf *edges;

  GPUBatch *all_verts;
  GPUBatch *all_edges;

  GPUBatch *overlay_verts;

  /* settings to determine if cache is invalid */
  bool is_dirty;

  struct {
    int u_len, v_len, w_len;
  } dims;
  bool show_only_outside;

  bool is_editmode;
} LatticeBatchCache;

/* GPUBatch cache management. */

static bool lattice_batch_cache_valid(Lattice *lt)
{
  LatticeBatchCache *cache = lt->batch_cache;

  if (cache == NULL) {
    return false;
  }

  if (cache->is_editmode != (lt->editlatt != NULL)) {
    return false;
  }

  if (cache->is_dirty) {
    return false;
  }
  else {
    if ((cache->dims.u_len != lt->pntsu) || (cache->dims.v_len != lt->pntsv) ||
        (cache->dims.w_len != lt->pntsw) ||
        ((cache->show_only_outside != ((lt->flag & LT_OUTSIDE) != 0)))) {
      return false;
    }
  }

  return true;
}

static void lattice_batch_cache_init(Lattice *lt)
{
  LatticeBatchCache *cache = lt->batch_cache;

  if (!cache) {
    cache = lt->batch_cache = MEM_callocN(sizeof(*cache), __func__);
  }
  else {
    memset(cache, 0, sizeof(*cache));
  }

  cache->dims.u_len = lt->pntsu;
  cache->dims.v_len = lt->pntsv;
  cache->dims.w_len = lt->pntsw;
  cache->show_only_outside = (lt->flag & LT_OUTSIDE) != 0;

  cache->is_editmode = lt->editlatt != NULL;

  cache->is_dirty = false;
}

void DRW_lattice_batch_cache_validate(Lattice *lt)
{
  if (!lattice_batch_cache_valid(lt)) {
    lattice_batch_cache_clear(lt);
    lattice_batch_cache_init(lt);
  }
}

static LatticeBatchCache *lattice_batch_cache_get(Lattice *lt)
{
  return lt->batch_cache;
}

void DRW_lattice_batch_cache_dirty_tag(Lattice *lt, int mode)
{
  LatticeBatchCache *cache = lt->batch_cache;
  if (cache == NULL) {
    return;
  }
  switch (mode) {
    case BKE_LATTICE_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    case BKE_LATTICE_BATCH_DIRTY_SELECT:
      /* TODO Separate Flag vbo */
      GPU_BATCH_DISCARD_SAFE(cache->overlay_verts);
      break;
    default:
      BLI_assert(0);
  }
}

static void lattice_batch_cache_clear(Lattice *lt)
{
  LatticeBatchCache *cache = lt->batch_cache;
  if (!cache) {
    return;
  }

  GPU_BATCH_DISCARD_SAFE(cache->all_verts);
  GPU_BATCH_DISCARD_SAFE(cache->all_edges);
  GPU_BATCH_DISCARD_SAFE(cache->overlay_verts);

  GPU_VERTBUF_DISCARD_SAFE(cache->pos);
  GPU_INDEXBUF_DISCARD_SAFE(cache->edges);
}

void DRW_lattice_batch_cache_free(Lattice *lt)
{
  lattice_batch_cache_clear(lt);
  MEM_SAFE_FREE(lt->batch_cache);
}

/* GPUBatch cache usage. */
static GPUVertBuf *lattice_batch_cache_get_pos(LatticeRenderData *rdata,
                                               LatticeBatchCache *cache,
                                               bool use_weight,
                                               const int actdef)
{
  BLI_assert(rdata->types & LR_DATATYPE_VERT);

  if (cache->pos == NULL) {
    static GPUVertFormat format = {0};
    static struct {
      uint pos, col;
    } attr_id;

    GPU_vertformat_clear(&format);

    /* initialize vertex format */
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    if (use_weight) {
      attr_id.col = GPU_vertformat_attr_add(&format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    }

    const int vert_len = lattice_render_data_verts_len_get(rdata);

    cache->pos = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(cache->pos, vert_len);
    for (int i = 0; i < vert_len; ++i) {
      const BPoint *bp = lattice_render_data_vert_bpoint(rdata, i);
      GPU_vertbuf_attr_set(cache->pos, attr_id.pos, i, bp->vec);

      if (use_weight) {
        float w_col[4];
        lattice_render_data_weight_col_get(rdata, i, actdef, w_col);
        w_col[3] = 1.0f;

        GPU_vertbuf_attr_set(cache->pos, attr_id.col, i, w_col);
      }
    }
  }

  return cache->pos;
}

static GPUIndexBuf *lattice_batch_cache_get_edges(LatticeRenderData *rdata,
                                                  LatticeBatchCache *cache)
{
  BLI_assert(rdata->types & (LR_DATATYPE_VERT | LR_DATATYPE_EDGE));

  if (cache->edges == NULL) {
    const int vert_len = lattice_render_data_verts_len_get(rdata);
    const int edge_len = lattice_render_data_edges_len_get(rdata);
    int edge_len_real = 0;

    GPUIndexBufBuilder elb;
    GPU_indexbuf_init(&elb, GPU_PRIM_LINES, edge_len, vert_len);

#define LATT_INDEX(u, v, w) ((((w)*rdata->dims.v_len + (v)) * rdata->dims.u_len) + (u))

    for (int w = 0; w < rdata->dims.w_len; w++) {
      int wxt = (w == 0 || w == rdata->dims.w_len - 1);
      for (int v = 0; v < rdata->dims.v_len; v++) {
        int vxt = (v == 0 || v == rdata->dims.v_len - 1);
        for (int u = 0; u < rdata->dims.u_len; u++) {
          int uxt = (u == 0 || u == rdata->dims.u_len - 1);

          if (w && ((uxt || vxt) || !rdata->show_only_outside)) {
            GPU_indexbuf_add_line_verts(&elb, LATT_INDEX(u, v, w - 1), LATT_INDEX(u, v, w));
            BLI_assert(edge_len_real <= edge_len);
            edge_len_real++;
          }
          if (v && ((uxt || wxt) || !rdata->show_only_outside)) {
            GPU_indexbuf_add_line_verts(&elb, LATT_INDEX(u, v - 1, w), LATT_INDEX(u, v, w));
            BLI_assert(edge_len_real <= edge_len);
            edge_len_real++;
          }
          if (u && ((vxt || wxt) || !rdata->show_only_outside)) {
            GPU_indexbuf_add_line_verts(&elb, LATT_INDEX(u - 1, v, w), LATT_INDEX(u, v, w));
            BLI_assert(edge_len_real <= edge_len);
            edge_len_real++;
          }
        }
      }
    }

#undef LATT_INDEX

    if (rdata->show_only_outside) {
      BLI_assert(edge_len_real <= edge_len);
    }
    else {
      BLI_assert(edge_len_real == edge_len);
    }

    cache->edges = GPU_indexbuf_build(&elb);
  }

  return cache->edges;
}

static void lattice_batch_cache_create_overlay_batches(Lattice *lt)
{
  /* Since LR_DATATYPE_OVERLAY is slow to generate, generate them all at once */
  int options = LR_DATATYPE_VERT | LR_DATATYPE_OVERLAY;

  LatticeBatchCache *cache = lattice_batch_cache_get(lt);
  LatticeRenderData *rdata = lattice_render_data_create(lt, options);

  if (cache->overlay_verts == NULL) {
    static GPUVertFormat format = {0};
    static struct {
      uint pos, data;
    } attr_id;
    if (format.attr_len == 0) {
      /* initialize vertex format */
      attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
      attr_id.data = GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 1, GPU_FETCH_INT);
    }

    const int vert_len = lattice_render_data_verts_len_get(rdata);

    GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(vbo, vert_len);
    for (int i = 0; i < vert_len; ++i) {
      const BPoint *bp = lattice_render_data_vert_bpoint(rdata, i);

      char vflag = 0;
      if (bp->f1 & SELECT) {
        if (i == rdata->actbp) {
          vflag |= VFLAG_VERT_ACTIVE;
        }
        else {
          vflag |= VFLAG_VERT_SELECTED;
        }
      }

      GPU_vertbuf_attr_set(vbo, attr_id.pos, i, bp->vec);
      GPU_vertbuf_attr_set(vbo, attr_id.data, i, &vflag);
    }

    cache->overlay_verts = GPU_batch_create_ex(GPU_PRIM_POINTS, vbo, NULL, GPU_BATCH_OWNS_VBO);
  }

  lattice_render_data_free(rdata);
}

GPUBatch *DRW_lattice_batch_cache_get_all_edges(Lattice *lt, bool use_weight, const int actdef)
{
  LatticeBatchCache *cache = lattice_batch_cache_get(lt);

  if (cache->all_edges == NULL) {
    /* create batch from Lattice */
    LatticeRenderData *rdata = lattice_render_data_create(lt, LR_DATATYPE_VERT | LR_DATATYPE_EDGE);

    cache->all_edges = GPU_batch_create(
        GPU_PRIM_LINES,
        lattice_batch_cache_get_pos(rdata, cache, use_weight, actdef),
        lattice_batch_cache_get_edges(rdata, cache));

    lattice_render_data_free(rdata);
  }

  return cache->all_edges;
}

GPUBatch *DRW_lattice_batch_cache_get_all_verts(Lattice *lt)
{
  LatticeBatchCache *cache = lattice_batch_cache_get(lt);

  if (cache->all_verts == NULL) {
    LatticeRenderData *rdata = lattice_render_data_create(lt, LR_DATATYPE_VERT);

    cache->all_verts = GPU_batch_create(
        GPU_PRIM_POINTS, lattice_batch_cache_get_pos(rdata, cache, false, -1), NULL);

    lattice_render_data_free(rdata);
  }

  return cache->all_verts;
}

GPUBatch *DRW_lattice_batch_cache_get_edit_verts(Lattice *lt)
{
  LatticeBatchCache *cache = lattice_batch_cache_get(lt);

  if (cache->overlay_verts == NULL) {
    lattice_batch_cache_create_overlay_batches(lt);
  }

  return cache->overlay_verts;
}
