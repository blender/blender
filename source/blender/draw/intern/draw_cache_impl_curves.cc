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
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "DNA_curves_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

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
#define EDIT_CURVES_LEFT_HANDLE_TYPES_SHIFT (6u)
#define EDIT_CURVES_RIGHT_HANDLE_TYPES_SHIFT (4u)

/* ---------------------------------------------------------------------- */
struct CurvesUboStorage {
  int32_t bezier_point_count;
  float _pad1, _pad2, _pad3;
};

struct CurvesBatchCache {
  CurvesEvalCache eval_cache;

  GPUBatch *edit_points;
  GPUBatch *edit_handles;

  GPUBatch *sculpt_cage;
  GPUIndexBuf *sculpt_cage_ibo;

  /* Crazy-space point positions for original points. */
  GPUVertBuf *edit_points_pos;

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
  GPUVertBuf *edit_points_data;

  /* Buffer used to store CurvesUboStorage value. push_constant() could not be used for this
   * value, as it is not know in overlay_edit_curves.cc as other constants. */
  GPUUniformBuf *curves_ubo_storage;

  /* Selection of original points. */
  GPUVertBuf *edit_points_selection;

  GPUIndexBuf *edit_handles_ibo;

  GPUBatch *edit_curves_lines;
  GPUVertBuf *edit_curves_lines_pos;
  GPUIndexBuf *edit_curves_lines_ibo;

  /* Whether the cache is invalid. */
  bool is_dirty;

  /**
   * The draw cache extraction is currently not multi-threaded for multiple objects, but if it was,
   * some locking would be necessary because multiple objects can use the same curves data with
   * different materials, etc. This is a placeholder to make multi-threading easier in the future.
   */
  std::mutex render_mutex;
};

static uint DUMMY_ID;

static GPUVertFormat single_attr_vbo_format(const char *name,
                                            const GPUVertCompType comp_type,
                                            const uint comp_len,
                                            const GPUVertFetchMode fetch_mode,
                                            uint &attr_id = DUMMY_ID)
{
  GPUVertFormat format{};
  attr_id = GPU_vertformat_attr_add(&format, name, comp_type, comp_len, fetch_mode);
  return format;
}

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
    cache->curves_ubo_storage = GPU_uniformbuf_create_ex(
        sizeof(CurvesUboStorage), nullptr, "CurvesUboStorage");
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

  for (const int i : IndexRange(MAX_HAIR_SUBDIV)) {
    for (const int j : IndexRange(GPU_MAX_ATTR)) {
      GPU_VERTBUF_DISCARD_SAFE(eval_cache.final[i].attributes_buf[j]);
    }

    drw_attributes_clear(&eval_cache.final[i].attr_used);
  }
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

