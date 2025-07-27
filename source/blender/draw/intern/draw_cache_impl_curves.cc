/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Curves API for render engines
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_string_utf8.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "DNA_curves_types.h"
#include "DNA_object_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph_query.hh"

#include "BKE_crazyspace.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_customdata.hh"
#include "BKE_geometry_set.hh"

#include "GPU_batch.hh"
#include "GPU_context.hh"
#include "GPU_material.hh"
#include "GPU_texture.hh"

#include "DRW_render.hh"

#include "draw_attributes.hh"
#include "draw_cache_impl.hh" /* own include */
#include "draw_cache_inline.hh"
#include "draw_curves_private.hh" /* own include */

namespace blender::draw {

#define EDIT_CURVES_NURBS_CONTROL_POINT (1u)
#define EDIT_CURVES_BEZIER_HANDLE (1u << 1)
#define EDIT_CURVES_ACTIVE_HANDLE (1u << 2)
/* Bezier curve control point lying on the curve.
 * The one between left and right handles. */
#define EDIT_CURVES_BEZIER_KNOT (1u << 3)
#define EDIT_CURVES_HANDLE_TYPES_SHIFT (4u)

/* ---------------------------------------------------------------------- */

struct CurvesBatchCache {
  CurvesEvalCache eval_cache;

  gpu::Batch *edit_points;
  gpu::Batch *edit_handles;

  gpu::Batch *sculpt_cage;
  gpu::IndexBuf *sculpt_cage_ibo;

  /* Crazy-space point positions for original points. */
  gpu::VertBuf *edit_points_pos;

  /* Additional data needed for shader to choose color for each point in edit_points_pos.
   * If first bit is set, then point is NURBS control point. EDIT_CURVES_NURBS_CONTROL_POINT is
   * used to set and test. If second, then point is Bezier handle point. Set and tested with
   * EDIT_CURVES_BEZIER_HANDLE.
   * In Bezier case two handle types of HandleType are also encoded.
   * Byte structure for Bezier knot point (handle middle point):
   * | left handle type | right handle type |      | BEZIER|  NURBS|
   * | 7              6 | 5               4 | 3  2 |     1 |     0 |
   *
   * If it is left or right handle point, then same handle type is repeated in both slots.
   */
  gpu::VertBuf *edit_points_data;

  /* Selection of original points. */
  gpu::VertBuf *edit_points_selection;

  gpu::IndexBuf *edit_handles_ibo;

  gpu::Batch *edit_curves_lines;
  gpu::VertBuf *edit_curves_lines_pos;
  gpu::IndexBuf *edit_curves_lines_ibo;

  /* Whether the cache is invalid. */
  bool is_dirty;
};

static bool batch_cache_is_dirty(const Curves &curves)
{
  const CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves.batch_cache);
  return (cache && cache->is_dirty == false);
}

static void init_batch_cache(Curves &curves)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves.batch_cache);

  if (!cache) {
    cache = MEM_new<CurvesBatchCache>(__func__);
    curves.batch_cache = cache;
  }
  else {
    cache->eval_cache = {};
  }

  cache->is_dirty = false;
}

static void discard_attributes(CurvesEvalCache &eval_cache)
{
  for (const int i : IndexRange(GPU_MAX_ATTR)) {
    GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_attributes_buf[i]);
  }

  for (const int j : IndexRange(GPU_MAX_ATTR)) {
    GPU_VERTBUF_DISCARD_SAFE(eval_cache.final.attributes_buf[j]);
  }

  eval_cache.final.attr_used.clear();
}

static void clear_edit_data(CurvesBatchCache *cache)
{
  /* TODO: more granular update tagging. */
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_points_pos);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_points_data);
  GPU_VERTBUF_DISCARD_SAFE(cache->edit_points_selection);
  GPU_INDEXBUF_DISCARD_SAFE(cache->edit_handles_ibo);

  GPU_BATCH_DISCARD_SAFE(cache->edit_points);
  GPU_BATCH_DISCARD_SAFE(cache->edit_handles);

  GPU_INDEXBUF_DISCARD_SAFE(cache->sculpt_cage_ibo);
  GPU_BATCH_DISCARD_SAFE(cache->sculpt_cage);

  GPU_VERTBUF_DISCARD_SAFE(cache->edit_curves_lines_pos);
  GPU_INDEXBUF_DISCARD_SAFE(cache->edit_curves_lines_ibo);
  GPU_BATCH_DISCARD_SAFE(cache->edit_curves_lines);
}

static void clear_final_data(CurvesEvalFinalCache &final_cache)
{
  GPU_VERTBUF_DISCARD_SAFE(final_cache.proc_buf);
  GPU_BATCH_DISCARD_SAFE(final_cache.proc_hairs);
  for (const int j : IndexRange(GPU_MAX_ATTR)) {
    GPU_VERTBUF_DISCARD_SAFE(final_cache.attributes_buf[j]);
  }
}

static void clear_eval_data(CurvesEvalCache &eval_cache)
{
  /* TODO: more granular update tagging. */
  GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_point_buf);
  GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_length_buf);
  GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_strand_buf);
  GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_strand_seg_buf);

  clear_final_data(eval_cache.final);

  discard_attributes(eval_cache);
}

