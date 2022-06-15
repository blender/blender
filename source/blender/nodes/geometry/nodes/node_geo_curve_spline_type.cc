/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <numeric>

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"

#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_spline_type_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveSplineType)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "spline_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSplineType *data = MEM_cnew<NodeGeometryCurveSplineType>(__func__);

  data->spline_type = CURVE_TYPE_POLY;
  node->storage = data;
}

/**
 * This function answers the question about possible conversion method for NURBS-to-Bezier. In
 * general for 3rd degree NURBS curves there is one-to-one relation with 3rd degree Bezier curves
 * that can be exploit for conversion - Bezier handles sit on NURBS hull segments and in the middle
 * between those handles are Bezier anchor points.
 */
static bool is_nurbs_to_bezier_one_to_one(const KnotsMode knots_mode)
{
  if (ELEM(knots_mode, NURBS_KNOT_MODE_NORMAL, NURBS_KNOT_MODE_ENDPOINT)) {
    return true;
  }
  return false;
}

/**
 * As an optimization, just change the types on a mutable curves data-block when the conversion is
 * simple. This could be expanded to more cases where the number of points doesn't change in the
 * future, though that might require properly initializing some attributes, or removing others.
 */
static bool conversion_can_change_point_num(const CurveType dst_type)
{
  if (ELEM(dst_type, CURVE_TYPE_CATMULL_ROM, CURVE_TYPE_POLY)) {
    /* The conversion to Catmull Rom or Poly should never change the number of points, no matter
     * the source type (Bezier to Catmull Rom conversion cannot maintain the same shape anyway). */
    return false;
  }
  return true;
}

template<typename T>
static void scale_input_assign(const Span<T> src,
                               const int scale,
                               const int offset,
                               MutableSpan<T> dst)
{
  for (const int i : dst.index_range()) {
    dst[i] = src[i * scale + offset];
  }
}

/**
 * The Bezier control point and its handles become three control points on the NURBS curve,
 * so each attribute value is duplicated three times.
 */
template<typename T> static void bezier_generic_to_nurbs(const Span<T> src, MutableSpan<T> dst)
{
  for (const int i : src.index_range()) {
    dst[i * 3] = src[i];
    dst[i * 3 + 1] = src[i];
    dst[i * 3 + 2] = src[i];
  }
}

static void bezier_generic_to_nurbs(const GSpan src, GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    bezier_generic_to_nurbs(src.typed<T>(), dst.typed<T>());
  });
}

static void bezier_positions_to_nurbs(const Span<float3> src_positions,
                                      const Span<float3> src_handles_l,
                                      const Span<float3> src_handles_r,
                                      MutableSpan<float3> dst_positions)
{
  for (const int i : src_positions.index_range()) {
    dst_positions[i * 3] = src_handles_l[i];
    dst_positions[i * 3 + 1] = src_positions[i];
    dst_positions[i * 3 + 2] = src_handles_r[i];
  }
}

static void catmull_rom_to_bezier_handles(const Span<float3> src_positions,
                                          const bool cyclic,
                                          MutableSpan<float3> dst_handles_l,
                                          MutableSpan<float3> dst_handles_r)
{
  /* Catmull Rom curves are the same as Bezier curves with automatically defined handle positions.
   * This constant defines the portion of the distance between the next/previous points to use for
   * the length of the handles. */
  constexpr float handle_scale = 1.0f / 6.0f;

  if (src_positions.size() == 1) {
    dst_handles_l.first() = src_positions.first();
    dst_handles_r.first() = src_positions.first();
    return;
  }

  const float3 first_offset = cyclic ? src_positions[1] - src_positions.last() :
                                       src_positions[1] - src_positions[0];
  dst_handles_r.first() = src_positions.first() + first_offset * handle_scale;
  dst_handles_l.first() = src_positions.first() - first_offset * handle_scale;

  const float3 last_offset = cyclic ? src_positions.first() - src_positions.last(1) :
                                      src_positions.last() - src_positions.last(1);
  dst_handles_l.last() = src_positions.last() - last_offset * handle_scale;
  dst_handles_r.last() = src_positions.last() + last_offset * handle_scale;

  for (const int i : src_positions.index_range().drop_front(1).drop_back(1)) {
    const float3 left_offset = src_positions[i - 1] - src_positions[i + 1];
    dst_handles_l[i] = src_positions[i] + left_offset * handle_scale;

    const float3 right_offset = src_positions[i + 1] - src_positions[i - 1];
    dst_handles_r[i] = src_positions[i] + right_offset * handle_scale;
  }
}

