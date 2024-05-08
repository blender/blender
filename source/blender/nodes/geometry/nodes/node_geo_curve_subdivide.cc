/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "GEO_subdivide_curves.hh"

#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_subdivide_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(
      {GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Int>("Cuts").default_value(1).min(0).max(1000).field_on_all().description(
      "The number of control points to create on the segment following each point");
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static Curves *subdivide_curves(const Curves &src_curves_id,
                                Field<int> &cuts_field,
                                const AnonymousAttributePropagationInfo &propagation_info)
{
  const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();

  const bke::CurvesFieldContext field_context{src_curves, AttrDomain::Point};
  fn::FieldEvaluator evaluator{field_context, src_curves.points_num()};
  evaluator.add(cuts_field);
  evaluator.evaluate();
  const VArray<int> cuts = evaluator.get_evaluated<int>(0);

  if (cuts.is_single() && cuts.get_internal_single() < 1) {
    return nullptr;
  }

  bke::CurvesGeometry dst_curves = geometry::subdivide_curves(
      src_curves, src_curves.curves_range(), cuts, propagation_info);

  Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
  bke::curves_copy_parameters(src_curves_id, *dst_curves_id);
  return dst_curves_id;
}

static void subdivide_grease_pencil_curves(
    GreasePencil &grease_pencil,
    Field<int> &cuts_field,
    const AnonymousAttributePropagationInfo &propagation_info)
{
  using namespace bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = grease_pencil.get_eval_drawing(*grease_pencil.layer(layer_index));

    if (drawing == nullptr) {
      continue;
    }

    const bke::CurvesGeometry &src_curves = drawing->strokes();
    const bke::GreasePencilLayerFieldContext field_context{
        grease_pencil, AttrDomain::Point, layer_index};

    fn::FieldEvaluator evaluator{field_context, src_curves.points_num()};
    evaluator.add(cuts_field);
    evaluator.evaluate();
    const VArray<int> cuts = evaluator.get_evaluated<int>(0);

    if (cuts.is_single() && cuts.get_internal_single() < 1) {
      continue;
    }

    bke::CurvesGeometry dst_curves = geometry::subdivide_curves(
        src_curves, src_curves.curves_range(), cuts, propagation_info);

    drawing->strokes_for_write() = std::move(dst_curves);
    drawing->tag_topology_changed();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<int> cuts_field = params.extract_input<Field<int>>("Cuts");

  GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry_set);
  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Curve");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curves()) {
      const Curves &src_curves_id = *geometry_set.get_curves();
      Curves *dst_curves_id = subdivide_curves(src_curves_id, cuts_field, propagation_info);
      if (dst_curves_id) {
        geometry_set.replace_curves(dst_curves_id);
      }
    }
    if (geometry_set.has_grease_pencil()) {
      GreasePencil &grease_pencil = *geometry_set.get_grease_pencil_for_write();
      subdivide_grease_pencil_curves(grease_pencil, cuts_field, propagation_info);
    }
  });
  params.set_output("Curve", geometry_set);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SUBDIVIDE_CURVE, "Subdivide Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_subdivide_cc
