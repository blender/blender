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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Mesh drawing using OpenGL VBO (Vertex Buffer Objects)
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_utildefines.h"

#include "DNA_meshdata_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_ccg.h"
#include "BKE_mesh.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

#include "GPU_batch.h"
#include "GPU_buffers.h"

#include "gpu_private.h"

#include "bmesh.h"

/* XXX: the rest of the code in this file is used for optimized PBVH
 * drawing and doesn't interact at all with the buffer code above */

struct GPU_PBVH_Buffers {
  GPUIndexBuf *index_buf, *index_buf_fast;
  GPUIndexBuf *index_lines_buf, *index_lines_buf_fast;
  GPUVertBuf *vert_buf;

  GPUBatch *lines;
  GPUBatch *lines_fast;
  GPUBatch *triangles;
  GPUBatch *triangles_fast;

  /* mesh pointers in case buffer allocation fails */
  const MPoly *mpoly;
  const MLoop *mloop;
  const MLoopTri *looptri;
  const MVert *mvert;

  const int *face_indices;
  int face_indices_len;

  /* grid pointers */
  CCGKey gridkey;
  CCGElem **grids;
  const DMFlagMat *grid_flag_mats;
  BLI_bitmap *const *grid_hidden;
  const int *grid_indices;
  int totgrid;

  bool use_bmesh;
  bool clear_bmesh_on_flush;

  uint tot_tri, tot_quad;

  short material_index;

  /* The PBVH ensures that either all faces in the node are
   * smooth-shaded or all faces are flat-shaded */
  bool smooth;

  bool show_overlay;
};

static struct {
  GPUVertFormat format;
  uint pos, nor, msk, col, fset;
} g_vbo_id = {{0}};

/** \} */

/* -------------------------------------------------------------------- */
/** \name PBVH Utils
 * \{ */

