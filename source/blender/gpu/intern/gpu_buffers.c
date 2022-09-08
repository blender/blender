/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Mesh drawing using OpenGL VBO (Vertex Buffer Objects)
 */

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_attribute.h"
#include "BKE_ccg.h"
#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

#include "GPU_batch.h"
#include "GPU_buffers.h"

#include "DRW_engine.h"

#include "gpu_private.h"

#include "bmesh.h"

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

typedef struct GPUAttrRef {
  uchar domain, type;
  ushort cd_offset;
  int layer_idx;
} GPUAttrRef;

#define MAX_GPU_ATTR 256

typedef struct PBVHGPUFormat {
  GPUVertFormat format;
  uint pos, nor, msk, fset;
  uint col[MAX_GPU_ATTR];
  uint uv[MAX_GPU_ATTR];
  int totcol, totuv;

  /* Upload only the active color and UV attributes,
   * used for workbench mode. */
  bool active_attrs_only;
} PBVHGPUFormat;

PBVHGPUFormat *GPU_pbvh_make_format(void)
{
  PBVHGPUFormat *vbo_id = MEM_callocN(sizeof(PBVHGPUFormat), "PBVHGPUFormat");

  GPU_pbvh_attribute_names_update(PBVH_FACES, vbo_id, NULL, NULL, false);

  return vbo_id;
}

void GPU_pbvh_free_format(PBVHGPUFormat *vbo_id)
{
  MEM_SAFE_FREE(vbo_id);
}

static int gpu_pbvh_make_attr_offs(eAttrDomainMask domain_mask,
                                   eCustomDataMask type_mask,
                                   const CustomData *vdata,
                                   const CustomData *edata,
                                   const CustomData *ldata,
                                   const CustomData *pdata,
                                   GPUAttrRef r_cd_attrs[MAX_GPU_ATTR],
                                   bool active_only,
                                   int active_type,
                                   int active_domain,
                                   const CustomDataLayer *active_layer,
                                   const CustomDataLayer *render_layer);

/** \} */

/* -------------------------------------------------------------------- */
/** \name PBVH Utils
 * \{ */

void gpu_pbvh_init()
{
}

void gpu_pbvh_exit()
{
  /* Nothing to do. */
}

static CustomDataLayer *get_active_layer(const CustomData *cdata, int type)
{
  int idx = CustomData_get_active_layer_index(cdata, type);
  return idx != -1 ? cdata->layers + idx : NULL;
}

static CustomDataLayer *get_render_layer(const CustomData *cdata, int type)
{
  int idx = CustomData_get_render_layer_index(cdata, type);
  return idx != -1 ? cdata->layers + idx : NULL;
}

/* Allocates a non-initialized buffer to be sent to GPU.
 * Return is false it indicates that the memory map failed. */
