/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_instances.hh"

#include "GEO_realize_instances.hh"

#include "FN_multi_function_builder.hh"

namespace blender::nodes::node_geo_realize_instances_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Geometry")
      .description("Geometry whose instances are (partially) realized");
  b.add_output<decl::Geometry>("Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .field_on_all()
      .description("Which top-level instances to realize");
  b.add_input<decl::Bool>("Realize All")
      .default_value(true)
      .field_on_all()
      .description(
          "Realize all levels of nested instances for a top-level instances. Overrides the value "
          "of the Depth input");
  b.add_input<decl::Int>("Depth").default_value(0).min(0).field_on_all().description(
      "Number of levels of nested instances to realize for each top-level instance");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  if (!geometry_set.has_instances()) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry_set);

  Field<bool> realize_all_field = params.extract_input<Field<bool>>("Realize All");
  Field<int> depth_field = params.extract_input<Field<int>>("Depth");

  static auto depth_override = mf::build::SI2_SO<int, bool, int>(
      "depth_override", [](int depth, bool realize_all_field) {
        return realize_all_field ? geometry::VariedDepthOptions::MAX_DEPTH : std::max(depth, 0);
      });

  Field<int> depth_field_overridden(FieldOperation::from(
      depth_override, {std::move(depth_field), std::move(realize_all_field)}));

  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  static auto selection_override = mf::build::SI2_SO<int, bool, bool>(
      "selection_override",
      [](int depth_override, bool selection) { return depth_override == 0 ? false : selection; });

  Field<bool> selection_field_overrided(FieldOperation::from(
      selection_override, {depth_field_overridden, std::move(selection_field)}));

  const bke::Instances &instances = *geometry_set.get_instances();
  const bke::InstancesFieldContext field_context(instances);
  fn::FieldEvaluator evaluator(field_context, instances.instances_num());

  const int evaluated_depth_index = evaluator.add(depth_field_overridden);
  evaluator.set_selection(selection_field_overrided);
  evaluator.evaluate();

  geometry::VariedDepthOptions varied_depth_option;
  varied_depth_option.depths = evaluator.get_evaluated<int>(evaluated_depth_index);
  varied_depth_option.selection = evaluator.get_evaluated_selection_as_mask();

  geometry::RealizeInstancesOptions options;
  options.keep_original_ids = false;
  options.realize_instance_attributes = true;
  const NodeAttributeFilter attribute_filter = params.get_attribute_filter("Geometry");
  options.attribute_filter = attribute_filter;
  geometry::RealizeInstancesResult realize_result = geometry::realize_instances(
      geometry_set, options, varied_depth_option);
  for (const StringRef error : realize_result.errors) {
    params.error_message_add(NodeWarningType::Error, error);
  }
  realize_result.geometry.name = geometry_set.name;
  params.set_output("Geometry", std::move(realize_result.geometry));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeRealizeInstances", GEO_NODE_REALIZE_INSTANCES);
  ntype.ui_name = "Realize Instances";
  ntype.ui_description = "Convert instances into real geometry data";
  ntype.enum_name_legacy = "REALIZE_INSTANCES";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_realize_instances_cc