void gpu_pbvh_init()
{
  /* Initialize vertex buffer (match 'VertexBufferFormat'). */
  if (g_vbo_id.format.attr_len == 0) {
    g_vbo_id.pos = GPU_vertformat_attr_add(
        &g_vbo_id.format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    g_vbo_id.nor = GPU_vertformat_attr_add(
        &g_vbo_id.format, "nor", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    /* TODO: Do not allocate these `.msk` and `.col` when they are not used. */
    g_vbo_id.msk = GPU_vertformat_attr_add(
        &g_vbo_id.format, "msk", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    g_vbo_id.col = GPU_vertformat_attr_add(
        &g_vbo_id.format, "ac", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    g_vbo_id.fset = GPU_vertformat_attr_add(
        &g_vbo_id.format, "fset", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }
}

void gpu_pbvh_exit()
{
  /* Nothing to do. */
}

/* Allocates a non-initialized buffer to be sent to GPU.
 * Return is false it indicates that the memory map failed. */
static bool gpu_pbvh_vert_buf_data_set(GPU_PBVH_Buffers *buffers, uint vert_len)
{
  /* Keep so we can test #GPU_USAGE_DYNAMIC buffer use.
   * Not that format initialization match in both blocks.
   * Do this to keep braces balanced - otherwise indentation breaks. */
#if 0
  if (buffers->vert_buf == NULL) {
    /* Initialize vertex buffer (match 'VertexBufferFormat'). */
    buffers->vert_buf = GPU_vertbuf_create_with_format_ex(&g_vbo_id.format, GPU_USAGE_DYNAMIC);
    GPU_vertbuf_data_alloc(buffers->vert_buf, vert_len);
  }
  else if (vert_len != buffers->vert_buf->vertex_len) {
    GPU_vertbuf_data_resize(buffers->vert_buf, vert_len);
  }
#else
  if (buffers->vert_buf == NULL) {
    /* Initialize vertex buffer (match 'VertexBufferFormat'). */
    buffers->vert_buf = GPU_vertbuf_create_with_format_ex(&g_vbo_id.format, GPU_USAGE_STATIC);
  }
  if (buffers->vert_buf->data == NULL || buffers->vert_buf->vertex_len != vert_len) {
    /* Allocate buffer if not allocated yet or size changed. */
    GPU_vertbuf_data_alloc(buffers->vert_buf, vert_len);
  }
#endif

  return buffers->vert_buf->data != NULL;
}

static void gpu_pbvh_batch_init(GPU_PBVH_Buffers *buffers, GPUPrimType prim)
{
  if (buffers->triangles == NULL) {
    buffers->triangles = GPU_batch_create(prim,
                                          buffers->vert_buf,
                                          /* can be NULL if buffer is empty */
                                          buffers->index_buf);
  }

  if ((buffers->triangles_fast == NULL) && buffers->index_buf_fast) {
    buffers->triangles_fast = GPU_batch_create(prim, buffers->vert_buf, buffers->index_buf_fast);
  }

  if (buffers->lines == NULL) {
    buffers->lines = GPU_batch_create(GPU_PRIM_LINES,
                                      buffers->vert_buf,
                                      /* can be NULL if buffer is empty */
                                      buffers->index_lines_buf);
  }

  if ((buffers->lines_fast == NULL) && buffers->index_lines_buf_fast) {
    buffers->lines_fast = GPU_batch_create(
        GPU_PRIM_LINES, buffers->vert_buf, buffers->index_lines_buf_fast);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh PBVH
 * \{ */

/* Returns the Face Set random color for rendering in the overlay given its ID and a color seed. */
#define GOLDEN_RATIO_CONJUGATE 0.618033988749895f
static void face_set_overlay_color_get(const int face_set, const int seed, uchar *r_color)
{
  float rgba[4];
  float random_mod_hue = GOLDEN_RATIO_CONJUGATE * (abs(face_set) + (seed % 10));
  random_mod_hue = random_mod_hue - floorf(random_mod_hue);
  const float random_mod_sat = BLI_hash_int_01(abs(face_set) + seed + 1);
  const float random_mod_val = BLI_hash_int_01(abs(face_set) + seed + 2);
  hsv_to_rgb(random_mod_hue,
             0.6f + (random_mod_sat * 0.25f),
             1.0f - (random_mod_val * 0.35f),
             &rgba[0],
             &rgba[1],
             &rgba[2]);
  rgba_float_to_uchar(r_color, rgba);
}

static bool gpu_pbvh_is_looptri_visible(const MLoopTri *lt,
                                        const MVert *mvert,
                                        const MLoop *mloop,
                                        const int *sculpt_face_sets)
{
  return (!paint_is_face_hidden(lt, mvert, mloop) && sculpt_face_sets &&
          sculpt_face_sets[lt->poly] > SCULPT_FACE_SET_NONE);
}

/* Threaded - do not call any functions that use OpenGL calls! */
void GPU_pbvh_mesh_buffers_update(GPU_PBVH_Buffers *buffers,
                                  const MVert *mvert,
                                  const float *vmask,
                                  const MLoopCol *vcol,
                                  const int *sculpt_face_sets,
                                  const int face_sets_color_seed,
                                  const int face_sets_color_default,
                                  const MPropCol *vtcol,
                                  const int update_flags)
{
  const bool show_mask = vmask && (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
  const bool show_face_sets = sculpt_face_sets &&
                              (update_flags & GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS) != 0;
  const bool show_vcol = (vcol || vtcol) && (update_flags & GPU_PBVH_BUFFERS_SHOW_VCOL) != 0;
  bool empty_mask = true;
  bool default_face_set = true;

  {
    const int totelem = buffers->tot_tri * 3;

    /* Build VBO */
    if (gpu_pbvh_vert_buf_data_set(buffers, totelem)) {
      GPUVertBufRaw pos_step = {0};
      GPUVertBufRaw nor_step = {0};
      GPUVertBufRaw msk_step = {0};
      GPUVertBufRaw fset_step = {0};
      GPUVertBufRaw col_step = {0};

      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.pos, &pos_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.nor, &nor_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.msk, &msk_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.fset, &fset_step);
      if (show_vcol) {
        GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.col, &col_step);
      }

      /* calculate normal for each polygon only once */
      uint mpoly_prev = UINT_MAX;
      short no[3] = {0, 0, 0};

      for (uint i = 0; i < buffers->face_indices_len; i++) {
        const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];
        const uint vtri[3] = {
            buffers->mloop[lt->tri[0]].v,
            buffers->mloop[lt->tri[1]].v,
            buffers->mloop[lt->tri[2]].v,
        };

        if (!gpu_pbvh_is_looptri_visible(lt, mvert, buffers->mloop, sculpt_face_sets)) {
          continue;
        }

        /* Face normal and mask */
        if (lt->poly != mpoly_prev && !buffers->smooth) {
          const MPoly *mp = &buffers->mpoly[lt->poly];
          float fno[3];
          BKE_mesh_calc_poly_normal(mp, &buffers->mloop[mp->loopstart], mvert, fno);
          normal_float_to_short_v3(no, fno);
          mpoly_prev = lt->poly;
        }

        uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
        if (show_face_sets) {
          const int fset = abs(sculpt_face_sets[lt->poly]);
          /* Skip for the default color Face Set to render it white. */
          if (fset != face_sets_color_default) {
            face_set_overlay_color_get(fset, face_sets_color_seed, face_set_color);
            default_face_set = false;
          }
        }

        float fmask = 0.0f;
        uchar cmask = 0;
        if (show_mask && !buffers->smooth) {
          fmask = (vmask[vtri[0]] + vmask[vtri[1]] + vmask[vtri[2]]) / 3.0f;
          cmask = (uchar)(fmask * 255);
        }

        for (uint j = 0; j < 3; j++) {
          const MVert *v = &mvert[vtri[j]];
          copy_v3_v3(GPU_vertbuf_raw_step(&pos_step), v->co);

          if (buffers->smooth) {
            copy_v3_v3_short(no, v->no);
          }
          copy_v3_v3_short(GPU_vertbuf_raw_step(&nor_step), no);

          if (show_mask && buffers->smooth) {
            cmask = (uchar)(vmask[vtri[j]] * 255);
          }

          *(uchar *)GPU_vertbuf_raw_step(&msk_step) = cmask;
          empty_mask = empty_mask && (cmask == 0);
          /* Vertex Colors. */
          if (show_vcol) {
            ushort scol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
            if (vtcol) {
              scol[0] = unit_float_to_ushort_clamp(vtcol[vtri[j]].color[0]);
              scol[1] = unit_float_to_ushort_clamp(vtcol[vtri[j]].color[1]);
              scol[2] = unit_float_to_ushort_clamp(vtcol[vtri[j]].color[2]);
              scol[3] = unit_float_to_ushort_clamp(vtcol[vtri[j]].color[3]);
              memcpy(GPU_vertbuf_raw_step(&col_step), scol, sizeof(scol));
            }
            else {
              const uint loop_index = lt->tri[j];
              const MLoopCol *mcol = &vcol[loop_index];
              scol[0] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol->r]);
              scol[1] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol->g]);
              scol[2] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol->b]);
              scol[3] = unit_float_to_ushort_clamp(mcol->a * (1.0f / 255.0f));
              memcpy(GPU_vertbuf_raw_step(&col_step), scol, sizeof(scol));
            }
          }
          /* Face Sets. */
          memcpy(GPU_vertbuf_raw_step(&fset_step), face_set_color, sizeof(uchar) * 3);
        }
      }
    }

    gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
  }

  /* Get material index from the first face of this buffer. */
  const MLoopTri *lt = &buffers->looptri[buffers->face_indices[0]];
  const MPoly *mp = &buffers->mpoly[lt->poly];
  buffers->material_index = mp->mat_nr;

  buffers->show_overlay = !empty_mask || !default_face_set;
  buffers->mvert = mvert;
}

