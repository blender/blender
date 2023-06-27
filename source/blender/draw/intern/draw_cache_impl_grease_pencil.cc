/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Grease Pencil API for render engines
 */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"

#include "BLI_task.hh"

#include "DNA_grease_pencil_types.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "GPU_batch.h"

#include "draw_cache_impl.h"

#include "../engines/gpencil/gpencil_defines.h"
#include "../engines/gpencil/gpencil_shader_shared.h"

namespace blender::draw {

struct GreasePencilBatchCache {
  /** Instancing Data */
  GPUVertBuf *vbo;
  GPUVertBuf *vbo_col;
  /** Indices in material order, then stroke order with fill first. */
  GPUIndexBuf *ibo;
  /** Batches */
  GPUBatch *geom_batch;
  GPUBatch *edit_points;

  /* Crazy-space point positions for original points. */
  GPUVertBuf *edit_points_pos;
  /* Selection of original points. */
  GPUVertBuf *edit_points_selection;

  /** Cache is dirty. */
  bool is_dirty;
  /** Last cached frame. */
  int cache_frame;
};

/* -------------------------------------------------------------------- */
/** \name Vertex Formats
 * \{ */

/* MUST match the format below. */
struct GreasePencilStrokeVert {
  /** Position and radius packed in the same attribute. */
  float pos[3], radius;
  /** Material Index, Stroke Index, Point Index, Packed aspect + hardness + rotation. */
  int32_t mat, stroke_id, point_id, packed_asp_hard_rot;
  /** UV and opacity packed in the same attribute. */
  float uv_fill[2], u_stroke, opacity;
};

static GPUVertFormat *grease_pencil_stroke_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "ma", GPU_COMP_I32, 4, GPU_FETCH_INT);
    GPU_vertformat_attr_add(&format, "uv", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  return &format;
}

/* MUST match the format below. */
struct GreasePencilColorVert {
  float vcol[4]; /* Vertex color */
  float fcol[4]; /* Fill color */
};

static GPUVertFormat *grease_pencil_color_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "col", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "fcol", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  return &format;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

static bool grease_pencil_batch_cache_valid(const GreasePencil &grease_pencil, int cfra)
{
  BLI_assert(grease_pencil.runtime != nullptr);
  const GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil.runtime->batch_cache);
  return (cache && cache->is_dirty == false && cache->cache_frame == cfra);
}

static GreasePencilBatchCache *grease_pencil_batch_cache_init(GreasePencil &grease_pencil,
                                                              int cfra)
{
  BLI_assert(grease_pencil.runtime != nullptr);
  GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil.runtime->batch_cache);
  if (cache == nullptr) {
    cache = MEM_new<GreasePencilBatchCache>(__func__);
    grease_pencil.runtime->batch_cache = cache;
  }
  else {
    *cache = {};
  }

  cache->is_dirty = false;
  cache->cache_frame = cfra;

  return cache;
}

static void grease_pencil_batch_cache_clear(GreasePencil &grease_pencil)
{
  BLI_assert(grease_pencil.runtime != nullptr);
  GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil.runtime->batch_cache);
  if (cache == nullptr) {
    return;
  }

  GPU_BATCH_DISCARD_SAFE(cache->geom_batch);
  GPU_VERTBUF_DISCARD_SAFE(cache->vbo);
  GPU_VERTBUF_DISCARD_SAFE(cache->vbo_col);
  GPU_INDEXBUF_DISCARD_SAFE(cache->ibo);

  GPU_BATCH_DISCARD_SAFE(cache->edit_points);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_points_pos);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_points_selection);

  cache->is_dirty = true;
}