static void clear_batch_cache(Curves &curves)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves.batch_cache);
  if (!cache) {
    return;
  }

  clear_eval_data(cache->eval_cache);
  clear_edit_data(cache);
}

static CurvesBatchCache &get_batch_cache(Curves &curves)
{
  DRW_curves_batch_cache_validate(&curves);
  return *static_cast<CurvesBatchCache *>(curves.batch_cache);
}

struct PositionAndParameter {
  float3 position;
  float parameter;
};

static void fill_points_position_time_vbo(const OffsetIndices<int> points_by_curve,
                                          const Span<float3> positions,
                                          MutableSpan<PositionAndParameter> posTime_data,
                                          MutableSpan<float> hairLength_data)
{
  threading::parallel_for(points_by_curve.index_range(), 1024, [&](const IndexRange range) {
    for (const int i_curve : range) {
      const IndexRange points = points_by_curve[i_curve];

      Span<float3> curve_positions = positions.slice(points);
      MutableSpan<PositionAndParameter> curve_posTime_data = posTime_data.slice(points);

      float total_len = 0.0f;
      for (const int i_point : curve_positions.index_range()) {
        if (i_point > 0) {
          total_len += math::distance(curve_positions[i_point - 1], curve_positions[i_point]);
        }
        curve_posTime_data[i_point].position = curve_positions[i_point];
        curve_posTime_data[i_point].parameter = total_len;
      }
      hairLength_data[i_curve] = total_len;

      /* Assign length value. */
      if (total_len > 0.0f) {
        const float factor = 1.0f / total_len;
        /* Divide by total length to have a [0-1] number. */
        for (const int i_point : curve_positions.index_range()) {
          curve_posTime_data[i_point].parameter *= factor;
        }
      }
    }
  });
}

static void create_points_position_time_vbo(const bke::CurvesGeometry &curves,
                                            CurvesEvalCache &cache)
{
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "posTime", gpu::VertAttrType::SFLOAT_32_32_32_32);

  cache.proc_point_buf = GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(*cache.proc_point_buf, cache.points_num);

  GPUVertFormat length_format = {0};
  GPU_vertformat_attr_add(&length_format, "hairLength", blender::gpu::VertAttrType::SFLOAT_32);

  cache.proc_length_buf = GPU_vertbuf_create_with_format_ex(
      length_format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(*cache.proc_length_buf, cache.curves_num);

  /* TODO: Only create hairLength VBO when necessary. */
  fill_points_position_time_vbo(curves.points_by_curve(),
                                curves.positions(),
                                cache.proc_point_buf->data<PositionAndParameter>(),
                                cache.proc_length_buf->data<float>());
}

static uint32_t bezier_data_value(int8_t handle_type, bool is_active)
{
  return (handle_type << EDIT_CURVES_HANDLE_TYPES_SHIFT) | EDIT_CURVES_BEZIER_HANDLE |
         (is_active ? EDIT_CURVES_ACTIVE_HANDLE : 0);
}

static int handles_and_points_num(const int points_num, const OffsetIndices<int> bezier_offsets)
{
  return points_num + bezier_offsets.total_size() * 2;
}

static IndexRange handle_range_left(const int points_num, const OffsetIndices<int> bezier_offsets)
{
  return IndexRange(points_num, bezier_offsets.total_size());
}

static IndexRange handle_range_right(const int points_num, const OffsetIndices<int> bezier_offsets)
{
  return IndexRange(points_num + bezier_offsets.total_size(), bezier_offsets.total_size());
}

static void extract_edit_data(const OffsetIndices<int> points_by_curve,
                              const IndexMask &curve_selection,
                              const VArray<bool> &selection_attr,
                              const bool mark_active,
                              const uint32_t fill_value,
                              MutableSpan<uint32_t> data)
{
  curve_selection.foreach_index(GrainSize(256), [&](const int curve) {
    const IndexRange points = points_by_curve[curve];
    bool is_active = false;
    if (mark_active) {
      is_active = array_utils::count_booleans(selection_attr, points) > 0;
    }
    uint32_t data_value = fill_value | (is_active ? EDIT_CURVES_ACTIVE_HANDLE : 0u);
    data.slice(points).fill(data_value);
  });
}

