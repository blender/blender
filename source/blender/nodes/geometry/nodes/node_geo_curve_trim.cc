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
 */

#include "BKE_spline.hh"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

using blender::attribute_math::mix2;

namespace blender::nodes {

static void geo_node_curve_trim_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Float>("Start").min(0.0f).max(1.0f).subtype(PROP_FACTOR).supports_field();
  b.add_input<decl::Float>("End")
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .supports_field();
  b.add_input<decl::Float>("Start", "Start_001").min(0.0f).subtype(PROP_DISTANCE).supports_field();
  b.add_input<decl::Float>("End", "End_001")
      .min(0.0f)
      .default_value(1.0f)
      .subtype(PROP_DISTANCE)
      .supports_field();
  b.add_output<decl::Geometry>("Curve");
}

static void geo_node_curve_trim_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_curve_trim_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveTrim *data = (NodeGeometryCurveTrim *)MEM_callocN(sizeof(NodeGeometryCurveTrim),
                                                                     __func__);

  data->mode = GEO_NODE_CURVE_SAMPLE_FACTOR;
  node->storage = data;
}

static void geo_node_curve_trim_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeGeometryCurveTrim &node_storage = *(NodeGeometryCurveTrim *)node->storage;
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)node_storage.mode;

  bNodeSocket *start_fac = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *end_fac = start_fac->next;
  bNodeSocket *start_len = end_fac->next;
  bNodeSocket *end_len = start_len->next;

  nodeSetSocketAvailability(start_fac, mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
  nodeSetSocketAvailability(end_fac, mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
  nodeSetSocketAvailability(start_len, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
  nodeSetSocketAvailability(end_len, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
}

struct TrimLocation {
  /* Control point index at the start side of the trim location. */
  int left_index;
  /* Control point index at the end of the trim location's segment. */
  int right_index;
  /* The factor between the left and right indices. */
  float factor;
};

template<typename T>
static void shift_slice_to_start(MutableSpan<T> data, const int start_index, const int size)
{
  BLI_assert(start_index + size - 1 <= data.size());
  memmove(data.data(), &data[start_index], sizeof(T) * size);
}

/* Shift slice to start of span and modifies start and end data. */
template<typename T>
static void linear_trim_data(const TrimLocation &start,
                             const TrimLocation &end,
                             MutableSpan<T> data)
{
  const int size = end.right_index - start.left_index + 1;

  if (start.left_index > 0) {
    shift_slice_to_start<T>(data, start.left_index, size);
  }

  const T start_data = mix2<T>(start.factor, data.first(), data[1]);
  const T end_data = mix2<T>(end.factor, data[size - 2], data[size - 1]);

  data.first() = start_data;
  data[size - 1] = end_data;
}

/**
 * Identical operation as #linear_trim_data, but copy data to a new #MutableSpan rather than
 * modifying the original data.
 */
template<typename T>
static void linear_trim_to_output_data(const TrimLocation &start,
                                       const TrimLocation &end,
                                       Span<T> src,
                                       MutableSpan<T> dst)
{
  const int size = end.right_index - start.left_index + 1;

  const T start_data = mix2<T>(start.factor, src[start.left_index], src[start.right_index]);
  const T end_data = mix2<T>(end.factor, src[end.left_index], src[end.right_index]);

  dst.copy_from(src.slice(start.left_index, size));
  dst.first() = start_data;
  dst.last() = end_data;
}

/* Look up the control points to the left and right of factor, and get the factor between them. */
static TrimLocation lookup_control_point_position(const Spline::LookupResult &lookup,
                                                  const BezierSpline &spline)
{
  Span<int> offsets = spline.control_point_offsets();

  const int *offset = std::lower_bound(offsets.begin(), offsets.end(), lookup.evaluated_index);
  const int index = offset - offsets.begin();

  const int left = offsets[index] > lookup.evaluated_index ? index - 1 : index;
  const int right = left == (spline.size() - 1) ? 0 : left + 1;

  const float offset_in_segment = lookup.evaluated_index + lookup.factor - offsets[left];
  const int segment_eval_size = offsets[left + 1] - offsets[left];
  const float factor = std::clamp(offset_in_segment / segment_eval_size, 0.0f, 1.0f);

  return {left, right, factor};
}

static void trim_poly_spline(Spline &spline,
                             const Spline::LookupResult &start_lookup,
                             const Spline::LookupResult &end_lookup)
{
  /* Poly splines have a 1 to 1 mapping between control points and evaluated points. */
  const TrimLocation start = {
      start_lookup.evaluated_index, start_lookup.next_evaluated_index, start_lookup.factor};
  const TrimLocation end = {
      end_lookup.evaluated_index, end_lookup.next_evaluated_index, end_lookup.factor};

  const int size = end.right_index - start.left_index + 1;

  linear_trim_data<float3>(start, end, spline.positions());
  linear_trim_data<float>(start, end, spline.radii());
  linear_trim_data<float>(start, end, spline.tilts());

  spline.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &UNUSED(meta_data)) {
        std::optional<GMutableSpan> src = spline.attributes.get_for_write(attribute_id);
        BLI_assert(src);
        attribute_math::convert_to_static_type(src->type(), [&](auto dummy) {
          using T = decltype(dummy);
          linear_trim_data<T>(start, end, src->typed<T>());
        });
        return true;
      },
      ATTR_DOMAIN_POINT);

  spline.resize(size);
}

/**
 * Trim NURB splines by converting to a poly spline.
 */
static PolySpline trim_nurbs_spline(const Spline &spline,
                                    const Spline::LookupResult &start_lookup,
                                    const Spline::LookupResult &end_lookup)
{
  /* Since this outputs a poly spline, the evaluated indices are the control point indices. */
  const TrimLocation start = {
      start_lookup.evaluated_index, start_lookup.next_evaluated_index, start_lookup.factor};
  const TrimLocation end = {
      end_lookup.evaluated_index, end_lookup.next_evaluated_index, end_lookup.factor};

  const int size = end.right_index - start.left_index + 1;

  /* Create poly spline and copy trimmed data to it. */
  PolySpline new_spline;
  new_spline.resize(size);

  /* Copy generic attribute data. */
  spline.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        std::optional<GSpan> src = spline.attributes.get_for_read(attribute_id);
        BLI_assert(src);
        if (!new_spline.attributes.create(attribute_id, meta_data.data_type)) {
          BLI_assert_unreachable();
          return false;
        }
        std::optional<GMutableSpan> dst = new_spline.attributes.get_for_write(attribute_id);
        BLI_assert(dst);

        attribute_math::convert_to_static_type(src->type(), [&](auto dummy) {
          using T = decltype(dummy);
          GVArray_Typed<T> eval_data = spline.interpolate_to_evaluated<T>(src->typed<T>());
          linear_trim_to_output_data<T>(
              start, end, eval_data->get_internal_span(), dst->typed<T>());
        });
        return true;
      },
      ATTR_DOMAIN_POINT);

  linear_trim_to_output_data<float3>(
      start, end, spline.evaluated_positions(), new_spline.positions());

  GVArray_Typed<float> evaluated_radii = spline.interpolate_to_evaluated(spline.radii());
  linear_trim_to_output_data<float>(
      start, end, evaluated_radii->get_internal_span(), new_spline.radii());

  GVArray_Typed<float> evaluated_tilts = spline.interpolate_to_evaluated(spline.tilts());
  linear_trim_to_output_data<float>(
      start, end, evaluated_tilts->get_internal_span(), new_spline.tilts());

  return new_spline;
}