static GreasePencilBatchCache *grease_pencil_batch_cache_get(GreasePencil &grease_pencil, int cfra)
{
  BLI_assert(grease_pencil.runtime != nullptr);
  GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil.runtime->batch_cache);
  if (!grease_pencil_batch_cache_valid(grease_pencil, cfra)) {
    grease_pencil_batch_cache_clear(grease_pencil);
    return grease_pencil_batch_cache_init(grease_pencil, cfra);
  }

  return cache;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Buffers
 * \{ */

BLI_INLINE int32_t pack_rotation_aspect_hardness(float rot, float asp, float hard)
{
  int32_t packed = 0;
  /* Aspect uses 9 bits */
  float asp_normalized = (asp > 1.0f) ? (1.0f / asp) : asp;
  packed |= int32_t(unit_float_to_uchar_clamp(asp_normalized));
  /* Store if inverted in the 9th bit. */
  if (asp > 1.0f) {
    packed |= 1 << 8;
  }
  /* Rotation uses 9 bits */
  /* Rotation are in [-90..90] degree range, so we can encode the sign of the angle + the cosine
   * because the cosine will always be positive. */
  packed |= int32_t(unit_float_to_uchar_clamp(cosf(rot))) << 9;
  /* Store sine sign in 9th bit. */
  if (rot < 0.0f) {
    packed |= 1 << 17;
  }
  /* Hardness uses 8 bits */
  packed |= int32_t(unit_float_to_uchar_clamp(hard)) << 18;
  return packed;
}

static void grease_pencil_geom_batch_ensure(GreasePencil &grease_pencil, int cfra)
{
  using namespace blender::bke::greasepencil;
  BLI_assert(grease_pencil.runtime != nullptr);
  GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil.runtime->batch_cache);

  if (cache->vbo != nullptr) {
    return;
  }

  /* Should be discarded together. */
  BLI_assert(cache->vbo == nullptr && cache->ibo == nullptr);
  BLI_assert(cache->geom_batch == nullptr);

  /* Get the visible drawings. */
  Vector<Drawing *> drawings;
  grease_pencil.foreach_visible_drawing(
      cfra, [&](int /*drawing_index*/, Drawing &drawing) { drawings.append(&drawing); });

  /* First, count how many vertices and triangles are needed for the whole object. Also record the
   * offsets into the curves for the vertices and triangles. */
  int total_points_num = 0;
  int total_verts_num = 0;
  int total_triangles_num = 0;
  int v_offset = 0;
  Vector<Array<int>> verts_start_offsets_per_visible_drawing;
  Vector<Array<int>> tris_start_offsets_per_visible_drawing;
  for (const Drawing *drawing : drawings) {
    const bke::CurvesGeometry &curves = drawing->strokes();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();

    int verts_start_offsets_size = curves.curves_num();
    int tris_start_offsets_size = curves.curves_num();
    Array<int> verts_start_offsets(verts_start_offsets_size);
    Array<int> tris_start_offsets(tris_start_offsets_size);

    /* Calculate the vertex and triangle offsets for all the curves. */
    int t_offset = 0;
    int num_cyclic = 0;
    for (const int curve_i : curves.curves_range()) {
      IndexRange points = points_by_curve[curve_i];
      const bool is_cyclic = cyclic[curve_i];

      if (is_cyclic) {
        num_cyclic++;
      }

      tris_start_offsets[curve_i] = t_offset;
      if (points.size() >= 3) {
        t_offset += points.size() - 2;
      }

      verts_start_offsets[curve_i] = v_offset;
      v_offset += 1 + points.size() + (is_cyclic ? 1 : 0) + 1;
    }

    total_points_num += curves.points_num();

    /* One vertex is stored before and after as padding. Cyclic strokes have one extra
     * vertex.*/
    total_verts_num += curves.points_num() + num_cyclic + curves.curves_num() * 2;
    total_triangles_num += (curves.points_num() + num_cyclic) * 2;
    total_triangles_num += drawing->triangles().size();

    verts_start_offsets_per_visible_drawing.append(std::move(verts_start_offsets));
    tris_start_offsets_per_visible_drawing.append(std::move(tris_start_offsets));
  }

  if (grease_pencil.runtime->has_stroke_buffer()) {
    const int num_buffer_points = grease_pencil.runtime->stroke_buffer().size();
    total_verts_num += 1 + num_buffer_points + 1;
    total_triangles_num += num_buffer_points * 2;
    /* TODO: triangles for stroke buffer. */
  }

  static GPUVertFormat format_edit_points_pos = {0};
  if (format_edit_points_pos.attr_len == 0) {
    GPU_vertformat_attr_add(&format_edit_points_pos, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  static GPUVertFormat format_edit_points_selection = {0};
  if (format_edit_points_selection.attr_len == 0) {
    GPU_vertformat_attr_add(
        &format_edit_points_selection, "selection", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  cache->edit_points_pos = GPU_vertbuf_create_with_format(&format_edit_points_pos);
  cache->edit_points_selection = GPU_vertbuf_create_with_format(&format_edit_points_selection);
  GPU_vertbuf_data_alloc(cache->edit_points_pos, total_points_num);
  GPU_vertbuf_data_alloc(cache->edit_points_selection, total_points_num);

  GPUUsageType vbo_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
  /* Create VBOs. */
  GPUVertFormat *format = grease_pencil_stroke_format();
  GPUVertFormat *format_col = grease_pencil_color_format();
  cache->vbo = GPU_vertbuf_create_with_format_ex(format, vbo_flag);
  cache->vbo_col = GPU_vertbuf_create_with_format_ex(format_col, vbo_flag);
  /* Add extra space at the end of the buffer because of quad load. */
  GPU_vertbuf_data_alloc(cache->vbo, total_verts_num + 2);
  GPU_vertbuf_data_alloc(cache->vbo_col, total_verts_num + 2);

  GPUIndexBufBuilder ibo;
  MutableSpan<GreasePencilStrokeVert> verts = {
      static_cast<GreasePencilStrokeVert *>(GPU_vertbuf_get_data(cache->vbo)),
      GPU_vertbuf_get_vertex_len(cache->vbo)};
  MutableSpan<GreasePencilColorVert> cols = {
      static_cast<GreasePencilColorVert *>(GPU_vertbuf_get_data(cache->vbo_col)),
      GPU_vertbuf_get_vertex_len(cache->vbo_col)};
  MutableSpan<float3> edit_points = {
      static_cast<float3 *>(GPU_vertbuf_get_data(cache->edit_points_pos)),
      GPU_vertbuf_get_vertex_len(cache->edit_points_pos)};
  MutableSpan<float> edit_points_selection = {
      static_cast<float *>(GPU_vertbuf_get_data(cache->edit_points_selection)),
      GPU_vertbuf_get_vertex_len(cache->edit_points_selection)};
  /* Create IBO. */
  GPU_indexbuf_init(&ibo, GPU_PRIM_TRIS, total_triangles_num, 0xFFFFFFFFu);

  /* Fill buffers with data. */
  int drawing_start_offset = 0;
  for (const int drawing_i : drawings.index_range()) {
    const Drawing &drawing = *drawings[drawing_i];
    const bke::CurvesGeometry &curves = drawing.strokes();
    const bke::AttributeAccessor attributes = curves.attributes();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const Span<float3> positions = curves.positions();
    const VArray<bool> cyclic = curves.cyclic();
    const VArray<float> radii = *attributes.lookup_or_default<float>(
        "radius", ATTR_DOMAIN_POINT, 1.0f);
    const VArray<float> opacities = *attributes.lookup_or_default<float>(
        "opacity", ATTR_DOMAIN_POINT, 1.0f);
    /* Assumes that if the ".selection" attribute does not exist, all points are selected. */
    const VArray<float> selection_float = *attributes.lookup_or_default<float>(
        ".selection", ATTR_DOMAIN_POINT, true);
    const VArray<int8_t> start_caps = *attributes.lookup_or_default<int8_t>(
        "start_cap", ATTR_DOMAIN_CURVE, 0);
    const VArray<int8_t> end_caps = *attributes.lookup_or_default<int8_t>(
        "end_cap", ATTR_DOMAIN_CURVE, 0);
    const VArray<int> materials = *attributes.lookup_or_default<int>(
        "material_index", ATTR_DOMAIN_CURVE, -1);
    const Span<uint3> triangles = drawing.triangles();
    const Span<int> verts_start_offsets = verts_start_offsets_per_visible_drawing[drawing_i];
    const Span<int> tris_start_offsets = tris_start_offsets_per_visible_drawing[drawing_i];

    edit_points.slice(drawing_start_offset, curves.points_num()).copy_from(curves.positions());
    MutableSpan<float> selection_slice = edit_points_selection.slice(drawing_start_offset,
                                                                     curves.points_num());
    selection_float.materialize(selection_slice);
    drawing_start_offset += curves.points_num();

    auto populate_point = [&](IndexRange verts_range,
                              int curve_i,
                              int8_t start_cap,
                              int8_t end_cap,
                              int point_i,
                              int idx,
                              GreasePencilStrokeVert &s_vert,
                              GreasePencilColorVert &c_vert) {
      copy_v3_v3(s_vert.pos, positions[point_i]);
      s_vert.radius = radii[point_i] * ((end_cap == GP_STROKE_CAP_TYPE_ROUND) ? 1.0f : -1.0f);
      s_vert.opacity = opacities[point_i] *
                       ((start_cap == GP_STROKE_CAP_TYPE_ROUND) ? 1.0f : -1.0f);
      s_vert.point_id = verts_range[idx];
      s_vert.stroke_id = verts_range.first();
      s_vert.mat = materials[curve_i] % GPENCIL_MATERIAL_BUFFER_LEN;

      /* TODO: Populate rotation, aspect and hardness. */
      s_vert.packed_asp_hard_rot = pack_rotation_aspect_hardness(0.0f, 1.0f, 1.0f);
      /* TODO: Populate stroke UVs. */
      s_vert.u_stroke = 0;
      /* TODO: Populate fill UVs. */
      s_vert.uv_fill[0] = s_vert.uv_fill[1] = 0;

      /* TODO: Populate vertex color and fill color. */
      copy_v4_v4(c_vert.vcol, float4(0.0f, 0.0f, 0.0f, 0.0f));
      copy_v4_v4(c_vert.fcol, float4(0.0f, 0.0f, 0.0f, 0.0f));
      c_vert.fcol[3] = (int(c_vert.fcol[3] * 10000.0f) * 10.0f) + 1.0f;

      int v_mat = (verts_range[idx] << GP_VERTEX_ID_SHIFT) | GP_IS_STROKE_VERTEX_BIT;
      GPU_indexbuf_add_tri_verts(&ibo, v_mat + 0, v_mat + 1, v_mat + 2);
      GPU_indexbuf_add_tri_verts(&ibo, v_mat + 2, v_mat + 1, v_mat + 3);
    };

    threading::parallel_for(curves.curves_range(), 512, [&](IndexRange range) {
      for (const int curve_i : range) {
        IndexRange points = points_by_curve[curve_i];
        const bool is_cyclic = cyclic[curve_i];
        const int verts_start_offset = verts_start_offsets[curve_i];
        const int tris_start_offset = tris_start_offsets[curve_i];
        const int num_verts = 1 + points.size() + (is_cyclic ? 1 : 0) + 1;
        IndexRange verts_range = IndexRange(verts_start_offset, num_verts);
        MutableSpan<GreasePencilStrokeVert> verts_slice = verts.slice(verts_range);
        MutableSpan<GreasePencilColorVert> cols_slice = cols.slice(verts_range);

        /* First vertex is not drawn. */
        verts_slice.first().mat = -1;

        /* If the stroke has more than 2 points, add the triangle indices to the index buffer. */
        if (points.size() >= 3) {
          const Span<uint3> tris_slice = triangles.slice(tris_start_offset, points.size() - 2);
          for (const uint3 tri : tris_slice) {
            GPU_indexbuf_add_tri_verts(&ibo,
                                       (verts_range[1] + tri.x) << GP_VERTEX_ID_SHIFT,
                                       (verts_range[1] + tri.y) << GP_VERTEX_ID_SHIFT,
                                       (verts_range[1] + tri.z) << GP_VERTEX_ID_SHIFT);
          }
        }

        /* Write all the point attributes to the vertex buffers. Create a quad for each point. */
        for (const int i : IndexRange(points.size())) {
          const int idx = i + 1;
          populate_point(verts_range,
                         curve_i,
                         start_caps[curve_i],
                         end_caps[curve_i],
                         points[i],
                         idx,
                         verts_slice[idx],
                         cols_slice[idx]);
        }

        if (is_cyclic) {
          const int idx = points.size() + 1;
          populate_point(verts_range,
                         curve_i,
                         start_caps[curve_i],
                         end_caps[curve_i],
                         points[0],
                         idx,
                         verts_slice[idx],
                         cols_slice[idx]);
        }

        /* Last vertex is not drawn. */
        verts_slice.last().mat = -1;
      }
    });
  }

  if (grease_pencil.runtime->has_stroke_buffer()) {
    Span<bke::greasepencil::StrokePoint> points = grease_pencil.runtime->stroke_buffer();
    const int verts_start_offset = v_offset;
    const int num_verts = 1 + points.size() + 1;
    IndexRange verts_range = IndexRange(verts_start_offset, num_verts);
    MutableSpan<GreasePencilStrokeVert> verts_slice = verts.slice(verts_range);
    MutableSpan<GreasePencilColorVert> cols_slice = cols.slice(verts_range);
    const int material_nr = grease_pencil.runtime->stroke_cache.mat;

    verts_slice.first().mat = -1;
    for (const int i : IndexRange(points.size())) {
      const int idx = i + 1;
      GreasePencilStrokeVert &s_vert = verts_slice[idx];
      GreasePencilColorVert &c_vert = cols_slice[idx];
      const bke::greasepencil::StrokePoint &point = points[i];

      copy_v3_v3(s_vert.pos, point.position);
      s_vert.radius = point.radius;
      s_vert.opacity = point.opacity;
      s_vert.point_id = verts_range[idx];
      s_vert.stroke_id = verts_range.first();
      s_vert.mat = material_nr;

      /* TODO */
      s_vert.packed_asp_hard_rot = pack_rotation_aspect_hardness(0.0f, 1.0f, 1.0f);
      /* TODO */
      s_vert.u_stroke = 0;
      /* TODO */
      s_vert.uv_fill[0] = s_vert.uv_fill[1] = 0;

      /* TODO */
      copy_v4_v4(c_vert.vcol, float4(0.0f, 0.0f, 0.0f, 0.0f));
      copy_v4_v4(c_vert.fcol, float4(0.0f, 0.0f, 0.0f, 0.0f));

      /* TODO */
      c_vert.fcol[3] = (int(c_vert.fcol[3] * 10000.0f) * 10.0f) + 1.0f;

      int v_mat = (verts_range[idx] << GP_VERTEX_ID_SHIFT) | GP_IS_STROKE_VERTEX_BIT;
      GPU_indexbuf_add_tri_verts(&ibo, v_mat + 0, v_mat + 1, v_mat + 2);
      GPU_indexbuf_add_tri_verts(&ibo, v_mat + 2, v_mat + 1, v_mat + 3);
    }

    verts_slice.last().mat = -1;
  }

  /* Mark last 2 verts as invalid. */
  verts[total_verts_num + 0].mat = -1;
  verts[total_verts_num + 1].mat = -1;
  /* Also mark first vert as invalid. */
  verts[0].mat = -1;

  /* Finish the IBO. */
  cache->ibo = GPU_indexbuf_build(&ibo);
  /* Create the batches */
  cache->geom_batch = GPU_batch_create(GPU_PRIM_TRIS, cache->vbo, cache->ibo);
  /* Allow creation of buffer texture. */
  GPU_vertbuf_use(cache->vbo);
  GPU_vertbuf_use(cache->vbo_col);

  /* Create the batches */
  cache->edit_points = GPU_batch_create(GPU_PRIM_POINTS, cache->edit_points_pos, nullptr);
  GPU_batch_vertbuf_add(cache->edit_points, cache->edit_points_selection, false);

  cache->is_dirty = false;
}

/** \} */

}  // namespace blender::draw

void DRW_grease_pencil_batch_cache_dirty_tag(GreasePencil *grease_pencil, int mode)
{
  using namespace blender::draw;
  BLI_assert(grease_pencil->runtime != nullptr);
  GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil->runtime->batch_cache);
  if (cache == nullptr) {
    return;
  }
  switch (mode) {
    case BKE_GREASEPENCIL_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    default:
      BLI_assert_unreachable();
  }
}

void DRW_grease_pencil_batch_cache_validate(GreasePencil *grease_pencil)
{
  using namespace blender::draw;
  BLI_assert(grease_pencil->runtime != nullptr);
  /* TODO: pass correct frame here? */
  if (!grease_pencil_batch_cache_valid(*grease_pencil, 0)) {
    grease_pencil_batch_cache_clear(*grease_pencil);
    /* TODO: pass correct frame here? */
    grease_pencil_batch_cache_init(*grease_pencil, 0);
  }
}

void DRW_grease_pencil_batch_cache_free(GreasePencil *grease_pencil)
{
  using namespace blender::draw;
  grease_pencil_batch_cache_clear(*grease_pencil);
  MEM_delete(static_cast<GreasePencilBatchCache *>(grease_pencil->runtime->batch_cache));
  grease_pencil->runtime->batch_cache = nullptr;
}

GPUBatch *DRW_cache_grease_pencil_get(Object *ob, int cfra)
{
  using namespace blender::draw;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil, cfra);
  grease_pencil_geom_batch_ensure(grease_pencil, cfra);

  return cache->geom_batch;
}

GPUBatch *DRW_cache_grease_pencil_edit_points_get(Object *ob, int cfra)
{
  using namespace blender::draw;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil, cfra);
  grease_pencil_geom_batch_ensure(grease_pencil, cfra);

  return cache->edit_points;
}

GPUVertBuf *DRW_cache_grease_pencil_position_buffer_get(Object *ob, int cfra)
{
  using namespace blender::draw;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil, cfra);
  grease_pencil_geom_batch_ensure(grease_pencil, cfra);

  return cache->vbo;
}

GPUVertBuf *DRW_cache_grease_pencil_color_buffer_get(Object *ob, int cfra)
{
  using namespace blender::draw;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil, cfra);
  grease_pencil_geom_batch_ensure(grease_pencil, cfra);

  return cache->vbo_col;
}