static void create_edit_points_data(const OffsetIndices<int> points_by_curve,
                                    const IndexMask &catmull_rom_curves,
                                    const IndexMask &poly_curves,
                                    const IndexMask &bezier_curves,
                                    const IndexMask &nurbs_curves,
                                    const OffsetIndices<int> bezier_offsets,
                                    const bke::CurvesGeometry &curves,
                                    gpu::VertBuf &vbo)
{
  const int points_num = points_by_curve.total_size();
  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray selection = *attributes.lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);

  static const GPUVertFormat format = GPU_vertformat_from_attribute("data",
                                                                    gpu::VertAttrType::UINT_32);
  GPU_vertbuf_init_with_format(vbo, format);
  GPU_vertbuf_data_alloc(vbo, handles_and_points_num(points_num, bezier_offsets));
  MutableSpan<uint32_t> data = vbo.data<uint32_t>();

  extract_edit_data(points_by_curve, catmull_rom_curves, selection, false, 0, data);

  extract_edit_data(points_by_curve, poly_curves, selection, false, 0, data);

  if (!bezier_curves.is_empty()) {
    const VArray<int8_t> type_right = curves.handle_types_left();
    const VArray<int8_t> types_left = curves.handle_types_right();
    const VArray selection_left = *attributes.lookup_or_default<bool>(
        ".selection_handle_left", bke::AttrDomain::Point, true);
    const VArray selection_right = *attributes.lookup_or_default<bool>(
        ".selection_handle_right", bke::AttrDomain::Point, true);

    MutableSpan data_left = data.slice(handle_range_left(points_num, bezier_offsets));
    MutableSpan data_right = data.slice(handle_range_right(points_num, bezier_offsets));

    bezier_curves.foreach_index(GrainSize(256), [&](const int curve, const int64_t pos) {
      const IndexRange points = points_by_curve[curve];
      const IndexRange bezier_range = bezier_offsets[pos];
      for (const int i : points.index_range()) {
        const int point = points[i];
        data[point] = EDIT_CURVES_BEZIER_KNOT;

        const bool selected = selection[point] || selection_left[point] || selection_right[point];
        const int bezier_point = bezier_range[i];
        data_left[bezier_point] = bezier_data_value(type_right[point], selected);
        data_right[bezier_point] = bezier_data_value(types_left[point], selected);
      }
    });
  }

  extract_edit_data(
      points_by_curve, nurbs_curves, selection, true, EDIT_CURVES_NURBS_CONTROL_POINT, data);
}

static void create_edit_points_position(const bke::CurvesGeometry &curves,
                                        const OffsetIndices<int> points_by_curve,
                                        const IndexMask &bezier_curves,
                                        const OffsetIndices<int> bezier_offsets,
                                        const bke::crazyspace::GeometryDeformation deformation,
                                        gpu::VertBuf &vbo)
{
  const Span<float3> positions = deformation.positions;
  const int points_num = positions.size();

  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "pos", gpu::VertAttrType::SFLOAT_32_32_32);
  GPU_vertbuf_init_with_format(vbo, format);
  GPU_vertbuf_data_alloc(vbo, handles_and_points_num(points_num, bezier_offsets));

  MutableSpan<float3> data = vbo.data<float3>();
  data.take_front(positions.size()).copy_from(positions);

  /* TODO: Use deformed left_handle_positions and left_handle_positions. */
  array_utils::gather_group_to_group(points_by_curve,
                                     bezier_offsets,
                                     bezier_curves,
                                     curves.handle_positions_left(),
                                     data.slice(handle_range_left(points_num, bezier_offsets)));
  array_utils::gather_group_to_group(points_by_curve,
                                     bezier_offsets,
                                     bezier_curves,
                                     curves.handle_positions_right(),
                                     data.slice(handle_range_right(points_num, bezier_offsets)));
}

static void create_edit_points_selection(const OffsetIndices<int> points_by_curve,
                                         const IndexMask &bezier_curves,
                                         const OffsetIndices<int> bezier_offsets,
                                         const bke::AttributeAccessor attributes,
                                         gpu::VertBuf &vbo)
{
  static const GPUVertFormat format_data = GPU_vertformat_from_attribute(
      "selection", gpu::VertAttrType::SFLOAT_32);

  const int points_num = points_by_curve.total_size();
  GPU_vertbuf_init_with_format(vbo, format_data);
  GPU_vertbuf_data_alloc(vbo, handles_and_points_num(points_num, bezier_offsets));
  MutableSpan<float> data = vbo.data<float>();

  const VArray attribute = *attributes.lookup_or_default<float>(
      ".selection", bke::AttrDomain::Point, 1.0f);
  attribute.materialize(data.take_front(points_num));

  if (bezier_curves.is_empty()) {
    return;
  }

  const VArray selection_left = *attributes.lookup_or_default<float>(
      ".selection_handle_left", bke::AttrDomain::Point, 1.0f);
  const VArray selection_right = *attributes.lookup_or_default<float>(
      ".selection_handle_right", bke::AttrDomain::Point, 1.0f);

  array_utils::gather_group_to_group(points_by_curve,
                                     bezier_offsets,
                                     bezier_curves,
                                     selection_left,
                                     data.slice(handle_range_left(points_num, bezier_offsets)));
  array_utils::gather_group_to_group(points_by_curve,
                                     bezier_offsets,
                                     bezier_curves,
                                     selection_right,
                                     data.slice(handle_range_right(points_num, bezier_offsets)));
}

static void create_lines_ibo_no_cyclic(const OffsetIndices<int> points_by_curve,
                                       gpu::IndexBuf &ibo)
{
  const int points_num = points_by_curve.total_size();
  const int curves_num = points_by_curve.size();
  const int indices_num = points_num + curves_num;
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINE_STRIP, indices_num, points_num);
  MutableSpan<uint> ibo_data = GPU_indexbuf_get_data(&builder);
  threading::parallel_for(IndexRange(curves_num), 1024, [&](const IndexRange range) {
    for (const int curve : range) {
      const IndexRange points = points_by_curve[curve];
      const IndexRange ibo_range = IndexRange(points.start() + curve, points.size() + 1);
      for (const int i : points.index_range()) {
        ibo_data[ibo_range[i]] = points[i];
      }
      ibo_data[ibo_range.last()] = gpu::RESTART_INDEX;
    }
  });
  GPU_indexbuf_build_in_place_ex(&builder, 0, points_num, true, &ibo);
}

