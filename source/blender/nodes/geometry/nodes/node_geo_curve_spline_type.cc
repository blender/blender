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

  data->spline_type = GEO_NODE_SPLINE_TYPE_POLY;
  node->storage = data;
}

template<class T>
static void scale_input_assign(const Span<T> input,
                               const int scale,
                               const int offset,
                               const MutableSpan<T> r_output)
{
  for (const int i : IndexRange(r_output.size())) {
    r_output[i] = input[i * scale + offset];
  }
}

template<class T>
static void scale_output_assign(const Span<T> input,
                                const int scale,
                                const int offset,
                                const MutableSpan<T> &r_output)
{
  for (const int i : IndexRange(input.size())) {
    r_output[i * scale + offset] = input[i];
  }
}

template<class T>
static void nurbs_to_bezier_assign(const Span<T> input,
                                   const MutableSpan<T> r_output,
                                   const NURBSpline::KnotsMode knotsMode)
{
  const int input_size = input.size();
  const int output_size = r_output.size();

  switch (knotsMode) {
    case NURBSpline::KnotsMode::Bezier:
      scale_input_assign<T>(input, 3, 1, r_output);
      break;
    case NURBSpline::KnotsMode::Normal:
      for (const int i : IndexRange(output_size)) {
        r_output[i] = input[(i + 1) % input_size];
      }
      break;
    case NURBSpline::KnotsMode::EndPoint:
      for (const int i : IndexRange(1, output_size - 2)) {
        r_output[i] = input[i + 1];
      }
      r_output.first() = input.first();
      r_output.last() = input.last();
      break;
  }
}

template<typename CopyFn>
static void copy_attributes(const Spline &input_spline, Spline &output_spline, CopyFn copy_fn)
{
  input_spline.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        std::optional<GSpan> src = input_spline.attributes.get_for_read(attribute_id);
        BLI_assert(src);
        if (!output_spline.attributes.create(attribute_id, meta_data.data_type)) {
          BLI_assert_unreachable();
          return false;
        }
        std::optional<GMutableSpan> dst = output_spline.attributes.get_for_write(attribute_id);
        if (!dst) {
          BLI_assert_unreachable();
          return false;
        }

        copy_fn(*src, *dst);

        return true;
      },
      ATTR_DOMAIN_POINT);
}