/**
 * Trim Bezier splines by adjusting the first and last handles
 * and control points to maintain the original shape.
 */
static void trim_bezier_spline(Spline &spline,
                               const Spline::LookupResult &start_lookup,
                               const Spline::LookupResult &end_lookup)
{
  BezierSpline &bezier_spline = static_cast<BezierSpline &>(spline);

  const TrimLocation start = lookup_control_point_position(start_lookup, bezier_spline);
  TrimLocation end = lookup_control_point_position(end_lookup, bezier_spline);

  const Span<int> control_offsets = bezier_spline.control_point_offsets();

  /* The number of control points in the resulting spline. */
  const int size = end.right_index - start.left_index + 1;

  /* Trim the spline attributes. Done before end.factor recalculation as it needs
   * the original end.factor value. */
  linear_trim_data<float>(start, end, bezier_spline.radii());
  linear_trim_data<float>(start, end, bezier_spline.tilts());
  spline.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &UNUSED(meta_data)) {
        std::optional<GMutableSpan> src = spline.attributes.get_for_write(attribute_id);
        BLI_assert(src);
        attribute_math::convert_to_static_type(src->type(), [&](auto dummy) {
          using T = decltype(dummy);
          linear_trim_data<T>(start, end, src->typed<T>());
        });
        return true;
      },
      ATTR_DOMAIN_POINT);

  /* Recalculate end.factor if the size is two, because the adjustment in the
   * position of the control point of the spline to the left of the new end point will change the
   * factor between them. */
  if (size == 2) {
    if (start_lookup.factor == 1.0f) {
      end.factor = 0.0f;
    }
    else {
      end.factor = (end_lookup.evaluated_index + end_lookup.factor -
                    (start_lookup.evaluated_index + start_lookup.factor)) /
                   (control_offsets[end.right_index] -
                    (start_lookup.evaluated_index + start_lookup.factor));
      end.factor = std::clamp(end.factor, 0.0f, 1.0f);
    }
  }

  BezierSpline::InsertResult start_point = bezier_spline.calculate_segment_insertion(
      start.left_index, start.right_index, start.factor);

  /* Update the start control point parameters so they are used calculating the new end point. */
  bezier_spline.positions()[start.left_index] = start_point.position;
  bezier_spline.handle_positions_right()[start.left_index] = start_point.right_handle;
  bezier_spline.handle_positions_left()[start.right_index] = start_point.handle_next;

  const BezierSpline::InsertResult end_point = bezier_spline.calculate_segment_insertion(
      end.left_index, end.right_index, end.factor);

  /* If size is two, then the start point right handle needs to change to reflect the end point
   * previous handle update. */
  if (size == 2) {
    start_point.right_handle = end_point.handle_prev;
  }

  /* Shift control point position data to start at beginning of array. */
  if (start.left_index > 0) {
    shift_slice_to_start(bezier_spline.positions(), start.left_index, size);
    shift_slice_to_start(bezier_spline.handle_positions_left(), start.left_index, size);
    shift_slice_to_start(bezier_spline.handle_positions_right(), start.left_index, size);
  }

  bezier_spline.positions().first() = start_point.position;
  bezier_spline.positions()[size - 1] = end_point.position;

  bezier_spline.handle_positions_left().first() = start_point.left_handle;
  bezier_spline.handle_positions_left()[size - 1] = end_point.left_handle;

  bezier_spline.handle_positions_right().first() = start_point.right_handle;
  bezier_spline.handle_positions_right()[size - 1] = end_point.right_handle;

  /* If there is at least one control point between the endpoints, update the control
   * point handle to the right of the start point and to the left of the end point. */
  if (size > 2) {
    bezier_spline.handle_positions_left()[start.right_index - start.left_index] =
        start_point.handle_next;
    bezier_spline.handle_positions_right()[end.left_index - start.left_index] =
        end_point.handle_prev;
  }

  bezier_spline.resize(size);
}

