/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.hh"

#include "GPU_capabilities.h"

#include "draw_subdivision.h"
#include "extract_mesh.hh"

#define FORCE_HIDE 255
namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edge Factor
 * Defines how much an edge is visible.
 * \{ */

struct MEdgeDataPrev {
  int corner_a;

  /* Data that represents:
   * - the index of the polygon of `corner_a` before the 2nd loop is found
   * - the index of the next radial corner after the 2nd loop is found */
  int data;
};

struct MeshExtract_EdgeFac_Data {
  uint8_t *vbo_data;
  bool use_edge_render;
  /* Number of loop per edge. */
  uint8_t *edge_loop_count;

  MEdgeDataPrev *edge_pdata;
};

/**
 * Calculates a factor that is used to identify the minimum angle in the shader to display an edge.
 * NOTE: Keep in sync with `common_subdiv_vbo_edge_fac_comp.glsl`.
 */
BLI_INLINE uint8_t loop_edge_factor_get(const float3 &fa_no, const float3 &fb_no)
{
  const float cosine = math::dot(fa_no, fb_no);

  /* Re-scale to the slider range. */
  float fac = (200 * (cosine - 1.0f)) + 1.0f;
  CLAMP(fac, 0.0f, 1.0f);

  /* 255 is a reserved value to force hide the wire. */
  return uint8_t(fac * 254);
}

static void extract_edge_fac_init(const MeshRenderData *mr,
                                  MeshBatchCache * /*cache*/,
                                  void *buf,
                                  void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(tls_data);

  if (mr->extract_type == MR_EXTRACT_MESH) {
    data->use_edge_render = !mr->me->runtime->subsurf_optimal_display_edges.is_empty();
    data->edge_loop_count = MEM_cnew_array<uint8_t>(mr->edge_len, __func__);
    data->edge_pdata = (MEdgeDataPrev *)MEM_malloc_arrayN(
        mr->edge_len, sizeof(MEdgeDataPrev), __func__);
  }
  else {
    /* HACK to bypass non-manifold check in mesh_edge_fac_finish(). */
    data->use_edge_render = true;
  }

  data->vbo_data = static_cast<uchar *>(GPU_vertbuf_get_data(vbo));
}

static void extract_edge_fac_iter_poly_bm(const MeshRenderData *mr,
                                          const BMFace *f,
                                          const int /*f_index*/,
                                          void *_data)
{
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    if (BM_edge_is_manifold(l_iter->e)) {
      BMFace *fb = l_iter->f != f ? l_iter->f : l_iter->radial_next->f;
      data->vbo_data[l_index] = loop_edge_factor_get(float3(bm_face_no_get(mr, f)),
                                                     float3(bm_face_no_get(mr, fb)));
    }
    else {
      data->vbo_data[l_index] = 0;
    }
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edge_fac_iter_poly_mesh(const MeshRenderData *mr,
                                            const int poly_index,
                                            void *_data)
{
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);
  const IndexRange poly = mr->polys[poly_index];
  const BitSpan optimal_display_edges = mr->me->runtime->subsurf_optimal_display_edges;

  for (const int ml_index : poly) {
    const int edge = mr->corner_edges[ml_index];

    if (data->use_edge_render && !optimal_display_edges[edge]) {
      data->vbo_data[ml_index] = FORCE_HIDE;
    }
    else {
      MEdgeDataPrev *medata = &data->edge_pdata[edge];

      uint8_t corner_count = data->edge_loop_count[edge];
      if (corner_count < 4) {
        if (corner_count == 0) {
          /* Prepare to calculate the factor. */
          medata->corner_a = ml_index;
          medata->data = poly_index;

          /* Consider boundary edge while second corner is not detected. Always visible. */
          data->vbo_data[ml_index] = 0;
        }
        else if (corner_count == 1) {
          /* Calculate the factor for both corners. */
          const int poly_index_a = medata->data;
          uint8_t fac = loop_edge_factor_get(float3(mr->poly_normals[poly_index_a]),
                                             float3(mr->poly_normals[poly_index]));
          data->vbo_data[medata->corner_a] = fac;
          data->vbo_data[ml_index] = fac;

          /* If the count still changes, use this `data` member to inform the corner. */
          medata->data = ml_index;
        }
        else {
          /* Non-manifold edge. Always visible. */
          const int corner_a = medata->corner_a;
          const int corner_b = medata->data;
          data->vbo_data[corner_a] = 0;
          data->vbo_data[corner_b] = 0;
        }

        /* Increment the corner_count count. */
        data->edge_loop_count[edge] = corner_count + 1;
      }
    }
  }
}

static void extract_edge_fac_iter_loose_edge_bm(const MeshRenderData *mr,
                                                const BMEdge * /*eed*/,
                                                const int loose_edge_i,
                                                void *_data)
{
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);
  data->vbo_data[mr->loop_len + (loose_edge_i * 2) + 0] = 0;
  data->vbo_data[mr->loop_len + (loose_edge_i * 2) + 1] = 0;
}

static void extract_edge_fac_iter_loose_edge_mesh(const MeshRenderData *mr,
                                                  const int2 /*edge*/,
                                                  const int loose_edge_i,
                                                  void *_data)
{
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);

  data->vbo_data[mr->loop_len + loose_edge_i * 2 + 0] = 0;
  data->vbo_data[mr->loop_len + loose_edge_i * 2 + 1] = 0;
}