static void clear_eval_data(CurvesEvalCache &eval_cache)
{
  /* TODO: more granular update tagging. */
  GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_point_buf);
  GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_length_buf);
  GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_strand_buf);
  GPU_VERTBUF_DISCARD_SAFE(eval_cache.proc_strand_seg_buf);

  for (const int i : IndexRange(MAX_HAIR_SUBDIV)) {
    GPU_VERTBUF_DISCARD_SAFE(eval_cache.final[i].proc_buf);
    for (const int j : IndexRange(MAX_THICKRES)) {
      GPU_BATCH_DISCARD_SAFE(eval_cache.final[i].proc_hairs[j]);
    }
  }

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
  GPU_vertformat_attr_add(&format, "posTime", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  cache.proc_point_buf = GPU_vertbuf_create_with_format_ex(
      &format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(cache.proc_point_buf, cache.points_num);

  MutableSpan posTime_data{
      static_cast<PositionAndParameter *>(GPU_vertbuf_get_data(cache.proc_point_buf)),
      cache.points_num};

  GPUVertFormat length_format = {0};
  GPU_vertformat_attr_add(&length_format, "hairLength", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);

  cache.proc_length_buf = GPU_vertbuf_create_with_format_ex(
      &length_format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(cache.proc_length_buf, cache.curves_num);

  /* TODO: Only create hairLength VBO when necessary. */
  MutableSpan hairLength_data{static_cast<float *>(GPU_vertbuf_get_data(cache.proc_length_buf)),
                              cache.curves_num};

  fill_points_position_time_vbo(
      curves.points_by_curve(), curves.positions(), posTime_data, hairLength_data);
}

static uint32_t bezier_data_value(int8_t left_handle_type, int8_t right_handle_type)
{
  return (left_handle_type << EDIT_CURVES_LEFT_HANDLE_TYPES_SHIFT) |
         (right_handle_type << EDIT_CURVES_RIGHT_HANDLE_TYPES_SHIFT) | EDIT_CURVES_BEZIER_HANDLE;
}

static uint32_t bezier_data_value(int8_t handle_type)
{
  return bezier_data_value(handle_type, handle_type);
}

static void create_edit_points_position_and_data(
    const bke::CurvesGeometry &curves,
    const IndexMask bezier_curves,
    const OffsetIndices<int> bezier_dst_offsets,
    const bke::crazyspace::GeometryDeformation deformation,
    CurvesBatchCache &cache)
{
  static GPUVertFormat format_pos = single_attr_vbo_format(
      "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  /* GPU_COMP_U32 is used instead of GPU_COMP_U8 because depending on running hardware stride might
   * still be 4. Thus adding complexity to the code and still sparing no memory. */
  static GPUVertFormat format_data = single_attr_vbo_format(
      "data", GPU_COMP_U32, 1, GPU_FETCH_INT);

  Span<float3> deformed_positions = deformation.positions;
  const int bezier_point_count = bezier_dst_offsets.total_size();
  const int size = deformed_positions.size() + bezier_point_count * 2;
  GPU_vertbuf_init_with_format(cache.edit_points_pos, &format_pos);
  GPU_vertbuf_data_alloc(cache.edit_points_pos, size);

  GPU_vertbuf_init_with_format(cache.edit_points_data, &format_data);
  GPU_vertbuf_data_alloc(cache.edit_points_data, size);

  float3 *pos_buffer_data = static_cast<float3 *>(GPU_vertbuf_get_data(cache.edit_points_pos));
  uint32_t *data_buffer_data = static_cast<uint32_t *>(
      GPU_vertbuf_get_data(cache.edit_points_data));

  MutableSpan<float3> pos_dst(pos_buffer_data, deformed_positions.size());
  pos_dst.copy_from(deformed_positions);

  MutableSpan<uint32_t> data_dst(data_buffer_data, size);

  MutableSpan<uint32_t> handle_data_left(data_buffer_data + deformed_positions.size(),
                                         bezier_point_count);
  MutableSpan<uint32_t> handle_data_right(
      data_buffer_data + deformed_positions.size() + bezier_point_count, bezier_point_count);

  const Span<float3> left_handle_positions = curves.handle_positions_left();
  const Span<float3> right_handle_positions = curves.handle_positions_right();
  const VArray<int8_t> left_handle_types = curves.handle_types_left();
  const VArray<int8_t> right_handle_types = curves.handle_types_right();
  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  auto handle_other_curves = [&](const uint32_t fill_value) {
    return [&, fill_value](const IndexMask &selection) {
      selection.foreach_index(GrainSize(256), [&](const int curve_i) {
        const IndexRange points = points_by_curve[curve_i];
        data_dst.slice(points).fill(fill_value);
      });
    };
  };

  bke::curves::foreach_curve_by_type(
      curves.curve_types(),
      curves.curve_type_counts(),
      curves.curves_range(),
      handle_other_curves(0),
      handle_other_curves(0),
      [&](const IndexMask &selection) {
        selection.foreach_index(GrainSize(256), [&](const int src_i, const int64_t dst_i) {
          for (const int point : points_by_curve[src_i]) {
            const int point_in_curve = point - points_by_curve[src_i].start();
            const int dst_index = bezier_dst_offsets[dst_i].start() + point_in_curve;

            data_dst[point] = bezier_data_value(left_handle_types[point],
                                                right_handle_types[point]);
            handle_data_left[dst_index] = bezier_data_value(left_handle_types[point]);
            handle_data_right[dst_index] = bezier_data_value(right_handle_types[point]);
          }
        });
      },
      handle_other_curves(EDIT_CURVES_NURBS_CONTROL_POINT));

  if (!bezier_point_count) {
    return;
  }

  MutableSpan<float3> left_handles(pos_buffer_data + deformed_positions.size(),
                                   bezier_point_count);
  MutableSpan<float3> right_handles(
      pos_buffer_data + deformed_positions.size() + bezier_point_count, bezier_point_count);

  /* TODO: Use deformed left_handle_positions and left_handle_positions. */
  array_utils::gather_group_to_group(
      points_by_curve, bezier_dst_offsets, bezier_curves, left_handle_positions, left_handles);
  array_utils::gather_group_to_group(
      points_by_curve, bezier_dst_offsets, bezier_curves, right_handle_positions, right_handles);
}

static void create_edit_points_selection(const bke::CurvesGeometry &curves,
                                         const IndexMask bezier_curves,
                                         const OffsetIndices<int> bezier_dst_offsets,
                                         CurvesBatchCache &cache)
{
  static GPUVertFormat format_data = single_attr_vbo_format(
      "selection", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);

  const int bezier_point_count = bezier_dst_offsets.total_size();
  const int vert_count = curves.points_num() + bezier_point_count * 2;
  GPU_vertbuf_init_with_format(cache.edit_points_selection, &format_data);
  GPU_vertbuf_data_alloc(cache.edit_points_selection, vert_count);
  MutableSpan<float> data(static_cast<float *>(GPU_vertbuf_get_data(cache.edit_points_selection)),
                          vert_count);

  const VArray<float> attribute = *curves.attributes().lookup_or_default<float>(
      ".selection", bke::AttrDomain::Point, 1.0f);
  attribute.materialize(data.slice(0, curves.points_num()));

  if (!bezier_point_count) {
    return;
  }

  const VArray<float> attribute_left = *curves.attributes().lookup_or_default<float>(
      ".selection_handle_left", bke::AttrDomain::Point, 0.0f);
  const VArray<float> attribute_right = *curves.attributes().lookup_or_default<float>(
      ".selection_handle_right", bke::AttrDomain::Point, 0.0f);

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();

  IndexRange dst_range = IndexRange::from_begin_size(curves.points_num(), bezier_point_count);
  array_utils::gather_group_to_group(
      points_by_curve, bezier_dst_offsets, bezier_curves, attribute_left, data.slice(dst_range));

  dst_range = dst_range.shift(bezier_point_count);
  array_utils::gather_group_to_group(
      points_by_curve, bezier_dst_offsets, bezier_curves, attribute_right, data.slice(dst_range));
}

static void create_sculpt_cage_ibo(const OffsetIndices<int> points_by_curve,
                                   CurvesBatchCache &cache)
{
  const int points_num = points_by_curve.total_size();
  const int curves_num = points_by_curve.size();
  const int indices_num = points_num + curves_num;

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, indices_num, points_num);

  for (const int i : points_by_curve.index_range()) {
    const IndexRange points = points_by_curve[i];
    for (const int i_point : points) {
      GPU_indexbuf_add_generic_vert(&elb, i_point);
    }
    GPU_indexbuf_add_primitive_restart(&elb);
  }
  GPU_indexbuf_build_in_place(&elb, cache.sculpt_cage_ibo);
}

static void calc_edit_handles_vbo(const bke::CurvesGeometry &curves,
                                  const IndexMask bezier_curves,
                                  const OffsetIndices<int> bezier_offsets,
                                  const IndexMask nurbs_curves,
                                  const OffsetIndices<int> nurbs_offsets,
                                  CurvesBatchCache &cache)
{
  const int bezier_point_count = bezier_offsets.total_size();
  /* Left and right handle will be appended for each Bezier point. */
  const int vert_len = curves.points_num() + 2 * bezier_point_count;
  /* For each point has 2 lines from 2 point and one restart entry. */
  const int index_len_for_bezier_handles = 6 * bezier_point_count;
  const VArray<bool> cyclic = curves.cyclic();
  /* All NURBS control points plus restart for every curve.
   * Add space for possible cyclic curves.
   * If one point curves or two point cyclic curves are present, not all builder's buffer space
   * will be used. */
  const int index_len_for_nurbs = nurbs_offsets.total_size() + nurbs_curves.size() +
                                  array_utils::count_booleans(cyclic, nurbs_curves);
  const int index_len = index_len_for_bezier_handles + index_len_for_nurbs;
  /* Use two index buffer builders for the same underlying memory. */
  GPUIndexBufBuilder elb, right_elb;
  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, index_len, vert_len);
  memcpy(&right_elb, &elb, sizeof(elb));
  right_elb.index_len = 3 * bezier_point_count;

  const OffsetIndices points_by_curve = curves.points_by_curve();

  bezier_curves.foreach_index([&](const int64_t src_i, const int64_t dst_i) {
    IndexRange bezier_points = points_by_curve[src_i];
    const int index_shift = curves.points_num() - bezier_points.first() +
                            bezier_offsets[dst_i].first();
    for (const int point : bezier_points) {
      const int point_left_i = index_shift + point;
      GPU_indexbuf_add_generic_vert(&elb, point_left_i);
      GPU_indexbuf_add_generic_vert(&elb, point);
      GPU_indexbuf_add_primitive_restart(&elb);
      GPU_indexbuf_add_generic_vert(&right_elb, point_left_i + bezier_point_count);
      GPU_indexbuf_add_generic_vert(&right_elb, point);
      GPU_indexbuf_add_primitive_restart(&right_elb);
    }
  });
  nurbs_curves.foreach_index([&](const int64_t src_i) {
    IndexRange curve_points = points_by_curve[src_i];
    if (curve_points.size() <= 1) {
      return;
    }
    for (const int point : curve_points) {
      GPU_indexbuf_add_generic_vert(&right_elb, point);
    }
    if (cyclic[src_i] && curve_points.size() > 2) {
      GPU_indexbuf_add_generic_vert(&right_elb, curve_points.first());
    }
    GPU_indexbuf_add_primitive_restart(&right_elb);
  });
  GPU_indexbuf_join(&elb, &right_elb);
  GPU_indexbuf_build_in_place(&elb, cache.edit_handles_ibo);

  CurvesUboStorage ubo_storage{bezier_point_count};
  GPU_uniformbuf_update(cache.curves_ubo_storage, &ubo_storage);
}

static void alloc_final_attribute_vbo(CurvesEvalCache &cache,
                                      const GPUVertFormat *format,
                                      const int subdiv,
                                      const int index,
                                      const char * /*name*/)
{
  CurvesEvalFinalCache &final_cache = cache.final[subdiv];
  final_cache.attributes_buf[index] = GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_DEVICE_ONLY | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

  /* Create a destination buffer for the transform feedback. Sized appropriately */
  /* Those are points! not line segments. */
  GPU_vertbuf_data_alloc(final_cache.attributes_buf[index],
                         final_cache.resolution * cache.curves_num);
}

static void ensure_control_point_attribute(const Curves &curves,
                                           CurvesEvalCache &cache,
                                           const DRW_AttributeRequest &request,
                                           const int index,
                                           const GPUVertFormat *format)
{
  if (cache.proc_attributes_buf[index] != nullptr) {
    return;
  }

  GPU_VERTBUF_DISCARD_SAFE(cache.proc_attributes_buf[index]);

  cache.proc_attributes_buf[index] = GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPUVertBuf *attr_vbo = cache.proc_attributes_buf[index];

  GPU_vertbuf_data_alloc(attr_vbo,
                         request.domain == bke::AttrDomain::Point ? curves.geometry.point_num :
                                                                    curves.geometry.curve_num);

  const bke::AttributeAccessor attributes = curves.geometry.wrap().attributes();

  /* TODO(@kevindietrich): float4 is used for scalar attributes as the implicit conversion done
   * by OpenGL to vec4 for a scalar `s` will produce a `vec4(s, 0, 0, 1)`. However, following
   * the Blender convention, it should be `vec4(s, s, s, 1)`. This could be resolved using a
   * similar texture state swizzle to map the attribute correctly as for volume attributes, so we
   * can control the conversion ourselves. */
  bke::AttributeReader<ColorGeometry4f> attribute = attributes.lookup_or_default<ColorGeometry4f>(
      request.attribute_name, request.domain, {0.0f, 0.0f, 0.0f, 1.0f});

  MutableSpan<ColorGeometry4f> vbo_span{
      static_cast<ColorGeometry4f *>(GPU_vertbuf_get_data(attr_vbo)),
      attributes.domain_size(request.domain)};

  attribute.varray.materialize(vbo_span);
}

static void ensure_final_attribute(const Curves &curves,
                                   CurvesEvalCache &cache,
                                   const DRW_AttributeRequest &request,
                                   const int subdiv,
                                   const int index)
{
  char sampler_name[32];
  drw_curves_get_attribute_sampler_name(request.attribute_name, sampler_name);

  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);
  /* All attributes use vec4, see comment below. */
  GPU_vertformat_attr_add(&format, sampler_name, GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  ensure_control_point_attribute(curves, cache, request, index, &format);

  /* Existing final data may have been for a different attribute (with a different name or domain),
   * free the data. */
  GPU_VERTBUF_DISCARD_SAFE(cache.final[subdiv].attributes_buf[index]);

  /* Ensure final data for points. */
  if (request.domain == bke::AttrDomain::Point) {
    alloc_final_attribute_vbo(cache, &format, subdiv, index, sampler_name);
  }
}

static void fill_curve_offsets_vbos(const OffsetIndices<int> points_by_curve,
                                    GPUVertBufRaw &data_step,
                                    GPUVertBufRaw &seg_step)
{
  for (const int i : points_by_curve.index_range()) {
    const IndexRange points = points_by_curve[i];

    *(uint *)GPU_vertbuf_raw_step(&data_step) = points.start();
    *(ushort *)GPU_vertbuf_raw_step(&seg_step) = points.size() - 1;
  }
}

static void create_curve_offsets_vbos(const OffsetIndices<int> points_by_curve,
                                      CurvesEvalCache &cache)
{
  GPUVertBufRaw data_step, seg_step;

  GPUVertFormat format_data = {0};
  uint data_id = GPU_vertformat_attr_add(&format_data, "data", GPU_COMP_U32, 1, GPU_FETCH_INT);

  GPUVertFormat format_seg = {0};
  uint seg_id = GPU_vertformat_attr_add(&format_seg, "data", GPU_COMP_U16, 1, GPU_FETCH_INT);

  /* Curve Data. */
  cache.proc_strand_buf = GPU_vertbuf_create_with_format_ex(
      &format_data, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(cache.proc_strand_buf, cache.curves_num);
  GPU_vertbuf_attr_get_raw_data(cache.proc_strand_buf, data_id, &data_step);

  cache.proc_strand_seg_buf = GPU_vertbuf_create_with_format_ex(
      &format_seg, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);
  GPU_vertbuf_data_alloc(cache.proc_strand_seg_buf, cache.curves_num);
  GPU_vertbuf_attr_get_raw_data(cache.proc_strand_seg_buf, seg_id, &seg_step);

  fill_curve_offsets_vbos(points_by_curve, data_step, seg_step);
}

static void alloc_final_points_vbo(CurvesEvalCache &cache, int subdiv)
{
  /* Same format as proc_point_buf. */
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  cache.final[subdiv].proc_buf = GPU_vertbuf_create_with_format_ex(
      &format, GPU_USAGE_DEVICE_ONLY | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

  /* Create a destination buffer for the transform feedback. Sized appropriately */
  /* Those are points! not line segments. */
  GPU_vertbuf_data_alloc(cache.final[subdiv].proc_buf,
                         cache.final[subdiv].resolution * cache.curves_num);
}

static void calc_final_indices(const bke::CurvesGeometry &curves,
                               CurvesEvalCache &cache,
                               const int thickness_res,
                               const int subdiv)
{
  BLI_assert(thickness_res <= MAX_THICKRES); /* Cylinder strip not currently supported. */
  /* Determine prim type and element count.
   * NOTE: Metal backend uses non-restart prim types for optimal HW performance. */
  bool use_strip_prims = (GPU_backend_get_type() != GPU_BACKEND_METAL);
  int verts_per_curve;
  GPUPrimType prim_type;

  if (use_strip_prims) {
    /* +1 for primitive restart */
    verts_per_curve = cache.final[subdiv].resolution * thickness_res;
    prim_type = (thickness_res == 1) ? GPU_PRIM_LINE_STRIP : GPU_PRIM_TRI_STRIP;
  }
  else {
    /* Use full primitive type. */
    prim_type = (thickness_res == 1) ? GPU_PRIM_LINES : GPU_PRIM_TRIS;
    int verts_per_segment = ((prim_type == GPU_PRIM_LINES) ? 2 : 6);
    verts_per_curve = (cache.final[subdiv].resolution - 1) * verts_per_segment;
  }

  static GPUVertFormat format = {0};
  GPU_vertformat_clear(&format);

  /* initialize vertex format */
  GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);

  GPUVertBuf *vbo = GPU_vertbuf_create_with_format(&format);
  GPU_vertbuf_data_alloc(vbo, 1);

  GPUIndexBuf *ibo = nullptr;
  eGPUBatchFlag owns_flag = GPU_BATCH_OWNS_VBO;
  if (curves.curves_num()) {
    ibo = GPU_indexbuf_build_curves_on_device(prim_type, curves.curves_num(), verts_per_curve);
    owns_flag |= GPU_BATCH_OWNS_INDEX;
  }
  cache.final[subdiv].proc_hairs[thickness_res - 1] = GPU_batch_create_ex(
      prim_type, vbo, ibo, owns_flag);
}

static bool ensure_attributes(const Curves &curves,
                              CurvesBatchCache &cache,
                              const GPUMaterial *gpu_material,
                              const int subdiv)
{
  const CustomData *cd_curve = &curves.geometry.curve_data;
  const CustomData *cd_point = &curves.geometry.point_data;
  CurvesEvalFinalCache &final_cache = cache.eval_cache.final[subdiv];

  if (gpu_material) {
    DRW_Attributes attrs_needed;
    drw_attributes_clear(&attrs_needed);
    ListBase gpu_attrs = GPU_material_attributes(gpu_material);
    LISTBASE_FOREACH (const GPUMaterialAttribute *, gpu_attr, &gpu_attrs) {
      const char *name = gpu_attr->name;

      int layer_index;
      eCustomDataType type;
      bke::AttrDomain domain;
      if (drw_custom_data_match_attribute(cd_curve, name, &layer_index, &type)) {
        domain = bke::AttrDomain::Curve;
      }
      else if (drw_custom_data_match_attribute(cd_point, name, &layer_index, &type)) {
        domain = bke::AttrDomain::Point;
      }
      else {
        continue;
      }

      drw_attributes_add_request(&attrs_needed, name, type, layer_index, domain);
    }

    if (!drw_attributes_overlap(&final_cache.attr_used, &attrs_needed)) {
      /* Some new attributes have been added, free all and start over. */
      for (const int i : IndexRange(GPU_MAX_ATTR)) {
        GPU_VERTBUF_DISCARD_SAFE(cache.eval_cache.proc_attributes_buf[i]);
      }
      drw_attributes_merge(&final_cache.attr_used, &attrs_needed, cache.render_mutex);
    }
    drw_attributes_merge(&final_cache.attr_used_over_time, &attrs_needed, cache.render_mutex);
  }

  bool need_tf_update = false;

  for (const int i : IndexRange(final_cache.attr_used.num_requests)) {
    const DRW_AttributeRequest &request = final_cache.attr_used.requests[i];

    if (cache.eval_cache.final[subdiv].attributes_buf[i] != nullptr) {
      continue;
    }

    if (request.domain == bke::AttrDomain::Point) {
      need_tf_update = true;
    }

    ensure_final_attribute(curves, cache.eval_cache, request, subdiv, i);
  }

  return need_tf_update;
}

static void request_attribute(Curves &curves, const char *name)
{
  CurvesBatchCache &cache = get_batch_cache(curves);
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  const int subdiv = scene->r.hair_subdiv;
  CurvesEvalFinalCache &final_cache = cache.eval_cache.final[subdiv];

  DRW_Attributes attributes{};

  bke::CurvesGeometry &curves_geometry = curves.geometry.wrap();
  std::optional<bke::AttributeMetaData> meta_data = curves_geometry.attributes().lookup_meta_data(
      name);
  if (!meta_data) {
    return;
  }
  const bke::AttrDomain domain = meta_data->domain;
  const eCustomDataType type = meta_data->data_type;
  const CustomData &custom_data = domain == bke::AttrDomain::Point ? curves.geometry.point_data :
                                                                     curves.geometry.curve_data;

  drw_attributes_add_request(
      &attributes, name, type, CustomData_get_named_layer(&custom_data, type, name), domain);

  drw_attributes_merge(&final_cache.attr_used, &attributes, cache.render_mutex);
}

void drw_curves_get_attribute_sampler_name(const char *layer_name, char r_sampler_name[32])
{
  char attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
  GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
  /* Attributes use auto-name. */
  BLI_snprintf(r_sampler_name, 32, "a%s", attr_safe_name);
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
  eval_cache.curves_num = curves.curves_num();
  eval_cache.points_num = curves.points_num();

  const int steps = 3; /* TODO: don't hard-code? */
  eval_cache.final[subdiv].resolution = 1 << (steps + subdiv);

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
  if (eval_cache.final[subdiv].proc_buf == nullptr) {
    alloc_final_points_vbo(eval_cache, subdiv);
    need_ft_update = true;
  }
  if (eval_cache.final[subdiv].proc_hairs[thickness_res - 1] == nullptr) {
    calc_final_indices(curves, eval_cache, thickness_res, subdiv);
  }

  need_ft_update |= ensure_attributes(*curves_id, cache, gpu_material, subdiv);

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
  DRW_UBO_FREE_SAFE(batch_cache->curves_ubo_storage);
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

  for (const int i : IndexRange(MAX_HAIR_SUBDIV)) {
    CurvesEvalFinalCache &final_cache = cache->eval_cache.final[i];

    if (drw_attributes_overlap(&final_cache.attr_used_over_time, &final_cache.attr_used)) {
      final_cache.last_attr_matching_time = ctime;
    }

    if (ctime - final_cache.last_attr_matching_time > U.vbotimeout) {
      do_discard = true;
    }

    drw_attributes_clear(&final_cache.attr_used_over_time);
  }

  if (do_discard) {
    discard_attributes(cache->eval_cache);
  }
}

int DRW_curves_material_count_get(const Curves *curves)
{
  return max_ii(1, curves->totcol);
}

GPUUniformBuf *DRW_curves_batch_cache_ubo_storage(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return cache.curves_ubo_storage;
}

GPUBatch *DRW_curves_batch_cache_get_edit_points(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return DRW_batch_request(&cache.edit_points);
}

GPUBatch *DRW_curves_batch_cache_get_sculpt_curves_cage(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return DRW_batch_request(&cache.sculpt_cage);
}

GPUBatch *DRW_curves_batch_cache_get_edit_curves_handles(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return DRW_batch_request(&cache.edit_handles);
}

GPUBatch *DRW_curves_batch_cache_get_edit_curves_lines(Curves *curves)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  return DRW_batch_request(&cache.edit_curves_lines);
}

GPUVertBuf **DRW_curves_texture_for_evaluated_attribute(Curves *curves,
                                                        const char *name,
                                                        bool *r_is_point_domain)
{
  CurvesBatchCache &cache = get_batch_cache(*curves);
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  const int subdiv = scene->r.hair_subdiv;
  CurvesEvalFinalCache &final_cache = cache.eval_cache.final[subdiv];

  request_attribute(*curves, name);

  int request_i = -1;
  for (const int i : IndexRange(final_cache.attr_used.num_requests)) {
    if (STREQ(final_cache.attr_used.requests[i].attribute_name, name)) {
      request_i = i;
      break;
    }
  }
  if (request_i == -1) {
    *r_is_point_domain = false;
    return nullptr;
  }
  switch (final_cache.attr_used.requests[request_i].domain) {
    case bke::AttrDomain::Point:
      *r_is_point_domain = true;
      return &final_cache.attributes_buf[request_i];
    case bke::AttrDomain::Curve:
      *r_is_point_domain = false;
      return &cache.eval_cache.proc_attributes_buf[request_i];
    default:
      BLI_assert_unreachable();
      return nullptr;
  }
}

static void create_edit_lines_ibo(const bke::CurvesGeometry &curves, CurvesBatchCache &cache)
{
  const OffsetIndices points_by_curve = curves.evaluated_points_by_curve();
  const VArray<bool> cyclic = curves.cyclic();

  int edges_len = 0;
  for (const int i : curves.curves_range()) {
    edges_len += bke::curves::segments_num(points_by_curve[i].size(), cyclic[i]);
  }

  const int index_len = edges_len + curves.curves_num() * 2;

  GPUIndexBufBuilder elb;
  GPU_indexbuf_init_ex(&elb, GPU_PRIM_LINE_STRIP, index_len, points_by_curve.total_size());

  for (const int i : curves.curves_range()) {
    const IndexRange points = points_by_curve[i];
    if (cyclic[i] && points.size() > 1) {
      GPU_indexbuf_add_generic_vert(&elb, points.last());
    }
    for (const int i_point : points) {
      GPU_indexbuf_add_generic_vert(&elb, i_point);
    }
    GPU_indexbuf_add_primitive_restart(&elb);
  }

  GPU_indexbuf_build_in_place(&elb, cache.edit_curves_lines_ibo);
}

static void create_edit_points_position_vbo(
    const bke::CurvesGeometry &curves,
    const bke::crazyspace::GeometryDeformation & /*deformation*/,
    CurvesBatchCache &cache)
{
  static uint attr_id;
  static GPUVertFormat format = single_attr_vbo_format(
      "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT, attr_id);

  /* TODO: Deform curves using deformations. */
  const Span<float3> positions = curves.evaluated_positions();

  GPU_vertbuf_init_with_format(cache.edit_curves_lines_pos, &format);
  GPU_vertbuf_data_alloc(cache.edit_curves_lines_pos, positions.size());
  GPU_vertbuf_attr_fill(cache.edit_curves_lines_pos, attr_id, positions.data());
}

void DRW_curves_batch_cache_create_requested(Object *ob)
{
  Curves *curves_id = static_cast<Curves *>(ob->data);
  Object *ob_orig = DEG_get_original_object(ob);
  if (ob_orig == nullptr) {
    return;
  }
  const Curves *curves_orig_id = static_cast<Curves *>(ob_orig->data);

  draw::CurvesBatchCache &cache = draw::get_batch_cache(*curves_id);
  const bke::CurvesGeometry &curves_orig = curves_orig_id->geometry.wrap();

  IndexMaskMemory memory;
  const IndexMask bezier_curves = bke::curves::indices_for_type(curves_orig.curve_types(),
                                                                curves_orig.curve_type_counts(),
                                                                CURVE_TYPE_BEZIER,
                                                                curves_orig.curves_range(),
                                                                memory);
  Array<int> bezier_point_offset_data(bezier_curves.size() + 1);
  const OffsetIndices<int> bezier_offsets = offset_indices::gather_selected_offsets(
      curves_orig.points_by_curve(), bezier_curves, bezier_point_offset_data);

  const bke::crazyspace::GeometryDeformation deformation =
      bke::crazyspace::get_evaluated_curves_deformation(ob, *ob_orig);

  if (DRW_batch_requested(cache.edit_points, GPU_PRIM_POINTS)) {
    DRW_vbo_request(cache.edit_points, &cache.edit_points_pos);
    DRW_vbo_request(cache.edit_points, &cache.edit_points_selection);
  }
  if (DRW_batch_requested(cache.sculpt_cage, GPU_PRIM_LINE_STRIP)) {
    DRW_ibo_request(cache.sculpt_cage, &cache.sculpt_cage_ibo);
    DRW_vbo_request(cache.sculpt_cage, &cache.edit_points_pos);
    DRW_vbo_request(cache.sculpt_cage, &cache.edit_points_data);
    DRW_vbo_request(cache.sculpt_cage, &cache.edit_points_selection);
  }
  if (DRW_batch_requested(cache.edit_handles, GPU_PRIM_LINE_STRIP)) {
    DRW_ibo_request(cache.edit_handles, &cache.edit_handles_ibo);
    DRW_vbo_request(cache.edit_handles, &cache.edit_points_pos);
    DRW_vbo_request(cache.edit_handles, &cache.edit_points_data);
    DRW_vbo_request(cache.edit_handles, &cache.edit_points_selection);
  }
  if (DRW_batch_requested(cache.edit_curves_lines, GPU_PRIM_LINE_STRIP)) {
    DRW_vbo_request(cache.edit_curves_lines, &cache.edit_curves_lines_pos);
    DRW_ibo_request(cache.edit_curves_lines, &cache.edit_curves_lines_ibo);
  }
  if (DRW_vbo_requested(cache.edit_points_pos)) {
    create_edit_points_position_and_data(
        curves_orig, bezier_curves, bezier_offsets, deformation, cache);
  }
  if (DRW_vbo_requested(cache.edit_points_selection)) {
    create_edit_points_selection(curves_orig, bezier_curves, bezier_offsets, cache);
  }
  if (DRW_ibo_requested(cache.edit_handles_ibo)) {
    IndexMaskMemory nurbs_memory;
    const IndexMask nurbs_curves = bke::curves::indices_for_type(curves_orig.curve_types(),
                                                                 curves_orig.curve_type_counts(),
                                                                 CURVE_TYPE_NURBS,
                                                                 curves_orig.curves_range(),
                                                                 nurbs_memory);
    Array<int> nurbs_point_offset_data(nurbs_curves.size() + 1);
    const OffsetIndices<int> nurbs_offsets = offset_indices::gather_selected_offsets(
        curves_orig.points_by_curve(), nurbs_curves, nurbs_point_offset_data);

    calc_edit_handles_vbo(
        curves_orig, bezier_curves, bezier_offsets, nurbs_curves, nurbs_offsets, cache);
  }
  if (DRW_ibo_requested(cache.sculpt_cage_ibo)) {
    create_sculpt_cage_ibo(curves_orig.points_by_curve(), cache);
  }

  if (DRW_vbo_requested(cache.edit_curves_lines_pos)) {
    create_edit_points_position_vbo(curves_orig, deformation, cache);
  }

  if (DRW_ibo_requested(cache.edit_curves_lines_ibo)) {
    create_edit_lines_ibo(curves_orig, cache);
  }
}

}  // namespace blender::draw