static bool gpu_pbvh_vert_buf_data_set(PBVHGPUFormat *vbo_id,
                                       GPU_PBVH_Buffers *buffers,
                                       uint vert_len)
{
  /* Keep so we can test #GPU_USAGE_DYNAMIC buffer use.
   * Not that format initialization match in both blocks.
   * Do this to keep braces balanced - otherwise indentation breaks. */

  if (buffers->vert_buf == NULL) {
    /* Initialize vertex buffer (match 'VertexBufferFormat'). */
    buffers->vert_buf = GPU_vertbuf_create_with_format_ex(&vbo_id->format, GPU_USAGE_STATIC);
  }
  if (GPU_vertbuf_get_data(buffers->vert_buf) == NULL ||
      GPU_vertbuf_get_vertex_len(buffers->vert_buf) != vert_len) {
    /* Allocate buffer if not allocated yet or size changed. */
    GPU_vertbuf_data_alloc(buffers->vert_buf, vert_len);
  }

  return GPU_vertbuf_get_data(buffers->vert_buf) != NULL;
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

static bool gpu_pbvh_is_looptri_visible(const MLoopTri *lt,
                                        const bool *hide_vert,
                                        const MLoop *mloop,
                                        const int *sculpt_face_sets)
{
  return (!paint_is_face_hidden(lt, hide_vert, mloop) && sculpt_face_sets &&
          sculpt_face_sets[lt->poly] > SCULPT_FACE_SET_NONE);
}

void GPU_pbvh_mesh_buffers_update(PBVHGPUFormat *vbo_id,
                                  GPU_PBVH_Buffers *buffers,
                                  const Mesh *mesh,
                                  const MVert *mvert,
                                  const float *vmask,
                                  const int *sculpt_face_sets,
                                  int face_sets_color_seed,
                                  int face_sets_color_default,
                                  int update_flags,
                                  const float (*vert_normals)[3])
{
  GPUAttrRef vcol_refs[MAX_GPU_ATTR];
  GPUAttrRef cd_uvs[MAX_GPU_ATTR];

  const bool *hide_vert = (const bool *)CustomData_get_layer_named(
      &mesh->vdata, CD_PROP_BOOL, ".hide_vert");
  const int *material_indices = (const int *)CustomData_get_layer_named(
      &mesh->pdata, CD_PROP_INT32, "material_index");

  const CustomDataLayer *actcol = BKE_id_attributes_active_color_get(&mesh->id);
  eAttrDomain actcol_domain = actcol ? BKE_id_attribute_domain(&mesh->id, actcol) :
                                       ATTR_DOMAIN_AUTO;

  const CustomDataLayer *rendercol = BKE_id_attributes_render_color_get(&mesh->id);

  int totcol;

  if (update_flags & GPU_PBVH_BUFFERS_SHOW_VCOL) {
    totcol = gpu_pbvh_make_attr_offs(ATTR_DOMAIN_MASK_COLOR,
                                     CD_MASK_COLOR_ALL,
                                     &mesh->vdata,
                                     NULL,
                                     &mesh->ldata,
                                     NULL,
                                     vcol_refs,
                                     vbo_id->active_attrs_only,
                                     actcol ? actcol->type : 0,
                                     actcol_domain,
                                     actcol,
                                     rendercol);
  }
  else {
    totcol = 0;
  }

  int totuv = gpu_pbvh_make_attr_offs(ATTR_DOMAIN_MASK_CORNER,
                                      CD_MASK_MLOOPUV,
                                      NULL,
                                      NULL,
                                      &mesh->ldata,
                                      NULL,
                                      cd_uvs,
                                      vbo_id->active_attrs_only,
                                      CD_MLOOPUV,
                                      ATTR_DOMAIN_CORNER,
                                      get_active_layer(&mesh->ldata, CD_MLOOPUV),
                                      get_render_layer(&mesh->ldata, CD_MLOOPUV));

  const bool show_mask = vmask && (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
  const bool show_face_sets = sculpt_face_sets &&
                              (update_flags & GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS) != 0;
  bool empty_mask = true;
  bool default_face_set = true;

  {
    const int totelem = buffers->tot_tri * 3;

    /* Build VBO */
    if (gpu_pbvh_vert_buf_data_set(vbo_id, buffers, totelem)) {
      GPUVertBufRaw pos_step = {0};
      GPUVertBufRaw nor_step = {0};
      GPUVertBufRaw msk_step = {0};
      GPUVertBufRaw fset_step = {0};
      GPUVertBufRaw col_step = {0};
      GPUVertBufRaw uv_step = {0};

      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, vbo_id->pos, &pos_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, vbo_id->nor, &nor_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, vbo_id->msk, &msk_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, vbo_id->fset, &fset_step);

      /* calculate normal for each polygon only once */
      uint mpoly_prev = UINT_MAX;
      short no[3] = {0, 0, 0};

      if (totuv > 0) {
        for (int uv_i = 0; uv_i < totuv; uv_i++) {
          GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, vbo_id->uv[uv_i], &uv_step);

          GPUAttrRef *ref = cd_uvs + uv_i;
          CustomDataLayer *layer = mesh->ldata.layers + ref->layer_idx;
          MLoopUV *muv = layer->data;

          for (uint i = 0; i < buffers->face_indices_len; i++) {
            const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];

            if (!gpu_pbvh_is_looptri_visible(lt, hide_vert, buffers->mloop, sculpt_face_sets)) {
              continue;
            }

            for (uint j = 0; j < 3; j++) {
              MLoopUV *muv2 = muv + lt->tri[j];

              memcpy(GPU_vertbuf_raw_step(&uv_step), muv2->uv, sizeof(muv2->uv));
            }
          }
        }
      }

      for (int col_i = 0; col_i < totcol; col_i++) {
        GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, vbo_id->col[col_i], &col_step);

        const MPropCol *pcol = NULL;
        const MLoopCol *mcol = NULL;

        GPUAttrRef *ref = vcol_refs + col_i;
        const CustomData *cdata = ref->domain == ATTR_DOMAIN_POINT ? &mesh->vdata : &mesh->ldata;
        const CustomDataLayer *layer = cdata->layers + ref->layer_idx;

        bool color_loops = ref->domain == ATTR_DOMAIN_CORNER;

        if (layer->type == CD_PROP_COLOR) {
          pcol = (const MPropCol *)layer->data;
        }
        else {
          mcol = (const MLoopCol *)layer->data;
        }

        for (uint i = 0; i < buffers->face_indices_len; i++) {
          const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];
          const uint vtri[3] = {
              buffers->mloop[lt->tri[0]].v,
              buffers->mloop[lt->tri[1]].v,
              buffers->mloop[lt->tri[2]].v,
          };

          if (!gpu_pbvh_is_looptri_visible(lt, hide_vert, buffers->mloop, sculpt_face_sets)) {
            continue;
          }

          for (uint j = 0; j < 3; j++) {
            /* Vertex Colors. */
            const uint loop_index = lt->tri[j];

            ushort scol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};

            if (pcol) {
              const MPropCol *pcol2 = pcol + (color_loops ? loop_index : vtri[j]);

              scol[0] = unit_float_to_ushort_clamp(pcol2->color[0]);
              scol[1] = unit_float_to_ushort_clamp(pcol2->color[1]);
              scol[2] = unit_float_to_ushort_clamp(pcol2->color[2]);
              scol[3] = unit_float_to_ushort_clamp(pcol2->color[3]);
            }
            else {
              const MLoopCol *mcol2 = mcol + (color_loops ? loop_index : vtri[j]);

              scol[0] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->r]);
              scol[1] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->g]);
              scol[2] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->b]);
              scol[3] = unit_float_to_ushort_clamp(mcol2->a * (1.0f / 255.0f));
            }

            memcpy(GPU_vertbuf_raw_step(&col_step), scol, sizeof(scol));
          }
        }
      }

      for (uint i = 0; i < buffers->face_indices_len; i++) {
        const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];
        const uint vtri[3] = {
            buffers->mloop[lt->tri[0]].v,
            buffers->mloop[lt->tri[1]].v,
            buffers->mloop[lt->tri[2]].v,
        };

        if (!gpu_pbvh_is_looptri_visible(lt, hide_vert, buffers->mloop, sculpt_face_sets)) {
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
            BKE_paint_face_set_overlay_color_get(fset, face_sets_color_seed, face_set_color);
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
            normal_float_to_short_v3(no, vert_normals[vtri[j]]);
          }
          copy_v3_v3_short(GPU_vertbuf_raw_step(&nor_step), no);

          if (show_mask && buffers->smooth) {
            cmask = (uchar)(vmask[vtri[j]] * 255);
          }

          *(uchar *)GPU_vertbuf_raw_step(&msk_step) = cmask;
          empty_mask = empty_mask && (cmask == 0);
          /* Face Sets. */
          memcpy(GPU_vertbuf_raw_step(&fset_step), face_set_color, sizeof(uchar[3]));
        }
      }
    }

    gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
  }

  /* Get material index from the first face of this buffer. */
  const MLoopTri *lt = &buffers->looptri[buffers->face_indices[0]];
  buffers->material_index = material_indices ? material_indices[lt->poly] : 0;

  buffers->show_overlay = !empty_mask || !default_face_set;
  buffers->mvert = mvert;
}