/* Threaded - do not call any functions that use OpenGL calls! */
GPU_PBVH_Buffers *GPU_pbvh_mesh_buffers_build(const MPoly *mpoly,
                                              const MLoop *mloop,
                                              const MLoopTri *looptri,
                                              const MVert *mvert,
                                              const int *face_indices,
                                              const int *sculpt_face_sets,
                                              const int face_indices_len,
                                              const struct Mesh *mesh)
{
  GPU_PBVH_Buffers *buffers;
  int i, tottri;
  int tot_real_edges = 0;

  buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");

  /* smooth or flat for all */
  buffers->smooth = mpoly[looptri[face_indices[0]].poly].flag & ME_SMOOTH;

  buffers->show_overlay = false;

  /* Count the number of visible triangles */
  for (i = 0, tottri = 0; i < face_indices_len; i++) {
    const MLoopTri *lt = &looptri[face_indices[i]];
    if (gpu_pbvh_is_looptri_visible(lt, mvert, mloop, sculpt_face_sets)) {
      int r_edges[3];
      BKE_mesh_looptri_get_real_edges(mesh, lt, r_edges);
      for (int j = 0; j < 3; j++) {
        if (r_edges[j] != -1) {
          tot_real_edges++;
        }
      }
      tottri++;
    }
  }

  if (tottri == 0) {
    buffers->tot_tri = 0;

    buffers->mpoly = mpoly;
    buffers->mloop = mloop;
    buffers->looptri = looptri;
    buffers->face_indices = face_indices;
    buffers->face_indices_len = 0;

    return buffers;
  }

  /* Fill the only the line buffer. */
  GPUIndexBufBuilder elb_lines;
  GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, tot_real_edges, INT_MAX);
  int vert_idx = 0;

  for (i = 0; i < face_indices_len; i++) {
    const MLoopTri *lt = &looptri[face_indices[i]];

    /* Skip hidden faces */
    if (!gpu_pbvh_is_looptri_visible(lt, mvert, mloop, sculpt_face_sets)) {
      continue;
    }

    int r_edges[3];
    BKE_mesh_looptri_get_real_edges(mesh, lt, r_edges);
    if (r_edges[0] != -1) {
      GPU_indexbuf_add_line_verts(&elb_lines, vert_idx * 3 + 0, vert_idx * 3 + 1);
    }
    if (r_edges[1] != -1) {
      GPU_indexbuf_add_line_verts(&elb_lines, vert_idx * 3 + 1, vert_idx * 3 + 2);
    }
    if (r_edges[2] != -1) {
      GPU_indexbuf_add_line_verts(&elb_lines, vert_idx * 3 + 2, vert_idx * 3 + 0);
    }

    vert_idx++;
  }
  buffers->index_lines_buf = GPU_indexbuf_build(&elb_lines);

  buffers->tot_tri = tottri;

  buffers->mpoly = mpoly;
  buffers->mloop = mloop;
  buffers->looptri = looptri;

  buffers->face_indices = face_indices;
  buffers->face_indices_len = face_indices_len;

  return buffers;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grid PBVH
 * \{ */

static void gpu_pbvh_grid_fill_index_buffers(GPU_PBVH_Buffers *buffers,
                                             SubdivCCG *UNUSED(subdiv_ccg),
                                             const int *UNUSED(face_sets),
                                             int *grid_indices,
                                             uint visible_quad_len,
                                             int totgrid,
                                             int gridsize)
{
  GPUIndexBufBuilder elb, elb_lines;
  GPUIndexBufBuilder elb_fast, elb_lines_fast;

  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, 2 * visible_quad_len, INT_MAX);
  GPU_indexbuf_init(&elb_fast, GPU_PRIM_TRIS, 2 * totgrid, INT_MAX);
  GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, 2 * totgrid * gridsize * (gridsize - 1), INT_MAX);
  GPU_indexbuf_init(&elb_lines_fast, GPU_PRIM_LINES, 4 * totgrid, INT_MAX);

  if (buffers->smooth) {
    uint offset = 0;
    const uint grid_vert_len = gridsize * gridsize;
    for (int i = 0; i < totgrid; i++, offset += grid_vert_len) {
      uint v0, v1, v2, v3;
      bool grid_visible = false;

      BLI_bitmap *gh = buffers->grid_hidden[grid_indices[i]];

      for (int j = 0; j < gridsize - 1; j++) {
        for (int k = 0; k < gridsize - 1; k++) {
          /* Skip hidden grid face */
          if (gh && paint_is_grid_face_hidden(gh, gridsize, k, j)) {
            continue;
          }
          /* Indices in a Clockwise QUAD disposition. */
          v0 = offset + j * gridsize + k;
          v1 = v0 + 1;
          v2 = v1 + gridsize;
          v3 = v2 - 1;

          GPU_indexbuf_add_tri_verts(&elb, v0, v2, v1);
          GPU_indexbuf_add_tri_verts(&elb, v0, v3, v2);

          GPU_indexbuf_add_line_verts(&elb_lines, v0, v1);
          GPU_indexbuf_add_line_verts(&elb_lines, v0, v3);

          if (j + 2 == gridsize) {
            GPU_indexbuf_add_line_verts(&elb_lines, v2, v3);
          }
          grid_visible = true;
        }

        if (grid_visible) {
          GPU_indexbuf_add_line_verts(&elb_lines, v1, v2);
        }
      }

      if (grid_visible) {
        /* Grid corners */
        v0 = offset;
        v1 = offset + gridsize - 1;
        v2 = offset + grid_vert_len - 1;
        v3 = offset + grid_vert_len - gridsize;

        GPU_indexbuf_add_tri_verts(&elb_fast, v0, v2, v1);
        GPU_indexbuf_add_tri_verts(&elb_fast, v0, v3, v2);

        GPU_indexbuf_add_line_verts(&elb_lines_fast, v0, v1);
        GPU_indexbuf_add_line_verts(&elb_lines_fast, v1, v2);
        GPU_indexbuf_add_line_verts(&elb_lines_fast, v2, v3);
        GPU_indexbuf_add_line_verts(&elb_lines_fast, v3, v0);
      }
    }
  }
  else {
    uint offset = 0;
    const uint grid_vert_len = square_uint(gridsize - 1) * 4;
    for (int i = 0; i < totgrid; i++, offset += grid_vert_len) {
      bool grid_visible = false;
      BLI_bitmap *gh = buffers->grid_hidden[grid_indices[i]];

      uint v0, v1, v2, v3;
      for (int j = 0; j < gridsize - 1; j++) {
        for (int k = 0; k < gridsize - 1; k++) {
          /* Skip hidden grid face */
          if (gh && paint_is_grid_face_hidden(gh, gridsize, k, j)) {
            continue;
          }
          /* VBO data are in a Clockwise QUAD disposition. */
          v0 = offset + (j * (gridsize - 1) + k) * 4;
          v1 = v0 + 1;
          v2 = v0 + 2;
          v3 = v0 + 3;

          GPU_indexbuf_add_tri_verts(&elb, v0, v2, v1);
          GPU_indexbuf_add_tri_verts(&elb, v0, v3, v2);

          GPU_indexbuf_add_line_verts(&elb_lines, v0, v1);
          GPU_indexbuf_add_line_verts(&elb_lines, v0, v3);

          if (j + 2 == gridsize) {
            GPU_indexbuf_add_line_verts(&elb_lines, v2, v3);
          }
          grid_visible = true;
        }

        if (grid_visible) {
          GPU_indexbuf_add_line_verts(&elb_lines, v1, v2);
        }
      }

      if (grid_visible) {
        /* Grid corners */
        v0 = offset;
        v1 = offset + (gridsize - 1) * 4 - 3;
        v2 = offset + grid_vert_len - 2;
        v3 = offset + grid_vert_len - (gridsize - 1) * 4 + 3;

        GPU_indexbuf_add_tri_verts(&elb_fast, v0, v2, v1);
        GPU_indexbuf_add_tri_verts(&elb_fast, v0, v3, v2);

        GPU_indexbuf_add_line_verts(&elb_lines_fast, v0, v1);
        GPU_indexbuf_add_line_verts(&elb_lines_fast, v1, v2);
        GPU_indexbuf_add_line_verts(&elb_lines_fast, v2, v3);
        GPU_indexbuf_add_line_verts(&elb_lines_fast, v3, v0);
      }
    }
  }

  buffers->index_buf = GPU_indexbuf_build(&elb);
  buffers->index_buf_fast = GPU_indexbuf_build(&elb_fast);
  buffers->index_lines_buf = GPU_indexbuf_build(&elb_lines);
  buffers->index_lines_buf_fast = GPU_indexbuf_build(&elb_lines_fast);
}

