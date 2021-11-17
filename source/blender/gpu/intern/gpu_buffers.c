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
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_meshdata_types.h"
#include "DNA_userdef_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_attribute.h"
#include "BKE_ccg.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"

#include "GPU_batch.h"
#include "GPU_buffers.h"

#include "DRW_engine.h"

#include "gpu_private.h"

#include "bmesh.h"

//#define GPU_PERF_TEST  // will disable vcol, uvs, mask, fset colors, etc

/* XXX: the rest of the code in this file is used for optimized PBVH
 * drawing and doesn't interact at all with the buffer code above */

/*
this tests a low-data draw mode.  faceset and mask overlays must be disabled,
meshes cannot have uv layers, and DynTopo must be on, along with "draw smooth."

Normalizes coordinates to 16 bit integers, normals to 8-bit bytes, and skips
all other attributes.

To test, enable #if 0 branch in sculpt_draw_cb in draw_manager_data.c
*/

//#define NEW_ATTR_SYSTEM

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

  void *last_tribuf_tris;  // used to detect if we can reuse index buffers

  bool show_overlay;
};

#ifdef NEW_ATTR_SYSTEM
typedef struct CDLayerType {
  int type;
  char gpu_attr_name[32];
  GPUVertCompType source_type;
  GPUVertCompType comp_type;
  uint comp_len;
  GPUVertFetchMode fetch_mode;
  char gpu_attr_code[8];
} CDLayerType;

typedef struct CDAttrLayers {
  CDLayerType type;
  uint totlayer;
  uint *layers;
  int *offsets;
  uint *attrs;
} CDAttrLayers;
#endif

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

#ifdef NEW_ATTR_SYSTEM
  CDAttrLayers *vertex_attrs;
  CDAttrLayers *loop_attrs;
  int vertex_attrs_len;
  int loop_attrs_len;
#endif

  bool active_vcol_only;
  bool need_full_render;
  bool fast_mode;
} PBVHGPUFormat;

static PBVHGPUFormat g_vbo_id = {{0}};

static int gpu_pbvh_gpu_make_attr_offs(AttributeDomainMask domain_mask,
                                       CustomDataMask type_mask,
                                       const CustomData *vdata,
                                       const CustomData *edata,
                                       const CustomData *ldata,
                                       const CustomData *pdata,
                                       GPUAttrRef r_cd_vcols[MAX_GPU_ATTR],
                                       bool active_only,
                                       int active_type,
                                       int active_domain,
                                       const CustomDataLayer *active_vcol_layer,
                                       const CustomDataLayer *render_vcol_layer);

#ifdef NEW_ATTR_SYSTEM
static CDLayerType cd_vert_layers[] = {
    {CD_PROP_COLOR, "c", GPU_COMP_F32, GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT, "c"}};
static CDLayerType cd_loop_layers[] = {
    {CD_MLOOPUV, "uvs", GPU_COMP_F32, GPU_COMP_F32, 2, GPU_FETCH_FLOAT, "u"}};

static void build_cd_layers(GPUVertFormat *format,
                            CDAttrLayers *cdattr,
                            CustomData *cd,
                            CDLayerType *type)
{
  uint *layers = NULL;
  int *offsets = NULL;
  uint *attrs = NULL;

  BLI_array_declare(layers);
  BLI_array_declare(offsets);
  BLI_array_declare(attrs);

  cdattr->type = *type;
  cdattr->totlayer = 0;

  int act = 0;
  int actidx = CustomData_get_active_layer_index(cd, type->type);

  for (int i = 0; i < cd->totlayer; i++) {
    CustomDataLayer *cl = cd->layers + i;

    if (cl->type != type->type || (cl->flag & CD_FLAG_TEMPORARY)) {
      continue;
    }

    cdattr->totlayer++;

    /*
              g_vbo_id.col[ci++] = GPU_vertformat_attr_add(
              &g_vbo_id.format, "c", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
          g_vbo_id.totcol++;

          DRW_make_cdlayer_attr_aliases(&g_vbo_id.format, "c", vdata, cl);

          if (idx == act) {
            GPU_vertformat_alias_add(&g_vbo_id.format, "ac");
          }

    */

    uint attr = GPU_vertformat_attr_add(
        format, type->gpu_attr_name, type->comp_type, type->comp_len, type->fetch_mode);

    BLI_array_append(layers, i);
    BLI_array_append(offsets, cl->offset);
    BLI_array_append(attrs, attr);

    DRW_make_cdlayer_attr_aliases(format, type->gpu_attr_code, cd, cl);

    if (i == actidx) {
      char buf[128];

      BLI_snprintf(buf, sizeof(buf), "a%s", type->gpu_attr_code);
      GPU_vertformat_alias_add(&g_vbo_id.format, buf);
    }
  }

  cdattr->offsets = offsets;
  cdattr->layers = layers;
  cdattr->attrs = attrs;
}

/*must match GPUVertCompType*/
static int gpu_comp_map[] = {
    1,  //  GPU_COMP_I8 = 0,
    1,  // GPU_COMP_U8,
    2,  // GPU_COMP_I16,
    2,  // GPU_COMP_U16,
    4,  // GPU_COMP_I32,
    4,  // GPU_COMP_U32,

    4,  // GPU_COMP_F32,

    4  // GPU_COMP_I10,
};

static void convert_gpu_data(void *src,
                             void *dst,
                             GPUVertCompType srcType,
                             GPUVertCompType dstType)
{
  if (srcType == dstType) {
    memcpy(dst, src, gpu_comp_map[(int)srcType]);
    return;
  }

  double val = 0;

  switch (srcType) {
    case GPU_COMP_I8:
      val = ((float)*((signed char *)(src))) / 127.0;
      break;
    case GPU_COMP_U8:
      val = ((float)*((unsigned char *)(src))) / 255.0;
      break;
    case GPU_COMP_I16:
      val = ((float)*((unsigned short *)(src))) / 32767.0;
      break;
    case GPU_COMP_U16:
      val = ((float)*((signed short *)(src))) / 65535.0;
      break;
    case GPU_COMP_I32:
      val = ((float)*((signed int *)(src))) / 2147483647.0;
      break;
    case GPU_COMP_U32:
      val = ((float)*((unsigned int *)(src))) / 4294967295.0;
      break;
    case GPU_COMP_F32:
      val = *(float *)src;
      break;
    case GPU_COMP_I10:  // handle elsewhere
      break;
  }

  switch (dstType) {
    case GPU_COMP_I8:
      *((signed char *)dst) = (signed char)(val * 127.0);
      break;
    case GPU_COMP_U8:
      *((unsigned char *)dst) = (unsigned char)(val * 255.0);
      break;
    case GPU_COMP_I16:
      *((signed short *)dst) = (signed short)(val * 32767.0);
      break;
    case GPU_COMP_U16:
      *((unsigned short *)dst) = (unsigned short)(val * 65535.0);
      break;
    case GPU_COMP_I32:
      *((signed int *)dst) = (signed int)(val * 2147483647.0);
      break;
    case GPU_COMP_U32:
      *((unsigned int *)dst) = (unsigned int)(val * 4294967295.0);
      break;
    case GPU_COMP_F32:
      *((float *)dst) = (float)val;
      break;
    case GPU_COMP_I10:  // handle elsewhere
      break;
  }
}

/*
  GPUVertBuf *vert_buf
  GPU_vertbuf_attr_set(vert_buf, g_vbo_id.pos, v_index, v->co);
*/

