/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_string.h"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

static const GPUVertFormat &get_sculpt_data_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "fset", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_attr_add(&format, "msk", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  return format;
}

void extract_sculpt_data(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  GPU_vertbuf_init_with_format(vbo, get_sculpt_data_format());
  GPU_vertbuf_data_alloc(vbo, mr.corners_num);

  struct gpuSculptData {
    uchar4 face_set_color;
    float mask;
  };

  MutableSpan vbo_data(static_cast<gpuSculptData *>(GPU_vertbuf_get_data(vbo)), mr.corners_num);

  const int default_face_set = mr.mesh->face_sets_color_default;
  const int face_set_seed = mr.mesh->face_sets_color_seed;
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    const BMesh &bm = *mr.bm;
    const int mask_offset = CustomData_get_offset_named(
        &mr.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
    const int face_set_offset = CustomData_get_offset_named(
        &mr.bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const IndexRange face_range(BM_elem_index_get(BM_FACE_FIRST_LOOP(&face)), face.len);

        uchar4 face_set_color(UCHAR_MAX);
        if (face_set_offset != -1) {
          const int face_set_id = BM_ELEM_CD_GET_INT(&face, face_set_offset);
          if (face_set_id != default_face_set) {
            BKE_paint_face_set_overlay_color_get(face_set_id, face_set_seed, face_set_color);
          }
        }
        vbo_data.slice(face_range).fill(gpuSculptData{face_set_color, 0.0f});

        if (mask_offset != -1) {
          const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
          for ([[maybe_unused]] const int i : IndexRange(face.len)) {
            const int index = BM_elem_index_get(loop);
            vbo_data[index].mask = BM_ELEM_CD_GET_FLOAT(loop->v, mask_offset);
            loop = loop->next;
          }
        }
      }
    });
  }
  else {
    const OffsetIndices faces = mr.faces;
    const Span<int> corner_verts = mr.corner_verts;
    const bke::AttributeAccessor attributes = mr.mesh->attributes();
    const VArraySpan mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point);
    const VArraySpan face_set = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
    threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
      for (const int face_index : range) {
        const IndexRange face = faces[face_index];
        uchar4 face_set_color(UCHAR_MAX);
        if (!face_set.is_empty()) {
          const int face_set_id = face_set[face_index];
          if (face_set_id != default_face_set) {
            BKE_paint_face_set_overlay_color_get(face_set_id, face_set_seed, face_set_color);
          }
        }
        vbo_data.slice(face).fill(gpuSculptData{face_set_color, 0.0f});

        if (!mask.is_empty()) {
          for (const int corner : face) {
            vbo_data[corner].mask = mask[corner_verts[corner]];
          }
        }
      }
    });
  }
}

void extract_sculpt_data_subdiv(const MeshRenderData &mr,
                                const DRWSubdivCache &subdiv_cache,
                                gpu::VertBuf &vbo)
{
  const Mesh &coarse_mesh = *mr.mesh;
  const int subdiv_corners_num = subdiv_cache.num_subdiv_loops;

  /* First, interpolate mask if available. */
  gpu::VertBuf *mask_vbo = nullptr;
  gpu::VertBuf *subdiv_mask_vbo = nullptr;

  const bke::AttributeAccessor attributes = coarse_mesh.attributes();

  if (const VArray mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point)) {
    GPUVertFormat mask_format = {0};
    GPU_vertformat_attr_add(&mask_format, "msk", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    const Span<int> corner_verts = coarse_mesh.corner_verts();
    mask_vbo = GPU_vertbuf_calloc();
    GPU_vertbuf_init_with_format(*mask_vbo, mask_format);
    GPU_vertbuf_data_alloc(*mask_vbo, corner_verts.size());

    MutableSpan mask_vbo_data(static_cast<float *>(GPU_vertbuf_get_data(*mask_vbo)),
                              corner_verts.size());
    array_utils::gather(mask, corner_verts, mask_vbo_data);

    subdiv_mask_vbo = GPU_vertbuf_calloc();
    GPU_vertbuf_init_build_on_device(*subdiv_mask_vbo, mask_format, subdiv_corners_num);

    draw_subdiv_interp_custom_data(subdiv_cache, *mask_vbo, *subdiv_mask_vbo, GPU_COMP_F32, 1, 0);
  }

  /* Then, gather face sets. */
  GPUVertFormat face_set_format = {0};
  GPU_vertformat_attr_add(&face_set_format, "msk", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

  gpu::VertBuf *face_set_vbo = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format(*face_set_vbo, face_set_format);
  GPU_vertbuf_data_alloc(*face_set_vbo, subdiv_corners_num);

  struct gpuFaceSet {
    uchar4 color;
  };

  MutableSpan face_set_vbo_data(static_cast<gpuFaceSet *>(GPU_vertbuf_get_data(*face_set_vbo)),
                                subdiv_corners_num);
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  if (face_sets.is_empty()) {
    face_set_vbo_data.fill({uchar4{UCHAR_MAX}});
  }
  else {
    const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                           subdiv_corners_num);
    const int default_face_set = coarse_mesh.face_sets_color_default;
    const int face_set_seed = coarse_mesh.face_sets_color_seed;
    threading::parallel_for(IndexRange(subdiv_corners_num), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        const int face_index = subdiv_loop_face_index[i];

        uchar4 face_set_color(UCHAR_MAX);
        const int face_set_id = face_sets[face_index];
        /* Skip for the default color Face Set to render it white. */
        if (face_set_id != default_face_set) {
          BKE_paint_face_set_overlay_color_get(face_set_id, face_set_seed, face_set_color);
        }

        copy_v3_v3_uchar(face_set_vbo_data[i].color, face_set_color);
      }
    });
  }

  /* Finally, interleave mask and face sets. */
  GPU_vertbuf_init_build_on_device(vbo, get_sculpt_data_format(), subdiv_corners_num);
  draw_subdiv_build_sculpt_data_buffer(subdiv_cache, subdiv_mask_vbo, face_set_vbo, &vbo);

  if (mask_vbo) {
    GPU_vertbuf_discard(mask_vbo);
    GPU_vertbuf_discard(subdiv_mask_vbo);
  }
  GPU_vertbuf_discard(face_set_vbo);
}

}  // namespace blender::draw
