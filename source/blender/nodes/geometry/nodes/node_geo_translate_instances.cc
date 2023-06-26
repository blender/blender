/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BLI_math_matrix.hh"

#include "BKE_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_translate_instances_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Instances").only_instances();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Vector>("Translation").subtype(PROP_TRANSLATION).field_on_all();
  b.add_input<decl::Bool>("Local Space").default_value(true).field_on_all();
  b.add_output<decl::Geometry>("Instances").propagate_all();
}

static void translate_instances(GeoNodeExecParams &params, bke::Instances &instances)
{
  const bke::InstancesFieldContext context{instances};
  fn::FieldEvaluator evaluator{context, instances.instances_num()};
  evaluator.set_selection(params.extract_input<Field<bool>>("Selection"));
  evaluator.add(params.extract_input<Field<float3>>("Translation"));
  evaluator.add(params.extract_input<Field<bool>>("Local Space"));
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> translations = evaluator.get_evaluated<float3>(0);
  const VArray<bool> local_spaces = evaluator.get_evaluated<bool>(1);

  MutableSpan<float4x4> transforms = instances.transforms();

  selection.foreach_index(GrainSize(1024), [&](const int64_t i) {
    if (local_spaces[i]) {
      transforms[i] *= math::from_location<float4x4>(translations[i]);
    }
    else {
      transforms[i].location() += translations[i];
    }
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Instances");
  if (bke::Instances *instances = geometry_set.get_instances_for_write()) {
    translate_instances(params, *instances);
  }
  params.set_output("Instances", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_translate_instances_cc

void register_node_type_geo_translate_instances()
{
  namespace file_ns = blender::nodes::node_geo_translate_instances_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_TRANSLATE_INSTANCES, "Translate Instances", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