static void extract_edge_fac_finish(const MeshRenderData *mr,
                                    MeshBatchCache * /*cache*/,
                                    void *buf,
                                    void *_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);

  if (GPU_crappy_amd_driver() || GPU_minimum_per_vertex_stride() > 1) {
    /* Some AMD drivers strangely crash with VBO's with a one byte format.
     * To workaround we reinitialize the VBO with another format and convert
     * all bytes to floats. */
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
    /* We keep the data reference in data->vbo_data. */
    data->vbo_data = static_cast<uchar *>(GPU_vertbuf_steal_data(vbo));
    GPU_vertbuf_clear(vbo);

    int buf_len = mr->loop_len + mr->loop_loose_len;
    GPU_vertbuf_init_with_format(vbo, &format);
    GPU_vertbuf_data_alloc(vbo, buf_len);

    float *fdata = (float *)GPU_vertbuf_get_data(vbo);
    for (int ml_index = 0; ml_index < buf_len; ml_index++, fdata++) {
      *fdata = data->vbo_data[ml_index] / 255.0f;
    }
    /* Free old byte data. */
    MEM_freeN(data->vbo_data);
  }
  MEM_SAFE_FREE(data->edge_loop_count);
  MEM_SAFE_FREE(data->edge_pdata);
}

/* Different function than the one used for the non-subdivision case, as we directly take care of
 * the buggy AMD driver case. */
static GPUVertFormat *get_subdiv_edge_fac_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    if (GPU_crappy_amd_driver() || GPU_minimum_per_vertex_stride() > 1) {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
    else {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }
  }
  return &format;
}

static GPUVertBuf *build_poly_other_map_vbo(const DRWSubdivCache *subdiv_cache)
{
  GPUVertBuf *vbo = GPU_vertbuf_calloc();

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "poly_other", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, subdiv_cache->num_subdiv_loops);

  MutableSpan vbo_data{static_cast<int *>(GPU_vertbuf_get_data(vbo)),
                       subdiv_cache->num_subdiv_loops};

  Array<MEdgeDataPrev> edge_data(subdiv_cache->num_subdiv_edges);
  Array<int> tmp_edge_corner_count(subdiv_cache->num_subdiv_edges, 0);
  int *subdiv_loop_subdiv_edge_index = subdiv_cache->subdiv_loop_subdiv_edge_index;

  for (int corner : IndexRange(subdiv_cache->num_subdiv_loops)) {
    const int edge = subdiv_loop_subdiv_edge_index[corner];
    const int quad = corner / 4;
    const int corner_count = tmp_edge_corner_count[edge]++;

    vbo_data[corner] = -1;
    if (corner_count == 0) {
      edge_data[edge].corner_a = corner;
      edge_data[edge].data = quad;
    }
    else if (corner_count == 1) {
      const int corner_a = edge_data[edge].corner_a;
      const int quad_a = edge_data[edge].data;
      vbo_data[corner_a] = quad;
      vbo_data[corner] = quad_a;
      edge_data[edge].data = corner;
    }
    else if (corner_count == 2) {
      const int corner_a = edge_data[edge].corner_a;
      const int corner_b = edge_data[edge].data;
      vbo_data[corner_a] = -1;
      vbo_data[corner_b] = -1;
    }
  }

  return vbo;
}

static void extract_edge_fac_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                         const MeshRenderData * /*mr*/,
                                         MeshBatchCache *cache,
                                         void *buffer,
                                         void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);

  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  GPU_vertbuf_init_build_on_device(
      vbo, get_subdiv_edge_fac_format(), subdiv_cache->num_subdiv_loops + loose_geom.loop_len);

  GPUVertBuf *pos_nor = cache->final.buff.vbo.pos_nor;
  GPUVertBuf *poly_other_map = build_poly_other_map_vbo(subdiv_cache);

  draw_subdiv_build_edge_fac_buffer(
      subdiv_cache, pos_nor, subdiv_cache->edges_draw_flag, poly_other_map, vbo);

  GPU_vertbuf_discard(poly_other_map);
}

static void extract_edge_fac_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                               const MeshRenderData * /*mr*/,
                                               void *buffer,
                                               void * /*data*/)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache->loose_geom;
  if (loose_geom.edge_len == 0) {
    return;
  }

  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);

  /* Make sure buffer is active for sending loose data. */
  GPU_vertbuf_use(vbo);

  uint offset = subdiv_cache->num_subdiv_loops;
  for (int i = 0; i < loose_geom.edge_len; i++) {
    if (GPU_crappy_amd_driver() || GPU_minimum_per_vertex_stride() > 1) {
      float loose_edge_fac[2] = {1.0f, 1.0f};
      GPU_vertbuf_update_sub(vbo, offset * sizeof(float), sizeof(loose_edge_fac), loose_edge_fac);
    }
    else {
      char loose_edge_fac[2] = {255, 255};
      GPU_vertbuf_update_sub(vbo, offset * sizeof(char), sizeof(loose_edge_fac), loose_edge_fac);
    }

    offset += 2;
  }
}

constexpr MeshExtract create_extractor_edge_fac()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_edge_fac_init;
  extractor.iter_poly_bm = extract_edge_fac_iter_poly_bm;
  extractor.iter_poly_mesh = extract_edge_fac_iter_poly_mesh;
  extractor.iter_loose_edge_bm = extract_edge_fac_iter_loose_edge_bm;
  extractor.iter_loose_edge_mesh = extract_edge_fac_iter_loose_edge_mesh;
  extractor.init_subdiv = extract_edge_fac_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_edge_fac_loose_geom_subdiv;
  extractor.finish = extract_edge_fac_finish;
  extractor.data_type = MR_DATA_POLY_NOR;
  extractor.data_size = sizeof(MeshExtract_EdgeFac_Data);
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.edge_fac);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_edge_fac = blender::draw::create_extractor_edge_fac();