static void trim_spline(SplinePtr &spline,
                        const Spline::LookupResult start,
                        const Spline::LookupResult end)
{
  switch (spline->type()) {
    case Spline::Type::Bezier:
      trim_bezier_spline(*spline, start, end);
      break;
    case Spline::Type::Poly:
      trim_poly_spline(*spline, start, end);
      break;
    case Spline::Type::NURBS:
      spline = std::make_unique<PolySpline>(trim_nurbs_spline(*spline, start, end));
      break;
  }
  spline->mark_cache_invalid();
}

template<typename T>
static void to_single_point_data(const TrimLocation &trim, MutableSpan<T> data)
{
  data.first() = mix2<T>(trim.factor, data[trim.left_index], data[trim.right_index]);
}
template<typename T>
static void to_single_point_data(const TrimLocation &trim, Span<T> src, MutableSpan<T> dst)
{
  dst.first() = mix2<T>(trim.factor, src[trim.left_index], src[trim.right_index]);
}

static void to_single_point_bezier(Spline &spline, const Spline::LookupResult &lookup)
{
  BezierSpline &bezier = static_cast<BezierSpline &>(spline);

  const TrimLocation trim = lookup_control_point_position(lookup, bezier);

  const BezierSpline::InsertResult new_point = bezier.calculate_segment_insertion(
      trim.left_index, trim.right_index, trim.factor);
  bezier.positions().first() = new_point.position;
  bezier.handle_types_left().first() = BezierSpline::HandleType::Free;
  bezier.handle_types_right().first() = BezierSpline::HandleType::Free;
  bezier.handle_positions_left().first() = new_point.left_handle;
  bezier.handle_positions_right().first() = new_point.right_handle;

  to_single_point_data<float>(trim, bezier.radii());
  to_single_point_data<float>(trim, bezier.tilts());
  spline.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &UNUSED(meta_data)) {
        std::optional<GMutableSpan> data = spline.attributes.get_for_write(attribute_id);
        attribute_math::convert_to_static_type(data->type(), [&](auto dummy) {
          using T = decltype(dummy);
          to_single_point_data<T>(trim, data->typed<T>());
        });
        return true;
      },
      ATTR_DOMAIN_POINT);
  spline.resize(1);
}