static void set_cd_data_bmesh(
    GPUVertBuf *vert_buf, CDAttrLayers *attr_array, int attr_array_len, BMElem *elem, int vertex)
{
  for (int i = 0; i < attr_array_len; i++) {
    CDAttrLayers *attr = attr_array + i;

    int dst_size = gpu_comp_map[(int)attr->type.comp_type];
    int src_size = gpu_comp_map[(int)attr->type.source_type];
    void *dest = alloca(dst_size *
                        attr->type.comp_len);  // ensure proper alignment by making this a
    void *dest2 = dest;

    for (int j = 0; j < attr->totlayer; j++) {
      void *data = BM_ELEM_CD_GET_VOID_P(elem, attr->offsets[j]);

      for (int k = 0; k < attr->type.comp_len; k++) {
        convert_gpu_data(data, dest2, attr->type.source_type, attr->type.comp_type);

        data = (void *)(((char *)data) + src_size);
        dest2 = (void *)(((char *)dest2) + dst_size);
      }

      GPU_vertbuf_attr_set(vert_buf, attr->attrs[j], vertex, dest);
    }
  }
}

static void free_cd_layers(CDAttrLayers *cdattr)
{
  MEM_SAFE_FREE(cdattr->layers);
  MEM_SAFE_FREE(cdattr->offsets);
  MEM_SAFE_FREE(cdattr->attrs);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name PBVH Utils
 * \{ */

void gpu_pbvh_init()
{
  GPU_pbvh_update_attribute_names(NULL, NULL, false, false, -1, -1, NULL, NULL);
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
static bool gpu_pbvh_vert_buf_data_set(GPU_PBVH_Buffers *buffers, uint vert_len)
{
  /* Keep so we can test #GPU_USAGE_DYNAMIC buffer use.
   * Not that format initialization match in both blocks.
   * Do this to keep braces balanced - otherwise indentation breaks. */
#if 0
  if (buffers->vert_buf == NULL) {
    /* Initialize vertex buffer (match 'VertexBufferFormat'). */
    buffers->vert_buf = GPU_vertbuf_create_with_format_ex(&g_vbo_id.format, GPU_USAGE_DYNAMIC);
  }
  if (GPU_vertbuf_get_data(buffers->vert_buf) == NULL ||
      GPU_vertbuf_get_vertex_len(buffers->vert_buf) != vert_len) {
    /* Allocate buffer if not allocated yet or size changed. */
    GPU_vertbuf_data_alloc(buffers->vert_buf, vert_len);
  }
#else
  if (buffers->vert_buf == NULL) {
    /* Initialize vertex buffer (match 'VertexBufferFormat'). */
    buffers->vert_buf = GPU_vertbuf_create_with_format_ex(&g_vbo_id.format, GPU_USAGE_STATIC);
  }
  if (GPU_vertbuf_get_data(buffers->vert_buf) == NULL ||
      GPU_vertbuf_get_vertex_len(buffers->vert_buf) != vert_len) {
    /* Allocate buffer if not allocated yet or size changed. */
    GPU_vertbuf_data_alloc(buffers->vert_buf, vert_len);
  }
#endif

  return GPU_vertbuf_get_data(buffers->vert_buf) != NULL;
}

/* was used by QUANTIZED_PERF_TEST, now unused */
float *GPU_pbvh_get_extra_matrix(GPU_PBVH_Buffers *buffers)
{
  return NULL;
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
                                  const CustomData *vdata,
                                  const CustomData *ldata,
                                  const float *vmask,
                                  const CustomDataLayer *active_vcol_layer,
                                  const CustomDataLayer *render_vcol_layer,
                                  const AttributeDomain active_vcol_domain,
                                  const int *sculpt_face_sets,
                                  const int face_sets_color_seed,
                                  const int face_sets_color_default,
                                  const int update_flags)
{
  GPUAttrRef vcol_refs[MAX_GPU_ATTR];
  GPUAttrRef cd_uvs[MAX_GPU_ATTR];

  int totcol = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER,
                                           CD_MASK_PROP_COLOR | CD_MASK_MLOOPCOL,
                                           vdata,
                                           NULL,
                                           ldata,
                                           NULL,
                                           vcol_refs,
                                           g_vbo_id.active_vcol_only,
                                           active_vcol_layer ? active_vcol_layer->type : -1,
                                           active_vcol_domain,
                                           active_vcol_layer,
                                           render_vcol_layer);

  int cd_uv_count = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_CORNER,
                                                CD_MASK_MLOOPUV,
                                                NULL,
                                                NULL,
                                                ldata,
                                                NULL,
                                                cd_uvs,
                                                g_vbo_id.active_vcol_only,
                                                CD_MLOOPUV,
                                                ATTR_DOMAIN_CORNER,
                                                get_active_layer(ldata, CD_MLOOPUV),
                                                get_render_layer(ldata, CD_MLOOPUV));

  const bool show_vcol = totcol > 0 && (update_flags & GPU_PBVH_BUFFERS_SHOW_VCOL) != 0;

  const bool show_mask = vmask && (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
  const bool show_face_sets = sculpt_face_sets &&
                              (update_flags & GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS) != 0;
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
      GPUVertBufRaw uv_step = {0};

      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.pos, &pos_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.nor, &nor_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.msk, &msk_step);
      GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.fset, &fset_step);

      /* calculate normal for each polygon only once */
      uint mpoly_prev = UINT_MAX;
      short no[3] = {0, 0, 0};

      if (cd_uv_count > 0) {
        for (int uv_i = 0; uv_i < cd_uv_count; uv_i++) {
          GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.uv[uv_i], &uv_step);

          GPUAttrRef *ref = cd_uvs + uv_i;
          CustomDataLayer *layer = ldata->layers + ref->layer_idx;
          MLoopUV *muv = layer->data;

          for (uint i = 0; i < buffers->face_indices_len; i++) {
            const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];

            if (!gpu_pbvh_is_looptri_visible(lt, mvert, buffers->mloop, sculpt_face_sets)) {
              continue;
            }

            for (uint j = 0; j < 3; j++) {
              MLoopUV *muv2 = muv + lt->tri[j];

              memcpy(GPU_vertbuf_raw_step(&uv_step), muv2->uv, sizeof(muv2->uv));
            }
          }
        }
      }

      if (show_vcol) {
        for (int col_i = 0; col_i < totcol; col_i++) {
          GPU_vertbuf_attr_get_raw_data(buffers->vert_buf, g_vbo_id.col[col_i], &col_step);

          MPropCol *pcol = NULL;
          MLoopCol *mcol = NULL;

          GPUAttrRef *ref = vcol_refs + col_i;
          const CustomData *cdata = ref->domain == ATTR_DOMAIN_POINT ? vdata : ldata;
          CustomDataLayer *layer = cdata->layers + ref->layer_idx;

          bool color_loops = ref->domain == ATTR_DOMAIN_CORNER;

          if (layer->type == CD_PROP_COLOR) {
            pcol = (MPropCol *)layer->data;
          }
          else {
            mcol = (MLoopCol *)layer->data;
          }

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

            for (uint j = 0; j < 3; j++) {
              /* Vertex Colors. */
              if (show_vcol) {
                ushort scol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
                if (pcol) {
                  if (color_loops) {
                    scol[0] = unit_float_to_ushort_clamp(pcol[lt->tri[j]].color[0]);
                    scol[1] = unit_float_to_ushort_clamp(pcol[lt->tri[j]].color[1]);
                    scol[2] = unit_float_to_ushort_clamp(pcol[lt->tri[j]].color[2]);
                    scol[3] = unit_float_to_ushort_clamp(pcol[lt->tri[j]].color[3]);
                  }
                  else {
                    scol[0] = unit_float_to_ushort_clamp(pcol[vtri[j]].color[0]);
                    scol[1] = unit_float_to_ushort_clamp(pcol[vtri[j]].color[1]);
                    scol[2] = unit_float_to_ushort_clamp(pcol[vtri[j]].color[2]);
                    scol[3] = unit_float_to_ushort_clamp(pcol[vtri[j]].color[3]);
                  }

                  memcpy(GPU_vertbuf_raw_step(&col_step), scol, sizeof(scol));
                }
                else if (mcol) {
                  const uint loop_index = lt->tri[j];
                  const MLoopCol *mcol2 = mcol + (color_loops ? loop_index : vtri[j]);

                  scol[0] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->r]);
                  scol[1] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->g]);
                  scol[2] = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mcol2->b]);
                  scol[3] = unit_float_to_ushort_clamp(mcol2->a * (1.0f / 255.0f));
                  memcpy(GPU_vertbuf_raw_step(&col_step), scol, sizeof(scol));
                }
              }
            }
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
            copy_v3_v3_short(no, v->no);
          }
          copy_v3_v3_short(GPU_vertbuf_raw_step(&nor_step), no);

          if (show_mask && buffers->smooth) {
            cmask = (uchar)(vmask[vtri[j]] * 255);
          }

          *(uchar *)GPU_vertbuf_raw_step(&msk_step) = cmask;
          empty_mask = empty_mask && (cmask == 0);

          if (!g_vbo_id.fast_mode) {
            /* Face Sets. */
            memcpy(GPU_vertbuf_raw_step(&fset_step), face_set_color, sizeof(uchar[3]));
          }
        }
      }
    }

    gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
  }

  /* Get material index from the first face of this buffer. */
  const MLoopTri *lt = &buffers->looptri[buffers->face_indices[0]];
  const MPoly *mp = &buffers->mpoly[lt->poly];
  buffers->material_index = mp->mat_nr;

  buffers->show_overlay = (!empty_mask || !default_face_set) && !g_vbo_id.fast_mode;
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
  buffers->smooth = (mpoly[looptri[face_indices[0]].poly].flag & ME_SMOOTH) || g_vbo_id.fast_mode;

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

  if (buffers->smooth || g_vbo_id.fast_mode) {
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
  const bool smooth = (grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH) || g_vbo_id.fast_mode;

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
  const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0 && !g_vbo_id.fast_mode;
  const bool show_vcol = (update_flags & GPU_PBVH_BUFFERS_SHOW_VCOL) != 0;
  const bool show_face_sets = sculpt_face_sets &&
                              (update_flags & GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS) != 0 &&
                              !g_vbo_id.fast_mode;
  bool empty_mask = true;
  bool default_face_set = true;

  int i, j, k, x, y;

  /* Build VBO */
  const int has_mask = key->has_mask;

  bool smooth = (grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH) || g_vbo_id.fast_mode;
  ;
  if (smooth != buffers->smooth) {
    GPU_pbvh_grid_buffers_update_free(buffers, grid_flag_mats, grid_indices);
  }

  buffers->smooth = smooth;

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
          BKE_paint_face_set_overlay_color_get(fset, face_sets_color_seed, face_set_color);
          default_face_set = false;
        }
      }

      if (buffers->smooth || g_vbo_id.fast_mode) {
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
              const ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col[0], vbo_index, &vcol);
            }

            if (!g_vbo_id.fast_mode) {
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.fset, vbo_index, &face_set_color);
            }

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

            const ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
            if (1) {  // g_vbo_id.totcol > 0 || !g_vbo_id.fast_mode) {
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col[0], vbo_index + 0, &vcol);
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col[0], vbo_index + 1, &vcol);
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col[0], vbo_index + 2, &vcol);
              GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.col[0], vbo_index + 3, &vcol);
            }

            if (!g_vbo_id.fast_mode) {
              GPU_vertbuf_attr_set(
                  buffers->vert_buf, g_vbo_id.fset, vbo_index + 0, &face_set_color);
              GPU_vertbuf_attr_set(
                  buffers->vert_buf, g_vbo_id.fset, vbo_index + 1, &face_set_color);
              GPU_vertbuf_attr_set(
                  buffers->vert_buf, g_vbo_id.fset, vbo_index + 2, &face_set_color);
              GPU_vertbuf_attr_set(
                  buffers->vert_buf, g_vbo_id.fset, vbo_index + 3, &face_set_color);
            }

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
  buffers->show_overlay = (!empty_mask || !default_face_set) && !g_vbo_id.fast_mode;
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

