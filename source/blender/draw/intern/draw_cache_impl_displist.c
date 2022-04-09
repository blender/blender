/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief DispList API for render engines
 *
 * \note DispList may be removed soon! This is a utility for object types that use render.
 */

#include "BLI_edgehash.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_scene_types.h"

#include "BKE_displist.h"

#include "GPU_batch.h"
#include "GPU_capabilities.h"

#include "draw_cache_inline.h"

#include "draw_cache_impl.h" /* own include */

static int dl_vert_len(const DispList *dl)
{
  switch (dl->type) {
    case DL_INDEX3:
    case DL_INDEX4:
      return dl->nr;
    case DL_SURF:
      return dl->parts * dl->nr;
  }
  return 0;
}

static int dl_tri_len(const DispList *dl)
{
  switch (dl->type) {
    case DL_INDEX3:
      return dl->parts;
    case DL_INDEX4:
      return dl->parts * 2;
    case DL_SURF:
      return dl->totindex * 2;
  }
  return 0;
}

/* see: displist_vert_coords_alloc */
static int curve_render_surface_vert_len_get(const ListBase *lb)
{
  int vert_len = 0;
  LISTBASE_FOREACH (const DispList *, dl, lb) {
    vert_len += dl_vert_len(dl);
  }
  return vert_len;
}

static int curve_render_surface_tri_len_get(const ListBase *lb)
{
  int tri_len = 0;
  LISTBASE_FOREACH (const DispList *, dl, lb) {
    tri_len += dl_tri_len(dl);
  }
  return tri_len;
}

typedef void(SetTriIndicesFn)(void *thunk, uint v1, uint v2, uint v3);

static void displist_indexbufbuilder_set(
    SetTriIndicesFn *set_tri_indices,
    SetTriIndicesFn *set_quad_tri_indices, /* meh, find a better solution. */
    void *thunk,
    const DispList *dl,
    const int ofs)
{
  if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
    const int *idx = dl->index;
    if (dl->type == DL_INDEX3) {
      const int i_end = dl->parts;
      for (int i = 0; i < i_end; i++, idx += 3) {
        set_tri_indices(thunk, idx[0] + ofs, idx[2] + ofs, idx[1] + ofs);
      }
    }
    else if (dl->type == DL_SURF) {
      const int i_end = dl->totindex;
      for (int i = 0; i < i_end; i++, idx += 4) {
        set_quad_tri_indices(thunk, idx[0] + ofs, idx[2] + ofs, idx[1] + ofs);
        set_quad_tri_indices(thunk, idx[2] + ofs, idx[0] + ofs, idx[3] + ofs);
      }
    }
    else {
      BLI_assert(dl->type == DL_INDEX4);
      const int i_end = dl->parts;
      for (int i = 0; i < i_end; i++, idx += 4) {
        if (idx[2] != idx[3]) {
          set_quad_tri_indices(thunk, idx[2] + ofs, idx[0] + ofs, idx[1] + ofs);
          set_quad_tri_indices(thunk, idx[0] + ofs, idx[2] + ofs, idx[3] + ofs);
        }
        else {
          set_tri_indices(thunk, idx[2] + ofs, idx[0] + ofs, idx[1] + ofs);
        }
      }
    }
  }
}