GPU_PBVH_Buffers *GPU_pbvh_mesh_buffers_build(const Mesh *mesh,
                                              const MLoopTri *looptri,
                                              const int *sculpt_face_sets,
                                              const int *face_indices,
                                              const int face_indices_len)
{
  GPU_PBVH_Buffers *buffers;
  int i, tottri;
  int tot_real_edges = 0;

  const MPoly *polys = BKE_mesh_polys(mesh);
  const MLoop *loops = BKE_mesh_loops(mesh);

  buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");

  const bool *hide_vert = (bool *)CustomData_get_layer_named(
      &mesh->vdata, CD_PROP_BOOL, ".hide_vert");

  /* smooth or flat for all */
  buffers->smooth = polys[looptri[face_indices[0]].poly].flag & ME_SMOOTH;

  buffers->show_overlay = false;

  /* Count the number of visible triangles */
  for (i = 0, tottri = 0; i < face_indices_len; i++) {
    const MLoopTri *lt = &looptri[face_indices[i]];
    if (gpu_pbvh_is_looptri_visible(lt, hide_vert, loops, sculpt_face_sets)) {
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

    buffers->mpoly = polys;
    buffers->mloop = loops;
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
    if (!gpu_pbvh_is_looptri_visible(lt, hide_vert, loops, sculpt_face_sets)) {
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

  buffers->mpoly = polys;
  buffers->mloop = loops;
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
                                             const int *grid_indices,
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
                                       const int *grid_indices)
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

void GPU_pbvh_grid_buffers_update(PBVHGPUFormat *vbo_id,
                                  GPU_PBVH_Buffers *buffers,
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
  if (gpu_pbvh_vert_buf_data_set(vbo_id, buffers, vert_count)) {
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
          BKE_paint_face_set_overlay_color_get(fset, face_sets_color_seed, face_set_color);
          default_face_set = false;
        }
      }

      if (buffers->smooth) {
        for (y = 0; y < key->grid_size; y++) {
          for (x = 0; x < key->grid_size; x++) {
            CCGElem *elem = CCG_grid_elem(key, grid, x, y);
            GPU_vertbuf_attr_set(
                buffers->vert_buf, vbo_id->pos, vbo_index, CCG_elem_co(key, elem));

            short no_short[3];
            normal_float_to_short_v3(no_short, CCG_elem_no(key, elem));
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->nor, vbo_index, no_short);

            if (has_mask && show_mask) {
              float fmask = *CCG_elem_mask(key, elem);
              uchar cmask = (uchar)(fmask * 255);
              GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->msk, vbo_index, &cmask);
              empty_mask = empty_mask && (cmask == 0);
            }

            if (show_vcol) {
              const ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
              GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->col[0], vbo_index, &vcol);
            }

            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->fset, vbo_index, &face_set_color);

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
            /* NOTE: Clockwise indices ordering, that's why we invert order here. */
            normal_quad_v3(fno, co[3], co[2], co[1], co[0]);
            normal_float_to_short_v3(no_short, fno);

            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->pos, vbo_index + 0, co[0]);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->nor, vbo_index + 0, no_short);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->pos, vbo_index + 1, co[1]);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->nor, vbo_index + 1, no_short);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->pos, vbo_index + 2, co[2]);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->nor, vbo_index + 2, no_short);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->pos, vbo_index + 3, co[3]);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->nor, vbo_index + 3, no_short);

            if (has_mask && show_mask) {
              float fmask = (*CCG_elem_mask(key, elems[0]) + *CCG_elem_mask(key, elems[1]) +
                             *CCG_elem_mask(key, elems[2]) + *CCG_elem_mask(key, elems[3])) *
                            0.25f;
              uchar cmask = (uchar)(fmask * 255);
              GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->msk, vbo_index + 0, &cmask);
              GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->msk, vbo_index + 1, &cmask);
              GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->msk, vbo_index + 2, &cmask);
              GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->msk, vbo_index + 3, &cmask);
              empty_mask = empty_mask && (cmask == 0);
            }

            const ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->col[0], vbo_index + 0, &vcol);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->col[0], vbo_index + 1, &vcol);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->col[0], vbo_index + 2, &vcol);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->col[0], vbo_index + 3, &vcol);

            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->fset, vbo_index + 0, &face_set_color);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->fset, vbo_index + 1, &face_set_color);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->fset, vbo_index + 2, &face_set_color);
            GPU_vertbuf_attr_set(buffers->vert_buf, vbo_id->fset, vbo_index + 3, &face_set_color);

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

GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(int totgrid, BLI_bitmap **grid_hidden, bool smooth)
{
  GPU_PBVH_Buffers *buffers;

  buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
  buffers->grid_hidden = grid_hidden;
  buffers->totgrid = totgrid;
  buffers->smooth = smooth;

  buffers->show_overlay = false;

  return buffers;
}

#undef FILL_QUAD_BUFFER

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh PBVH
 * \{ */

/* Output a BMVert into a VertexBufferFormat array at v_index. */
static void gpu_bmesh_vert_to_buffer_copy(PBVHGPUFormat *vbo_id,
                                          BMVert *v,
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
  GPU_vertbuf_attr_set(vert_buf, vbo_id->pos, v_index, v->co);

  short no_short[3];
  normal_float_to_short_v3(no_short, fno ? fno : v->no);
  GPU_vertbuf_attr_set(vert_buf, vbo_id->nor, v_index, no_short);

  if (show_mask) {
    float effective_mask = fmask ? *fmask : BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);
    uchar cmask = (uchar)(effective_mask * 255);
    GPU_vertbuf_attr_set(vert_buf, vbo_id->msk, v_index, &cmask);
    *empty_mask = *empty_mask && (cmask == 0);
  }

  if (show_vcol) {
    const ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
    GPU_vertbuf_attr_set(vert_buf, vbo_id->col[0], v_index, &vcol);
  }

  /* Add default face sets color to avoid artifacts. */
  const uchar face_set[3] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
  GPU_vertbuf_attr_set(vert_buf, vbo_id->fset, v_index, &face_set);
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

void GPU_pbvh_bmesh_buffers_update(PBVHGPUFormat *vbo_id,
                                   GPU_PBVH_Buffers *buffers,
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

  /* TODO: make mask layer optional for bmesh buffer. */
  const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);

  /* Fill vertex buffer */
  if (!gpu_pbvh_vert_buf_data_set(vbo_id, buffers, totvert)) {
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

            gpu_bmesh_vert_to_buffer_copy(vbo_id,
                                          v[i],
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
          gpu_bmesh_vert_to_buffer_copy(vbo_id,
                                        v[i],
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

GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading)
{
  GPU_PBVH_Buffers *buffers;

  buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
  buffers->use_bmesh = true;
  buffers->smooth = smooth_shading;
  buffers->show_overlay = true;

  return buffers;
}

/**
 * Builds a list of attributes from a set of domains and a set of
 * customdata types.
 *
 * \param active_only: Returns only one item, a #GPUAttrRef to active_layer.
 * \param active_layer: #CustomDataLayer to use for the active layer.
 * \param active_layer: #CustomDataLayer to use for the render layer.
 */
static int gpu_pbvh_make_attr_offs(eAttrDomainMask domain_mask,
                                   eCustomDataMask type_mask,
                                   const CustomData *vdata,
                                   const CustomData *edata,
                                   const CustomData *ldata,
                                   const CustomData *pdata,
                                   GPUAttrRef r_cd_attrs[MAX_GPU_ATTR],
                                   bool active_only,
                                   int active_type,
                                   int active_domain,
                                   const CustomDataLayer *active_layer,
                                   const CustomDataLayer *render_layer)
{
  const CustomData *cdata_active = active_domain == ATTR_DOMAIN_POINT ? vdata : ldata;

  if (!cdata_active) {
    return 0;
  }

  if (active_only) {
    int idx = active_layer ? active_layer - cdata_active->layers : -1;

    if (idx >= 0 && idx < cdata_active->totlayer) {
      r_cd_attrs[0].cd_offset = cdata_active->layers[idx].offset;
      r_cd_attrs[0].domain = active_domain;
      r_cd_attrs[0].type = active_type;
      r_cd_attrs[0].layer_idx = idx;

      return 1;
    }

    return 0;
  }

  const CustomData *datas[4] = {vdata, edata, pdata, ldata};

  int count = 0;
  for (eAttrDomain domain = 0; domain < 4; domain++) {
    const CustomData *cdata = datas[domain];

    if (!cdata || !((1 << domain) & domain_mask)) {
      continue;
    }

    const CustomDataLayer *cl = cdata->layers;

    for (int i = 0; count < MAX_GPU_ATTR && i < cdata->totlayer; i++, cl++) {
      if ((CD_TYPE_AS_MASK(cl->type) & type_mask) && !(cl->flag & CD_FLAG_TEMPORARY)) {
        GPUAttrRef *ref = r_cd_attrs + count;

        ref->cd_offset = cl->offset;
        ref->type = cl->type;
        ref->layer_idx = i;
        ref->domain = domain;

        count++;
      }
    }
  }

  /* Ensure render layer is last, draw cache code seems to need this. */

  for (int i = 0; i < count; i++) {
    GPUAttrRef *ref = r_cd_attrs + i;
    const CustomData *cdata = datas[ref->domain];

    if (cdata->layers + ref->layer_idx == render_layer) {
      SWAP(GPUAttrRef, r_cd_attrs[i], r_cd_attrs[count - 1]);
      break;
    }
  }

  return count;
}

static bool gpu_pbvh_format_equals(PBVHGPUFormat *a, PBVHGPUFormat *b)
{
  bool bad = false;

  bad |= a->active_attrs_only != b->active_attrs_only;

  bad |= a->pos != b->pos;
  bad |= a->fset != b->fset;
  bad |= a->msk != b->msk;
  bad |= a->nor != b->nor;

  for (int i = 0; i < MIN2(a->totuv, b->totuv); i++) {
    bad |= a->uv[i] != b->uv[i];
  }

  for (int i = 0; i < MIN2(a->totcol, b->totcol); i++) {
    bad |= a->col[i] != b->col[i];
  }

  bad |= a->totuv != b->totuv;
  bad |= a->totcol != b->totcol;

  return !bad;
}

bool GPU_pbvh_attribute_names_update(PBVHType pbvh_type,
                                     PBVHGPUFormat *vbo_id,
                                     const CustomData *vdata,
                                     const CustomData *ldata,
                                     bool active_attrs_only)
{
  const bool active_only = active_attrs_only;
  PBVHGPUFormat old_format = *vbo_id;

  GPU_vertformat_clear(&vbo_id->format);

  vbo_id->active_attrs_only = active_attrs_only;

  if (vbo_id->format.attr_len == 0) {
    vbo_id->pos = GPU_vertformat_attr_add(
        &vbo_id->format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    vbo_id->nor = GPU_vertformat_attr_add(
        &vbo_id->format, "nor", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);

    /* TODO: Do not allocate these `.msk` and `.col` when they are not used. */
    vbo_id->msk = GPU_vertformat_attr_add(
        &vbo_id->format, "msk", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);

    vbo_id->totcol = 0;
    if (pbvh_type == PBVH_FACES) {
      int ci = 0;

      Mesh me_query;

      BKE_id_attribute_copy_domains_temp(ID_ME, vdata, NULL, ldata, NULL, NULL, &me_query.id);

      const CustomDataLayer *active_color_layer = BKE_id_attributes_active_color_get(&me_query.id);
      const CustomDataLayer *render_color_layer = BKE_id_attributes_render_color_get(&me_query.id);
      eAttrDomain active_color_domain = active_color_layer ?
                                            BKE_id_attribute_domain(&me_query.id,
                                                                    active_color_layer) :
                                            ATTR_DOMAIN_POINT;

      GPUAttrRef vcol_layers[MAX_GPU_ATTR];
      int totlayer = gpu_pbvh_make_attr_offs(ATTR_DOMAIN_MASK_COLOR,
                                             CD_MASK_COLOR_ALL,
                                             vdata,
                                             NULL,
                                             ldata,
                                             NULL,
                                             vcol_layers,
                                             active_only,
                                             active_color_layer ? active_color_layer->type : -1,
                                             active_color_domain,
                                             active_color_layer,
                                             render_color_layer);

      for (int i = 0; i < totlayer; i++) {
        GPUAttrRef *ref = vcol_layers + i;
        const CustomData *cdata = ref->domain == ATTR_DOMAIN_POINT ? vdata : ldata;

        const CustomDataLayer *layer = cdata->layers + ref->layer_idx;

        if (vbo_id->totcol < MAX_GPU_ATTR) {
          vbo_id->col[ci++] = GPU_vertformat_attr_add(
              &vbo_id->format, "c", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
          vbo_id->totcol++;

          bool is_render = render_color_layer == layer;
          bool is_active = active_color_layer == layer;

          DRW_cdlayer_attr_aliases_add(&vbo_id->format, "c", cdata, layer, is_render, is_active);
        }
      }
    }

    /* ensure at least one vertex color layer */
    if (vbo_id->totcol == 0) {
      vbo_id->col[0] = GPU_vertformat_attr_add(
          &vbo_id->format, "c", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
      vbo_id->totcol = 1;

      GPU_vertformat_alias_add(&vbo_id->format, "ac");
    }

    vbo_id->fset = GPU_vertformat_attr_add(
        &vbo_id->format, "fset", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);

    vbo_id->totuv = 0;
    if (pbvh_type == PBVH_FACES && ldata && CustomData_has_layer(ldata, CD_MLOOPUV)) {
      GPUAttrRef uv_layers[MAX_GPU_ATTR];
      const CustomDataLayer *active = NULL, *render = NULL;

      active = get_active_layer(ldata, CD_MLOOPUV);
      render = get_render_layer(ldata, CD_MLOOPUV);

      int totlayer = gpu_pbvh_make_attr_offs(ATTR_DOMAIN_MASK_CORNER,
                                             CD_MASK_MLOOPUV,
                                             NULL,
                                             NULL,
                                             ldata,
                                             NULL,
                                             uv_layers,
                                             active_only,
                                             CD_MLOOPUV,
                                             ATTR_DOMAIN_CORNER,
                                             active,
                                             render);

      vbo_id->totuv = totlayer;

      for (int i = 0; i < totlayer; i++) {
        GPUAttrRef *ref = uv_layers + i;

        vbo_id->uv[i] = GPU_vertformat_attr_add(
            &vbo_id->format, "uvs", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

        const CustomDataLayer *cl = ldata->layers + ref->layer_idx;
        bool is_active = ref->layer_idx == CustomData_get_active_layer_index(ldata, CD_MLOOPUV);

        DRW_cdlayer_attr_aliases_add(&vbo_id->format, "u", ldata, cl, cl == render, is_active);

        /* Apparently the render attribute is 'a' while active is 'au',
         * at least going by the draw cache extractor code.
         */
        if (cl == render) {
          GPU_vertformat_alias_add(&vbo_id->format, "a");
        }
      }
    }
  }

  if (!gpu_pbvh_format_equals(&old_format, vbo_id)) {
    return true;
  }

  return false;
}

GPUBatch *GPU_pbvh_buffers_batch_get(GPU_PBVH_Buffers *buffers, bool fast, bool wires)
{
  if (wires) {
    return (fast && buffers->lines_fast) ? buffers->lines_fast : buffers->lines;
  }

  return (fast && buffers->triangles_fast) ? buffers->triangles_fast : buffers->triangles;
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
  if (buffers->vert_buf && GPU_vertbuf_get_data(buffers->vert_buf)) {
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