static void create_lines_ibo_with_cyclic(const OffsetIndices<int> points_by_curve,
                                         const Span<bool> cyclic,
                                         gpu::IndexBuf &ibo)
{
  const int points_num = points_by_curve.total_size();
  const int curves_num = points_by_curve.size();
  const int indices_num = points_num + curves_num * 2;
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINE_STRIP, indices_num, points_num);
  MutableSpan<uint> ibo_data = GPU_indexbuf_get_data(&builder);
  threading::parallel_for(IndexRange(curves_num), 1024, [&](const IndexRange range) {
    for (const int curve : range) {
      const IndexRange points = points_by_curve[curve];
      const IndexRange ibo_range = IndexRange(points.start() + curve * 2, points.size() + 2);
      for (const int i : points.index_range()) {
        ibo_data[ibo_range[i]] = points[i];
      }
      ibo_data[ibo_range.last(1)] = cyclic[curve] ? points.first() : gpu::RESTART_INDEX;
      ibo_data[ibo_range.last()] = gpu::RESTART_INDEX;
    }
  });
  GPU_indexbuf_build_in_place_ex(&builder, 0, points_num, true, &ibo);
}

static void create_lines_ibo_with_cyclic(const OffsetIndices<int> points_by_curve,
                                         const VArray<bool> &cyclic,
                                         gpu::IndexBuf &ibo)
{
  const array_utils::BooleanMix cyclic_mix = array_utils::booleans_mix_calc(cyclic);
  if (cyclic_mix == array_utils::BooleanMix::AllFalse) {
    create_lines_ibo_no_cyclic(points_by_curve, ibo);
  }
  else {
    const VArraySpan<bool> cyclic_span(cyclic);
    create_lines_ibo_with_cyclic(points_by_curve, cyclic_span, ibo);
  }
}

static void extract_curve_lines(const OffsetIndices<int> points_by_curve,
                                const VArray<bool> &cyclic,
                                const IndexMask &selection,
                                const int cyclic_segment_offset,
                                MutableSpan<uint2> lines)
{
  selection.foreach_index(GrainSize(512), [&](const int curve) {
    const IndexRange points = points_by_curve[curve];
    MutableSpan<uint2> curve_lines = lines.slice(points.start() + cyclic_segment_offset,
                                                 points.size() + 1);
    for (const int i : IndexRange(points.size() - 1)) {
      const int point = points[i];
      curve_lines[i] = uint2(point, point + 1);
    }
    if (cyclic[curve]) {
      curve_lines.last() = uint2(points.first(), points.last());
    }
    else {
      curve_lines.last() = uint2(points.last(), points.last());
    }
  });
}

static void calc_edit_handles_ibo(const OffsetIndices<int> points_by_curve,
                                  const IndexMask &catmull_rom_curves,
                                  const IndexMask &poly_curves,
                                  const IndexMask &bezier_curves,
                                  const IndexMask &nurbs_curves,
                                  const OffsetIndices<int> bezier_offsets,
                                  const VArray<bool> &cyclic,
                                  gpu::IndexBuf &ibo)
{
  const int curves_num = points_by_curve.size();
  const int points_num = points_by_curve.total_size();
  const int non_bezier_points_num = points_num - bezier_offsets.total_size();
  const int non_bezier_curves_num = curves_num - bezier_curves.size();

  int lines_num = 0;
  /* Lines for all non-cyclic non-Bezier segments. */
  lines_num += non_bezier_points_num;
  /* Lines for all potential non-Bezier cyclic segments. */
  lines_num += non_bezier_curves_num;
  /* Lines for all Bezier handles. */
  lines_num += bezier_offsets.total_size() * 2;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(
      &builder, GPU_PRIM_LINES, lines_num, handles_and_points_num(points_num, bezier_offsets));
  MutableSpan<uint2> lines = GPU_indexbuf_get_data(&builder).cast<uint2>();

  int cyclic_segment_offset = 0;
  extract_curve_lines(points_by_curve, cyclic, catmull_rom_curves, cyclic_segment_offset, lines);
  cyclic_segment_offset += catmull_rom_curves.size();

  extract_curve_lines(points_by_curve, cyclic, poly_curves, cyclic_segment_offset, lines);
  cyclic_segment_offset += catmull_rom_curves.size();

  if (!bezier_curves.is_empty()) {
    const IndexRange handles_left = handle_range_left(points_num, bezier_offsets);
    const IndexRange handles_right = handle_range_right(points_num, bezier_offsets);

    MutableSpan lines_left = lines.slice(
        handle_range_left(non_bezier_points_num, bezier_offsets).shift(non_bezier_curves_num));
    MutableSpan lines_right = lines.slice(
        handle_range_right(non_bezier_points_num, bezier_offsets).shift(non_bezier_curves_num));

    bezier_curves.foreach_index(GrainSize(512), [&](const int curve, const int pos) {
      const IndexRange points = points_by_curve[curve];
      const IndexRange bezier_point_range = bezier_offsets[pos];
      for (const int i : points.index_range()) {
        const int point = points[i];
        const int bezier_point = bezier_point_range[i];
        lines_left[bezier_point] = uint2(handles_left[bezier_point], point);
        lines_right[bezier_point] = uint2(handles_right[bezier_point], point);
      }
    });
  }

  extract_curve_lines(points_by_curve, cyclic, nurbs_curves, cyclic_segment_offset, lines);

  GPU_indexbuf_build_in_place_ex(
      &builder, 0, handles_and_points_num(points_num, bezier_offsets), false, &ibo);
}

