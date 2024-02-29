/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_position_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Vector>("Position").implicit_field_on_all(implicit_field_inputs::position);
  b.add_input<decl::Vector>("Offset").subtype(PROP_TRANSLATION).field_on_all();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static const auto &get_add_fn()
{
  static const auto fn = mf::build::SI2_SO<float3, float3, float3>(
      "Add",
      [](const float3 a, const float3 b) { return a + b; },
      mf::build::exec_presets::AllSpanOrSingle());
  return fn;
}

static const auto &get_sub_fn()
{
  static const auto fn = mf::build::SI2_SO<float3, float3, float3>(
      "Add",
      [](const float3 a, const float3 b) { return a - b; },
      mf::build::exec_presets::AllSpanOrSingle());
  return fn;
}

static void set_points_position(bke::MutableAttributeAccessor attributes,
                                const fn::FieldContext &field_context,
                                const Field<bool> &selection_field,
                                const Field<float3> &position_field)
{
  bke::try_capture_field_on_geometry(attributes,
                                     field_context,
                                     "position",
                                     bke::AttrDomain::Point,
                                     selection_field,
                                     position_field);
}

static void set_curves_position(bke::CurvesGeometry &curves,
                                const fn::FieldContext &field_context,
                                const Field<bool> &selection_field,
                                const Field<float3> &position_field)
{
  MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (attributes.contains("handle_right") && attributes.contains("handle_left")) {
    fn::Field<float3> delta(fn::FieldOperation::Create(
        get_sub_fn(), {position_field, bke::AttributeFieldInput::Create<float3>("position")}));
    for (const StringRef name : {"handle_left", "handle_right"}) {
      bke::try_capture_field_on_geometry(
          attributes,
          field_context,
          name,
          bke::AttrDomain::Point,
          selection_field,
          Field<float3>(fn::FieldOperation::Create(
              get_add_fn(), {bke::AttributeFieldInput::Create<float3>(name), delta})));
    }
  }
  set_points_position(attributes, field_context, selection_field, position_field);
  curves.calculate_bezier_auto_handles();
}

static void set_position_in_grease_pencil(GreasePencil &grease_pencil,
                                          const Field<bool> &selection_field,
                                          const Field<float3> &position_field)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = bke::greasepencil::get_eval_grease_pencil_layer_drawing_for_write(
        grease_pencil, layer_index);
    if (drawing == nullptr || drawing->strokes().points_num() == 0) {
      continue;
    }
    set_curves_position(
        drawing->strokes_for_write(),
        bke::GreasePencilLayerFieldContext(grease_pencil, bke::AttrDomain::Point, layer_index),
        selection_field,
        position_field);
    drawing->tag_positions_changed();
  }
}

static void set_instances_position(bke::Instances &instances,
                                   const Field<bool> &selection_field,
                                   const Field<float3> &position_field)
{
  const bke::InstancesFieldContext context(instances);
  fn::FieldEvaluator evaluator(context, instances.instances_num());
  evaluator.set_selection(selection_field);

  /* Use a temporary array for the output to avoid potentially reading from freed memory if
   * retrieving the transforms has to make a mutable copy (then we can't depend on the user count
   * of the original read-only data). */
  Array<float3> result(instances.instances_num());
  evaluator.add_with_destination(position_field, result.as_mutable_span());
  evaluator.evaluate();

  MutableSpan<float4x4> transforms = instances.transforms_for_write();
  threading::parallel_for(transforms.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      transforms[i].location() = result[i];
    }
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const fn::Field<float3> position_field(
      fn::FieldOperation::Create(get_add_fn(),
                                 {params.extract_input<Field<float3>>("Position"),
                                  params.extract_input<Field<float3>>("Offset")}));

  if (Mesh *mesh = geometry.get_mesh_for_write()) {
    set_points_position(mesh->attributes_for_write(),
                        bke::MeshFieldContext(*mesh, bke::AttrDomain::Point),
                        selection_field,
                        position_field);
  }
  if (PointCloud *point_cloud = geometry.get_pointcloud_for_write()) {
    set_points_position(point_cloud->attributes_for_write(),
                        bke::PointCloudFieldContext(*point_cloud),
                        selection_field,
                        position_field);
  }
  if (Curves *curves_id = geometry.get_curves_for_write()) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    set_curves_position(curves,
                        bke::CurvesFieldContext(curves, bke::AttrDomain::Point),
                        selection_field,
                        position_field);
  }
  if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
    set_position_in_grease_pencil(*grease_pencil, selection_field, position_field);
  }
  if (bke::Instances *instances = geometry.get_instances_for_write()) {
    set_instances_position(*instances, selection_field, position_field);
  }

  params.set_output("Geometry", std::move(geometry));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_POSITION, "Set Position", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_position_cc