static Vector<float3> create_nurbs_to_bezier_handles(const Span<float3> nurbs_positions,
                                                     const NURBSpline::KnotsMode knots_mode)
{
  const int nurbs_positions_size = nurbs_positions.size();
  Vector<float3> handle_positions;
  if (knots_mode == NURBSpline::KnotsMode::Bezier) {
    for (const int i : IndexRange(nurbs_positions_size)) {
      if (i % 3 == 1) {
        continue;
      }
      handle_positions.append(nurbs_positions[i]);
    }
    if (nurbs_positions_size % 3 == 1) {
      handle_positions.pop_last();
    }
    else if (nurbs_positions_size % 3 == 2) {
      const int last_index = nurbs_positions_size - 1;
      handle_positions.append(2 * nurbs_positions[last_index] - nurbs_positions[last_index - 1]);
    }
  }
  else {
    const bool is_periodic = knots_mode == NURBSpline::KnotsMode::Normal;
    if (is_periodic) {
      handle_positions.append(nurbs_positions[1] +
                              ((nurbs_positions[0] - nurbs_positions[1]) / 3));
    }
    else {
      handle_positions.append(2 * nurbs_positions[0] - nurbs_positions[1]);
      handle_positions.append(nurbs_positions[1]);
    }
    const int segments_size = nurbs_positions_size - 1;
    const bool ignore_interior_segment = segments_size == 3 && is_periodic == false;
    if (ignore_interior_segment == false) {
      const float mid_offset = (float)(segments_size - 1) / 2.0f;
      for (const int i : IndexRange(1, segments_size - 2)) {
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
    const int last_index = nurbs_positions_size - 1;
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
  return handle_positions;
}

static Array<float3> create_nurbs_to_bezier_positions(const Span<float3> nurbs_positions,
                                                      const Span<float3> handle_positions,
                                                      const NURBSpline::KnotsMode knots_mode)
{
  if (knots_mode == NURBSpline::KnotsMode::Bezier) {
    /* Every third NURBS position (starting from index 1) should be converted to Bezier position */
    const int scale = 3;
    const int offset = 1;
    Array<float3> bezier_positions((nurbs_positions.size() + offset) / scale);
    scale_input_assign(nurbs_positions, scale, offset, bezier_positions.as_mutable_span());
    return bezier_positions;
  }

  Array<float3> bezier_positions(handle_positions.size() / 2);
  for (const int i : IndexRange(bezier_positions.size())) {
    bezier_positions[i] = math::interpolate(
        handle_positions[i * 2], handle_positions[i * 2 + 1], 0.5f);
  }
  return bezier_positions;
}

static SplinePtr convert_to_poly_spline(const Spline &input)
{
  std::unique_ptr<PolySpline> output = std::make_unique<PolySpline>();
  output->resize(input.positions().size());
  output->positions().copy_from(input.positions());
  output->radii().copy_from(input.radii());
  output->tilts().copy_from(input.tilts());
  Spline::copy_base_settings(input, *output);
  output->attributes = input.attributes;
  return output;
}

static SplinePtr poly_to_nurbs(const Spline &input)
{
  std::unique_ptr<NURBSpline> output = std::make_unique<NURBSpline>();
  output->resize(input.positions().size());
  output->positions().copy_from(input.positions());
  output->radii().copy_from(input.radii());
  output->tilts().copy_from(input.tilts());
  output->weights().fill(1.0f);
  output->set_resolution(12);
  output->set_order(4);
  Spline::copy_base_settings(input, *output);
  output->knots_mode = NURBSpline::KnotsMode::Bezier;
  output->attributes = input.attributes;
  return output;
}

static SplinePtr bezier_to_nurbs(const Spline &input)
{
  const BezierSpline &bezier_spline = static_cast<const BezierSpline &>(input);
  std::unique_ptr<NURBSpline> output = std::make_unique<NURBSpline>();
  output->resize(input.size() * 3);

  scale_output_assign(bezier_spline.handle_positions_left(), 3, 0, output->positions());
  scale_output_assign(input.radii(), 3, 0, output->radii());
  scale_output_assign(input.tilts(), 3, 0, output->tilts());

  scale_output_assign(bezier_spline.positions(), 3, 1, output->positions());
  scale_output_assign(input.radii(), 3, 1, output->radii());
  scale_output_assign(input.tilts(), 3, 1, output->tilts());

  scale_output_assign(bezier_spline.handle_positions_right(), 3, 2, output->positions());
  scale_output_assign(input.radii(), 3, 2, output->radii());
  scale_output_assign(input.tilts(), 3, 2, output->tilts());

  Spline::copy_base_settings(input, *output);
  output->weights().fill(1.0f);
  output->set_resolution(12);
  output->set_order(4);
  output->set_cyclic(input.is_cyclic());
  output->knots_mode = NURBSpline::KnotsMode::Bezier;
  output->attributes.reallocate(output->size());
  copy_attributes(input, *output, [](GSpan src, GMutableSpan dst) {
    attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
      using T = decltype(dummy);
      scale_output_assign<T>(src.typed<T>(), 3, 0, dst.typed<T>());
      scale_output_assign<T>(src.typed<T>(), 3, 1, dst.typed<T>());
      scale_output_assign<T>(src.typed<T>(), 3, 2, dst.typed<T>());
    });
  });
  return output;
}

static SplinePtr poly_to_bezier(const Spline &input)
{
  std::unique_ptr<BezierSpline> output = std::make_unique<BezierSpline>();
  output->resize(input.size());
  output->positions().copy_from(input.positions());
  output->radii().copy_from(input.radii());
  output->tilts().copy_from(input.tilts());
  output->handle_types_left().fill(BezierSpline::HandleType::Vector);
  output->handle_types_right().fill(BezierSpline::HandleType::Vector);
  output->set_resolution(12);
  Spline::copy_base_settings(input, *output);
  output->attributes = input.attributes;
  return output;
}

static SplinePtr nurbs_to_bezier(const Spline &input)
{
  const NURBSpline &nurbs_spline = static_cast<const NURBSpline &>(input);
  Span<float3> nurbs_positions;
  Vector<float3> nurbs_positions_vector;
  NURBSpline::KnotsMode knots_mode;
  if (nurbs_spline.is_cyclic()) {
    nurbs_positions_vector = nurbs_spline.positions();
    nurbs_positions_vector.append(nurbs_spline.positions()[0]);
    nurbs_positions_vector.append(nurbs_spline.positions()[1]);
    nurbs_positions = nurbs_positions_vector;
    knots_mode = NURBSpline::KnotsMode::Normal;
  }
  else {
    nurbs_positions = nurbs_spline.positions();
    knots_mode = nurbs_spline.knots_mode;
  }
  const Vector<float3> handle_positions = create_nurbs_to_bezier_handles(nurbs_positions,
                                                                         knots_mode);
  BLI_assert(handle_positions.size() % 2 == 0);
  const Array<float3> bezier_positions = create_nurbs_to_bezier_positions(
      nurbs_positions, handle_positions.as_span(), knots_mode);
  BLI_assert(handle_positions.size() == bezier_positions.size() * 2);

  std::unique_ptr<BezierSpline> output = std::make_unique<BezierSpline>();
  output->resize(bezier_positions.size());
  output->positions().copy_from(bezier_positions);
  nurbs_to_bezier_assign(nurbs_spline.radii(), output->radii(), knots_mode);
  nurbs_to_bezier_assign(nurbs_spline.tilts(), output->tilts(), knots_mode);
  scale_input_assign(handle_positions.as_span(), 2, 0, output->handle_positions_left());
  scale_input_assign(handle_positions.as_span(), 2, 1, output->handle_positions_right());
  output->handle_types_left().fill(BezierSpline::HandleType::Align);
  output->handle_types_right().fill(BezierSpline::HandleType::Align);
  output->set_resolution(nurbs_spline.resolution());
  Spline::copy_base_settings(nurbs_spline, *output);
  output->attributes.reallocate(output->size());
  copy_attributes(nurbs_spline, *output, [knots_mode](GSpan src, GMutableSpan dst) {
    attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
      using T = decltype(dummy);
      nurbs_to_bezier_assign(src.typed<T>(), dst.typed<T>(), knots_mode);
    });
  });
  return output;
}