void GPU_pbvh_grid_buffers_update_free(GPU_PBVH_Buffers *buffers,
                                       const struct DMFlagMat *grid_flag_mats,
                                       int *grid_indices)
{
  const bool smooth = grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH;

  if (buffers->smooth != smooth) {
    buffers->smooth = smooth;
    GPU_BATCH_DISCARD_SAFE(buffers->triangles);
    GPU_BATCH_DISCARD_SAFE(buffers->triangles_fast);
    GPU_BATCH_DISCARD_SAFE(buffers->lines);
    GPU_BATCH_DISCARD_SAFE(buffers->lines_fast);

    GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
    GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf_fast);
    GPU_INDEXBUF_DISCARD_SAFE(buffers->index_lines_buf);
    GPU_INDEXBUF_DISCARD_SAFE(buffers->index_lines_buf_fast);
  }
}

/* Threaded - do not call any functions that use OpenGL calls! */
void GPU_pbvh_grid_buffers_update(GPU_PBVH_Buffers *buffers,
                                  SubdivCCG *subdiv_ccg,
                                  CCGElem **grids,
                                  const struct DMFlagMat *grid_flag_mats,
                                  int *grid_indices,
                                  int totgrid,
                                  const int *sculpt_face_sets,
                                  const int face_sets_color_seed,
                                  const int face_sets_color_default,
                                  const struct CCGKey *key,
                                  const int update_flags)
{
  const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
  const bool show_vcol = (update_flags & GPU_PBVH_BUFFERS_SHOW_VCOL) != 0;
  const bool show_face_sets = sculpt_face_sets &&
                              (update_flags & GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS) != 0;
  bool empty_mask = true;
  bool default_face_set = true;

  int i, j, k, x, y;

  /* Build VBO */
  const int has_mask = key->has_mask;

  buffers->smooth = grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH;

  uint vert_per_grid = (buffers->smooth) ? key->grid_area : (square_i(key->grid_size - 1) * 4);
  uint vert_count = totgrid * vert_per_grid;

  if (buffers->index_buf == NULL) {
    uint visible_quad_len = BKE_pbvh_count_grid_quads(
        (BLI_bitmap **)buffers->grid_hidden, grid_indices, totgrid, key->grid_size);

    /* totally hidden node, return here to avoid BufferData with zero below. */
    if (visible_quad_len == 0) {
      return;
    }

    gpu_pbvh_grid_fill_index_buffers(buffers,
                                     subdiv_ccg,
                                     sculpt_face_sets,
                                     grid_indices,
                                     visible_quad_len,
                                     totgrid,
                                     key->grid_size);
  }

  uint vbo_index_offset = 0;
  /* Build VBO */
  if (gpu_pbvh_vert_buf_data_set(buffers, vert_count)) {
    GPUIndexBufBuilder elb_lines;

    if (buffers->index_lines_buf == NULL) {
      GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, totgrid * key->grid_area * 2, vert_count);
    }

    for (i = 0; i < totgrid; i++) {
      const int grid_index = grid_indices[i];
      CCGElem *grid = grids[grid_index];
      int vbo_index = vbo_index_offset;

      uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};

      if (show_face_sets && subdiv_ccg && sculpt_face_sets) {
        const int face_index = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, grid_index);

        const int fset = abs(sculpt_face_sets[face_index]);
        /* Skip for the default color Face Set to render it white. */
        if (fset != face_sets_color_default) {
          face_set_overlay_color_get(fset, face_sets_color_seed, face_set_color);
          default_face_set = false;
        }
      }

      if (buffers->smooth) {
        for (y = 0; y < key->grid_size; y++) {
          for (x = 0; x < key->grid_size; x++) {
            CCGElem *elem = CCG_grid_elem(key, grid, x, y);
            GPU_vertbuf_attr_set(
                buffers->vert_buf, g_vbo_id.pos, vbo_index, CCG_elem_co(key, elem));

            short no_short[3];
            normal_float_to_short_v3(no_short, CCG_elem_no(key, elem));
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, vbo_index, no_short);

            if (has_mask && show_mask) {
              float fmask = *CCG_elem_mask(key, elem);
              uchar cmask = (uchar)(fmask * 255);
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.msk, vbo_index, &cmask);
              empty_mask = empty_mask && (cmask == 0);
            }

            if (show_vcol) {
              ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vbo_index, &vcol);
            }

            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.fset, vbo_index, &face_set_color);

            vbo_index += 1;
          }
        }
        vbo_index_offset += key->grid_area;
      }
      else {
        for (j = 0; j < key->grid_size - 1; j++) {
          for (k = 0; k < key->grid_size - 1; k++) {
            CCGElem *elems[4] = {
                CCG_grid_elem(key, grid, k, j),
                CCG_grid_elem(key, grid, k + 1, j),
                CCG_grid_elem(key, grid, k + 1, j + 1),
                CCG_grid_elem(key, grid, k, j + 1),
            };
            float *co[4] = {
                CCG_elem_co(key, elems[0]),
                CCG_elem_co(key, elems[1]),
                CCG_elem_co(key, elems[2]),
                CCG_elem_co(key, elems[3]),
            };

            float fno[3];
            short no_short[3];
            /* Note: Clockwise indices ordering, that's why we invert order here. */
            normal_quad_v3(fno, co[3], co[2], co[1], co[0]);
            normal_float_to_short_v3(no_short, fno);

            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.pos, vbo_index + 0, co[0]);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, vbo_index + 0, no_short);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.pos, vbo_index + 1, co[1]);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, vbo_index + 1, no_short);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.pos, vbo_index + 2, co[2]);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, vbo_index + 2, no_short);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.pos, vbo_index + 3, co[3]);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.nor, vbo_index + 3, no_short);

            if (has_mask && show_mask) {
              float fmask = (*CCG_elem_mask(key, elems[0]) + *CCG_elem_mask(key, elems[1]) +
                             *CCG_elem_mask(key, elems[2]) + *CCG_elem_mask(key, elems[3])) *
                            0.25f;
              uchar cmask = (uchar)(fmask * 255);
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.msk, vbo_index + 0, &cmask);
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.msk, vbo_index + 1, &cmask);
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.msk, vbo_index + 2, &cmask);
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.msk, vbo_index + 3, &cmask);
              empty_mask = empty_mask && (cmask == 0);
            }

            ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vbo_index + 0, &vcol);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vbo_index + 1, &vcol);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vbo_index + 2, &vcol);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col, vbo_index + 3, &vcol);

            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.fset, vbo_index + 0, &face_set_color);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.fset, vbo_index + 1, &face_set_color);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.fset, vbo_index + 2, &face_set_color);
            GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.fset, vbo_index + 3, &face_set_color);

            vbo_index += 4;
          }
        }
        vbo_index_offset += square_i(key->grid_size - 1) * 4;
      }
    }

    gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
  }

  /* Get material index from the first face of this buffer. */
  buffers->material_index = grid_flag_mats[grid_indices[0]].mat_nr;

  buffers->grids = grids;
  buffers->grid_indices = grid_indices;
  buffers->totgrid = totgrid;
  buffers->grid_flag_mats = grid_flag_mats;
  buffers->gridkey = *key;
  buffers->show_overlay = !empty_mask || !default_face_set;
}