static int debug_pass = 0;
bool pbvh_show_orig_co = false;

static void gpu_bmesh_get_vcol(BMVert *v, BMLoop *l, const GPUAttrRef *ref, float color[4])
{
  if (ref->domain == ATTR_DOMAIN_POINT) {
    switch (ref->type) {
      case CD_PROP_COLOR:
        copy_v4_v4(color, (float *)BM_ELEM_CD_GET_VOID_P(v, ref->cd_offset));
        break;
      case CD_MLOOPCOL: {
        MLoopCol *mp = (MLoopCol *)BM_ELEM_CD_GET_VOID_P(v, ref->cd_offset);

        rgba_uchar_to_float(color, (const char *)mp);
        srgb_to_linearrgb_v3_v3(color, color);

        break;
      }
    }
  }
  else if (l) {
    switch (ref->type) {
      case CD_PROP_COLOR:
        copy_v4_v4(color, (float *)BM_ELEM_CD_GET_VOID_P(l, ref->cd_offset));
        break;
      case CD_MLOOPCOL: {
        MLoopCol *mp = (MLoopCol *)BM_ELEM_CD_GET_VOID_P(l, ref->cd_offset);

        rgba_uchar_to_float(color, (const char *)mp);
        srgb_to_linearrgb_v3_v3(color, color);

        break;
      }
    }
  }
  else { /*average all loop colors*/
    BMEdge *e = v->e;

    zero_v4(color);

    if (!e) {
      return;
    }

    int tot = 0;

    do {
      BMLoop *l = e->l;

      if (!l) {
        continue;
      }

      do {
        switch (ref->type) {
          case CD_PROP_COLOR:
            add_v4_v4(color, (float *)BM_ELEM_CD_GET_VOID_P(l, ref->cd_offset));
            tot++;

            break;
          case CD_MLOOPCOL: {
            MLoopCol *mp = (MLoopCol *)BM_ELEM_CD_GET_VOID_P(l, ref->cd_offset);

            float temp[4];

            rgba_uchar_to_float(temp, (const char *)mp);
            srgb_to_linearrgb_v3_v3(temp, temp);

            add_v4_v4(color, temp);
            tot++;
            break;
          }
        }
      } while ((l = l->radial_next) != e->l);
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

    if (tot > 0) {
      mul_v4_fl(color, 1.0f / (float)tot);
    }
  }
}

/* Output a BMVert into a VertexBufferFormat array at v_index. */
static void gpu_bmesh_vert_to_buffer_copy(BMesh *bm,
                                          BMVert *v,
                                          BMLoop *l,
                                          GPUVertBuf *vert_buf,
                                          int v_index,
                                          const float fno[3],
                                          const float *fmask,
                                          const int cd_vert_mask_offset,
                                          const int cd_vert_node_offset,
                                          const bool show_mask,
                                          const bool show_vcol,
                                          bool *empty_mask,
                                          const GPUAttrRef *vcol_layers,
                                          const int totvcol)
{
  /* Vertex should always be visible if it's used by a visible face. */
  BLI_assert(!BM_elem_flag_test(v, BM_ELEM_HIDDEN));

#ifdef NEW_ATTR_SYSTEM
  // static void set_cd_data_bmesh(GPUVertBuf *vert_buf, CDAttrLayers *attr, BMElem *elem, int
  // vertex)
  set_cd_data_bmesh(
      vert_buf, g_vbo_id.vertex_attrs, g_vbo_id.vertex_attrs_len, (BMElem *)v, v_index);
#endif

  short no_short[3];

  /* Set coord, normal, and mask */
  if (G.debug_value == 890 || pbvh_show_orig_co) {
    const int cd_sculpt_vert = bm->vdata.layers[bm->vdata.typemap[CD_DYNTOPO_VERT]].offset;
    MSculptVert *mv = BM_ELEM_CD_GET_VOID_P(v, cd_sculpt_vert);

    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.pos, v_index, mv->origco);
    normal_float_to_short_v3(no_short, mv->origno);
  }
  else {
    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.pos, v_index, v->co);
    normal_float_to_short_v3(no_short, fno ? fno : v->no);
  }

  GPU_vertbuf_attr_set(vert_buf, g_vbo_id.nor, v_index, no_short);

