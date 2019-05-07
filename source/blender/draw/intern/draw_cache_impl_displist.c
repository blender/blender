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
 * \brief DispList API for render engines
 *
 * \note DispList may be removed soon! This is a utility for object types that use render.
 */

#include "BLI_alloca.h"
#include "BLI_utildefines.h"
#include "BLI_edgehash.h"
#include "BLI_math_vector.h"

#include "DNA_curve_types.h"

#include "BKE_displist.h"

#include "GPU_batch.h"
#include "GPU_extensions.h"

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

/* see: displist_get_allverts */
static int curve_render_surface_vert_len_get(const ListBase *lb)
{
  int vert_len = 0;
  for (const DispList *dl = lb->first; dl; dl = dl->next) {
    vert_len += dl_vert_len(dl);
  }
  return vert_len;
}

static int curve_render_surface_tri_len_get(const ListBase *lb)
{
  int tri_len = 0;
  for (const DispList *dl = lb->first; dl; dl = dl->next) {
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

static int displist_indexbufbuilder_tess_set(
    SetTriIndicesFn *set_tri_indices,
    SetTriIndicesFn *set_quad_tri_indices, /* meh, find a better solution. */
    void *thunk,
    const DispList *dl,
    const int ofs)
{
  int v_idx = ofs;
  if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
    if (dl->type == DL_INDEX3) {
      for (int i = 0; i < dl->parts; i++) {
        set_tri_indices(thunk, v_idx + 0, v_idx + 1, v_idx + 2);
        v_idx += 3;
      }
    }
    else if (dl->type == DL_SURF) {
      for (int a = 0; a < dl->parts; a++) {
        if ((dl->flag & DL_CYCL_V) == 0 && a == dl->parts - 1) {
          break;
        }
        int b = (dl->flag & DL_CYCL_U) ? 0 : 1;
        for (; b < dl->nr; b++) {
          set_quad_tri_indices(thunk, v_idx + 0, v_idx + 1, v_idx + 2);
          set_quad_tri_indices(thunk, v_idx + 3, v_idx + 4, v_idx + 5);
          v_idx += 6;
        }
      }
    }
    else {
      BLI_assert(dl->type == DL_INDEX4);
      const int *idx = dl->index;
      for (int i = 0; i < dl->parts; i++, idx += 4) {
        if (idx[2] != idx[3]) {
          set_quad_tri_indices(thunk, v_idx + 0, v_idx + 1, v_idx + 2);
          set_quad_tri_indices(thunk, v_idx + 3, v_idx + 4, v_idx + 5);
          v_idx += 6;
        }
        else {
          set_tri_indices(thunk, v_idx + 0, v_idx + 1, v_idx + 2);
          v_idx += 3;
        }
      }
    }
  }
  return v_idx;
}

void DRW_displist_vertbuf_create_pos_and_nor(ListBase *lb, GPUVertBuf *vbo)
{
  static GPUVertFormat format = {0};
  static struct {
    uint pos, nor;
  } attr_id;
  if (format.attr_len == 0) {
    /* initialize vertex format */
    attr_id.pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor = GPU_vertformat_attr_add(
        &format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, curve_render_surface_vert_len_get(lb));

  BKE_displist_normals_add(lb);

  int vbo_len_used = 0;
  for (const DispList *dl = lb->first; dl; dl = dl->next) {
    const bool ndata_is_single = dl->type == DL_INDEX3;
    if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
      const float *fp_co = dl->verts;
      const float *fp_no = dl->nors;
      const int vbo_end = vbo_len_used + dl_vert_len(dl);
      while (vbo_len_used < vbo_end) {
        GPU_vertbuf_attr_set(vbo, attr_id.pos, vbo_len_used, fp_co);
        if (fp_no) {
          GPUPackedNormal vnor_pack = GPU_normal_convert_i10_v3(fp_no);
          GPU_vertbuf_attr_set(vbo, attr_id.nor, vbo_len_used, &vnor_pack);
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

void DRW_displist_vertbuf_create_wiredata(ListBase *lb, GPUVertBuf *vbo)
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

  int vbo_len_used = curve_render_surface_vert_len_get(lb);

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, vbo_len_used);

  if (vbo->format.stride == 1) {
    memset(vbo->data, 0xFF, (size_t)vbo_len_used);
  }
  else {
    GPUVertBufRaw wd_step;
    GPU_vertbuf_attr_get_raw_data(vbo, attr_id.wd, &wd_step);
    for (int i = 0; i < vbo_len_used; i++) {
      *((float *)GPU_vertbuf_raw_step(&wd_step)) = 1.0f;
    }
  }
}

void DRW_displist_indexbuf_create_triangles_in_order(ListBase *lb, GPUIndexBuf *ibo)
{
  const int tri_len = curve_render_surface_tri_len_get(lb);
  const int vert_len = curve_render_surface_vert_len_get(lb);

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tri_len, vert_len);

  int ofs = 0;
  for (const DispList *dl = lb->first; dl; dl = dl->next) {
    displist_indexbufbuilder_set((SetTriIndicesFn *)GPU_indexbuf_add_tri_verts,
                                 (SetTriIndicesFn *)GPU_indexbuf_add_tri_verts,
                                 &elb,
                                 dl,
                                 ofs);
    ofs += dl_vert_len(dl);
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

void DRW_displist_indexbuf_create_triangles_loop_split_by_material(ListBase *lb,
                                                                   GPUIndexBuf **ibo_mats,
                                                                   uint mat_len)
{
  GPUIndexBufBuilder *elb = BLI_array_alloca(elb, mat_len);

  const int tri_len = curve_render_surface_tri_len_get(lb);

  /* Init each index buffer builder */
  for (int i = 0; i < mat_len; i++) {
    GPU_indexbuf_init(&elb[i], GPU_PRIM_TRIS, tri_len * 3, tri_len * 3);
  }

  /* calc each index buffer builder */
  uint v_idx = 0;
  for (const DispList *dl = lb->first; dl; dl = dl->next) {
    v_idx = displist_indexbufbuilder_tess_set((SetTriIndicesFn *)GPU_indexbuf_add_tri_verts,
                                              (SetTriIndicesFn *)GPU_indexbuf_add_tri_verts,
                                              &elb[dl->col],
                                              dl,
                                              v_idx);
  }

  /* build each indexbuf */
  for (int i = 0; i < mat_len; i++) {
    GPU_indexbuf_build_in_place(&elb[i], ibo_mats[i]);
  }
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
  for (const DispList *dl = lb->first; dl; dl = dl->next) {
    displist_indexbufbuilder_set(
        set_overlay_wires_tri_indices, set_overlay_wires_quad_tri_indices, &elb, dl, ofs);
    ofs += dl_vert_len(dl);
  }

  GPU_indexbuf_build_in_place(&elb, ibo);
}

static void surf_uv_quad(const DispList *dl, const uint quad[4], float r_uv[4][2])
{
  int orco_sizeu = dl->nr - 1;
  int orco_sizev = dl->parts - 1;

  /* exception as handled in convertblender.c too */
  if (dl->flag & DL_CYCL_U) {
    orco_sizeu++;
  }
  if (dl->flag & DL_CYCL_V) {
    orco_sizev++;
  }

  for (int i = 0; i < 4; i++) {
    /* find uv based on vertex index into grid array */
    r_uv[i][0] = (quad[i] / dl->nr) / (float)orco_sizev;
    r_uv[i][1] = (quad[i] % dl->nr) / (float)orco_sizeu;

    /* cyclic correction */
    if ((i == 1 || i == 2) && r_uv[i][0] == 0.0f) {
      r_uv[i][0] = 1.0f;
    }
    if ((i == 0 || i == 1) && r_uv[i][1] == 0.0f) {
      r_uv[i][1] = 1.0f;
    }
  }
}

static void displist_vertbuf_attr_set_tri_pos_nor_uv(GPUVertBufRaw *pos_step,
                                                     GPUVertBufRaw *nor_step,
                                                     GPUVertBufRaw *uv_step,
                                                     const float v1[3],
                                                     const float v2[3],
                                                     const float v3[3],
                                                     const GPUPackedNormal *n1,
                                                     const GPUPackedNormal *n2,
                                                     const GPUPackedNormal *n3,
                                                     const float uv1[2],
                                                     const float uv2[2],
                                                     const float uv3[2])
{
  if (pos_step->size != 0) {
    copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v1);
    copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v2);
    copy_v3_v3(GPU_vertbuf_raw_step(pos_step), v3);

    *(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = *n1;
    *(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = *n2;
    *(GPUPackedNormal *)GPU_vertbuf_raw_step(nor_step) = *n3;
  }

  if (uv_step->size != 0) {
    normal_float_to_short_v2(GPU_vertbuf_raw_step(uv_step), uv1);
    normal_float_to_short_v2(GPU_vertbuf_raw_step(uv_step), uv2);
    normal_float_to_short_v2(GPU_vertbuf_raw_step(uv_step), uv3);
  }
}

void DRW_displist_vertbuf_create_loop_pos_and_nor_and_uv(ListBase *lb,
                                                         GPUVertBuf *vbo_pos_nor,
                                                         GPUVertBuf *vbo_uv)
{
  static GPUVertFormat format_pos_nor = {0};
  static GPUVertFormat format_uv = {0};
  static struct {
    uint pos, nor, uv;
  } attr_id;
  if (format_pos_nor.attr_len == 0) {
    /* initialize vertex format */
    attr_id.pos = GPU_vertformat_attr_add(
        &format_pos_nor, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    attr_id.nor = GPU_vertformat_attr_add(
        &format_pos_nor, "nor", GPU_COMP_I10, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_triple_load(&format_pos_nor);
    /* UVs are in [0..1] range. We can compress them. */
    attr_id.uv = GPU_vertformat_attr_add(
        &format_uv, "u", GPU_COMP_I16, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  int vbo_len_capacity = curve_render_surface_tri_len_get(lb) * 3;

  GPUVertBufRaw pos_step = {0};
  GPUVertBufRaw nor_step = {0};
  GPUVertBufRaw uv_step = {0};

  if (DRW_TEST_ASSIGN_VBO(vbo_pos_nor)) {
    GPU_vertbuf_init_with_format(vbo_pos_nor, &format_pos_nor);
    GPU_vertbuf_data_alloc(vbo_pos_nor, vbo_len_capacity);
    GPU_vertbuf_attr_get_raw_data(vbo_pos_nor, attr_id.pos, &pos_step);
    GPU_vertbuf_attr_get_raw_data(vbo_pos_nor, attr_id.nor, &nor_step);
  }
  if (DRW_TEST_ASSIGN_VBO(vbo_uv)) {
    GPU_vertbuf_init_with_format(vbo_uv, &format_uv);
    GPU_vertbuf_data_alloc(vbo_uv, vbo_len_capacity);
    GPU_vertbuf_attr_get_raw_data(vbo_uv, attr_id.uv, &uv_step);
  }

  BKE_displist_normals_add(lb);

  for (const DispList *dl = lb->first; dl; dl = dl->next) {
    const bool is_smooth = (dl->rt & CU_SMOOTH) != 0;
    if (ELEM(dl->type, DL_INDEX3, DL_INDEX4, DL_SURF)) {
      const float(*verts)[3] = (float(*)[3])dl->verts;
      const float(*nors)[3] = (float(*)[3])dl->nors;
      const int *idx = dl->index;
      float uv[4][2];

      if (dl->type == DL_INDEX3) {
        /* Currently 'DL_INDEX3' is always a flat surface with a single normal. */
        const GPUPackedNormal pnor = GPU_normal_convert_i10_v3(dl->nors);
        const float x_max = (float)(dl->nr - 1);
        uv[0][1] = uv[1][1] = uv[2][1] = 0.0f;
        const int i_end = dl->parts;
        for (int i = 0; i < i_end; i++, idx += 3) {
          if (vbo_uv) {
            uv[0][0] = idx[0] / x_max;
            uv[1][0] = idx[1] / x_max;
            uv[2][0] = idx[2] / x_max;
          }

          displist_vertbuf_attr_set_tri_pos_nor_uv(&pos_step,
                                                   &nor_step,
                                                   &uv_step,
                                                   verts[idx[0]],
                                                   verts[idx[2]],
                                                   verts[idx[1]],
                                                   &pnor,
                                                   &pnor,
                                                   &pnor,
                                                   uv[0],
                                                   uv[2],
                                                   uv[1]);
        }
      }
      else if (dl->type == DL_SURF) {
        uint quad[4];
        for (int a = 0; a < dl->parts; a++) {
          if ((dl->flag & DL_CYCL_V) == 0 && a == dl->parts - 1) {
            break;
          }

          int b;
          if (dl->flag & DL_CYCL_U) {
            quad[0] = dl->nr * a;
            quad[3] = quad[0] + dl->nr - 1;
            quad[1] = quad[0] + dl->nr;
            quad[2] = quad[3] + dl->nr;
            b = 0;
          }
          else {
            quad[3] = dl->nr * a;
            quad[0] = quad[3] + 1;
            quad[2] = quad[3] + dl->nr;
            quad[1] = quad[0] + dl->nr;
            b = 1;
          }
          if ((dl->flag & DL_CYCL_V) && a == dl->parts - 1) {
            quad[1] -= dl->parts * dl->nr;
            quad[2] -= dl->parts * dl->nr;
          }

          for (; b < dl->nr; b++) {
            if (vbo_uv) {
              surf_uv_quad(dl, quad, uv);
            }

            GPUPackedNormal pnors_quad[4];
            if (is_smooth) {
              for (int j = 0; j < 4; j++) {
                pnors_quad[j] = GPU_normal_convert_i10_v3(nors[quad[j]]);
              }
            }
            else {
              float nor_flat[3];
              normal_quad_v3(
                  nor_flat, verts[quad[0]], verts[quad[1]], verts[quad[2]], verts[quad[3]]);
              pnors_quad[0] = GPU_normal_convert_i10_v3(nor_flat);
              pnors_quad[1] = pnors_quad[0];
              pnors_quad[2] = pnors_quad[0];
              pnors_quad[3] = pnors_quad[0];
            }

            displist_vertbuf_attr_set_tri_pos_nor_uv(&pos_step,
                                                     &nor_step,
                                                     &uv_step,
                                                     verts[quad[2]],
                                                     verts[quad[0]],
                                                     verts[quad[1]],
                                                     &pnors_quad[2],
                                                     &pnors_quad[0],
                                                     &pnors_quad[1],
                                                     uv[2],
                                                     uv[0],
                                                     uv[1]);

            displist_vertbuf_attr_set_tri_pos_nor_uv(&pos_step,
                                                     &nor_step,
                                                     &uv_step,
                                                     verts[quad[0]],
                                                     verts[quad[2]],
                                                     verts[quad[3]],
                                                     &pnors_quad[0],
                                                     &pnors_quad[2],
                                                     &pnors_quad[3],
                                                     uv[0],
                                                     uv[2],
                                                     uv[3]);

            quad[2] = quad[1];
            quad[1]++;
            quad[3] = quad[0];
            quad[0]++;
          }
        }
      }
      else {
        BLI_assert(dl->type == DL_INDEX4);
        uv[0][0] = uv[0][1] = uv[1][0] = uv[3][1] = 0.0f;
        uv[1][1] = uv[2][0] = uv[2][1] = uv[3][0] = 1.0f;

        const int i_end = dl->parts;
        for (int i = 0; i < i_end; i++, idx += 4) {
          const bool is_tri = idx[2] != idx[3];

          GPUPackedNormal pnors_idx[4];
          if (is_smooth) {
            int idx_len = is_tri ? 3 : 4;
            for (int j = 0; j < idx_len; j++) {
              pnors_idx[j] = GPU_normal_convert_i10_v3(nors[idx[j]]);
            }
          }
          else {
            float nor_flat[3];
            if (is_tri) {
              normal_tri_v3(nor_flat, verts[idx[0]], verts[idx[1]], verts[idx[2]]);
            }
            else {
              normal_quad_v3(nor_flat, verts[idx[0]], verts[idx[1]], verts[idx[2]], verts[idx[3]]);
            }
            pnors_idx[0] = GPU_normal_convert_i10_v3(nor_flat);
            pnors_idx[1] = pnors_idx[0];
            pnors_idx[2] = pnors_idx[0];
            pnors_idx[3] = pnors_idx[0];
          }

          displist_vertbuf_attr_set_tri_pos_nor_uv(&pos_step,
                                                   &nor_step,
                                                   &uv_step,
                                                   verts[idx[0]],
                                                   verts[idx[2]],
                                                   verts[idx[1]],
                                                   &pnors_idx[0],
                                                   &pnors_idx[2],
                                                   &pnors_idx[1],
                                                   uv[0],
                                                   uv[2],
                                                   uv[1]);

          if (idx[2] != idx[3]) {
            displist_vertbuf_attr_set_tri_pos_nor_uv(&pos_step,
                                                     &nor_step,
                                                     &uv_step,
                                                     verts[idx[2]],
                                                     verts[idx[0]],
                                                     verts[idx[3]],
                                                     &pnors_idx[2],
                                                     &pnors_idx[0],
                                                     &pnors_idx[3],
                                                     uv[2],
                                                     uv[0],
                                                     uv[3]);
          }
        }
      }
    }
  }
  /* Resize and finish. */
  if (pos_step.size != 0) {
    int vbo_len_used = GPU_vertbuf_raw_used(&pos_step);
    if (vbo_len_used < vbo_len_capacity) {
      GPU_vertbuf_data_resize(vbo_pos_nor, vbo_len_used);
    }
  }
  if (uv_step.size != 0) {
    int vbo_len_used = GPU_vertbuf_raw_used(&uv_step);
    if (vbo_len_used < vbo_len_capacity) {
      GPU_vertbuf_data_resize(vbo_uv, vbo_len_used);
    }
  }
}

/* Edge detection/adjecency */
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
  for (const DispList *dl = lb->first; dl; dl = dl->next) {
    displist_indexbufbuilder_set((SetTriIndicesFn *)set_edges_adjacency_lines_indices,
                                 (SetTriIndicesFn *)set_edges_adjacency_lines_indices,
                                 thunk,
                                 dl,
                                 v_idx);
    v_idx += dl_vert_len(dl);
  }

  /* Create edges for remaning non manifold edges. */
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
    if (v_data < 0) { /* inv_opposite  */
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