static void catmull_rom_to_nurbs_positions(const Span<float3> src_positions,
                                           const bool cyclic,
                                           MutableSpan<float3> dst_positions)
{
  /* Convert the Catmull Rom position data to Bezier handles in order to reuse the Bezier to
   * NURBS positions assignment. If this becomes a bottleneck, this step could be avoided. */
  Array<float3, 32> bezier_handles_l(src_positions.size());
  Array<float3, 32> bezier_handles_r(src_positions.size());
  catmull_rom_to_bezier_handles(src_positions, cyclic, bezier_handles_l, bezier_handles_r);
  bezier_positions_to_nurbs(src_positions, bezier_handles_l, bezier_handles_r, dst_positions);
}

template<typename T>
static void nurbs_to_bezier_assign(const Span<T> src,
                                   const MutableSpan<T> dst,
                                   const KnotsMode knots_mode)
{
  switch (knots_mode) {
    case NURBS_KNOT_MODE_NORMAL:
      for (const int i : dst.index_range()) {
        dst[i] = src[(i + 1) % src.size()];
      }
      break;
    case NURBS_KNOT_MODE_ENDPOINT:
      for (const int i : dst.index_range().drop_back(1).drop_front(1)) {
        dst[i] = src[i + 1];
      }
      dst.first() = src.first();
      dst.last() = src.last();
      break;
    default:
      /* Every 3rd NURBS position (starting from index 1) should have its attributes transferred.
       */
      scale_input_assign<T>(src, 3, 1, dst);
  }
}

static void nurbs_to_bezier_assign(const GSpan src, const KnotsMode knots_mode, GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    nurbs_to_bezier_assign(src.typed<T>(), dst.typed<T>(), knots_mode);
  });
}

static Vector<float3> create_nurbs_to_bezier_handles(const Span<float3> nurbs_positions,
                                                     const KnotsMode knots_mode)
{
  const int nurbs_positions_num = nurbs_positions.size();
  Vector<float3> handle_positions;

  if (is_nurbs_to_bezier_one_to_one(knots_mode)) {
    const bool is_periodic = knots_mode == NURBS_KNOT_MODE_NORMAL;
    if (is_periodic) {
      handle_positions.append(nurbs_positions[1] +
                              ((nurbs_positions[0] - nurbs_positions[1]) / 3));
    }
    else {
      handle_positions.append(2 * nurbs_positions[0] - nurbs_positions[1]);
      handle_positions.append(nurbs_positions[1]);
    }

    /* Place Bezier handles on interior NURBS hull segments. Those handles can be either placed on
     * endpoints, midpoints or 1/3 of the distance of a hull segment. */
    const int segments_num = nurbs_positions_num - 1;
    const bool ignore_interior_segment = segments_num == 3 && is_periodic == false;
    if (ignore_interior_segment == false) {
      const float mid_offset = (float)(segments_num - 1) / 2.0f;
      for (const int i : IndexRange(1, segments_num - 2)) {
        /* Divisor can have values: 1, 2 or 3. */
        const int divisor = is_periodic ?
                                3 :
                                std::min(3, (int)(-std::abs(i - mid_offset) + mid_offset + 1.0f));
        const float3 &p1 = nurbs_positions[i];
        const float3 &p2 = nurbs_positions[i + 1];
        const float3 displacement = (p2 - p1) / divisor;
        const int num_handles_on_segment = divisor < 3 ? 1 : 2;
        for (int j : IndexRange(1, num_handles_on_segment)) {
          handle_positions.append(p1 + (displacement * j));
        }
      }
    }

    const int last_index = nurbs_positions_num - 1;
    if (is_periodic) {
      handle_positions.append(
          nurbs_positions[last_index - 1] +
          ((nurbs_positions[last_index] - nurbs_positions[last_index - 1]) / 3));
    }
    else {
      handle_positions.append(nurbs_positions[last_index - 1]);
      handle_positions.append(2 * nurbs_positions[last_index] - nurbs_positions[last_index - 1]);
    }
  }
  else {
    for (const int i : IndexRange(nurbs_positions_num)) {
      if (i % 3 == 1) {
        continue;
      }
      handle_positions.append(nurbs_positions[i]);
    }
    if (nurbs_positions_num % 3 == 1) {
      handle_positions.pop_last();
    }
    else if (nurbs_positions_num % 3 == 2) {
      const int last_index = nurbs_positions_num - 1;
      handle_positions.append(2 * nurbs_positions[last_index] - nurbs_positions[last_index - 1]);
    }
  }

  return handle_positions;
}