/* Threaded - do not call any functions that use OpenGL calls! */
GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(int totgrid, BLI_bitmap **grid_hidden)
{
  GPU_PBVH_Buffers *buffers;

  buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
  buffers->grid_hidden = grid_hidden;
  buffers->totgrid = totgrid;

  buffers->show_overlay = false;

  return buffers;
}

#undef FILL_QUAD_BUFFER

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh PBVH
 * \{ */

/* Output a BMVert into a VertexBufferFormat array at v_index. */
static void gpu_bmesh_vert_to_buffer_copy(BMVert *v,
                                          GPUVertBuf *vert_buf,
                                          int v_index,
                                          const float fno[3],
                                          const float *fmask,
                                          const int cd_vert_mask_offset,
                                          const bool show_mask,
                                          const bool show_vcol,
                                          bool *empty_mask)
{
  /* Vertex should always be visible if it's used by a visible face. */
  BLI_assert(!BM_elem_flag_test(v, BM_ELEM_HIDDEN));

  /* Set coord, normal, and mask */
  GPU_vertbuf_attr_set(vert_buf, g_vbo_id.pos, v_index, v->co);

  short no_short[3];
  normal_float_to_short_v3(no_short, fno ? fno : v->no);
  GPU_vertbuf_attr_set(vert_buf, g_vbo_id.nor, v_index, no_short);

  if (show_mask) {
    float effective_mask = fmask ? *fmask : BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);
    uchar cmask = (uchar)(effective_mask * 255);
    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.msk, v_index, &cmask);
    *empty_mask = *empty_mask && (cmask == 0);
  }

  if (show_vcol) {
    ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.col, v_index, &vcol);
  }

  /* Add default face sets color to avoid artifacts. */
  uchar face_set[3] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
  GPU_vertbuf_attr_set(vert_buf, g_vbo_id.fset, v_index, &face_set);
}