static void alloc_final_attribute_vbo(CurvesEvalCache &cache,
                                      const GPUVertFormat &format,
                                      const int index,
                                      const char * /*name*/)
{
  cache.final.attributes_buf[index] = GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_DEVICE_ONLY | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

  /* Create a destination buffer for the transform feedback. Sized appropriately */
  /* Those are points! not line segments. */
  GPU_vertbuf_data_alloc(*cache.final.attributes_buf[index],
                         cache.final.resolution * cache.curves_num);
}

static gpu::VertBufPtr ensure_control_point_attribute(const Curves &curves_id,
                                                      const StringRef name,
                                                      const GPUVertFormat &format,
                                                      bool &r_is_point_domain)
{
  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY));

  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  const bke::AttributeAccessor attributes = curves.wrap().attributes();

  /* TODO(@kevindietrich): float4 is used for scalar attributes as the implicit conversion done
   * by OpenGL to float4 for a scalar `s` will produce a `float4(s, 0, 0, 1)`. However, following
   * the Blender convention, it should be `float4(s, s, s, 1)`. This could be resolved using a
   * similar texture state swizzle to map the attribute correctly as for volume attributes, so we
   * can control the conversion ourselves. */
  const bke::AttributeReader<ColorGeometry4f> attribute = attributes.lookup<ColorGeometry4f>(name);
  if (!attribute) {
    GPU_vertbuf_data_alloc(*vbo, curves.curves_num());
    vbo->data<ColorGeometry4f>().fill({0.0f, 0.0f, 0.0f, 1.0f});
    r_is_point_domain = false;
    return vbo;
  }

  r_is_point_domain = attribute.domain == bke::AttrDomain::Point;
  GPU_vertbuf_data_alloc(*vbo, r_is_point_domain ? curves.points_num() : curves.curves_num());
  attribute.varray.materialize(vbo->data<ColorGeometry4f>());
  return vbo;
}

static void ensure_final_attribute(const Curves &curves,
                                   const StringRef name,
                                   const int index,
                                   CurvesEvalCache &cache)
{
  char sampler_name[32];
  drw_curves_get_attribute_sampler_name(name, sampler_name);

  GPUVertFormat format = {0};
  /* All attributes use float4, see comment below. */
  GPU_vertformat_attr_add(&format, sampler_name, blender::gpu::VertAttrType::SFLOAT_32_32_32_32);

  if (!cache.proc_attributes_buf[index]) {
    gpu::VertBufPtr vbo = ensure_control_point_attribute(
        curves, name, format, cache.proc_attributes_point_domain[index]);
    cache.proc_attributes_buf[index] = vbo.release();
  }

  /* Existing final data may have been for a different attribute (with a different name or domain),
   * free the data. */
  GPU_VERTBUF_DISCARD_SAFE(cache.final.attributes_buf[index]);

  /* Ensure final data for points. */
  if (cache.proc_attributes_point_domain[index]) {
    alloc_final_attribute_vbo(cache, format, index, sampler_name);
  }
}

static void fill_curve_offsets_vbos(const OffsetIndices<int> points_by_curve,
                                    GPUVertBufRaw &data_step,
                                    GPUVertBufRaw &seg_step)
{
  for (const int i : points_by_curve.index_range()) {
    const IndexRange points = points_by_curve[i];

    *(uint *)GPU_vertbuf_raw_step(&data_step) = points.start();
    *(uint *)GPU_vertbuf_raw_step(&seg_step) = points.size() - 1;
  }
}