static void create_nurbs_to_bezier_positions(const Span<float3> nurbs_positions,
                                             const Span<float3> handle_positions,
                                             const KnotsMode knots_mode,
                                             MutableSpan<float3> bezier_positions)
{
  if (is_nurbs_to_bezier_one_to_one(knots_mode)) {
    for (const int i : bezier_positions.index_range()) {
      bezier_positions[i] = math::interpolate(
          handle_positions[i * 2], handle_positions[i * 2 + 1], 0.5f);
    }
  }
  else {
    /* Every 3rd NURBS position (starting from index 1) should be converted to Bezier position. */
    scale_input_assign(nurbs_positions, 3, 1, bezier_positions);
  }
}

static int to_bezier_size(const CurveType src_type,
                          const bool cyclic,
                          const KnotsMode knots_mode,
                          const int src_size)
{
  switch (src_type) {
    case CURVE_TYPE_NURBS: {
      if (is_nurbs_to_bezier_one_to_one(knots_mode)) {
        return cyclic ? src_size : src_size - 2;
      }
      return (src_size + 1) / 3;
    }
    default:
      return src_size;
  }
}

static int to_nurbs_size(const CurveType src_type, const int src_size)
{
  switch (src_type) {
    case CURVE_TYPE_BEZIER:
    case CURVE_TYPE_CATMULL_ROM:
      return src_size * 3;
    default:
      return src_size;
  }
}

static void retrieve_curve_sizes(const bke::CurvesGeometry &curves, MutableSpan<int> sizes)
{
  threading::parallel_for(curves.curves_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      sizes[i] = curves.points_for_curve(i).size();
    }
  });
}

struct GenericAttributes : NonCopyable, NonMovable {
  Vector<GSpan> src;
  Vector<GMutableSpan> dst;

  Vector<OutputAttribute> attributes;
};

static void retrieve_generic_point_attributes(const CurveComponent &src_component,
                                              CurveComponent &dst_component,
                                              GenericAttributes &attributes)
{
  src_component.attribute_foreach(
      [&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
        if (meta_data.domain != ATTR_DOMAIN_POINT) {
          /* Curve domain attributes are all copied directly to the result in one step. */
          return true;
        }
        if (src_component.attribute_is_builtin(id)) {
          if (!(id.is_named() && ELEM(id, "tilt", "radius"))) {
            return true;
          }
        }

        GVArray src_attribute = src_component.attribute_try_get_for_read(id, ATTR_DOMAIN_POINT);
        BLI_assert(src_attribute);
        attributes.src.append(src_attribute.get_internal_span());

        OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
            id, ATTR_DOMAIN_POINT, meta_data.data_type);
        attributes.dst.append(dst_attribute.as_span());
        attributes.attributes.append(std::move(dst_attribute));

        return true;
      });
}

