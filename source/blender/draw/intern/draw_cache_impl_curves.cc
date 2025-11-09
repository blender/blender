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
#include "GPU_capabilities.hh"
#include "GPU_context.hh"
#include "GPU_material.hh"
#include "GPU_texture.hh"

#include "DRW_render.hh"

#include "draw_attributes.hh"
#include "draw_cache_impl.hh" /* own include */
#include "draw_cache_inline.hh"
#include "draw_context_private.hh"
#include "draw_curves_private.hh" /* own include */
#include "draw_hair_private.hh"

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

void CurvesEvalCache::discard_attributes()
{
  for (const int i : IndexRange(GPU_MAX_ATTR)) {
    this->evaluated_attributes_buf[i].reset();
  }
  for (const int i : IndexRange(GPU_MAX_ATTR)) {
    this->curve_attributes_buf[i].reset();
  }
  this->attr_used.clear();
}

void CurvesEvalCache::clear()
{
  /* TODO: more granular update tagging. */
  this->evaluated_pos_rad_buf.reset();
  this->evaluated_time_buf.reset();
  this->curves_length_buf.reset();

  this->points_by_curve_buf.reset();
  this->evaluated_points_by_curve_buf.reset();
  this->curves_type_buf.reset();
  this->curves_resolution_buf.reset();
  this->curves_cyclic_buf.reset();

  this->handles_positions_left_buf.reset();
  this->handles_positions_right_buf.reset();
  this->bezier_offsets_buf.reset();

  this->curves_order_buf.reset();
  this->control_weights_buf.reset();
  this->basis_cache_buf.reset();
  this->basis_cache_offset_buf.reset();

  this->indirection_cylinder_buf.reset();
  this->indirection_ribbon_buf.reset();

  for (gpu::Batch *&batch : this->batch) {
    GPU_BATCH_DISCARD_SAFE(batch);
  }

  this->discard_attributes();
}

static void clear_batch_cache(Curves &curves)
{
  CurvesBatchCache *cache = static_cast<CurvesBatchCache *>(curves.batch_cache);
  if (!cache) {
    return;
  }

  cache->eval_cache.clear();
  clear_edit_data(cache);
}