void DRW_displist_vertbuf_create_pos_and_nor(ListBase *lb, GPUVertBuf *vbo, const Scene *scene)
{
  const bool do_hq_normals = (scene->r.perf_flag & SCE_PERF_HQ_NORMALS) != 0 ||
                             GPU_use_hq_normals_workaround();

  static GPUVertFormat format = {0};
  static GPUVertFormat format_hq = {0};
  static struct {
    uint pos, nor;
    uint pos_hq, nor_hq;
  } attr_id;
  if (format.attr_len == 0) {
    /* initialize vertex format */
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor = GPU_vertformat_attr_add(
        &format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    /* initialize vertex format */
    attr_id.pos_hq = GPU_vertformat_attr_add(&format_hq, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor_hq = GPU_vertformat_attr_add(
        &format_hq, "nor", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  uint pos_id = do_hq_normals ? attr_id.pos_hq : attr_id.pos;
  uint nor_id = do_hq_normals ? attr_id.nor_hq : attr_id.nor;

  GPU_vertbuf_init_with_format(vbo, do_hq_normals ? &format_hq : &format);
  GPU_vertbuf_data_alloc(vbo, curve_render_surface_vert_len_get(lb));

  BKE_displist_normals_add(lb);

  int vbo_len_used = 0;
  LISTBASE_FOREACH (const DispList *, dl, lb) {
    const bool ndata_is_single = dl->type == DL_INDEX3;
    if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
      const float *fp_co = dl->verts;
      const float *fp_no = dl->nors;
      const int vbo_end = vbo_len_used + dl_vert_len(dl);
      while (vbo_len_used < vbo_end) {
        GPU_vertbuf_attr_set(vbo, pos_id, vbo_len_used, fp_co);
        if (fp_no) {
          GPUNormal vnor_pack;
          GPU_normal_convert_v3(&vnor_pack, fp_no, do_hq_normals);
          GPU_vertbuf_attr_set(vbo, nor_id, vbo_len_used, &vnor_pack);
          if (ndata_is_single == false) {
            fp_no += 3;
          }
        }
        fp_co += 3;
        vbo_len_used += 1;
      }
    }
  }
}

void DRW_vertbuf_create_wiredata(GPUVertBuf *vbo, const int vert_len)
{
  static GPUVertFormat format = {0};
  static struct {
    uint wd;
  } attr_id;
  if (format.attr_len == 0) {
    /* initialize vertex format */
    if (!GPU_crappy_amd_driver()) {
      /* Some AMD drivers strangely crash with a vbo with this format. */
      attr_id.wd = GPU_vertformat_attr_add(
          &format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }
    else {
      attr_id.wd = GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, vert_len);

  if (GPU_vertbuf_get_format(vbo)->stride == 1) {
    memset(GPU_vertbuf_get_data(vbo), 0xFF, (size_t)vert_len);
  }
  else {
    GPUVertBufRaw wd_step;
    GPU_vertbuf_attr_get_raw_data(vbo, attr_id.wd, &wd_step);
    for (int i = 0; i < vert_len; i++) {
      *((float *)GPU_vertbuf_raw_step(&wd_step)) = 1.0f;
    }
  }
}

void DRW_displist_vertbuf_create_wiredata(ListBase *lb, GPUVertBuf *vbo)
{
  const int vert_len = curve_render_surface_vert_len_get(lb);
  DRW_vertbuf_create_wiredata(vbo, vert_len);
}

void DRW_displist_indexbuf_create_triangles_in_order(ListBase *lb, GPUIndexBuf *ibo)
{
  const int tri_len = curve_render_surface_tri_len_get(lb);
  const int vert_len = curve_render_surface_vert_len_get(lb);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, vert_len);

  int ofs = 0;
  LISTBASE_FOREACH (const DispList *, dl, lb) {
    displist_indexbufbuilder_set((SetTriIndicesFn *)GPU_indexbuf_add_tri_verts,
                                 (SetTriIndicesFn *)GPU_indexbuf_add_tri_verts,
                                 &elb,
                                 dl,
                                 ofs);
    ofs += dl_vert_len(dl);
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

static void set_overlay_wires_tri_indices(void *thunk, uint v1, uint v2, uint v3)
{
  GPUIndexBufBuilder *eld = (GPUIndexBufBuilder *)thunk;
  GPU_indexbuf_add_line_verts(eld, v1, v2);
  GPU_indexbuf_add_line_verts(eld, v2, v3);
  GPU_indexbuf_add_line_verts(eld, v3, v1);
}

static void set_overlay_wires_quad_tri_indices(void *thunk, uint v1, uint v2, uint v3)
{
  GPUIndexBufBuilder *eld = (GPUIndexBufBuilder *)thunk;
  GPU_indexbuf_add_line_verts(eld, v1, v3);
  GPU_indexbuf_add_line_verts(eld, v3, v2);
}

void DRW_displist_indexbuf_create_lines_in_order(ListBase *lb, GPUIndexBuf *ibo)
{
  const int tri_len = curve_render_surface_tri_len_get(lb);
  const int vert_len = curve_render_surface_vert_len_get(lb);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_LINES, tri_len * 3, vert_len);

  int ofs = 0;
  LISTBASE_FOREACH (const DispList *, dl, lb) {
    displist_indexbufbuilder_set(
        set_overlay_wires_tri_indices, set_overlay_wires_quad_tri_indices, &elb, dl, ofs);
    ofs += dl_vert_len(dl);
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

/* Edge detection/adjacency. */
#define NO_EDGE INT_MAX
static void set_edge_adjacency_lines_indices(
    EdgeHash *eh, GPUIndexBufBuilder *elb, bool *r_is_manifold, uint v1, uint v2, uint v3)
{
  bool inv_indices = (v2 > v3);
  void **pval;
  bool value_is_init = BLI_edgehash_ensure_p(eh, v2, v3, &pval);
  int v_data = POINTER_AS_INT(*pval);
  if (!value_is_init || v_data == NO_EDGE) {
    /* Save the winding order inside the sign bit. Because the
     * edgehash sort the keys and we need to compare winding later. */
    int value = (int)v1 + 1; /* Int 0 bm_looptricannot be signed */
    *pval = POINTER_FROM_INT((inv_indices) ? -value : value);
  }
  else {
    /* HACK Tag as not used. Prevent overhead of BLI_edgehash_remove. */
    *pval = POINTER_FROM_INT(NO_EDGE);
    bool inv_opposite = (v_data < 0);
    uint v_opposite = (uint)abs(v_data) - 1;

    if (inv_opposite == inv_indices) {
      /* Don't share edge if triangles have non matching winding. */
      GPU_indexbuf_add_line_adj_verts(elb, v1, v2, v3, v1);
      GPU_indexbuf_add_line_adj_verts(elb, v_opposite, v2, v3, v_opposite);
      *r_is_manifold = false;
    }
    else {
      GPU_indexbuf_add_line_adj_verts(elb, v1, v2, v3, v_opposite);
    }
  }
}

static void set_edges_adjacency_lines_indices(void *thunk, uint v1, uint v2, uint v3)
{
  void **packed = (void **)thunk;
  GPUIndexBufBuilder *elb = (GPUIndexBufBuilder *)packed[0];
  EdgeHash *eh = (EdgeHash *)packed[1];
  bool *r_is_manifold = (bool *)packed[2];

  set_edge_adjacency_lines_indices(eh, elb, r_is_manifold, v1, v2, v3);
  set_edge_adjacency_lines_indices(eh, elb, r_is_manifold, v2, v3, v1);
  set_edge_adjacency_lines_indices(eh, elb, r_is_manifold, v3, v1, v2);
}

void DRW_displist_indexbuf_create_edges_adjacency_lines(struct ListBase *lb,
                                                        struct GPUIndexBuf *ibo,
                                                        bool *r_is_manifold)
{
  const int tri_len = curve_render_surface_tri_len_get(lb);
  const int vert_len = curve_render_surface_vert_len_get(lb);

  *r_is_manifold = true;

  /* Allocate max but only used indices are sent to GPU. */
  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_LINES_ADJ, tri_len * 3, vert_len);

  EdgeHash *eh = BLI_edgehash_new_ex(__func__, tri_len * 3);

  /* pack values to pass to `set_edges_adjacency_lines_indices` function. */
  void *thunk[3] = {&elb, eh, r_is_manifold};
  int v_idx = 0;
  LISTBASE_FOREACH (const DispList *, dl, lb) {
    displist_indexbufbuilder_set((SetTriIndicesFn *)set_edges_adjacency_lines_indices,
                                 (SetTriIndicesFn *)set_edges_adjacency_lines_indices,
                                 thunk,
                                 dl,
                                 v_idx);
    v_idx += dl_vert_len(dl);
  }

  /* Create edges for remaining non manifold edges. */
  EdgeHashIterator *ehi;
  for (ehi = BLI_edgehashIterator_new(eh); BLI_edgehashIterator_isDone(ehi) == false;
       BLI_edgehashIterator_step(ehi)) {
    uint v1, v2;
    int v_data = POINTER_AS_INT(BLI_edgehashIterator_getValue(ehi));
    if (v_data == NO_EDGE) {
      continue;
    }
    BLI_edgehashIterator_getKey(ehi, &v1, &v2);
    uint v0 = (uint)abs(v_data) - 1;
    if (v_data < 0) { /* inv_opposite */
      SWAP(uint, v1, v2);
    }
    GPU_indexbuf_add_line_adj_verts(&elb, v0, v1, v2, v0);
    *r_is_manifold = false;
  }
  BLI_edgehashIterator_free(ehi);
  BLI_edgehash_free(eh, NULL);

  GPU_indexbuf_build_in_place(&elb, ibo);
}
#undef NO_EDGE