#ifndef GPU_PERF_TEST
  if (show_mask) {
    float effective_mask = fmask ? *fmask : BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);

    if (G.debug_value == 889) {
      int ni = BM_ELEM_CD_GET_INT(v, cd_vert_node_offset);

      effective_mask = ni == -1 ? 0.0f : (float)(((ni + debug_pass) * 511) % 64) / 64;
    }

    uchar cmask = (uchar)(effective_mask * 255);
    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.msk, v_index, &cmask);
    *empty_mask = *empty_mask && (cmask == 0);
  }

#  ifndef NEW_ATTR_SYSTEM
  if (show_vcol && totvcol > 0) {
    for (int i = 0; i < totvcol; i++) {
      float color[4];
      gpu_bmesh_get_vcol(v, l, vcol_layers + i, color);

      ushort vcol[4];

      vcol[0] = unit_float_to_ushort_clamp(color[0]);
      vcol[1] = unit_float_to_ushort_clamp(color[1]);
      vcol[2] = unit_float_to_ushort_clamp(color[2]);
      vcol[3] = unit_float_to_ushort_clamp(color[3]);

      // const ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
      GPU_vertbuf_attr_set(vert_buf, g_vbo_id.col[i], v_index, vcol);
    }
  }
  else if (show_vcol && !g_vbo_id.fast_mode) {  // ensure first vcol attribute is not zero
    const ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.col[0], v_index, vcol);
  }
#  else
  if (show_vcol && totvcol == 0) {  // ensure first vcol attribute is not zero
    const ushort vcol[4] = {USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.col[0], v_index, vcol);
  }
#  endif

  if (!g_vbo_id.fast_mode) {
    /* Add default face sets color to avoid artifacts. */
    const uchar face_set[3] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.fset, v_index, &face_set);
  }
#endif
}

/* Return the total number of vertices that don't have BM_ELEM_HIDDEN set */
static int gpu_bmesh_vert_visible_count(TableGSet *bm_unique_verts, TableGSet *bm_other_verts)
{
  int totvert = 0;
  BMVert *v;

  TGSET_ITER (v, bm_unique_verts) {
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      totvert++;
    }
  }
  TGSET_ITER_END

  TGSET_ITER (v, bm_other_verts) {
    if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      totvert++;
    }
  }
  TGSET_ITER_END

  return totvert;
}

