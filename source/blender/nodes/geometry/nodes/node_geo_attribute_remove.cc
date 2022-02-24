/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_attribute_remove_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Attribute")).multi_input();
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void remove_attribute(GeometryComponent &component,
                             GeoNodeExecParams &params,
                             Span<std::string> attribute_names)
{
  for (std::string attribute_name : attribute_names) {
    if (attribute_name.empty()) {
      continue;
    }

    if (!component.attribute_try_delete(attribute_name)) {
      params.error_message_add(NodeWarningType::Error,
                               TIP_("Cannot delete attribute with name \"") + attribute_name +
                                   "\"");
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Vector<std::string> attribute_names = params.extract_multi_input<std::string>("Attribute");

  for (const GeometryComponentType type : {GEO_COMPONENT_TYPE_MESH,
                                           GEO_COMPONENT_TYPE_POINT_CLOUD,
                                           GEO_COMPONENT_TYPE_CURVE,
                                           GEO_COMPONENT_TYPE_INSTANCES}) {
    if (geometry_set.has(type)) {
      remove_attribute(geometry_set.get_component_for_write(type), params, attribute_names);
    }
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes::node_geo_attribute_remove_cc

void register_node_type_geo_attribute_remove()
{
  namespace file_ns = blender::nodes::node_geo_attribute_remove_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_ATTRIBUTE_REMOVE, "Attribute Remove", NODE_CLASS_ATTRIBUTE);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