static void convert_to_bezier(const CurveComponent &src_component,
                              const bke::CurvesGeometry &src_curves,
                              const IndexMask selection,
                              CurveComponent &dst_component,
                              bke::CurvesGeometry &dst_curves)
{
  const VArray<int8_t> src_knot_modes = src_curves.nurbs_knots_modes();
  const VArray<int8_t> src_types = src_curves.curve_types();
  const VArray<bool> src_cyclic = src_curves.cyclic();
  const Span<float3> src_positions = src_curves.positions();

  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  retrieve_curve_sizes(src_curves, dst_curves.offsets_for_write());
  threading::parallel_for(selection.index_range(), 1024, [&](IndexRange range) {
    for (const int i : selection.slice(range)) {
      dst_offsets[i] = to_bezier_size(
          CurveType(src_types[i]), src_cyclic[i], KnotsMode(src_knot_modes[i]), dst_offsets[i]);
    }
  });
  bke::curves::accumulate_counts_to_offsets(dst_offsets);
  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());

  GenericAttributes attributes;
  retrieve_generic_point_attributes(src_component, dst_component, attributes);

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();
  MutableSpan<float3> dst_handles_l = dst_curves.handle_positions_left_for_write();
  MutableSpan<float3> dst_handles_r = dst_curves.handle_positions_right_for_write();
  MutableSpan<int8_t> dst_types_l = dst_curves.handle_types_left_for_write();
  MutableSpan<int8_t> dst_types_r = dst_curves.handle_types_right_for_write();
  MutableSpan<float> dst_weights = dst_curves.nurbs_weights_for_write();

  auto catmull_rom_to_bezier = [&](IndexMask selection) {
    bke::curves::fill_points<int8_t>(dst_curves, selection, BEZIER_HANDLE_ALIGN, dst_types_l);
    bke::curves::fill_points<int8_t>(dst_curves, selection, BEZIER_HANDLE_ALIGN, dst_types_r);
    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_positions, dst_positions);

    threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
      for (const int i : selection.slice(range)) {
        const IndexRange src_points = src_curves.points_for_curve(i);
        const IndexRange dst_points = dst_curves.points_for_curve(i);
        catmull_rom_to_bezier_handles(src_positions.slice(src_points),
                                      src_cyclic[i],
                                      dst_handles_l.slice(dst_points),
                                      dst_handles_r.slice(dst_points));
      }
    });

    for (const int i : attributes.src.index_range()) {
      bke::curves::copy_point_data(
          src_curves, dst_curves, selection, attributes.src[i], attributes.dst[i]);
    }
  };

  auto poly_to_bezier = [&](IndexMask selection) {
    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_positions, dst_positions);
    bke::curves::fill_points<int8_t>(dst_curves, selection, BEZIER_HANDLE_VECTOR, dst_types_l);
    bke::curves::fill_points<int8_t>(dst_curves, selection, BEZIER_HANDLE_VECTOR, dst_types_r);
    dst_curves.calculate_bezier_auto_handles();
    for (const int i : attributes.src.index_range()) {
      bke::curves::copy_point_data(
          src_curves, dst_curves, selection, attributes.src[i], attributes.dst[i]);
    }
  };

  auto bezier_to_bezier = [&](IndexMask selection) {
    const VArray_Span<int8_t> src_types_l = src_curves.handle_types_left();
    const VArray_Span<int8_t> src_types_r = src_curves.handle_types_right();
    const Span<float3> src_handles_l = src_curves.handle_positions_left();
    const Span<float3> src_handles_r = src_curves.handle_positions_right();

    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_positions, dst_positions);
    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_handles_l, dst_handles_l);
    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_handles_r, dst_handles_r);
    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_types_l, dst_types_l);
    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_types_r, dst_types_r);

    dst_curves.calculate_bezier_auto_handles();

    for (const int i : attributes.src.index_range()) {
      bke::curves::copy_point_data(
          src_curves, dst_curves, selection, attributes.src[i], attributes.dst[i]);
    }
  };

  auto nurbs_to_bezier = [&](IndexMask selection) {
    bke::curves::fill_points<int8_t>(dst_curves, selection, BEZIER_HANDLE_ALIGN, dst_types_l);
    bke::curves::fill_points<int8_t>(dst_curves, selection, BEZIER_HANDLE_ALIGN, dst_types_r);
    bke::curves::fill_points<float>(dst_curves, selection, 0.0f, dst_weights);

    threading::parallel_for(selection.index_range(), 64, [&](IndexRange range) {
      for (const int i : selection.slice(range)) {
        const IndexRange src_points = src_curves.points_for_curve(i);
        const IndexRange dst_points = dst_curves.points_for_curve(i);
        const Span<float3> src_curve_positions = src_positions.slice(src_points);

        KnotsMode knots_mode = KnotsMode(src_knot_modes[i]);
        Span<float3> nurbs_positions = src_curve_positions;
        Vector<float3> nurbs_positions_vector;
        if (src_cyclic[i] && is_nurbs_to_bezier_one_to_one(knots_mode)) {
          /* For conversion treat this as periodic closed curve. Extend NURBS hull to first and
           * second point which will act as a skeleton for placing Bezier handles. */
          nurbs_positions_vector.extend(src_curve_positions);
          nurbs_positions_vector.append(src_curve_positions[0]);
          nurbs_positions_vector.append(src_curve_positions[1]);
          nurbs_positions = nurbs_positions_vector;
          knots_mode = NURBS_KNOT_MODE_NORMAL;
        }

        const Vector<float3> handle_positions = create_nurbs_to_bezier_handles(nurbs_positions,
                                                                               knots_mode);

        scale_input_assign(handle_positions.as_span(), 2, 0, dst_handles_l.slice(dst_points));
        scale_input_assign(handle_positions.as_span(), 2, 1, dst_handles_r.slice(dst_points));

        create_nurbs_to_bezier_positions(
            nurbs_positions, handle_positions, knots_mode, dst_positions.slice(dst_points));
      }
    });

    for (const int i_attribute : attributes.src.index_range()) {
      threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
        for (const int i : selection.slice(range)) {
          const IndexRange src_points = src_curves.points_for_curve(i);
          const IndexRange dst_points = dst_curves.points_for_curve(i);
          nurbs_to_bezier_assign(attributes.src[i_attribute].slice(src_points),
                                 KnotsMode(src_knot_modes[i]),
                                 attributes.dst[i_attribute].slice(dst_points));
        }
      });
    }
  };

  bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                     src_curves.curve_type_counts(),
                                     selection,
                                     catmull_rom_to_bezier,
                                     poly_to_bezier,
                                     bezier_to_bezier,
                                     nurbs_to_bezier);

  const Vector<IndexRange> unselected_ranges = selection.extract_ranges_invert(
      src_curves.curves_range());

  for (const int i : attributes.src.index_range()) {
    bke::curves::copy_point_data(
        src_curves, dst_curves, unselected_ranges, attributes.src[i], attributes.dst[i]);
  }

  for (OutputAttribute &attribute : attributes.attributes) {
    attribute.save();
  }
}