static CurvesBatchCache &get_batch_cache(Curves &curves)
{
  DRW_curves_batch_cache_validate(&curves);
  return *static_cast<CurvesBatchCache *>(curves.batch_cache);
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

  if (!bezier_curves.is_empty()) {
    /* TODO: Use deformed left_handle_positions and left_handle_positions. */
    const std::optional<Span<float3>> handles_left = curves.handle_positions_left();
    const std::optional<Span<float3>> handles_right = curves.handle_positions_right();
    if (handles_left && handles_right) {
      array_utils::gather_group_to_group(
          points_by_curve,
          bezier_offsets,
          bezier_curves,
          *handles_left,
          data.slice(handle_range_left(points_num, bezier_offsets)));
      array_utils::gather_group_to_group(
          points_by_curve,
          bezier_offsets,
          bezier_curves,
          *handles_right,
          data.slice(handle_range_right(points_num, bezier_offsets)));
    }
  }
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

  if (!bezier_curves.is_empty()) {
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

static void create_segments_with_cyclic(const OffsetIndices<int> points_by_curve,
                                        const VArray<bool> &cyclic,
                                        const IndexMask &selection,
                                        MutableSpan<uint2> lines)
{
  selection.foreach_index(GrainSize(512), [&](const int curve) {
    const IndexRange points = points_by_curve[curve];
    MutableSpan<uint2> curve_lines = lines.slice(points);
    for (const int i : points.index_range().drop_back(1)) {
      curve_lines[i] = uint2(points[i]) + uint2(0, 1);
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
  /* All curve types have poly-line segments draw of original (non-evaluate) topology to connect
   * control points. Bezier have exception -- instead there is left and right handle segments. Left
   * bezier handle segments point to original and handle points and lie at index of curve segment.
   * Right bezier handle segments point to original and handle points and lie in a sequence after
   * all other segments. */
  const int points_num = points_by_curve.total_size();
  const int extra_bezier_segments = bezier_offsets.total_size();

  /* TODO: Use linestrip if there is no bezier curves. */
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder,
                    GPU_PRIM_LINES,
                    points_num + extra_bezier_segments,
                    handles_and_points_num(points_num, bezier_offsets));
  MutableSpan<uint2> lines = GPU_indexbuf_get_data(&builder).cast<uint2>();
  BLI_assert(lines.size() == points_num + extra_bezier_segments);
  MutableSpan<uint2> curve_or_handle_segments = lines.take_front(points_num);

#ifdef NDEBUG
  lines.fill(uint2(std::numeric_limits<uint32_t>::min()));
#endif

  create_segments_with_cyclic(
      points_by_curve, cyclic, catmull_rom_curves, curve_or_handle_segments);
  create_segments_with_cyclic(points_by_curve, cyclic, poly_curves, curve_or_handle_segments);
  create_segments_with_cyclic(points_by_curve, cyclic, nurbs_curves, curve_or_handle_segments);

  const IndexRange handles_left = handle_range_left(points_num, bezier_offsets);
  const IndexRange handles_right = handle_range_right(points_num, bezier_offsets);

  bezier_curves.foreach_index(GrainSize(512), [&](const int curve, const int pos) {
    const IndexRange points = points_by_curve[curve];
    const IndexRange bezier_point_range = bezier_offsets[pos];
    for (const int i : points.index_range()) {
      const int point = points[i];
      const int bezier_point = bezier_point_range[i];
      curve_or_handle_segments[point] = uint2(handles_left[bezier_point], point);
    }
  });

  MutableSpan<uint2> right_handle_segments = lines.drop_front(points_num);
  bezier_curves.foreach_index(GrainSize(512), [&](const int curve, const int pos) {
    const IndexRange points = points_by_curve[curve];
    const IndexRange bezier_point_range = bezier_offsets[pos];
    for (const int i : points.index_range()) {
      const int point = points[i];
      const int bezier_point = bezier_point_range[i];
      right_handle_segments[bezier_point] = uint2(handles_right[bezier_point], point);
    }
  });

  BLI_assert(!lines.contains(uint2(std::numeric_limits<uint32_t>::min())));

  GPU_indexbuf_build_in_place_ex(
      &builder, 0, handles_and_points_num(points_num, bezier_offsets), false, &ibo);
}

static gpu::VertBufPtr alloc_evaluated_point_attribute_vbo(const GPUVertFormat &format,
                                                           const StringRef /*name*/,
                                                           int64_t size)
{
  gpu::VertBufPtr buf = gpu::VertBufPtr(GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_DEVICE_ONLY | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY));
  /* Create a destination buffer for the evaluation. Sized appropriately */
  GPU_vertbuf_data_alloc(*buf, size);
  return buf;
}

static gpu::VertBufPtr ensure_control_point_attribute(const bke::CurvesGeometry &curves,
                                                      const StringRef name,
                                                      const GPUVertFormat &format,
                                                      bool &r_is_point_domain)
{
  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY));

  const bke::AttributeAccessor attributes = curves.wrap().attributes();

  /* TODO(@kevindietrich): float4 is used for scalar attributes as the implicit conversion done
   * by OpenGL to float4 for a scalar `s` will produce a `float4(s, 0, 0, 1)`. However, following
   * the Blender convention, it should be `float4(s, s, s, 1)`. This could be resolved using a
   * similar texture state swizzle to map the attribute correctly as for volume attributes, so we
   * can control the conversion ourselves. */
  const bke::AttributeReader<ColorGeometry4f> attribute = attributes.lookup<ColorGeometry4f>(name);

  if (!attribute) {
    /* Attribute doesn't exist or is of an incompatible type.
     * Replace it with a black curve domain attribute. */
    /* TODO(fclem): Eventually, this should become unecessary if merge all attributes in one buffer
     * and use an indirection table. */
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

static void request_attribute(Curves &curves, const StringRef name)
{
  CurvesEvalCache &cache = get_batch_cache(curves).eval_cache;

  VectorSet<std::string> attributes{};

  bke::CurvesGeometry &curves_geometry = curves.geometry.wrap();
  if (!curves_geometry.attributes().contains(name)) {
    return;
  }
  drw_attributes_add_request(&attributes, name);

  drw_attributes_merge(&cache.attr_used, &attributes);
}

void drw_curves_get_attribute_sampler_name(const StringRef layer_name, char r_sampler_name[32])
{
  char attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
  GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
  /* Attributes use auto-name. */
  BLI_snprintf_utf8(r_sampler_name, 32, "a%s", attr_safe_name);
}

void CurvesEvalCache::ensure_attribute(CurvesModule &module,
                                       const bke::CurvesGeometry &curves,
                                       const StringRef name,
                                       const int index)
{
  char sampler_name[32];
  drw_curves_get_attribute_sampler_name(name, sampler_name);

  GPUVertFormat format = {0};
  /* All attributes use float4, see comment below. */
  /* TODO(fclem): Other types. */
  GPU_vertformat_attr_add(&format, sampler_name, blender::gpu::VertAttrType::SFLOAT_32_32_32_32);

  gpu::VertBufPtr attr_buf = ensure_control_point_attribute(
      curves, name, format, attributes_point_domain[index]);

  /* Existing final data may have been for a different attribute (with a different name or domain),
   * free the data. */
  this->evaluated_attributes_buf[index].reset();

  /* Ensure final data for points. */
  if (attributes_point_domain[index]) {
    this->ensure_common(curves);
    if (curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
      this->ensure_bezier(curves);
    }
    if (curves.has_curve_with_type(CURVE_TYPE_NURBS)) {
      this->ensure_nurbs(curves);
    }

    this->evaluated_attributes_buf[index] = alloc_evaluated_point_attribute_vbo(
        format, name, evaluated_point_count_with_cyclic(curves));

    module.evaluate_curve_attribute(curves.has_curve_with_type(CURVE_TYPE_CATMULL_ROM),
                                    curves.has_curve_with_type(CURVE_TYPE_BEZIER),
                                    curves.has_curve_with_type(CURVE_TYPE_POLY),
                                    curves.has_curve_with_type(CURVE_TYPE_NURBS),
                                    curves.has_cyclic_curve(),
                                    curves.curves_num(),
                                    *this,
                                    CURVES_EVAL_FLOAT4,
                                    std::move(attr_buf),
                                    this->evaluated_attributes_buf[index]);
  }
  else {
    this->curve_attributes_buf[index] = std::move(attr_buf);
  }
}

void CurvesEvalCache::ensure_attributes(CurvesModule &module,
                                        const bke::CurvesGeometry &curves,
                                        const GPUMaterial *gpu_material)
{
  const bke::AttributeAccessor attributes = curves.attributes();

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

    if (!drw_attributes_overlap(&attr_used, &attrs_needed)) {
      /* Some new attributes have been added, free all and start over. */
      for (const int i : IndexRange(GPU_MAX_ATTR)) {
        this->curve_attributes_buf[i].reset();
        this->evaluated_attributes_buf[i].reset();
      }
      drw_attributes_merge(&attr_used, &attrs_needed);
    }
    drw_attributes_merge(&attr_used_over_time, &attrs_needed);
  }

  for (const int i : attr_used.index_range()) {
    if (this->curve_attributes_buf[i] || this->evaluated_attributes_buf[i]) {
      continue;
    }
    ensure_attribute(module, curves, attr_used[i], i);
  }
}

void CurvesEvalCache::ensure_common(const bke::CurvesGeometry &curves)
{
  if (this->points_by_curve_buf) {
    return;
  }
  this->points_by_curve_buf = gpu::VertBuf::from_span(curves.points_by_curve().data());
  this->evaluated_points_by_curve_buf = gpu::VertBuf::from_span(
      curves.evaluated_points_by_curve().data());

  /* TODO(fclem): Optimize shaders to avoid needing to upload this data if data is uniform.
   * This concerns all varray. */
  this->curves_type_buf = gpu::VertBuf::from_varray(curves.curve_types());
  this->curves_resolution_buf = gpu::VertBuf::from_varray(curves.resolution());
  this->curves_cyclic_buf = gpu::VertBuf::from_varray(curves.cyclic());
}

void CurvesEvalCache::ensure_bezier(const bke::CurvesGeometry &curves)
{
  if (this->handles_positions_left_buf) {
    return;
  }
  const Span<float3> left = curves.handle_positions_left().value_or(curves.positions());
  const Span<float3> right = curves.handle_positions_right().value_or(curves.positions());
  this->handles_positions_left_buf = gpu::VertBuf::from_span(left);
  this->handles_positions_right_buf = gpu::VertBuf::from_span(right);
  this->bezier_offsets_buf = gpu::VertBuf::from_span(
      curves.runtime->evaluated_offsets_cache.data().all_bezier_offsets.as_span());
}

void CurvesEvalCache::ensure_nurbs(const bke::CurvesGeometry &curves)
{
  if (curves_order_buf) {
    return;
  }
  using BasisCache = bke::curves::nurbs::BasisCache;

  /* TODO(fclem): Optimize shaders to avoid needing to upload this data if data is uniform.
   * This concerns all varray. */
  this->curves_order_buf = gpu::VertBuf::from_varray(curves.nurbs_orders());
  if (curves.nurbs_weights().has_value()) {
    this->control_weights_buf = gpu::VertBuf::from_span(curves.nurbs_weights().value());
  }

  curves.ensure_can_interpolate_to_evaluated();

  const Span<BasisCache> nurbs_basis_cache = curves.runtime->nurbs_basis_cache.data();

  Vector<int> basis_cache_offset;
  Vector<uint32_t> basis_cache_packed;
  for (const BasisCache &cache : nurbs_basis_cache) {
    basis_cache_offset.append(cache.invalid ? -1 : basis_cache_packed.size());
    if (!cache.invalid) {
      basis_cache_packed.extend(cache.start_indices.as_span().cast<uint32_t>());
      basis_cache_packed.extend(cache.weights.as_span().cast<uint32_t>());
    }
  }
  /* Ensure buffer is not empty. */
  if (basis_cache_packed.is_empty()) {
    basis_cache_packed.append(0);
  }

  this->basis_cache_offset_buf = gpu::VertBuf::from_span(basis_cache_offset.as_span());
  this->basis_cache_buf = gpu::VertBuf::from_span(basis_cache_packed.as_span());
}

int CurvesEvalCache::evaluated_point_count_with_cyclic(const bke::CurvesGeometry &curves)
{
  if (curves.has_cyclic_curve()) {
    return curves.evaluated_points_num() + curves.curves_num();
  }
  return curves.evaluated_points_num();
}

void CurvesEvalCache::ensure_positions(CurvesModule &module, const bke::CurvesGeometry &curves)
{
  if (evaluated_pos_rad_buf) {
    return;
  }

  if (curves.is_empty()) {
    /* Can happen when called from `curves_pos_buffer_get()`. Caller has to deal with nullptr. */
    return;
  }

  this->ensure_common(curves);
  if (curves.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    this->ensure_bezier(curves);
  }
  if (curves.has_curve_with_type(CURVE_TYPE_NURBS)) {
    this->ensure_nurbs(curves);
  }

  /* TODO(fclem): Optimize shaders to avoid needing to upload this data if data is uniform.
   * This concerns all varray. */
  gpu::VertBufPtr points_pos_buf = gpu::VertBuf::from_span(curves.positions());
  gpu::VertBufPtr points_rad_buf = gpu::VertBuf::from_varray(curves.radius());

  this->evaluated_pos_rad_buf = gpu::VertBuf::device_only<float4>(
      evaluated_point_count_with_cyclic(curves));

  module.evaluate_positions(curves.has_curve_with_type(CURVE_TYPE_CATMULL_ROM),
                            curves.has_curve_with_type(CURVE_TYPE_BEZIER),
                            curves.has_curve_with_type(CURVE_TYPE_POLY),
                            curves.has_curve_with_type(CURVE_TYPE_NURBS),
                            curves.has_cyclic_curve(),
                            curves.curves_num(),
                            *this,
                            std::move(points_pos_buf),
                            std::move(points_rad_buf),
                            evaluated_pos_rad_buf);

  /* TODO(fclem): Make time and length optional. */
  this->evaluated_time_buf = gpu::VertBuf::device_only<float>(
      evaluated_point_count_with_cyclic(curves));
  this->curves_length_buf = gpu::VertBuf::device_only<float>(curves.curves_num());

  module.evaluate_curve_length_intercept(curves.has_cyclic_curve(), curves.curves_num(), *this);
}

gpu::VertBufPtr &CurvesEvalCache::indirection_buf_get(CurvesModule &module,
                                                      const bke::CurvesGeometry &curves,
                                                      const int face_per_segment)
{
  const bool is_ribbon = face_per_segment < 2;

  gpu::VertBufPtr &indirection_buf = is_ribbon ? this->indirection_ribbon_buf :
                                                 this->indirection_cylinder_buf;
  if (indirection_buf) {
    return indirection_buf;
  }

  this->ensure_common(curves);

  indirection_buf = module.evaluate_topology_indirection(curves.curves_num(),
                                                         curves.evaluated_points_num(),
                                                         *this,
                                                         is_ribbon,
                                                         curves.has_cyclic_curve());

  return indirection_buf;
}

gpu::Batch *CurvesEvalCache::batch_get(const int evaluated_point_count,
                                       const int curve_count,
                                       const int face_per_segment,
                                       const bool use_cyclic,
                                       bool &r_over_limit)
{
  gpu::Batch *&batch = this->batch[face_per_segment];
  if (batch) {
    return batch;
  }

  int64_t segment_count = 0;
  int64_t vert_per_segment = 0;
  GPUPrimType prim_type = GPU_PRIM_NONE;

  if (face_per_segment == 0) {
    /* Add one point per curve to restart the primitive. */
    segment_count = int64_t(evaluated_point_count) + curve_count;
    if (use_cyclic) {
      segment_count += curve_count;
    }
    /* The last segment is always a restart vertex. However, it is not accounted for inside the
     * data buffers and can lead to out of bound reads (see #148914). */
    segment_count -= (segment_count > 0) ? 1 : 0;
    vert_per_segment = 1;
    prim_type = GPU_PRIM_LINE_STRIP;
  }
  else if (face_per_segment == 1) {
    /* Add one point per curve to restart the primitive. */
    segment_count = int64_t(evaluated_point_count) + curve_count;
    if (use_cyclic) {
      segment_count += curve_count;
    }
    /* The last segment is always a restart vertex. However, it is not accounted for inside the
     * data buffers and can lead to out of bound reads (see #148914). */
    segment_count -= (segment_count > 0) ? 1 : 0;
    vert_per_segment = 2;
    prim_type = GPU_PRIM_TRI_STRIP;
  }
  else if (face_per_segment >= 2) {
    segment_count = int64_t(evaluated_point_count) - curve_count;
    if (use_cyclic) {
      segment_count += curve_count;
    }
    /* Add one vertex per segment to restart the primitive. */
    vert_per_segment = (face_per_segment + 1) * 2 + 1;
    prim_type = GPU_PRIM_TRI_STRIP;
  }

  /* Since we rely on buffer textures for reading the indirection buffer we have to abide by their
   * size limit. This size is low enough on NVidia to discard strands after 130,000,000 points.
   * We detect this case and display an error message in the viewport. */
  uint32_t texel_buffer_limit = GPU_max_buffer_texture_size();
  /* We are also limited by the number of vertices in a batch, which is INT_MAX. */
  int64_t segment_limit = std::min(int64_t(texel_buffer_limit), int64_t(INT_MAX));
  if (segment_count > segment_limit) {
    segment_count = segment_limit;
    r_over_limit = true;
  }
  r_over_limit = false;

  uint32_t vertex_count = segment_count * vert_per_segment;
  batch = GPU_batch_create_procedural(prim_type, vertex_count);
  return batch;
}

CurvesEvalCache &curves_get_eval_cache(Curves &curves_id)
{
  return get_batch_cache(curves_id).eval_cache;
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

  CurvesEvalCache &eval_cache = cache->eval_cache;

  if (drw_attributes_overlap(&eval_cache.attr_used_over_time, &eval_cache.attr_used)) {
    eval_cache.last_attr_matching_time = ctime;
  }

  if (ctime - eval_cache.last_attr_matching_time > U.vbotimeout) {
    do_discard = true;
  }

  eval_cache.attr_used_over_time.clear();

  if (do_discard) {
    cache->eval_cache.discard_attributes();
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

gpu::VertBufPtr &DRW_curves_texture_for_evaluated_attribute(Curves *curves,
                                                            const StringRef name,
                                                            bool &r_is_point_domain,
                                                            bool &r_valid_attribute)
{
  CurvesEvalCache &cache = get_batch_cache(*curves).eval_cache;

  request_attribute(*curves, name);

  /* TODO(fclem): Remove Global access. */
  CurvesModule &module = *drw_get().data->curves_module;
  cache.ensure_attributes(module, curves->geometry.wrap(), nullptr);

  for (const int i : cache.attr_used.index_range()) {
    if (cache.attr_used[i] == name) {
      r_valid_attribute = true;
      if (cache.attributes_point_domain[i]) {
        r_is_point_domain = true;
        return cache.evaluated_attributes_buf[i];
      }
      r_is_point_domain = false;
      return cache.curve_attributes_buf[i];
    }
  }
  r_valid_attribute = false;
  r_is_point_domain = false;
  return cache.evaluated_attributes_buf[0];
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