static void create_curve_offsets_vbos(const OffsetIndices<int> points_by_curve,
                                      CurvesEvalCache &cache)
{
  GPUVertBufRaw data_step, seg_step;

  GPUVertFormat format_data = {0};
  uint data_id = GPU_vertformat_attr_add(
      &format_data, "data", blender::gpu::VertAttrType::UINT_32);

  GPUVertFormat format_seg = {0};
  uint seg_id = GPU_vertformat_attr_add(&format_seg, "data", blender::gpu::VertAttrType::UINT_32);

  /* Curve Data. */
  cache.proc_strand_buf = GPU_vertbuf_create_with_format_ex(
      format_data, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(*cache.proc_strand_buf, cache.curves_num);
  GPU_vertbuf_attr_get_raw_data(cache.proc_strand_buf, data_id, &data_step);

  cache.proc_strand_seg_buf = GPU_vertbuf_create_with_format_ex(
      format_seg, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(*cache.proc_strand_seg_buf, cache.curves_num);
  GPU_vertbuf_attr_get_raw_data(cache.proc_strand_seg_buf, seg_id, &seg_step);

  fill_curve_offsets_vbos(points_by_curve, data_step, seg_step);
}

static void alloc_final_points_vbo(CurvesEvalCache &cache)
{
  /* Same format as proc_point_buf. */
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32_32_32);

  cache.final.proc_buf = GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_DEVICE_ONLY | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

  /* Create a destination buffer for the transform feedback. Sized appropriately */

  /* Those are points! not line segments. */
  uint point_len = cache.final.resolution * cache.curves_num;
  /* Avoid creating null sized VBO which can lead to crashes on certain platforms. */
  point_len = max_ii(1, point_len);

  GPU_vertbuf_data_alloc(*cache.final.proc_buf, point_len);
}

static void calc_final_indices(const bke::CurvesGeometry &curves,
                               CurvesEvalCache &cache,
                               const int thickness_res)
{
  BLI_assert(thickness_res <= MAX_THICKRES); /* Cylinder strip not currently supported. */
  /* Determine prim type and element count.
   * NOTE: Metal backend uses non-restart prim types for optimal HW performance. */
  bool use_strip_prims = (GPU_backend_get_type() != GPU_BACKEND_METAL);
  int verts_per_curve;
  GPUPrimType prim_type;

  if (use_strip_prims) {
    /* +1 for primitive restart */
    verts_per_curve = cache.final.resolution * thickness_res;
    prim_type = (thickness_res == 1) ? GPU_PRIM_LINE_STRIP : GPU_PRIM_TRI_STRIP;
  }
  else {
    /* Use full primitive type. */
    prim_type = (thickness_res == 1) ? GPU_PRIM_LINES : GPU_PRIM_TRIS;
    int verts_per_segment = ((prim_type == GPU_PRIM_LINES) ? 2 : 6);
    verts_per_curve = (cache.final.resolution - 1) * verts_per_segment;
  }

  static const GPUVertFormat format = GPU_vertformat_from_attribute("dummy",
                                                                    gpu::VertAttrType::UINT_32);

  gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*vbo, 1);

  gpu::IndexBuf *ibo = nullptr;
  eGPUBatchFlag owns_flag = GPU_BATCH_OWNS_VBO;
  if (curves.curves_num()) {
    ibo = GPU_indexbuf_build_curves_on_device(prim_type, curves.curves_num(), verts_per_curve);
    owns_flag |= GPU_BATCH_OWNS_INDEX;
  }
  cache.final.proc_hairs = GPU_batch_create_ex(prim_type, vbo, ibo, owns_flag);
}

static std::optional<StringRef> get_first_uv_name(const bke::AttributeAccessor &attributes)
{
  std::optional<StringRef> name;
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.data_type == bke::AttrType::Float2) {
      name = iter.name;
      iter.stop();
    }
  });
  return name;
}

static bool ensure_attributes(const Curves &curves,
                              CurvesBatchCache &cache,
                              const GPUMaterial *gpu_material)
{
  const bke::AttributeAccessor attributes = curves.geometry.wrap().attributes();
  CurvesEvalFinalCache &final_cache = cache.eval_cache.final;

  if (gpu_material) {
    VectorSet<std::string> attrs_needed;
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      StringRef name = gpu_attr->name;
      if (name.is_empty()) {
        if (std::optional<StringRef> uv_name = get_first_uv_name(attributes)) {
          drw_attributes_add_request(&attrs_needed, *uv_name);
        }
      }
      if (!attributes.contains(name)) {
        continue;
      }
      drw_attributes_add_request(&attrs_needed, name);
    }

    if (!drw_attributes_overlap(&final_cache.attr_used, &attrs_needed)) {
      /* Some new attributes have been added, free all and start over. */
      for (const int i : IndexRange(GPU_MAX_ATTR)) {
        GPU_VERTBUF_DISCARD_SAFE(final_cache.attributes_buf[i]);
        GPU_VERTBUF_DISCARD_SAFE(cache.eval_cache.proc_attributes_buf[i]);
      }
      drw_attributes_merge(&final_cache.attr_used, &attrs_needed);
    }
    drw_attributes_merge(&final_cache.attr_used_over_time, &attrs_needed);
  }

  bool need_tf_update = false;

  for (const int i : final_cache.attr_used.index_range()) {
    if (cache.eval_cache.final.attributes_buf[i] != nullptr) {
      continue;
    }

    ensure_final_attribute(curves, final_cache.attr_used[i], i, cache.eval_cache);
    if (cache.eval_cache.proc_attributes_point_domain[i]) {
      need_tf_update = true;
    }
  }

  return need_tf_update;
}

static void request_attribute(Curves &curves, const StringRef name)
{
  CurvesBatchCache &cache = get_batch_cache(curves);
  CurvesEvalFinalCache &final_cache = cache.eval_cache.final;

  VectorSet<std::string> attributes{};

  bke::CurvesGeometry &curves_geometry = curves.geometry.wrap();
  if (!curves_geometry.attributes().contains(name)) {
    return;
  }
  drw_attributes_add_request(&attributes, name);

  drw_attributes_merge(&final_cache.attr_used, &attributes);
}

void drw_curves_get_attribute_sampler_name(const StringRef layer_name, char r_sampler_name[32])
{
  char attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
  GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
  /* Attributes use auto-name. */
  BLI_snprintf_utf8(r_sampler_name, 32, "a%s", attr_safe_name);
}

