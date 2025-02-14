/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_math_vector.hh"

#include "GPU_capabilities.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

#define FORCE_HIDE 255
namespace blender::draw {

struct MEdgeDataPrev {
  int corner_a;

  /* Data that represents:
   * - the index of the face of `corner_a` before the 2nd loop is found
   * - the index of the next radial corner after the 2nd loop is found */
  int data;
};

/**
 * Calculates a factor that is used to identify the minimum angle in the shader to display an edge.
 * NOTE: Keep in sync with `common_subdiv_vbo_edge_fac_comp.glsl`.
 */
template<typename T> T edge_factor_calc(const float3 &a, const float3 &b);

template<> inline float edge_factor_calc<float>(const float3 &a, const float3 &b)
{
  const float cosine = math::dot(a, b);

  /* Re-scale to the slider range. */
  float fac = (200 * (cosine - 1.0f)) + 1.0f;
  CLAMP(fac, 0.0f, 1.0f);
  /* 1.0 is a reserved value to force hide the wire. */
  constexpr float factor = 254.0f / 255.0f;
  return fac * factor;
}

template<> inline uint8_t edge_factor_calc<uint8_t>(const float3 &a, const float3 &b)
{
  const float cosine = math::dot(a, b);

  /* Re-scale to the slider range. */
  float fac = (200 * (cosine - 1.0f)) + 1.0f;
  CLAMP(fac, 0.0f, 1.0f);
  /* 255 is a reserved value to force hide the wire. */
  return fac * 254;
}

template<typename T>
static void extract_edge_factor_mesh(const MeshRenderData &mr, MutableSpan<T> vbo_data)
{
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_edges = mr.corner_edges;
  const Span<float3> face_normals = mr.face_normals;
  const BitSpan optimal_display_edges = mr.mesh->runtime->subsurf_optimal_display_edges;

  Array<int8_t> edge_face_count(mr.edges_num, 0);
  Array<MEdgeDataPrev> edge_data(mr.edges_num);

  for (const int face : faces.index_range()) {
    for (const int corner : faces[face]) {
      const int edge = corner_edges[corner];
      if (!optimal_display_edges.is_empty() && !optimal_display_edges[edge]) {
        if constexpr (std::is_same_v<T, float>) {
          vbo_data[corner] = 1.0f;
        }
        else {
          vbo_data[corner] = FORCE_HIDE;
        }
        continue;
      }

      MEdgeDataPrev *medata = &edge_data[edge];

      const int8_t face_count = edge_face_count[edge];
      vbo_data[corner] = 0;
      if (face_count < 4) {
        if (face_count == 0) {
          /* Prepare to calculate the factor. */
          medata->corner_a = corner;
          medata->data = face;
        }
        else if (face_count == 1) {
          /* Calculate the factor for both corners. */
          const int other_face = medata->data;
          const T factor = edge_factor_calc<T>(face_normals[other_face], face_normals[face]);
          vbo_data[medata->corner_a] = factor;
          vbo_data[corner] = factor;

          /* If the count still changes, use this `data` member to inform the corner. */
          medata->data = corner;
        }
        else {
          /* Non-manifold edge. Always visible. */
          const int corner_a = medata->corner_a;
          const int corner_b = medata->data;
          vbo_data[corner_a] = 0;
          vbo_data[corner_b] = 0;
        }

        edge_face_count[edge]++;
      }
    }
  }
}

template<typename T>
static void extract_edge_factor_bm(const MeshRenderData &mr, MutableSpan<T> vbo_data)
{
  BMesh &bm = *mr.bm;
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&bm, face_index);
      const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
      for ([[maybe_unused]] const int i : IndexRange(face.len)) {
        const int index = BM_elem_index_get(loop);
        if (BM_edge_is_manifold(loop->e)) {
          const BMFace *other_face = loop->radial_next->f;
          vbo_data[index] = edge_factor_calc<T>(float3(bm_face_no_get(mr, &face)),
                                                float3(bm_face_no_get(mr, other_face)));
        }
        else {
          vbo_data[index] = T(0);
        }
        loop = loop->next;
      }
    }
  });
}

