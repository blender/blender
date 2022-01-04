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

namespace blender::nodes::node_geo_legacy_curve_spline_type_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve"));
  b.add_input<decl::String>(N_("Selection"));
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
  std::unique_ptr<BezierSpline> output = std::make_unique<BezierSpline>();
  output->resize(input.size() / 3);
  scale_input_assign<float3>(input.positions(), 3, 1, output->positions());
  scale_input_assign<float3>(input.positions(), 3, 0, output->handle_positions_left());
  scale_input_assign<float3>(input.positions(), 3, 2, output->handle_positions_right());
  scale_input_assign<float>(input.radii(), 3, 2, output->radii());
  scale_input_assign<float>(input.tilts(), 3, 2, output->tilts());
  output->handle_types_left().fill(BezierSpline::HandleType::Align);
  output->handle_types_right().fill(BezierSpline::HandleType::Align);
  output->set_resolution(nurbs_spline.resolution());
  Spline::copy_base_settings(input, *output);
  output->attributes.reallocate(output->size());
  copy_attributes(input, *output, [](GSpan src, GMutableSpan dst) {
    attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
      using T = decltype(dummy);
      scale_input_assign<T>(src.typed<T>(), 3, 1, dst.typed<T>());
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
      if (input.size() < 6) {
        params.error_message_add(
            NodeWarningType::Info,
            TIP_("NURBS must have minimum of 6 points for Bezier Conversion"));
        return input.copy();
      }
      else {
        if (input.size() % 3 != 0) {
          params.error_message_add(NodeWarningType::Info,
                                   TIP_("NURBS must have multiples of 3 points for full Bezier "
                                        "conversion, curve truncated"));
        }
        return nurbs_to_bezier(input);
      }
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
  const NodeGeometryCurveSplineType *storage =
      (const NodeGeometryCurveSplineType *)params.node().storage;
  const GeometryNodeSplineType output_type = (const GeometryNodeSplineType)storage->spline_type;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  geometry_set = geometry::realize_instances_legacy(geometry_set);
  if (!geometry_set.has_curve()) {
    params.set_output("Curve", geometry_set);
    return;
  }

  const CurveComponent *curve_component = geometry_set.get_component_for_read<CurveComponent>();
  const CurveEval &curve = *curve_component->get_for_read();

  const std::string selection_name = params.extract_input<std::string>("Selection");
  VArray<bool> selection = curve_component->attribute_get_for_read(
      selection_name, ATTR_DOMAIN_CURVE, true);

  std::unique_ptr<CurveEval> new_curve = std::make_unique<CurveEval>();
  for (const int i : curve.splines().index_range()) {
    if (selection[i]) {
      switch (output_type) {
        case GEO_NODE_SPLINE_TYPE_POLY:
          new_curve->add_spline(convert_to_poly_spline(*curve.splines()[i]));
          break;
        case GEO_NODE_SPLINE_TYPE_BEZIER:
          new_curve->add_spline(convert_to_bezier(*curve.splines()[i], params));
          break;
        case GEO_NODE_SPLINE_TYPE_NURBS:
          new_curve->add_spline(convert_to_nurbs(*curve.splines()[i]));
          break;
      }
    }
    else {
      new_curve->add_spline(curve.splines()[i]->copy());
    }
  }

  new_curve->attributes = curve.attributes;
  params.set_output("Curve", GeometrySet::create_with_curve(new_curve.release()));
}

}  // namespace blender::nodes::node_geo_legacy_curve_spline_type_cc

void register_node_type_geo_legacy_curve_spline_type()
{
  namespace file_ns = blender::nodes::node_geo_legacy_curve_spline_type_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_CURVE_SPLINE_TYPE, "Set Spline Type", NODE_CLASS_GEOMETRY);
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