static void convert_to_nurbs(const CurveComponent &src_component,
                             const bke::CurvesGeometry &src_curves,
                             const IndexMask selection,
                             CurveComponent &dst_component,
                             bke::CurvesGeometry &dst_curves)
{
  const VArray<int8_t> src_types = src_curves.curve_types();
  const VArray<bool> src_cyclic = src_curves.cyclic();
  const Span<float3> src_positions = src_curves.positions();

  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();
  retrieve_curve_sizes(src_curves, dst_curves.offsets_for_write());
  threading::parallel_for(selection.index_range(), 1024, [&](IndexRange range) {
    for (const int i : selection.slice(range)) {
      dst_offsets[i] = to_nurbs_size(CurveType(src_types[i]), dst_offsets[i]);
    }
  });
  bke::curves::accumulate_counts_to_offsets(dst_offsets);
  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());

  GenericAttributes attributes;
  retrieve_generic_point_attributes(src_component, dst_component, attributes);

  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  auto fill_weights_if_necessary = [&](const IndexMask selection) {
    if (!src_curves.nurbs_weights().is_empty()) {
      bke::curves::fill_points(dst_curves, selection, 1.0f, dst_curves.nurbs_weights_for_write());
    }
  };

  auto catmull_rom_to_nurbs = [&](IndexMask selection) {
    dst_curves.nurbs_orders_for_write().fill_indices(selection, 4);
    dst_curves.nurbs_knots_modes_for_write().fill_indices(selection, NURBS_KNOT_MODE_BEZIER);
    fill_weights_if_necessary(selection);

    threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
      for (const int i : selection.slice(range)) {
        const IndexRange src_points = src_curves.points_for_curve(i);
        const IndexRange dst_points = dst_curves.points_for_curve(i);
        catmull_rom_to_nurbs_positions(
            src_positions.slice(src_points), src_cyclic[i], dst_positions.slice(dst_points));
      }
    });

    for (const int i_attribute : attributes.src.index_range()) {
      threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
        for (const int i : selection.slice(range)) {
          const IndexRange src_points = src_curves.points_for_curve(i);
          const IndexRange dst_points = dst_curves.points_for_curve(i);
          bezier_generic_to_nurbs(attributes.src[i_attribute].slice(src_points),
                                  attributes.dst[i_attribute].slice(dst_points));
        }
      });
    }
  };

  auto poly_to_nurbs = [&](IndexMask selection) {
    dst_curves.nurbs_orders_for_write().fill_indices(selection, 4);
    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_positions, dst_positions);
    fill_weights_if_necessary(selection);

    /* Avoid using "Endpoint" knots modes for cyclic curves, since it adds a sharp point at the
     * start/end. */
    if (src_cyclic.is_single()) {
      dst_curves.nurbs_knots_modes_for_write().fill_indices(
          selection,
          src_cyclic.get_internal_single() ? NURBS_KNOT_MODE_NORMAL : NURBS_KNOT_MODE_ENDPOINT);
    }
    else {
      VArray_Span<bool> cyclic{src_cyclic};
      MutableSpan<int8_t> knots_modes = dst_curves.nurbs_knots_modes_for_write();
      threading::parallel_for(selection.index_range(), 1024, [&](IndexRange range) {
        for (const int i : selection.slice(range)) {
          knots_modes[i] = cyclic[i] ? NURBS_KNOT_MODE_NORMAL : NURBS_KNOT_MODE_ENDPOINT;
        }
      });
    }

    for (const int i_attribute : attributes.src.index_range()) {
      bke::curves::copy_point_data(src_curves,
                                   dst_curves,
                                   selection,
                                   attributes.src[i_attribute],
                                   attributes.dst[i_attribute]);
    }
  };

  auto bezier_to_nurbs = [&](IndexMask selection) {
    const Span<float3> src_handles_l = src_curves.handle_positions_left();
    const Span<float3> src_handles_r = src_curves.handle_positions_right();

    dst_curves.nurbs_orders_for_write().fill_indices(selection, 4);
    dst_curves.nurbs_knots_modes_for_write().fill_indices(selection, NURBS_KNOT_MODE_BEZIER);
    fill_weights_if_necessary(selection);

    threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
      for (const int i : selection.slice(range)) {
        const IndexRange src_points = src_curves.points_for_curve(i);
        const IndexRange dst_points = dst_curves.points_for_curve(i);
        bezier_positions_to_nurbs(src_positions.slice(src_points),
                                  src_handles_l.slice(src_points),
                                  src_handles_r.slice(src_points),
                                  dst_positions.slice(dst_points));
      }
    });

    for (const int i_attribute : attributes.src.index_range()) {
      threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
        for (const int i : selection.slice(range)) {
          const IndexRange src_points = src_curves.points_for_curve(i);
          const IndexRange dst_points = dst_curves.points_for_curve(i);
          bezier_generic_to_nurbs(attributes.src[i_attribute].slice(src_points),
                                  attributes.dst[i_attribute].slice(dst_points));
        }
      });
    }
  };

  auto nurbs_to_nurbs = [&](IndexMask selection) {
    bke::curves::copy_point_data(src_curves, dst_curves, selection, src_positions, dst_positions);

    if (!src_curves.nurbs_weights().is_empty()) {
      bke::curves::copy_point_data(src_curves,
                                   dst_curves,
                                   selection,
                                   src_curves.nurbs_weights(),
                                   dst_curves.nurbs_weights_for_write());
    }

    for (const int i_attribute : attributes.src.index_range()) {
      bke::curves::copy_point_data(src_curves,
                                   dst_curves,
                                   selection,
                                   attributes.src[i_attribute],
                                   attributes.dst[i_attribute]);
    }
  };

  bke::curves::foreach_curve_by_type(src_curves.curve_types(),
                                     src_curves.curve_type_counts(),
                                     selection,
                                     catmull_rom_to_nurbs,
                                     poly_to_nurbs,
                                     bezier_to_nurbs,
                                     nurbs_to_nurbs);

  const Vector<IndexRange> unselected_ranges = selection.extract_ranges_invert(
      src_curves.curves_range());

  for (const int i : attributes.src.index_range()) {
    bke::curves::copy_point_data(
        src_curves, dst_curves, unselected_ranges, attributes.src[i], attributes.dst[i]);
  }

  for (OutputAttribute &attribute : attributes.attributes) {
    attribute.save();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSplineType &storage = node_storage(params.node());
  const CurveType dst_type = CurveType(storage.spline_type);

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curves()) {
      return;
    }
    const CurveComponent &src_component = *geometry_set.get_component_for_read<CurveComponent>();
    const Curves &src_curves_id = *src_component.get_for_read();
    const bke::CurvesGeometry &src_curves = bke::CurvesGeometry::wrap(src_curves_id.geometry);
    if (src_curves.is_single_type(dst_type)) {
      return;
    }

    GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_CURVE};
    const int domain_size = src_component.attribute_domain_num(ATTR_DOMAIN_CURVE);

    fn::FieldEvaluator evaluator{field_context, domain_size};
    evaluator.set_selection(selection_field);
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
    if (!conversion_can_change_point_num(dst_type)) {
      CurveComponent &dst_component = geometry_set.get_component_for_write<CurveComponent>();
      Curves &curves_id = *dst_component.get_for_write();
      bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
      curves.fill_curve_types(selection, dst_type);
      curves.remove_attributes_based_on_types();
      return;
    }

    Curves *dst_curves_id = bke::curves_new_nomain(0, src_curves.curves_num());
    bke::CurvesGeometry &dst_curves = bke::CurvesGeometry::wrap(dst_curves_id->geometry);
    CurveComponent dst_component;
    dst_component.replace(dst_curves_id, GeometryOwnershipType::Editable);
    /* Directly copy curve attributes, since they stay the same (except for curve types). */
    CustomData_copy(&src_curves.curve_data,
                    &dst_curves.curve_data,
                    CD_MASK_ALL,
                    CD_DUPLICATE,
                    src_curves.curves_num());

    dst_curves.fill_curve_types(selection, dst_type);

    switch (dst_type) {
      case CURVE_TYPE_CATMULL_ROM:
      case CURVE_TYPE_POLY:
        /* Converting to Catmull Rom curves or poly curves should be handled
         * above by the optimization to avoid changing the point count. */
        BLI_assert_unreachable();
        break;
      case CURVE_TYPE_BEZIER:
        convert_to_bezier(src_component, src_curves, selection, dst_component, dst_curves);
        break;
      case CURVE_TYPE_NURBS:
        convert_to_nurbs(src_component, src_curves, selection, dst_component, dst_curves);
        break;
    }

    geometry_set.replace_curves(dst_curves_id);
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_spline_type_cc

void register_node_type_geo_curve_spline_type()
{
  namespace file_ns = blender::nodes::node_geo_curve_spline_type_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_SPLINE_TYPE, "Set Spline Type", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype,
                    "NodeGeometryCurveSplineType",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;

  nodeRegisterType(&ntype);
}
