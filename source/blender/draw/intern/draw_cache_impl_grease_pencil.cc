/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Grease Pencil API for render engines
 */

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.h"
#include "BKE_grease_pencil.hh"

#include "BLI_task.hh"

#include "DNA_grease_pencil_types.h"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"

#include "GPU_batch.hh"

#include "draw_cache_impl.hh"

#include "../engines/gpencil/gpencil_defines.h"
#include "../engines/gpencil/gpencil_shader_shared.h"

namespace blender::draw {

struct GreasePencilBatchCache {
  /** Instancing Data */
  gpu::VertBuf *vbo;
  gpu::VertBuf *vbo_col;
  /** Indices in material order, then stroke order with fill first. */
  gpu::IndexBuf *ibo;
  /** Batches */
  gpu::Batch *geom_batch;
  gpu::Batch *edit_points;
  gpu::Batch *edit_lines;

  /* Crazy-space point positions for original points. */
  gpu::VertBuf *edit_points_pos;
  /* Selection of original points. */
  gpu::VertBuf *edit_points_selection;
  /* Indices for lines segments. */
  gpu::IndexBuf *edit_line_indices;
  /* Indices of visible points. */
  gpu::IndexBuf *edit_points_indices;

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

static bool grease_pencil_batch_cache_valid(const GreasePencil &grease_pencil)
{
  BLI_assert(grease_pencil.runtime != nullptr);
  const GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil.runtime->batch_cache);
  return (cache && cache->is_dirty == false &&
          cache->cache_frame == grease_pencil.runtime->eval_frame);
}

static GreasePencilBatchCache *grease_pencil_batch_cache_init(GreasePencil &grease_pencil)
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
  cache->cache_frame = grease_pencil.runtime->eval_frame;

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
  GPU_BATCH_DISCARD_SAFE(cache->edit_lines);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_points_pos);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_points_selection);
  GPU_INDEXBUF_DISCARD_SAFE(cache->edit_line_indices);
  GPU_INDEXBUF_DISCARD_SAFE(cache->edit_points_indices);

  cache->is_dirty = true;
}