/* Return the total number of vertices that don't have BM_ELEM_HIDDEN set */
static int gpu_bmesh_vert_visible_count(GSet *bm_unique_verts, GSet *bm_other_verts)
{
  GSetIterator gs_iter;
  int totvert = 0;

  GSET_ITER (gs_iter, bm_unique_verts) {
    BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      totvert++;
    }
  }
  GSET_ITER (gs_iter, bm_other_verts) {
    BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      totvert++;
    }
  }

  return totvert;
}

/* Return the total number of visible faces */
static int gpu_bmesh_face_visible_count(GSet *bm_faces)
{
  GSetIterator gh_iter;
  int totface = 0;

  GSET_ITER (gh_iter, bm_faces) {
    BMFace *f = BLI_gsetIterator_getKey(&gh_iter);

    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      totface++;
    }
  }

  return totface;
}

void GPU_pbvh_bmesh_buffers_update_free(GPU_PBVH_Buffers *buffers)
{
  if (buffers->smooth) {
    /* Smooth needs to recreate index buffer, so we have to invalidate the batch. */
    GPU_BATCH_DISCARD_SAFE(buffers->triangles);
    GPU_BATCH_DISCARD_SAFE(buffers->lines);
    GPU_INDEXBUF_DISCARD_SAFE(buffers->index_lines_buf);
    GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
  }
  else {
    GPU_BATCH_DISCARD_SAFE(buffers->lines);
    GPU_INDEXBUF_DISCARD_SAFE(buffers->index_lines_buf);
  }
}