static SplinePtr convert_to_bezier(const Spline &input, GeoNodeExecParams params)
{
  switch (input.type()) {
    case Spline::Type::Bezier:
      return input.copy();
    case Spline::Type::Poly:
      return poly_to_bezier(input);
    case Spline::Type::NURBS:
      if (input.size() < 4) {
        params.error_message_add(
            NodeWarningType::Info,
            TIP_("NURBS must have minimum of 4 points for Bezier Conversion"));
        return input.copy();
      }
      return nurbs_to_bezier(input);
  }
  BLI_assert_unreachable();
  return {};
}

static SplinePtr convert_to_nurbs(const Spline &input)
{
  switch (input.type()) {
    case Spline::Type::NURBS:
      return input.copy();
    case Spline::Type::Bezier:
      return bezier_to_nurbs(input);
    case Spline::Type::Poly:
      return poly_to_nurbs(input);
  }
  BLI_assert_unreachable();
  return {};
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSplineType &storage = node_storage(params.node());
  const GeometryNodeSplineType output_type = (const GeometryNodeSplineType)storage.spline_type;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curve()) {
      return;
    }

    const CurveComponent *curve_component = geometry_set.get_component_for_read<CurveComponent>();
    const CurveEval &curve = *curve_component->get_for_read();
    GeometryComponentFieldContext field_context{*curve_component, ATTR_DOMAIN_CURVE};
    const int domain_size = curve_component->attribute_domain_size(ATTR_DOMAIN_CURVE);

    fn::FieldEvaluator selection_evaluator{field_context, domain_size};
    selection_evaluator.add(selection_field);
    selection_evaluator.evaluate();
    const VArray<bool> &selection = selection_evaluator.get_evaluated<bool>(0);

    std::unique_ptr<CurveEval> new_curve = std::make_unique<CurveEval>();
    new_curve->resize(curve.splines().size());

    threading::parallel_for(curve.splines().index_range(), 512, [&](IndexRange range) {
      for (const int i : range) {
        if (selection[i]) {
          switch (output_type) {
            case GEO_NODE_SPLINE_TYPE_POLY:
              new_curve->splines()[i] = convert_to_poly_spline(*curve.splines()[i]);
              break;
            case GEO_NODE_SPLINE_TYPE_BEZIER:
              new_curve->splines()[i] = convert_to_bezier(*curve.splines()[i], params);
              break;
            case GEO_NODE_SPLINE_TYPE_NURBS:
              new_curve->splines()[i] = convert_to_nurbs(*curve.splines()[i]);
              break;
          }
        }
        else {
          new_curve->splines()[i] = curve.splines()[i]->copy();
        }
      }
    });
    new_curve->attributes = curve.attributes;
    geometry_set.replace_curve(new_curve.release());
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