bool curves_ensure_procedural_data(Curves *curves_id,
                                   CurvesEvalCache **r_cache,
                                   const GPUMaterial *gpu_material,
                                   const int subdiv,
                                   const int thickness_res)
{
  const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  bool need_ft_update = false;

  CurvesBatchCache &cache = get_batch_cache(*curves_id);
  CurvesEvalCache &eval_cache = cache.eval_cache;

  if (eval_cache.final.hair_subdiv != subdiv || eval_cache.final.thickres != thickness_res) {
    /* If the subdivision or indexing settings have changed, the evaluation cache is cleared. */
    clear_final_data(eval_cache.final);
    eval_cache.final.hair_subdiv = subdiv;
    eval_cache.final.thickres = thickness_res;
  }

  eval_cache.curves_num = curves.curves_num();
  eval_cache.points_num = curves.points_num();

  const int steps = 3; /* TODO: don't hard-code? */
  eval_cache.final.resolution = 1 << (steps + subdiv);

  /* Refreshed on combing and simulation. */
  if (eval_cache.proc_point_buf == nullptr || DRW_vbo_requested(eval_cache.proc_point_buf)) {
    create_points_position_time_vbo(curves, eval_cache);
    need_ft_update = true;
  }

  /* Refreshed if active layer or custom data changes. */
  if (eval_cache.proc_strand_buf == nullptr) {
    create_curve_offsets_vbos(curves.points_by_curve(), eval_cache);
  }

  /* Refreshed only on subdiv count change. */
  if (eval_cache.final.proc_buf == nullptr) {
    alloc_final_points_vbo(eval_cache);
    need_ft_update = true;
  }

  if (eval_cache.final.proc_hairs == nullptr) {
    calc_final_indices(curves, eval_cache, thickness_res);
  }
  eval_cache.final.thickres = thickness_res;

  need_ft_update |= ensure_attributes(*curves_id, cache, gpu_material);

  *r_cache = &eval_cache;
  return need_ft_update;
}

void DRW_curves_batch_cache_dirty_tag(Curves *curves, int mode)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves->batch_cache);
  if (cache == nullptr) {
    return;
  }
  switch (mode) {
    case BKE_CURVES_BATCH_DIRTY_ALL:
      cache->is_dirty = true;
      break;
    default:
      BLI_assert_unreachable();
  }
}

void DRW_curves_batch_cache_validate(Curves *curves)
{
  if (!batch_cache_is_dirty(*curves)) {
    clear_batch_cache(*curves);
    init_batch_cache(*curves);
  }
}

void DRW_curves_batch_cache_free(Curves *curves)
{
  clear_batch_cache(*curves);
  CurvesBatchCache *batch_cache = static_cast<CurvesBatchCache *>(curves->batch_cache);
  MEM_delete(batch_cache);
  curves->batch_cache = nullptr;
}

void DRW_curves_batch_cache_free_old(Curves *curves, int ctime)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves->batch_cache);
  if (cache == nullptr) {
    return;
  }

  bool do_discard = false;

  CurvesEvalFinalCache &final_cache = cache->eval_cache.final;

  if (drw_attributes_overlap(&final_cache.attr_used_over_time, &final_cache.attr_used)) {
    final_cache.last_attr_matching_time = ctime;
  }

  if (ctime - final_cache.last_attr_matching_time > U.vbotimeout) {
    do_discard = true;
  }

  final_cache.attr_used_over_time.clear();

  if (do_discard) {
    discard_attributes(cache->eval_cache);
  }
}

gpu::Batch *DRW_curves_batch_cache_get_edit_points(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return DRW_batch_request(&cache.edit_points);
}

gpu::Batch *DRW_curves_batch_cache_get_sculpt_curves_cage(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return DRW_batch_request(&cache.sculpt_cage);
}

gpu::Batch *DRW_curves_batch_cache_get_edit_curves_handles(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return DRW_batch_request(&cache.edit_handles);
}

gpu::Batch *DRW_curves_batch_cache_get_edit_curves_lines(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return DRW_batch_request(&cache.edit_curves_lines);
}

gpu::VertBuf **DRW_curves_texture_for_evaluated_attribute(Curves *curves,
                                                          const StringRef name,
                                                          bool *r_is_point_domain)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  CurvesEvalFinalCache &final_cache = cache.eval_cache.final;

  request_attribute(*curves, name);

  int request_i = -1;
  for (const int i : final_cache.attr_used.index_range()) {
    if (final_cache.attr_used[i] == name) {
      request_i = i;
      break;
    }
  }
  if (request_i == -1) {
    *r_is_point_domain = false;
    return nullptr;
  }
  if (cache.eval_cache.proc_attributes_point_domain[request_i]) {
    *r_is_point_domain = true;
    return &final_cache.attributes_buf[request_i];
  }
  *r_is_point_domain = false;
  return &cache.eval_cache.proc_attributes_buf[request_i];
}

static void create_edit_points_position_vbo(
    const bke::CurvesGeometry &curves,
    const bke::crazyspace::GeometryDeformation & /*deformation*/,
    CurvesBatchCache &cache)
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "pos", gpu::VertAttrType::SFLOAT_32_32_32);

  /* TODO: Deform curves using deformations. */
  const Span<float3> positions = curves.evaluated_positions();
  GPU_vertbuf_init_with_format(*cache.edit_curves_lines_pos, format);
  GPU_vertbuf_data_alloc(*cache.edit_curves_lines_pos, positions.size());
  cache.edit_curves_lines_pos->data<float3>().copy_from(positions);
}