/* Return the total number of visible faces */
static int gpu_bmesh_face_visible_count(PBVHTriBuf *tribuf, int mat_nr)
{
  int totface = 0;

  for (int i = 0; i < tribuf->tottri; i++) {
    PBVHTri *tri = tribuf->tris + i;

    BMFace *f = (BMFace *)tri->f.i;
    if (f->mat_nr != mat_nr || BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    totface++;
  }

  return totface;
}

void GPU_pbvh_bmesh_buffers_update_free(GPU_PBVH_Buffers *buffers)
{
  if (buffers->last_tribuf_tris) {
    // bmesh indexed drawing frees buffers by itself
    return;
  }

  GPU_BATCH_DISCARD_SAFE(buffers->lines);
  GPU_INDEXBUF_DISCARD_SAFE(buffers->index_lines_buf);
}

static int gpu_pbvh_gpu_make_attr_offs(AttributeDomainMask domain_mask,
                                       CustomDataMask type_mask,
                                       const CustomData *vdata,
                                       const CustomData *edata,
                                       const CustomData *ldata,
                                       const CustomData *pdata,
                                       GPUAttrRef r_cd_vcols[MAX_GPU_ATTR],
                                       bool active_only,
                                       int active_type,
                                       int active_domain,
                                       const CustomDataLayer *active_vcol_layer,
                                       const CustomDataLayer *render_vcol_layer)
{
  if (active_only) {
    const CustomData *cdata = active_domain == ATTR_DOMAIN_POINT ? vdata : ldata;

    int idx = active_vcol_layer ? active_vcol_layer - cdata->layers : -1;

    if (idx >= 0 && idx < cdata->totlayer) {
      r_cd_vcols[0].cd_offset = cdata->layers[idx].offset;
      r_cd_vcols[0].domain = active_domain;
      r_cd_vcols[0].type = active_type;
      r_cd_vcols[0].layer_idx = idx;

      return 1;
    }

    return 0;
  }

  const CustomData *datas[4] = {vdata, edata, pdata, ldata};

  int count = 0;
  for (AttributeDomain domain = 0; domain < 4; domain++) {
    const CustomData *cdata = datas[domain];

    if (!cdata || !((1 << domain) & domain_mask)) {
      continue;
    }

    CustomDataLayer *cl = cdata->layers;

    for (int i = 0; count < MAX_GPU_ATTR && i < cdata->totlayer; i++, cl++) {
      if (CD_TYPE_AS_MASK(cl->type) & type_mask && !(cl->flag & CD_FLAG_TEMPORARY)) {
        GPUAttrRef *ref = r_cd_vcols + count;

        ref->cd_offset = cl->offset;
        ref->type = cl->type;
        ref->layer_idx = i;
        ref->domain = domain;

        count++;
      }
    }
  }

  /* ensure render layer is last
    draw cache code seems to need this
   */

  for (int i = 0; i < count; i++) {
    GPUAttrRef *ref = r_cd_vcols + i;
    const CustomData *cdata = datas[ref->domain];

    if (cdata->layers + ref->layer_idx == render_vcol_layer) {
      SWAP(GPUAttrRef, r_cd_vcols[i], r_cd_vcols[count - 1]);
      break;
    }
  }

  return count;
}

void GPU_pbvh_need_full_render_set(bool state)
{
  g_vbo_id.need_full_render = state;
  g_vbo_id.active_vcol_only = !state;
}

bool GPU_pbvh_need_full_render_get()
{
  return g_vbo_id.need_full_render;
}

static bool gpu_pbvh_format_equals(PBVHGPUFormat *a, PBVHGPUFormat *b)
{
  bool bad = false;

  bad |= a->active_vcol_only != b->active_vcol_only;
  bad |= a->fast_mode != b->fast_mode;
  bad |= a->need_full_render != b->need_full_render;

#ifdef NEW_ATTR_SYSTEM
  bad |= a->vertex_attrs_len != b->vertex_attrs_len;
  bad |= a->loop_attrs_len != b->loop_attrs_len;
#endif

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

bool GPU_pbvh_update_attribute_names(CustomData *vdata,
                                     CustomData *ldata,
                                     bool need_full_render,
                                     bool fast_mode,
                                     int active_vcol_type,
                                     int active_vcol_domain,
                                     CustomDataLayer *active_vcol_layer,
                                     CustomDataLayer *render_vcol_layer)
{
  const bool active_only = !need_full_render;
  PBVHGPUFormat old_format = g_vbo_id;

  debug_pass++;

  GPU_vertformat_clear(&g_vbo_id.format);

  g_vbo_id.fast_mode = fast_mode;

  // g_vbo_id.loop_attrs = build_cd_layers(vdata, )
  /* Initialize vertex buffer (match 'VertexBufferFormat'). */
  if (g_vbo_id.format.attr_len == 0) {
    g_vbo_id.pos = GPU_vertformat_attr_add(
        &g_vbo_id.format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    g_vbo_id.nor = GPU_vertformat_attr_add(
        &g_vbo_id.format, "nor", GPU_COMP_I16, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);

    /* TODO: Do not allocate these `.msk` and `.col` when they are not used. */

    if (!fast_mode) {
      g_vbo_id.msk = GPU_vertformat_attr_add(
          &g_vbo_id.format, "msk", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }

    g_vbo_id.totcol = 0;

#ifdef NEW_ATTR_SYSTEM
    if (g_vbo_id.loop_attrs) {
      free_cd_layers(g_vbo_id.loop_attrs);
    }
    if (g_vbo_id.vertex_attrs) {
      free_cd_layers(g_vbo_id.vertex_attrs);
    }

    CDAttrLayers *vattr = NULL, *lattr = NULL;
    BLI_array_declare(vattr);
    BLI_array_declare(lattr);

    for (int i = 0; vdata && i < ARRAY_SIZE(cd_vert_layers); i++) {
      if (!CustomData_has_layer(vdata, cd_vert_layers[i].type)) {
        continue;
      }

      CDAttrLayers attr;
      build_cd_layers(&g_vbo_id.format, &attr, vdata, cd_vert_layers + i);
      BLI_array_append(vattr, attr);
    }

    for (int i = 0; ldata && i < ARRAY_SIZE(cd_loop_layers); i++) {
      if (!CustomData_has_layer(ldata, cd_loop_layers[i].type)) {
        continue;
      }

      CDAttrLayers attr;
      build_cd_layers(&g_vbo_id.format, &attr, ldata, cd_loop_layers + i);
      BLI_array_append(lattr, attr);
    }

    g_vbo_id.vertex_attrs = vattr;
    g_vbo_id.loop_attrs = lattr;
    g_vbo_id.vertex_attrs_len = BLI_array_len(vattr);
    g_vbo_id.loop_attrs_len = BLI_array_len(lattr);
#endif

#if !defined(NEW_ATTR_SYSTEM) && !defined(GPU_PERF_TEST)
    if (active_vcol_type != -1) {
      int ci = 0;

      GPUAttrRef vcol_layers[MAX_GPU_ATTR];
      int totlayer = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER,
                                                 CD_MASK_PROP_COLOR | CD_MASK_MLOOPCOL,
                                                 vdata,
                                                 NULL,
                                                 ldata,
                                                 NULL,
                                                 vcol_layers,
                                                 active_only,
                                                 active_vcol_type,
                                                 active_vcol_domain,
                                                 active_vcol_layer,
                                                 render_vcol_layer);

      for (int i = 0; i < totlayer; i++) {
        GPUAttrRef *ref = vcol_layers + i;
        CustomData *cdata = ref->domain == ATTR_DOMAIN_POINT ? vdata : ldata;

        CustomDataLayer *cl = cdata->layers + ref->layer_idx;

        if (g_vbo_id.totcol < MAX_GPU_ATTR) {
          g_vbo_id.col[ci++] = GPU_vertformat_attr_add(
              &g_vbo_id.format, "c", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
          g_vbo_id.totcol++;

          bool is_render = cl == render_vcol_layer;
          bool is_active = cl == active_vcol_layer;

          DRW_make_cdlayer_attr_aliases(&g_vbo_id.format, "c", cdata, cl, is_render, is_active);

          if (cl == active_vcol_layer) {
            GPU_vertformat_alias_add(&g_vbo_id.format, "ac");
          }
        }
      }
    }

    // ensure at least one vertex color layer
    if (!fast_mode && g_vbo_id.totcol == 0) {
      g_vbo_id.col[0] = GPU_vertformat_attr_add(
          &g_vbo_id.format, "c", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
      g_vbo_id.totcol = 1;

      GPU_vertformat_alias_add(&g_vbo_id.format, "ac");
    }
#elif !defined(GPU_PERF_TEST)
    // ensure at least one vertex color layer
    if (!vdata || !CustomData_has_layer(vdata, CD_PROP_COLOR)) {
      g_vbo_id.col[0] = GPU_vertformat_attr_add(
          &g_vbo_id.format, "c", GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
      g_vbo_id.totcol = 1;

      GPU_vertformat_alias_add(&g_vbo_id.format, "ac");
    }

#endif

#ifndef GPU_PERF_TEST
    if (!fast_mode) {
      g_vbo_id.fset = GPU_vertformat_attr_add(
          &g_vbo_id.format, "fset", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }

    if (ldata && CustomData_has_layer(ldata, CD_MLOOPUV)) {
      GPUAttrRef uv_layers[MAX_GPU_ATTR];
      CustomDataLayer *active = NULL, *render = NULL;
      int idx;

      idx = CustomData_get_active_layer_index(ldata, CD_MLOOPUV);
      if (idx != -1) {
        active = ldata->layers + idx;
      }

      idx = CustomData_get_render_layer_index(ldata, CD_MLOOPUV);
      if (idx != -1) {
        render = ldata->layers + idx;
      }

      int totlayer = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_CORNER,
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

      g_vbo_id.totuv = totlayer;

      for (int i = 0; i < totlayer; i++) {
        GPUAttrRef *ref = uv_layers + i;

        g_vbo_id.uv[i] = GPU_vertformat_attr_add(
            &g_vbo_id.format, "uvs", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

        CustomDataLayer *cl = ldata->layers + ref->layer_idx;
        bool is_active = ref->layer_idx == CustomData_get_active_layer_index(ldata, CD_MLOOPUV);

        DRW_make_cdlayer_attr_aliases(&g_vbo_id.format, "u", ldata, cl, cl == render, is_active);
      }
    }
#endif
  }

  if (!gpu_pbvh_format_equals(&old_format, &g_vbo_id)) {
    printf("gpu format update detected\n");
    return true;
  }

  return false;
}

static void gpu_flat_vcol_make_vert(float co[3],
                                    BMVert *v,
                                    BMLoop *l,
                                    GPUVertBuf *vert_buf,
                                    int v_index,
                                    GPUAttrRef vcol_refs[MAX_GPU_ATTR],
                                    int totoffsets,
                                    const float fno[3])
{
  for (int i = 0; i < totoffsets; i++) {
    float color[4];

    gpu_bmesh_get_vcol(v, l, vcol_refs + i, color);

    ushort vcol[4];

    // printf(
    //    "%.2f %.2f %.2f %.2f\n", mp->color[0], mp->color[1], mp->color[2], mp->color[3]);
    vcol[0] = unit_float_to_ushort_clamp(color[0]);
    vcol[1] = unit_float_to_ushort_clamp(color[1]);
    vcol[2] = unit_float_to_ushort_clamp(color[2]);
    vcol[3] = unit_float_to_ushort_clamp(color[3]);

    GPU_vertbuf_attr_set(vert_buf, g_vbo_id.col[i], v_index, vcol);
  }

  /* Set coord, normal, and mask */
  GPU_vertbuf_attr_set(vert_buf, g_vbo_id.pos, v_index, co);

  short no_short[3];

  normal_float_to_short_v3(no_short, fno ? fno : v->no);
  GPU_vertbuf_attr_set(vert_buf, g_vbo_id.nor, v_index, no_short);
}

/* Creates a vertex buffer (coordinate, normal, color) and, if smooth
 * shading, an element index buffer.
 * Threaded - do not call any functions that use OpenGL calls! */
static void GPU_pbvh_bmesh_buffers_update_flat_vcol(GPU_PBVH_Buffers *buffers,
                                                    BMesh *bm,
                                                    TableGSet *bm_faces,
                                                    TableGSet *bm_unique_verts,
                                                    TableGSet *bm_other_verts,
                                                    PBVHTriBuf *tribuf,
                                                    const int update_flags,
                                                    const int cd_vert_node_offset,
                                                    int face_sets_color_seed,
                                                    int face_sets_color_default,
                                                    short mat_nr,
                                                    int active_vcol_type,
                                                    int active_vcol_domain,
                                                    CustomDataLayer *active_vcol_layer,
                                                    CustomDataLayer *render_vcol_layer)
{
  bool active_vcol_only = g_vbo_id.active_vcol_only;

  const bool show_face_sets = CustomData_has_layer(&bm->pdata, CD_SCULPT_FACE_SETS) &&
                              (update_flags & GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS) != 0;

  int tottri, totvert;
  bool empty_mask = true;
  int cd_fset_offset = CustomData_get_offset(&bm->pdata, CD_SCULPT_FACE_SETS);

  GPUAttrRef cd_vcols[MAX_GPU_ATTR];
  GPUAttrRef cd_uvs[MAX_GPU_ATTR];

  const int cd_vcol_count = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_POINT |
                                                            ATTR_DOMAIN_MASK_CORNER,
                                                        CD_MASK_PROP_COLOR | CD_MASK_MLOOPCOL,
                                                        &bm->vdata,
                                                        NULL,
                                                        &bm->ldata,
                                                        NULL,
                                                        cd_vcols,
                                                        active_vcol_only,
                                                        active_vcol_type,
                                                        active_vcol_domain,
                                                        active_vcol_layer,
                                                        render_vcol_layer);

  int cd_uv_count = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_CORNER,
                                                CD_MASK_MLOOPUV,
                                                NULL,
                                                NULL,
                                                &bm->ldata,
                                                NULL,
                                                cd_uvs,
                                                active_vcol_only,
                                                CD_MLOOPUV,
                                                ATTR_DOMAIN_CORNER,
                                                get_active_layer(&bm->ldata, CD_MLOOPUV),
                                                get_render_layer(&bm->ldata, CD_MLOOPUV));
  /* Count visible triangles */
  tottri = gpu_bmesh_face_visible_count(tribuf, mat_nr) * 6;
  totvert = tottri * 3;

  if (!tottri) {
    if (BLI_table_gset_len(bm_faces) != 0) {
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

  bool default_face_set = true;

  /* Fill vertex buffer */
  if (!gpu_pbvh_vert_buf_data_set(buffers, totvert)) {
    /* Memory map failed */
    return;
  }

  int v_index = 0;

  GPUIndexBufBuilder elb_lines;
  GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, tottri * 3, tottri * 3);

  for (int i = 0; i < tribuf->tottri; i++) {
    PBVHTri *tri = tribuf->tris + i;
    BMFace *f = (BMFace *)tri->f.i;

    if (f->mat_nr != mat_nr) {
      continue;
    }

    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      BMVert *v[3];
      BMLoop *l[3];

      float fmask = 0.0f;

      v[0] = (BMVert *)tribuf->verts[tri->v[0]].i;
      v[1] = (BMVert *)tribuf->verts[tri->v[1]].i;
      v[2] = (BMVert *)tribuf->verts[tri->v[2]].i;

      if (tribuf->loops) {
        l[0] = (BMLoop *)tribuf->loops[tri->v[0]];
        l[1] = (BMLoop *)tribuf->loops[tri->v[1]];
        l[2] = (BMLoop *)tribuf->loops[tri->v[2]];
      }
      else {
        l[0] = l[1] = l[2] = NULL;
      }

      /* Average mask value */
      for (int j = 0; j < 3; j++) {
        fmask += BM_ELEM_CD_GET_FLOAT(v[j], cd_vert_mask_offset);
      }
      fmask /= 3.0f;

      uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};

      if (show_face_sets && cd_fset_offset >= 0) {
        const int fset = BM_ELEM_CD_GET_INT(f, cd_fset_offset);

        /* Skip for the default color Face Set to render it white. */
        if (fset != face_sets_color_default) {
          BKE_paint_face_set_overlay_color_get(fset, face_sets_color_seed, face_set_color);
          default_face_set = false;
        }
      }

      float cent[3] = {0.0f, 0.0f, 0.0f};
      add_v3_v3(cent, v[0]->co);
      add_v3_v3(cent, v[1]->co);
      add_v3_v3(cent, v[2]->co);
      mul_v3_fl(cent, 1.0 / 3.0);

      float cos[7][3];

      copy_v3_v3(cos[0], v[0]->co);
      copy_v3_v3(cos[1], v[1]->co);
      copy_v3_v3(cos[2], v[2]->co);

      copy_v3_v3(cos[6], cent);

      interp_v3_v3v3(cos[3], v[0]->co, v[1]->co, 0.5f);
      interp_v3_v3v3(cos[4], v[1]->co, v[2]->co, 0.5f);
      interp_v3_v3v3(cos[5], v[2]->co, v[0]->co, 0.5f);

      for (int k = 0; k < cd_uv_count; k++) {
        MLoopUV *uvs[3] = {
            BM_ELEM_CD_GET_VOID_P(l[0], cd_uvs[k].cd_offset),
            BM_ELEM_CD_GET_VOID_P(l[1], cd_uvs[k].cd_offset),
            BM_ELEM_CD_GET_VOID_P(l[2], cd_uvs[k].cd_offset),
        };

        float uvcent[2] = {0.0f, 0.0f};
        add_v2_v2(uvcent, uvs[0]->uv);
        add_v2_v2(uvcent, uvs[1]->uv);
        add_v2_v2(uvcent, uvs[2]->uv);
        mul_v2_fl(uvcent, 1.0 / 3.0);

        float uvcos[7][2];

        copy_v2_v2(uvcos[0], uvs[0]->uv);
        copy_v2_v2(uvcos[1], uvs[1]->uv);
        copy_v2_v2(uvcos[2], uvs[2]->uv);

        copy_v2_v2(uvcos[6], cent);

        interp_v2_v2v2(uvcos[3], uvs[0]->uv, uvs[1]->uv, 0.5f);
        interp_v2_v2v2(uvcos[4], uvs[1]->uv, uvs[2]->uv, 0.5f);
        interp_v2_v2v2(uvcos[5], uvs[2]->uv, uvs[0]->uv, 0.5f);

        for (int j = 0; j < 3; j++) {
          int next = 3 + ((j) % 3);
          int prev = 3 + ((j + 3 - 1) % 3);

          GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.uv[k], v_index, uvs[j]);
          GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.uv[k], v_index + 1, uvcos[next]);
          GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.uv[k], v_index + 2, uvcos[6]);

          GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.uv[k], v_index + 3, uvs[j]);
          GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.uv[k], v_index + 4, uvcos[6]);
          GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.uv[k], v_index + 5, uvcos[prev]);
        }
      }

      const int v_start = v_index;

      for (int j = 0; j < 3; j++) {
        int next = 3 + ((j) % 3);
        int prev = 3 + ((j + 3 - 1) % 3);

        gpu_flat_vcol_make_vert(
            v[j]->co, v[j], l[j], buffers->vert_buf, v_index, cd_vcols, cd_vcol_count, f->no);
        gpu_flat_vcol_make_vert(
            cos[next], v[j], l[j], buffers->vert_buf, v_index + 1, cd_vcols, cd_vcol_count, f->no);
        gpu_flat_vcol_make_vert(
            cos[6], v[j], l[j], buffers->vert_buf, v_index + 2, cd_vcols, cd_vcol_count, f->no);

        gpu_flat_vcol_make_vert(
            v[j]->co, v[j], l[j], buffers->vert_buf, v_index + 3, cd_vcols, cd_vcol_count, f->no);
        gpu_flat_vcol_make_vert(
            cos[6], v[j], l[j], buffers->vert_buf, v_index + 4, cd_vcols, cd_vcol_count, f->no);
        gpu_flat_vcol_make_vert(
            cos[prev], v[j], l[j], buffers->vert_buf, v_index + 5, cd_vcols, cd_vcol_count, f->no);

        /*
          v1
          |\
          |   \
          v3    v4
          |  v6   \
          |         \
          v0---v5---v2
          */

        next = j == 2 ? v_start : v_index + 6;

        if (tri->eflag & 1) {
          GPU_indexbuf_add_line_verts(&elb_lines, v_index, next);
          // GPU_indexbuf_add_line_verts(&elb_lines, v_index + 1, v_index + 2);
          // GPU_indexbuf_add_line_verts(&elb_lines, v_index + 2, v_index + 0);
        }

        if (tri->eflag & 2) {
          // GPU_indexbuf_add_line_verts(&elb_lines, v_index + 1, v_index + 2);
        }

        if (tri->eflag & 4) {
          // GPU_indexbuf_add_line_verts(&elb_lines, v_index + 2, v_index + 0);
        }

        v_index += 6;
      }
    }
  }

  buffers->index_lines_buf = GPU_indexbuf_build(&elb_lines);
  buffers->tot_tri = tottri;

  /* Get material index from the last face we iterated on. */
  buffers->material_index = mat_nr;

  buffers->show_overlay = (!empty_mask || !default_face_set) && !g_vbo_id.fast_mode;

  gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
}

