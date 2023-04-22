/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "GEO_realize_instances.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_geo_realize_instances_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "legacy_behavior", 0, nullptr, ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bool legacy_behavior = params.node().custom1 & GEO_NODE_REALIZE_INSTANCES_LEGACY_BEHAVIOR;

  if (legacy_behavior) {
    params.error_message_add(
        NodeWarningType::Info,
        TIP_("This node uses legacy behavior with regards to attributes on "
             "instances. The behavior can be changed in the node properties in "
             "the side bar. In most cases the new behavior is the same for files created in "
             "Blender 3.0"));
  }

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(geometry_set);
  geometry::RealizeInstancesOptions options;
  options.keep_original_ids = legacy_behavior;
  options.realize_instance_attributes = !legacy_behavior;
  options.propagation_info = params.get_output_propagation_info("Geometry");
  geometry_set = geometry::realize_instances(geometry_set, options);
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_realize_instances_cc

void register_node_type_geo_realize_instances()
{
  namespace file_ns = blender::nodes::node_geo_realize_instances_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_REALIZE_INSTANCES, "Realize Instances", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons_ex = file_ns::node_layout;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