/* Creates a vertex buffer (coordinate, normal, color) and, if smooth
 * shading, an element index buffer.
 * Threaded - do not call any functions that use OpenGL calls! */
void GPU_pbvh_bmesh_buffers_update(GPU_PBVH_Buffers *buffers,
                                   BMesh *bm,
                                   GSet *bm_faces,
                                   GSet *bm_unique_verts,
                                   GSet *bm_other_verts,
                                   const int update_flags)
{
  const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
  const bool show_vcol = (update_flags & GPU_PBVH_BUFFERS_SHOW_VCOL) != 0;
  int tottri, totvert;
  bool empty_mask = true;
  BMFace *f = NULL;

  /* Count visible triangles */
  tottri = gpu_bmesh_face_visible_count(bm_faces);

  if (buffers->smooth) {
    /* Count visible vertices */
    totvert = gpu_bmesh_vert_visible_count(bm_unique_verts, bm_other_verts);
  }
  else {
    totvert = tottri * 3;
  }

  if (!tottri) {
    if (BLI_gset_len(bm_faces) != 0) {
      /* Node is just hidden. */
    }
    else {
      buffers->clear_bmesh_on_flush = true;
    }
    buffers->tot_tri = 0;
    return;
  }

  /* TODO, make mask layer optional for bmesh buffer */
  const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);

  /* Fill vertex buffer */
  if (!gpu_pbvh_vert_buf_data_set(buffers, totvert)) {
    /* Memory map failed */
    return;
  }

  int v_index = 0;

  if (buffers->smooth) {
    /* Fill the vertex and triangle buffer in one pass over faces. */
    GPUIndexBufBuilder elb, elb_lines;
    GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tottri, totvert);
    GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, tottri * 3, totvert);

    GHash *bm_vert_to_index = BLI_ghash_int_new_ex("bm_vert_to_index", totvert);

    GSetIterator gs_iter;
    GSET_ITER (gs_iter, bm_faces) {
      f = BLI_gsetIterator_getKey(&gs_iter);

      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        BMVert *v[3];
        BM_face_as_array_vert_tri(f, v);

        uint idx[3];
        for (int i = 0; i < 3; i++) {
          void **idx_p;
          if (!BLI_ghash_ensure_p(bm_vert_to_index, v[i], &idx_p)) {
            /* Add vertex to the vertex buffer each time a new one is encountered */
            *idx_p = POINTER_FROM_UINT(v_index);

            gpu_bmesh_vert_to_buffer_copy(v[i],
                                          buffers->vert_buf,
                                          v_index,
                                          NULL,
                                          NULL,
                                          cd_vert_mask_offset,
                                          show_mask,
                                          show_vcol,
                                          &empty_mask);

            idx[i] = v_index;
            v_index++;
          }
          else {
            /* Vertex already in the vertex buffer, just get the index. */
            idx[i] = POINTER_AS_UINT(*idx_p);
          }
        }

        GPU_indexbuf_add_tri_verts(&elb, idx[0], idx[1], idx[2]);

        GPU_indexbuf_add_line_verts(&elb_lines, idx[0], idx[1]);
        GPU_indexbuf_add_line_verts(&elb_lines, idx[1], idx[2]);
        GPU_indexbuf_add_line_verts(&elb_lines, idx[2], idx[0]);
      }
    }

    BLI_ghash_free(bm_vert_to_index, NULL, NULL);

    buffers->tot_tri = tottri;
    if (buffers->index_buf == NULL) {
      buffers->index_buf = GPU_indexbuf_build(&elb);
    }
    else {
      GPU_indexbuf_build_in_place(&elb, buffers->index_buf);
    }
    buffers->index_lines_buf = GPU_indexbuf_build(&elb_lines);
  }
  else {
    GSetIterator gs_iter;

    GPUIndexBufBuilder elb_lines;
    GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, tottri * 3, tottri * 3);

    GSET_ITER (gs_iter, bm_faces) {
      f = BLI_gsetIterator_getKey(&gs_iter);

      BLI_assert(f->len == 3);

      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        BMVert *v[3];
        float fmask = 0.0f;
        int i;

        BM_face_as_array_vert_tri(f, v);

        /* Average mask value */
        for (i = 0; i < 3; i++) {
          fmask += BM_ELEM_CD_GET_FLOAT(v[i], cd_vert_mask_offset);
        }
        fmask /= 3.0f;

        GPU_indexbuf_add_line_verts(&elb_lines, v_index + 0, v_index + 1);
        GPU_indexbuf_add_line_verts(&elb_lines, v_index + 1, v_index + 2);
        GPU_indexbuf_add_line_verts(&elb_lines, v_index + 2, v_index + 0);

        for (i = 0; i < 3; i++) {
          gpu_bmesh_vert_to_buffer_copy(v[i],
                                        buffers->vert_buf,
                                        v_index++,
                                        f->no,
                                        &fmask,
                                        cd_vert_mask_offset,
                                        show_mask,
                                        show_vcol,
                                        &empty_mask);
        }
      }
    }

    buffers->index_lines_buf = GPU_indexbuf_build(&elb_lines);
    buffers->tot_tri = tottri;
  }

  /* Get material index from the last face we iterated on. */
  buffers->material_index = (f) ? f->mat_nr : 0;

  buffers->show_overlay = !empty_mask;

  gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic
 * \{ */

/* Threaded - do not call any functions that use OpenGL calls! */
GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading)
{
  GPU_PBVH_Buffers *buffers;

  buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
  buffers->use_bmesh = true;
  buffers->smooth = smooth_shading;
  buffers->show_overlay = true;

  return buffers;
}