static GreasePencilBatchCache *grease_pencil_batch_cache_get(GreasePencil &grease_pencil)
{
  BLI_assert(grease_pencil.runtime != nullptr);
  GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil.runtime->batch_cache);
  if (!grease_pencil_batch_cache_valid(grease_pencil)) {
    grease_pencil_batch_cache_clear(grease_pencil);
    return grease_pencil_batch_cache_init(grease_pencil);
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

static void copy_transformed_positions(const Span<float3> src_positions,
                                       const IndexRange range,
                                       const float4x4 &transform,
                                       MutableSpan<float3> dst_positions)
{
  for (const int point_i : range) {
    dst_positions[point_i] = math::transform_point(transform, src_positions[point_i]);
  }
}

static void grease_pencil_edit_batch_ensure(Object &object,
                                            const GreasePencil &grease_pencil,
                                            const Scene &scene)
{
  using namespace blender::bke::greasepencil;
  BLI_assert(grease_pencil.runtime != nullptr);
  GreasePencilBatchCache *cache = static_cast<GreasePencilBatchCache *>(
      grease_pencil.runtime->batch_cache);

  if (cache->edit_points_pos != nullptr) {
    return;
  }

  /* Should be discarded together. */
  BLI_assert(cache->edit_points_pos == nullptr && cache->edit_line_indices == nullptr &&
             cache->edit_points_indices == nullptr);
  BLI_assert(cache->edit_points == nullptr && cache->edit_lines == nullptr);

  /* Get the visible drawings. */
  const Vector<ed::greasepencil::DrawingInfo> drawings =
      ed::greasepencil::retrieve_visible_drawings(scene, grease_pencil, false);

  const Span<const Layer *> layers = grease_pencil.layers();

  static GPUVertFormat format_edit_points_pos = {0};
  if (format_edit_points_pos.attr_len == 0) {
    GPU_vertformat_attr_add(&format_edit_points_pos, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  static GPUVertFormat format_edit_points_selection = {0};
  if (format_edit_points_selection.attr_len == 0) {
    GPU_vertformat_attr_add(
        &format_edit_points_selection, "selection", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  GPUUsageType vbo_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
  cache->edit_points_pos = GPU_vertbuf_create_with_format_ex(&format_edit_points_pos, vbo_flag);
  cache->edit_points_selection = GPU_vertbuf_create_with_format_ex(&format_edit_points_selection,
                                                                   vbo_flag);

  int total_points_num = 0;
  for (const ed::greasepencil::DrawingInfo &info : drawings) {
    const bke::CurvesGeometry &curves = info.drawing.strokes();
    total_points_num += curves.points_num();
  }

  GPU_vertbuf_data_alloc(cache->edit_points_pos, total_points_num);
  GPU_vertbuf_data_alloc(cache->edit_points_selection, total_points_num);

  MutableSpan<float3> edit_points = {
      static_cast<float3 *>(GPU_vertbuf_get_data(cache->edit_points_pos)),
      GPU_vertbuf_get_vertex_len(cache->edit_points_pos)};
  MutableSpan<float> edit_points_selection = {
      static_cast<float *>(GPU_vertbuf_get_data(cache->edit_points_selection)),
      GPU_vertbuf_get_vertex_len(cache->edit_points_selection)};

  int visible_points_num = 0;
  int total_line_ids_num = 0;
  int drawing_start_offset = 0;
  for (const ed::greasepencil::DrawingInfo &info : drawings) {
    const Layer &layer = *layers[info.layer_index];
    const float4x4 layer_space_to_object_space = layer.to_object_space(object);
    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const bke::AttributeAccessor attributes = curves.attributes();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    IndexMaskMemory memory;
    const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_strokes(
        object, info.drawing, memory);

    /* Assumes that if the ".selection" attribute does not exist, all points are selected. */
    const VArray<float> selection_float = *attributes.lookup_or_default<float>(
        ".selection", bke::AttrDomain::Point, true);

    const IndexRange points(drawing_start_offset, curves.points_num());
    const Span<float3> positions = curves.positions();
    MutableSpan<float3> positions_slice = edit_points.slice(points);
    threading::parallel_for(curves.points_range(), 1024, [&](const IndexRange range) {
      copy_transformed_positions(positions, range, layer_space_to_object_space, positions_slice);
    });
    MutableSpan<float> selection_slice = edit_points_selection.slice(points);
    /* Do not show selection for locked layers. */
    if (layer.is_locked()) {
      selection_slice.fill(0.0f);
    }
    else {
      selection_float.materialize(selection_slice);
    }
    drawing_start_offset += curves.points_num();

    /* Add one id for the restart after every curve. */
    total_line_ids_num += editable_strokes.size();
    Array<int> size_per_editable_stroke(editable_strokes.size());
    offset_indices::gather_group_sizes(
        points_by_curve, editable_strokes, size_per_editable_stroke);
    /* Add one id for every non-cyclic segment. */
    total_line_ids_num += std::accumulate(
        size_per_editable_stroke.begin(), size_per_editable_stroke.end(), 0);
    /* Add one id for the last segment of every cyclic curve. */
    total_line_ids_num += array_utils::count_booleans(curves.cyclic(), editable_strokes);

    /* Do not show points for locked layers. */
    if (layer.is_locked()) {
      continue;
    }

    const VArray<bool> selection = *attributes.lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);

    editable_strokes.foreach_index([&](const int curve_i) {
      const IndexRange points = points_by_curve[curve_i];
      if (ed::curves::has_anything_selected(selection, points)) {
        visible_points_num += points.size();
      }
    });
  }

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb,
                       GPU_PRIM_LINE_STRIP,
                       total_line_ids_num,
                       GPU_vertbuf_get_vertex_len(cache->edit_points_pos));

  GPUIndexBufBuilder epb;
  GPU_indexbuf_init_ex(&epb,
                       GPU_PRIM_POINTS,
                       visible_points_num,
                       GPU_vertbuf_get_vertex_len(cache->edit_points_pos));

  /* Fill line index and point index buffers with data. */
  drawing_start_offset = 0;
  for (const ed::greasepencil::DrawingInfo &info : drawings) {
    const Layer *layer = layers[info.layer_index];
    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    IndexMaskMemory memory;
    const IndexMask editable_strokes = ed::greasepencil::retrieve_editable_strokes(
        object, info.drawing, memory);

    /* Fill line indices. */
    editable_strokes.foreach_index([&](const int curve_i) {
      const IndexRange points = points_by_curve[curve_i];
      const bool is_cyclic = cyclic[curve_i];

      for (const int point_i : points) {
        GPU_indexbuf_add_generic_vert(&elb, point_i + drawing_start_offset);
      }

      if (is_cyclic) {
        GPU_indexbuf_add_generic_vert(&elb, points.first() + drawing_start_offset);
      }

      GPU_indexbuf_add_primitive_restart(&elb);
    });

    /* Assumes that if the ".selection" attribute does not exist, all points are selected. */
    const VArray<bool> selection = *curves.attributes().lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);

    /* Fill point indices. */
    if (!layer->is_locked()) {
      editable_strokes.foreach_index([&](const int curve_i) {
        const IndexRange points = points_by_curve[curve_i];
        if (!ed::curves::has_anything_selected(selection, points)) {
          return;
        }

        for (const int point : points) {
          GPU_indexbuf_add_generic_vert(&epb, point + drawing_start_offset);
        }
      });
    }

    drawing_start_offset += curves.points_num();
  }

  cache->edit_line_indices = GPU_indexbuf_build(&elb);
  cache->edit_points_indices = GPU_indexbuf_build(&epb);

  /* Create the batches */
  cache->edit_points = GPU_batch_create(
      GPU_PRIM_POINTS, cache->edit_points_pos, cache->edit_points_indices);
  GPU_batch_vertbuf_add(cache->edit_points, cache->edit_points_selection, false);

  cache->edit_lines = GPU_batch_create(
      GPU_PRIM_LINE_STRIP, cache->edit_points_pos, cache->edit_line_indices);
  GPU_batch_vertbuf_add(cache->edit_lines, cache->edit_points_selection, false);
  /* Allow creation of buffer texture. */
  GPU_vertbuf_use(cache->edit_points_pos);
  GPU_vertbuf_use(cache->edit_points_selection);

  cache->is_dirty = false;
}

static void grease_pencil_geom_batch_ensure(Object &object,
                                            const GreasePencil &grease_pencil,
                                            const Scene &scene)
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
  const Vector<ed::greasepencil::DrawingInfo> drawings =
      ed::greasepencil::retrieve_visible_drawings(scene, grease_pencil, true);

  /* First, count how many vertices and triangles are needed for the whole object. Also record the
   * offsets into the curves for the vertices and triangles. */
  int total_verts_num = 0;
  int total_triangles_num = 0;
  int v_offset = 0;
  Vector<Array<int>> verts_start_offsets_per_visible_drawing;
  Vector<Array<int>> tris_start_offsets_per_visible_drawing;
  for (const ed::greasepencil::DrawingInfo &info : drawings) {
    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    IndexMaskMemory memory;
    const IndexMask visible_strokes = ed::greasepencil::retrieve_visible_strokes(
        object, info.drawing, memory);

    const int num_curves = visible_strokes.size();
    const int verts_start_offsets_size = num_curves;
    const int tris_start_offsets_size = num_curves;
    Array<int> verts_start_offsets(verts_start_offsets_size);
    Array<int> tris_start_offsets(tris_start_offsets_size);

    /* Calculate the triangle offsets for all the visible curves. */
    int t_offset = 0;
    int pos = 0;
    for (const int curve_i : curves.curves_range()) {
      IndexRange points = points_by_curve[curve_i];
      if (visible_strokes.contains(curve_i)) {
        tris_start_offsets[pos] = t_offset;
        pos++;
      }
      if (points.size() >= 3) {
        t_offset += points.size() - 2;
      }
    }

    /* Calculate the vertex offsets for all the visible curves. */
    int num_cyclic = 0;
    int num_points = 0;
    visible_strokes.foreach_index([&](const int curve_i, const int pos) {
      IndexRange points = points_by_curve[curve_i];
      const bool is_cyclic = cyclic[curve_i];

      if (is_cyclic) {
        num_cyclic++;
      }

      verts_start_offsets[pos] = v_offset;
      v_offset += 1 + points.size() + (is_cyclic ? 1 : 0) + 1;
      num_points += points.size();
    });

    /* One vertex is stored before and after as padding. Cyclic strokes have one extra vertex. */
    total_verts_num += num_points + num_cyclic + num_curves * 2;
    total_triangles_num += (num_points + num_cyclic) * 2;
    total_triangles_num += info.drawing.triangles().size();

    verts_start_offsets_per_visible_drawing.append(std::move(verts_start_offsets));
    tris_start_offsets_per_visible_drawing.append(std::move(tris_start_offsets));
  }

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
  /* Create IBO. */
  GPU_indexbuf_init(&ibo, GPU_PRIM_TRIS, total_triangles_num, 0xFFFFFFFFu);

  /* Fill buffers with data. */
  for (const int drawing_i : drawings.index_range()) {
    const ed::greasepencil::DrawingInfo &info = drawings[drawing_i];
    const Layer &layer = *grease_pencil.layers()[info.layer_index];
    const float4x4 layer_space_to_object_space = layer.to_object_space(object);
    const float4x4 object_space_to_layer_space = math::invert(layer_space_to_object_space);
    const bke::CurvesGeometry &curves = info.drawing.strokes();
    const bke::AttributeAccessor attributes = curves.attributes();
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    const Span<float3> positions = curves.positions();
    const VArray<bool> cyclic = curves.cyclic();
    const VArray<float> radii = info.drawing.radii();
    const VArray<float> opacities = info.drawing.opacities();
    const VArray<ColorGeometry4f> vertex_colors = *attributes.lookup_or_default<ColorGeometry4f>(
        "vertex_color", bke::AttrDomain::Point, ColorGeometry4f(0.0f, 0.0f, 0.0f, 0.0f));
    const VArray<float> rotations = *attributes.lookup_or_default<float>(
        "rotation", bke::AttrDomain::Point, 0.0f);
    /* Assumes that if the ".selection" attribute does not exist, all points are selected. */
    const VArray<float> selection_float = *attributes.lookup_or_default<float>(
        ".selection", bke::AttrDomain::Point, true);
    const VArray<int8_t> start_caps = *attributes.lookup_or_default<int8_t>(
        "start_cap", bke::AttrDomain::Curve, GP_STROKE_CAP_TYPE_ROUND);
    const VArray<int8_t> end_caps = *attributes.lookup_or_default<int8_t>(
        "end_cap", bke::AttrDomain::Curve, 0);
    const VArray<float> stroke_hardnesses = *attributes.lookup_or_default<float>(
        "hardness", bke::AttrDomain::Curve, 1.0f);
    const VArray<float> stroke_point_aspect_ratios = *attributes.lookup_or_default<float>(
        "aspect_ratio", bke::AttrDomain::Curve, 1.0f);
    const VArray<ColorGeometry4f> stroke_fill_colors = info.drawing.fill_colors();
    const VArray<int> materials = *attributes.lookup_or_default<int>(
        "material_index", bke::AttrDomain::Curve, 0);
    const VArray<float> u_translations = *attributes.lookup_or_default<float>(
        "u_translation", bke::AttrDomain::Curve, 0.0f);
    const VArray<float> u_scales = *attributes.lookup_or_default<float>(
        "u_scale", bke::AttrDomain::Curve, 1.0f);
    const Span<uint3> triangles = info.drawing.triangles();
    const Span<float4x2> texture_matrices = info.drawing.texture_matrices();
    const Span<int> verts_start_offsets = verts_start_offsets_per_visible_drawing[drawing_i];
    const Span<int> tris_start_offsets = tris_start_offsets_per_visible_drawing[drawing_i];
    IndexMaskMemory memory;
    const IndexMask visible_strokes = ed::greasepencil::retrieve_visible_strokes(
        object, info.drawing, memory);

    curves.ensure_evaluated_lengths();

    auto populate_point = [&](IndexRange verts_range,
                              int curve_i,
                              int8_t start_cap,
                              int8_t end_cap,
                              int point_i,
                              int idx,
                              float u_stroke,
                              const float4x2 &texture_matrix,
                              GreasePencilStrokeVert &s_vert,
                              GreasePencilColorVert &c_vert) {
      const float3 pos = math::transform_point(layer_space_to_object_space, positions[point_i]);
      copy_v3_v3(s_vert.pos, pos);
      s_vert.radius = radii[point_i] * ((end_cap == GP_STROKE_CAP_TYPE_ROUND) ? 1.0f : -1.0f);
      /* Convert to legacy "pixel" space. The shader expects the values to be in this space.
       * Otherwise the values will get clamped. */
      s_vert.radius *= 1000.0f;
      s_vert.opacity = opacities[point_i] *
                       ((start_cap == GP_STROKE_CAP_TYPE_ROUND) ? 1.0f : -1.0f);
      s_vert.point_id = verts_range[idx];
      s_vert.stroke_id = verts_range.first();
      s_vert.mat = materials[curve_i] % GPENCIL_MATERIAL_BUFFER_LEN;

      s_vert.packed_asp_hard_rot = pack_rotation_aspect_hardness(
          rotations[point_i], stroke_point_aspect_ratios[curve_i], stroke_hardnesses[curve_i]);
      s_vert.u_stroke = u_stroke;
      copy_v2_v2(s_vert.uv_fill, texture_matrix * float4(pos, 1.0f));

      copy_v4_v4(c_vert.vcol, vertex_colors[point_i]);
      copy_v4_v4(c_vert.fcol, stroke_fill_colors[curve_i]);
      c_vert.fcol[3] = (int(c_vert.fcol[3] * 10000.0f) * 10.0f) + 1.0f;

      int v_mat = (verts_range[idx] << GP_VERTEX_ID_SHIFT) | GP_IS_STROKE_VERTEX_BIT;
      GPU_indexbuf_add_tri_verts(&ibo, v_mat + 0, v_mat + 1, v_mat + 2);
      GPU_indexbuf_add_tri_verts(&ibo, v_mat + 2, v_mat + 1, v_mat + 3);
    };

    visible_strokes.foreach_index([&](const int curve_i, const int pos) {
      const IndexRange points = points_by_curve[curve_i];
      const bool is_cyclic = cyclic[curve_i];
      const int verts_start_offset = verts_start_offsets[pos];
      const int tris_start_offset = tris_start_offsets[pos];
      const int num_verts = 1 + points.size() + (is_cyclic ? 1 : 0) + 1;
      const IndexRange verts_range = IndexRange(verts_start_offset, num_verts);
      MutableSpan<GreasePencilStrokeVert> verts_slice = verts.slice(verts_range);
      MutableSpan<GreasePencilColorVert> cols_slice = cols.slice(verts_range);
      const float4x2 texture_matrix = texture_matrices[curve_i] * object_space_to_layer_space;

      const Span<float> lengths = curves.evaluated_lengths_for_curve(curve_i, is_cyclic);

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
      const float u_scale = u_scales[curve_i];
      const float u_translation = u_translations[curve_i];
      for (const int i : IndexRange(points.size())) {
        const int idx = i + 1;
        const float u_stroke = u_scale * (i > 0 ? lengths[i - 1] : 0.0f) + u_translation;
        populate_point(verts_range,
                       curve_i,
                       start_caps[curve_i],
                       end_caps[curve_i],
                       points[i],
                       idx,
                       u_stroke,
                       texture_matrix,
                       verts_slice[idx],
                       cols_slice[idx]);
      }

      if (is_cyclic && points.size() > 1) {
        const int idx = points.size() + 1;
        const float u_stroke = u_scale * lengths[points.size() - 1] + u_translation;
        populate_point(verts_range,
                       curve_i,
                       start_caps[curve_i],
                       end_caps[curve_i],
                       points[0],
                       idx,
                       u_stroke,
                       texture_matrix,
                       verts_slice[idx],
                       cols_slice[idx]);
      }

      /* Last vertex is not drawn. */
      verts_slice.last().mat = -1;
    });
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

  cache->is_dirty = false;
}

/** \} */

void DRW_grease_pencil_batch_cache_dirty_tag(GreasePencil *grease_pencil, int mode)
{
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
  BLI_assert(grease_pencil->runtime != nullptr);
  if (!grease_pencil_batch_cache_valid(*grease_pencil)) {
    grease_pencil_batch_cache_clear(*grease_pencil);
    grease_pencil_batch_cache_init(*grease_pencil);
  }
}

void DRW_grease_pencil_batch_cache_free(GreasePencil *grease_pencil)
{
  grease_pencil_batch_cache_clear(*grease_pencil);
  MEM_delete(static_cast<GreasePencilBatchCache *>(grease_pencil->runtime->batch_cache));
  grease_pencil->runtime->batch_cache = nullptr;
}

gpu::Batch *DRW_cache_grease_pencil_get(const Scene *scene, Object *ob)
{
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil);
  grease_pencil_geom_batch_ensure(*ob, grease_pencil, *scene);

  return cache->geom_batch;
}

gpu::Batch *DRW_cache_grease_pencil_edit_points_get(const Scene *scene, Object *ob)
{
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil);
  grease_pencil_edit_batch_ensure(*ob, grease_pencil, *scene);

  return cache->edit_points;
}

gpu::Batch *DRW_cache_grease_pencil_edit_lines_get(const Scene *scene, Object *ob)
{
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil);
  grease_pencil_edit_batch_ensure(*ob, grease_pencil, *scene);

  return cache->edit_lines;
}

gpu::VertBuf *DRW_cache_grease_pencil_position_buffer_get(const Scene *scene, Object *ob)
{
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil);
  grease_pencil_geom_batch_ensure(*ob, grease_pencil, *scene);

  return cache->vbo;
}

gpu::VertBuf *DRW_cache_grease_pencil_color_buffer_get(const Scene *scene, Object *ob)
{
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
  GreasePencilBatchCache *cache = grease_pencil_batch_cache_get(grease_pencil);
  grease_pencil_geom_batch_ensure(*ob, grease_pencil, *scene);

  return cache->vbo_col;
}

}  // namespace blender::draw