void extract_edge_factor(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  if (GPU_crappy_amd_driver() || GPU_minimum_per_vertex_stride() > 1) {
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
    GPU_vertbuf_init_with_format(vbo, format);
    GPU_vertbuf_data_alloc(vbo, mr.corners_num + mr.loose_indices_num);
    MutableSpan vbo_data = vbo.data<float>();
    if (mr.extract_type == MeshExtractType::Mesh) {
      extract_edge_factor_mesh(mr, vbo_data);
    }
    else {
      extract_edge_factor_bm(mr, vbo_data);
    }
    vbo_data.take_back(mr.loose_indices_num).fill(0.0f);
  }
  else {
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }
    GPU_vertbuf_init_with_format(vbo, format);
    GPU_vertbuf_data_alloc(vbo, mr.corners_num + mr.loose_indices_num);
    MutableSpan vbo_data = vbo.data<uint8_t>();
    if (mr.extract_type == MeshExtractType::Mesh) {
      extract_edge_factor_mesh(mr, vbo_data);
    }
    else {
      extract_edge_factor_bm(mr, vbo_data);
    }
    vbo_data.take_back(mr.loose_indices_num).fill(uint8_t(0));
  }
}

/* Different function than the one used for the non-subdivision case, as we directly take care of
 * the buggy AMD driver case. */
static const GPUVertFormat &get_subdiv_edge_fac_format()
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
  return format;
}

static gpu::VertBuf *build_poly_other_map_vbo(const DRWSubdivCache &subdiv_cache)
{
  gpu::VertBuf *vbo = GPU_vertbuf_calloc();

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "poly_other", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }

  GPU_vertbuf_init_with_format(*vbo, format);
  GPU_vertbuf_data_alloc(*vbo, subdiv_cache.num_subdiv_loops);
  MutableSpan vbo_data = vbo->data<int>();

  Array<MEdgeDataPrev> edge_data(subdiv_cache.num_subdiv_edges);
  Array<int> tmp_edge_corner_count(subdiv_cache.num_subdiv_edges, 0);
  int *subdiv_loop_subdiv_edge_index = subdiv_cache.subdiv_loop_subdiv_edge_index;

  for (int corner : IndexRange(subdiv_cache.num_subdiv_loops)) {
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

void extract_edge_factor_subdiv(const DRWSubdivCache &subdiv_cache,
                                const MeshRenderData &mr,
                                gpu::VertBuf &pos_nor,
                                gpu::VertBuf &vbo)
{
  GPU_vertbuf_init_build_on_device(vbo,
                                   get_subdiv_edge_fac_format(),
                                   subdiv_cache.num_subdiv_loops +
                                       subdiv_loose_edges_num(mr, subdiv_cache) * 2);

  if (mr.faces_num > 0) {
    gpu::VertBuf *poly_other_map = build_poly_other_map_vbo(subdiv_cache);

    draw_subdiv_build_edge_fac_buffer(
        subdiv_cache, &pos_nor, subdiv_cache.edges_draw_flag, poly_other_map, &vbo);

    GPU_vertbuf_discard(poly_other_map);
  }

  const int loose_edges_num = subdiv_loose_edges_num(mr, subdiv_cache);
  if (loose_edges_num == 0) {
    return;
  }

  /* Make sure buffer is active for sending loose data. */
  GPU_vertbuf_use(&vbo);

  const int offset = subdiv_cache.num_subdiv_loops;
  if (GPU_crappy_amd_driver() || GPU_minimum_per_vertex_stride() > 1) {
    const float values[2] = {1.0f, 1.0f};
    for (const int i : IndexRange(loose_edges_num)) {
      GPU_vertbuf_update_sub(&vbo, (offset + i * 2) * sizeof(float), sizeof(values), values);
    }
  }
  else {
    const uint8_t values[2] = {255, 255};
    for (const int i : IndexRange(loose_edges_num)) {
      GPU_vertbuf_update_sub(&vbo, (offset + i * 2) * sizeof(uint8_t), sizeof(values), values);
    }
  }
}

}  // namespace blender::draw