GPUBatch *GPU_pbvh_buffers_batch_get(GPU_PBVH_Buffers *buffers, bool fast, bool wires)
{
  if (wires) {
    return (fast && buffers->lines_fast) ? buffers->lines_fast : buffers->lines;
  }
  else {
    return (fast && buffers->triangles_fast) ? buffers->triangles_fast : buffers->triangles;
  }
}

bool GPU_pbvh_buffers_has_overlays(GPU_PBVH_Buffers *buffers)
{
  return buffers->show_overlay;
}

short GPU_pbvh_buffers_material_index_get(GPU_PBVH_Buffers *buffers)
{
  return buffers->material_index;
}

static void gpu_pbvh_buffers_clear(GPU_PBVH_Buffers *buffers)
{
  GPU_BATCH_DISCARD_SAFE(buffers->lines);
  GPU_BATCH_DISCARD_SAFE(buffers->lines_fast);
  GPU_BATCH_DISCARD_SAFE(buffers->triangles);
  GPU_BATCH_DISCARD_SAFE(buffers->triangles_fast);
  GPU_INDEXBUF_DISCARD_SAFE(buffers->index_lines_buf_fast);
  GPU_INDEXBUF_DISCARD_SAFE(buffers->index_lines_buf);
  GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf_fast);
  GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
  GPU_VERTBUF_DISCARD_SAFE(buffers->vert_buf);
}

void GPU_pbvh_buffers_update_flush(GPU_PBVH_Buffers *buffers)
{
  /* Free empty bmesh node buffers. */
  if (buffers->clear_bmesh_on_flush) {
    gpu_pbvh_buffers_clear(buffers);
    buffers->clear_bmesh_on_flush = false;
  }

  /* Force flushing to the GPU. */
  if (buffers->vert_buf && buffers->vert_buf->data) {
    GPU_vertbuf_use(buffers->vert_buf);
  }
}

void GPU_pbvh_buffers_free(GPU_PBVH_Buffers *buffers)
{
  if (buffers) {
    gpu_pbvh_buffers_clear(buffers);
    MEM_freeN(buffers);
  }
}

/** \} */