static void GPU_pbvh_bmesh_buffers_update_indexed(GPU_PBVH_Buffers *buffers,
                                                  BMesh *bm,
                                                  TableGSet *bm_faces,
                                                  TableGSet *bm_unique_verts,
                                                  TableGSet *bm_other_verts,
                                                  PBVHTriBuf *tribuf,
                                                  const int update_flags,
                                                  const int cd_vert_node_offset,
                                                  int face_sets_color_seed,
                                                  int face_sets_color_default,
                                                  bool flat_vcol,
                                                  short mat_nr,
                                                  int active_vcol_type,
                                                  int active_vcol_domain,
                                                  CustomDataLayer *active_vcol_layer,
                                                  CustomDataLayer *render_vcol_layer)
{

  bool active_vcol_only = g_vbo_id.active_vcol_only;

  const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0 && !g_vbo_id.fast_mode;
  bool show_vcol = (update_flags & GPU_PBVH_BUFFERS_SHOW_VCOL) != 0 && active_vcol_type != -1;

  if (g_vbo_id.totcol == 0 && g_vbo_id.fast_mode) {
    show_vcol = false;
  }

  bool need_indexed = buffers->last_tribuf_tris != tribuf->tris;

  buffers->last_tribuf_tris = tribuf->tris;

  int tottri, totvert;
  bool empty_mask = true;

  GPUAttrRef cd_vcols[MAX_GPU_ATTR];
  GPUAttrRef cd_uvs[MAX_GPU_ATTR];

  int cd_vcol_count = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER,
                                                  CD_MASK_PROP_COLOR | CD_MASK_MLOOPCOL,
                                                  &bm->vdata,
                                                  NULL,
                                                  &bm->ldata,
                                                  NULL,
                                                  cd_vcols,
                                                  active_vcol_only,
                                                  active_vcol_type,
                                                  active_vcol_domain,
                                                  active_vcol_layer,
                                                  render_vcol_layer);

  int cd_uv_count = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_CORNER,
                                                CD_MASK_MLOOPUV,
                                                NULL,
                                                NULL,
                                                &bm->ldata,
                                                NULL,
                                                cd_uvs,
                                                active_vcol_only,
                                                CD_MLOOPUV,
                                                ATTR_DOMAIN_CORNER,
                                                get_active_layer(&bm->ldata, CD_MLOOPUV),
                                                get_render_layer(&bm->ldata, CD_MLOOPUV));

  /* Count visible triangles */
  tottri = gpu_bmesh_face_visible_count(tribuf, mat_nr);

  /* Count visible vertices */
  totvert = tribuf->totvert;

  if (!tottri) {
    if (BLI_table_gset_len(bm_faces) != 0) {
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
  int cd_fset_offset = CustomData_get_offset(&bm->pdata, CD_SCULPT_FACE_SETS);

  // int totuv = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
  // int *cd_uvs = BLI_array_alloca(cd_uvs, totuv);
  const bool have_uv = cd_uv_count > 0;

  bool default_face_set = true;

  /* Fill vertex buffer */
  if (!gpu_pbvh_vert_buf_data_set(buffers, totvert)) {
    /* Memory map failed */
    return;
  }

  for (int i = 0; i < tribuf->totvert; i++) {
    BMVert *v = (BMVert *)tribuf->verts[i].i;
    BMLoop *l = (BMLoop *)tribuf->loops[i];

    gpu_bmesh_vert_to_buffer_copy(bm,
                                  v,
                                  l,
                                  buffers->vert_buf,
                                  i,
                                  NULL,
                                  NULL,
                                  cd_vert_mask_offset,
                                  cd_vert_node_offset,
                                  show_mask,
                                  show_vcol,
                                  &empty_mask,
                                  cd_vcols,
                                  cd_vcol_count);

    if (!g_vbo_id.fast_mode) {
      uchar face_set_color[3] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};

      /* Add default face sets color to avoid artifacts. */
      int fset = BM_ELEM_CD_GET_INT(l->f, cd_fset_offset);

      if (fset != face_sets_color_default) {
        default_face_set = false;
        BKE_paint_face_set_overlay_color_get(fset, face_sets_color_seed, face_set_color);
      }

      GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.fset, i, &face_set_color);
    }

    if (have_uv) {
      for (int j = 0; j < cd_uv_count; j++) {
        MLoopUV *mu = BM_ELEM_CD_GET_VOID_P(l, cd_uvs[j].cd_offset);
        GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.uv[j], i, mu->uv);
      }
    }
  }

  if (!need_indexed) {
    buffers->material_index = mat_nr;
    buffers->show_overlay = (!empty_mask || !default_face_set) && !g_vbo_id.fast_mode;

    gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
    return;
  }

  GPU_BATCH_DISCARD_SAFE(buffers->triangles);
  GPU_BATCH_DISCARD_SAFE(buffers->lines);
  GPU_INDEXBUF_DISCARD_SAFE(buffers->index_lines_buf);
  GPU_INDEXBUF_DISCARD_SAFE(buffers->index_buf);

  /* Fill the vertex and triangle buffer in one pass over faces. */
  GPUIndexBufBuilder elb, elb_lines;
  GPU_indexbuf_init(&elb, GPU_PRIM_TRIS, tottri, totvert);
  GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, tottri * 3, totvert);

  for (int i = 0; i < tribuf->tottri; i++) {
    PBVHTri *tri = tribuf->tris + i;

    BMFace *f = (BMFace *)tri->f.i;
    if (f->mat_nr != mat_nr || BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    GPU_indexbuf_add_tri_verts(&elb, tri->v[0], tri->v[1], tri->v[2]);

    GPU_indexbuf_add_line_verts(&elb_lines, tri->v[0], tri->v[1]);
    GPU_indexbuf_add_line_verts(&elb_lines, tri->v[1], tri->v[2]);
    GPU_indexbuf_add_line_verts(&elb_lines, tri->v[2], tri->v[0]);
  }

  buffers->tot_tri = tottri;

  if (buffers->index_buf == NULL) {
    buffers->index_buf = GPU_indexbuf_build(&elb);
  }
  else {
    GPU_indexbuf_build_in_place(&elb, buffers->index_buf);
  }
  buffers->index_lines_buf = GPU_indexbuf_build(&elb_lines);

  buffers->material_index = mat_nr;
  buffers->show_overlay = (!empty_mask || !default_face_set) && !g_vbo_id.fast_mode;

  gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
}