static void to_single_point_poly(Spline &spline, const Spline::LookupResult &lookup)
{
  const TrimLocation trim{lookup.evaluated_index, lookup.next_evaluated_index, lookup.factor};

  to_single_point_data<float3>(trim, spline.positions());
  to_single_point_data<float>(trim, spline.radii());
  to_single_point_data<float>(trim, spline.tilts());
  spline.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &UNUSED(meta_data)) {
        std::optional<GMutableSpan> data = spline.attributes.get_for_write(attribute_id);
        attribute_math::convert_to_static_type(data->type(), [&](auto dummy) {
          using T = decltype(dummy);
          to_single_point_data<T>(trim, data->typed<T>());
        });
        return true;
      },
      ATTR_DOMAIN_POINT);
  spline.resize(1);
}

static PolySpline to_single_point_nurbs(const Spline &spline, const Spline::LookupResult &lookup)
{
  /* Since this outputs a poly spline, the evaluated indices are the control point indices. */
  const TrimLocation trim{lookup.evaluated_index, lookup.next_evaluated_index, lookup.factor};

  /* Create poly spline and copy trimmed data to it. */
  PolySpline new_spline;
  new_spline.resize(1);

  spline.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        new_spline.attributes.create(attribute_id, meta_data.data_type);
        std::optional<GSpan> src = spline.attributes.get_for_read(attribute_id);
        std::optional<GMutableSpan> dst = new_spline.attributes.get_for_write(attribute_id);
        attribute_math::convert_to_static_type(src->type(), [&](auto dummy) {
          using T = decltype(dummy);
          GVArray_Typed<T> eval_data = spline.interpolate_to_evaluated<T>(src->typed<T>());
          to_single_point_data<T>(trim, eval_data->get_internal_span(), dst->typed<T>());
        });
        return true;
      },
      ATTR_DOMAIN_POINT);

  to_single_point_data<float3>(trim, spline.evaluated_positions(), new_spline.positions());

  GVArray_Typed<float> evaluated_radii = spline.interpolate_to_evaluated(spline.radii());
  to_single_point_data<float>(trim, evaluated_radii->get_internal_span(), new_spline.radii());

  GVArray_Typed<float> evaluated_tilts = spline.interpolate_to_evaluated(spline.tilts());
  to_single_point_data<float>(trim, evaluated_tilts->get_internal_span(), new_spline.tilts());

  return new_spline;
}