void DRW_curves_batch_cache_create_requested(Object *ob)
{
  Curves &curves_id = DRW_object_get_data_for_drawing<Curves>(*ob);
  Object *ob_orig = DEG_get_original(ob);
  if (ob_orig == nullptr) {
    return;
  }
  const Curves &curves_orig_id = DRW_object_get_data_for_drawing<Curves>(*ob_orig);

  draw::CurvesBatchCache &cache = draw::get_batch_cache(curves_id);
  const bke::CurvesGeometry &curves_orig = curves_orig_id.geometry.wrap();

  bool is_edit_data_needed = false;

  if (DRW_batch_requested(cache.edit_points, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache.edit_points, &cache.edit_points_pos);
    DRW_vbo_request(cache.edit_points, &cache.edit_points_data);
    DRW_vbo_request(cache.edit_points, &cache.edit_points_selection);
    is_edit_data_needed = true;
  }
  if (DRW_batch_requested(cache.sculpt_cage, GPU_PRIM_LINE_STRIP)) {
    DRW_ibo_request(cache.sculpt_cage, &cache.sculpt_cage_ibo);
    DRW_vbo_request(cache.sculpt_cage, &cache.edit_points_pos);
    DRW_vbo_request(cache.sculpt_cage, &cache.edit_points_data);
    DRW_vbo_request(cache.sculpt_cage, &cache.edit_points_selection);
    is_edit_data_needed = true;
  }
  if (DRW_batch_requested(cache.edit_handles, GPU_PRIM_LINES)) {
    DRW_ibo_request(cache.edit_handles, &cache.edit_handles_ibo);
    DRW_vbo_request(cache.edit_handles, &cache.edit_points_pos);
    DRW_vbo_request(cache.edit_handles, &cache.edit_points_data);
    DRW_vbo_request(cache.edit_handles, &cache.edit_points_selection);
    is_edit_data_needed = true;
  }
  if (DRW_batch_requested(cache.edit_curves_lines, GPU_PRIM_LINE_STRIP)) {
    DRW_vbo_request(cache.edit_curves_lines, &cache.edit_curves_lines_pos);
    DRW_ibo_request(cache.edit_curves_lines, &cache.edit_curves_lines_ibo);
  }

  const OffsetIndices<int> points_by_curve = curves_orig.points_by_curve();
  const VArray<bool> cyclic = curves_orig.cyclic();

  const bke::crazyspace::GeometryDeformation deformation =
      is_edit_data_needed || DRW_vbo_requested(cache.edit_curves_lines_pos) ?
          bke::crazyspace::get_evaluated_curves_deformation(ob, *ob_orig) :
          bke::crazyspace::GeometryDeformation();

  if (DRW_ibo_requested(cache.sculpt_cage_ibo)) {
    create_lines_ibo_no_cyclic(points_by_curve, *cache.sculpt_cage_ibo);
  }

  if (DRW_vbo_requested(cache.edit_curves_lines_pos)) {
    create_edit_points_position_vbo(curves_orig, deformation, cache);
  }

  if (DRW_ibo_requested(cache.edit_curves_lines_ibo)) {
    create_lines_ibo_with_cyclic(
        curves_orig.evaluated_points_by_curve(), cyclic, *cache.edit_curves_lines_ibo);
  }

  if (!is_edit_data_needed) {
    return;
  }

  const IndexRange curves_range = curves_orig.curves_range();
  const VArray<int8_t> curve_types = curves_orig.curve_types();
  const std::array<int, CURVE_TYPES_NUM> type_counts = curves_orig.curve_type_counts();
  const bke::AttributeAccessor attributes = curves_orig.attributes();

  IndexMaskMemory memory;
  const IndexMask catmull_rom_curves = bke::curves::indices_for_type(
      curve_types, type_counts, CURVE_TYPE_CATMULL_ROM, curves_range, memory);
  const IndexMask poly_curves = bke::curves::indices_for_type(
      curve_types, type_counts, CURVE_TYPE_POLY, curves_range, memory);
  const IndexMask bezier_curves = bke::curves::indices_for_type(
      curve_types, type_counts, CURVE_TYPE_BEZIER, curves_range, memory);
  const IndexMask nurbs_curves = bke::curves::indices_for_type(
      curve_types, type_counts, CURVE_TYPE_NURBS, curves_range, memory);

  Array<int> bezier_point_offset_data(bezier_curves.size() + 1);
  const OffsetIndices<int> bezier_offsets = offset_indices::gather_selected_offsets(
      points_by_curve, bezier_curves, bezier_point_offset_data);

  if (DRW_vbo_requested(cache.edit_points_pos)) {
    create_edit_points_position(curves_orig,
                                points_by_curve,
                                bezier_curves,
                                bezier_offsets,
                                deformation,
                                *cache.edit_points_pos);
  }
  if (DRW_vbo_requested(cache.edit_points_data)) {
    create_edit_points_data(points_by_curve,
                            catmull_rom_curves,
                            poly_curves,
                            bezier_curves,
                            nurbs_curves,
                            bezier_offsets,
                            curves_orig,
                            *cache.edit_points_data);
  }
  if (DRW_vbo_requested(cache.edit_points_selection)) {
    create_edit_points_selection(
        points_by_curve, bezier_curves, bezier_offsets, attributes, *cache.edit_points_selection);
  }
  if (DRW_ibo_requested(cache.edit_handles_ibo)) {
    calc_edit_handles_ibo(points_by_curve,
                          catmull_rom_curves,
                          poly_curves,
                          bezier_curves,
                          nurbs_curves,
                          bezier_offsets,
                          cyclic,
                          *cache.edit_handles_ibo);
  }
}

}  // namespace blender::draw