/* Creates a vertex buffer (coordinate, normal, color) and, if smooth
 * shading, an element index buffer.
 * Threaded - do not call any functions that use OpenGL calls! */
void GPU_pbvh_bmesh_buffers_update(PBVHGPUBuildArgs *args)
{
  BMesh *bm = args->bm;
  GPU_PBVH_Buffers *buffers = args->buffers;
  PBVHTriBuf *tribuf = args->tribuf;
  const int update_flags = args->update_flags;
  const int mat_nr = args->mat_nr;

  bool active_vcol_only = g_vbo_id.active_vcol_only;

  if (args->flat_vcol && args->active_vcol_type != -1) {
    GPU_pbvh_bmesh_buffers_update_flat_vcol(buffers,
                                            bm,
                                            args->bm_faces,
                                            args->bm_unique_verts,
                                            args->bm_other_verts,
                                            tribuf,
                                            update_flags,
                                            args->cd_vert_node_offset,
                                            args->face_sets_color_seed,
                                            args->face_sets_color_default,
                                            mat_nr,
                                            args->active_vcol_type,
                                            args->active_vcol_domain,
                                            args->active_vcol_layer,
                                            args->render_vcol_layer);
    return;
  }

  const bool have_uv = CustomData_has_layer(&bm->ldata, CD_MLOOPUV);
  const bool show_vcol = (update_flags & GPU_PBVH_BUFFERS_SHOW_VCOL) != 0 &&
                         args->active_vcol_type != -1;
  const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0 && !g_vbo_id.fast_mode;
  const bool show_face_sets = CustomData_has_layer(&bm->pdata, CD_SCULPT_FACE_SETS) &&
                              (update_flags & GPU_PBVH_BUFFERS_SHOW_SCULPT_FACE_SETS) != 0 &&
                              !g_vbo_id.fast_mode;

  int tottri, totvert;
  bool empty_mask = true;
  int cd_fset_offset = CustomData_get_offset(&bm->pdata, CD_SCULPT_FACE_SETS);

  GPUAttrRef cd_vcols[MAX_GPU_ATTR];
  GPUAttrRef cd_uvs[MAX_GPU_ATTR];

  int cd_vcol_count = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER,
                                                  CD_MASK_PROP_COLOR | CD_MASK_MLOOPCOL,
                                                  &bm->vdata,
                                                  NULL,
                                                  &bm->ldata,
                                                  NULL,
                                                  cd_vcols,
                                                  active_vcol_only,
                                                  args->active_vcol_type,
                                                  args->active_vcol_domain,
                                                  args->active_vcol_layer,
                                                  args->render_vcol_layer);

  int cd_uv_count = gpu_pbvh_gpu_make_attr_offs(ATTR_DOMAIN_MASK_CORNER,
                                                CD_MASK_MLOOPUV,
                                                NULL,
                                                NULL,
                                                &bm->ldata,
                                                NULL,
                                                cd_uvs,
                                                active_vcol_only,
                                                CD_MLOOPUV,
                                                ATTR_DOMAIN_CORNER,
                                                get_active_layer(&bm->ldata, CD_MLOOPUV),
                                                get_render_layer(&bm->ldata, CD_MLOOPUV));
  /* Count visible triangles */
  if (buffers->smooth) {
    GPU_pbvh_bmesh_buffers_update_indexed(buffers,
                                          bm,
                                          args->bm_faces,
                                          args->bm_unique_verts,
                                          args->bm_other_verts,
                                          tribuf,
                                          update_flags,
                                          args->cd_vert_node_offset,
                                          args->face_sets_color_seed,
                                          args->face_sets_color_default,
                                          args->flat_vcol,
                                          mat_nr,
                                          args->active_vcol_type,
                                          args->active_vcol_domain,
                                          args->active_vcol_layer,
                                          args->render_vcol_layer);
    return;
  }

  buffers->last_tribuf_tris = NULL;

  /* TODO, make mask layer optional for bmesh buffer */
  const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);
  int face_sets_color_default = args->face_sets_color_default;
  int face_sets_color_seed = args->face_sets_color_seed;
  int cd_vert_node_offset = args->cd_vert_node_offset;

  bool default_face_set = true;

  tottri = gpu_bmesh_face_visible_count(tribuf, mat_nr);
  totvert = tottri * 3;

  if (!tottri) {
    /* empty node (i.e. not just hidden)? */
    if (BLI_table_gset_len(args->bm_faces) == 0) {
      buffers->clear_bmesh_on_flush = true;
    }

    buffers->tot_tri = 0;
    return;
  }
  /* Fill vertex buffer */
  if (!gpu_pbvh_vert_buf_data_set(buffers, totvert)) {
    /* Memory map failed */
    return;
  }

  int v_index = 0;

  GPUIndexBufBuilder elb_lines;
  GPU_indexbuf_init(&elb_lines, GPU_PRIM_LINES, tottri * 3, tottri * 3);

  for (int i = 0; i < tribuf->tottri; i++) {
    PBVHTri *tri = tribuf->tris + i;
    BMFace *f = (BMFace *)tri->f.i;
    BMLoop **l = (BMLoop **)tri->l;
    BMVert *v[3];

    if (f->mat_nr != mat_nr || BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }

    v[0] = l[0]->v;
    v[1] = l[1]->v;
    v[2] = l[2]->v;

    float fmask = 0.0f;
    int i;

    /* Average mask value */
    for (i = 0; i < 3; i++) {
      fmask += BM_ELEM_CD_GET_FLOAT(v[i], cd_vert_mask_offset);
    }
    fmask /= 3.0f;

    if (tri->eflag & 1) {
      GPU_indexbuf_add_line_verts(&elb_lines, v_index + 0, v_index + 1);
    }

    if (tri->eflag & 2) {
      GPU_indexbuf_add_line_verts(&elb_lines, v_index + 1, v_index + 2);
    }

    if (tri->eflag & 4) {
      GPU_indexbuf_add_line_verts(&elb_lines, v_index + 2, v_index + 0);
    }

    uchar face_set_color[4] = {UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};

    if (show_face_sets && cd_fset_offset >= 0) {
      const int fset = BM_ELEM_CD_GET_INT(f, cd_fset_offset);

      /* Skip for the default color Face Set to render it white. */
      if (fset != face_sets_color_default) {
        BKE_paint_face_set_overlay_color_get(fset, face_sets_color_seed, face_set_color);
        default_face_set = false;
      }
    }

    for (int j = 0; j < 3; j++) {
      float *no = buffers->smooth ? v[j]->no : f->no;

      gpu_bmesh_vert_to_buffer_copy(bm,
                                    v[j],
                                    l[j],
                                    buffers->vert_buf,
                                    v_index,
                                    no,
                                    &fmask,
                                    cd_vert_mask_offset,
                                    cd_vert_node_offset,
                                    show_mask,
                                    show_vcol,
                                    &empty_mask,
                                    cd_vcols,
                                    cd_vcol_count);
#ifndef GPU_PERF_TEST

      if (have_uv) {
        for (int k = 0; k < cd_uv_count; k++) {
          MLoopUV *mu = BM_ELEM_CD_GET_VOID_P(l[j], cd_uvs[k].cd_offset);
          GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.uv[k], v_index, mu->uv);
        }
      }

      if (show_face_sets) {
        GPU_vertbuf_attr_set(buffers->vert_buf, g_vbo_id.fset, v_index, face_set_color);
      }
#endif
      v_index++;
    }
  }

  buffers->index_lines_buf = GPU_indexbuf_build(&elb_lines);
  buffers->tot_tri = tottri;

  /* Get material index from the last face we iterated on. */
  buffers->material_index = mat_nr;
  buffers->show_overlay = (!empty_mask || !default_face_set) && !g_vbo_id.fast_mode;

  gpu_pbvh_batch_init(buffers, GPU_PRIM_TRIS);
}

/* -------------------------------------------------------------------- */
/** \name Generic
 * \{ */

/* Threaded - do not call any functions that use OpenGL calls! */
GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading)
{
  GPU_PBVH_Buffers *buffers;

  buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
  buffers->use_bmesh = true;
  buffers->smooth = smooth_shading || g_vbo_id.fast_mode;
  buffers->show_overlay = (!g_vbo_id.fast_mode) && !g_vbo_id.fast_mode;

  return buffers;
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
  return buffers->show_overlay && !g_vbo_id.fast_mode;
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