static void to_single_point_spline(SplinePtr &spline, const Spline::LookupResult &lookup)
{
  switch (spline->type()) {
    case Spline::Type::Bezier:
      to_single_point_bezier(*spline, lookup);
      break;
    case Spline::Type::Poly:
      to_single_point_poly(*spline, lookup);
      break;
    case Spline::Type::NURBS:
      spline = std::make_unique<PolySpline>(to_single_point_nurbs(*spline, lookup));
      break;
  }
}

static void geometry_set_curve_trim(GeometrySet &geometry_set,
                                    const GeometryNodeCurveSampleMode mode,
                                    Field<float> &start_field,
                                    Field<float> &end_field)
{
  if (!geometry_set.has_curve()) {
    return;
  }

  CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_CURVE};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_CURVE);

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.add(start_field);
  evaluator.add(end_field);
  evaluator.evaluate();
  const blender::VArray<float> &starts = evaluator.get_evaluated<float>(0);
  const blender::VArray<float> &ends = evaluator.get_evaluated<float>(1);

  CurveEval &curve = *geometry_set.get_curve_for_write();
  MutableSpan<SplinePtr> splines = curve.splines();

  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      SplinePtr &spline = splines[i];

      /* Currently trimming cyclic splines is not supported. It could be in the future though. */
      if (spline->is_cyclic()) {
        continue;
      }

      if (spline->evaluated_edges_size() == 0) {
        continue;
      }

      const float length = spline->length();
      if (length == 0.0f) {
        continue;
      }

      const float start = starts[i];
      const float end = ends[i];

      /* When the start and end samples are reversed, instead of implicitly reversing the spline
       * or switching the parameters, create a single point spline with the end sample point. */
      if (end <= start) {
        if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
          to_single_point_spline(spline,
                                 spline->lookup_evaluated_length(std::clamp(start, 0.0f, length)));
        }
        else {
          to_single_point_spline(spline,
                                 spline->lookup_evaluated_factor(std::clamp(start, 0.0f, 1.0f)));
        }
        continue;
      }

      if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
        trim_spline(spline,
                    spline->lookup_evaluated_length(std::clamp(start, 0.0f, length)),
                    spline->lookup_evaluated_length(std::clamp(end, 0.0f, length)));
      }
      else {
        trim_spline(spline,
                    spline->lookup_evaluated_factor(std::clamp(start, 0.0f, 1.0f)),
                    spline->lookup_evaluated_factor(std::clamp(end, 0.0f, 1.0f)));
      }
    }
  });
}

static void geo_node_curve_trim_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveTrim &node_storage = *(NodeGeometryCurveTrim *)params.node().storage;
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)node_storage.mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  if (mode == GEO_NODE_CURVE_SAMPLE_FACTOR) {
    Field<float> start_field = params.extract_input<Field<float>>("Start");
    Field<float> end_field = params.extract_input<Field<float>>("End");
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      geometry_set_curve_trim(geometry_set, mode, start_field, end_field);
    });
  }
  else if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
    Field<float> start_field = params.extract_input<Field<float>>("Start_001");
    Field<float> end_field = params.extract_input<Field<float>>("End_001");
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      geometry_set_curve_trim(geometry_set, mode, start_field, end_field);
    });
  }

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_trim()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TRIM_CURVE, "Trim Curve", NODE_CLASS_GEOMETRY, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_trim_exec;
  ntype.draw_buttons = blender::nodes::geo_node_curve_trim_layout;
  ntype.declare = blender::nodes::geo_node_curve_trim_declare;
  node_type_storage(
      &ntype, "NodeGeometryCurveTrim", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, blender::nodes::geo_node_curve_trim_init);
  node_type_update(&ntype, blender::nodes::geo_node_curve_trim_update);
  nodeRegisterType(&ntype);
}
