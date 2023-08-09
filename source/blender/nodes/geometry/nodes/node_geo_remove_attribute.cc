/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_socket_search_link.hh"

#include <fmt/format.h>

namespace blender::nodes::node_geo_remove_attribute_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::String>("Name").is_attribute_name();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const std::string name = params.extract_input<std::string>("Name");
  if (name.empty()) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }
  if (!bke::allow_procedural_attribute_access(name)) {
    params.error_message_add(NodeWarningType::Info, TIP_(bke::no_procedural_access_message));
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  std::atomic<bool> attribute_exists = false;
  std::atomic<bool> cannot_delete = false;

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                               GeometryComponent::Type::PointCloud,
                                               GeometryComponent::Type::Curve,
                                               GeometryComponent::Type::Instance})
    {
      if (geometry_set.has(type)) {
        /* First check if the attribute exists before getting write access,
         * to avoid potentially expensive unnecessary copies. */
        const GeometryComponent &read_only_component = *geometry_set.get_component(type);
        if (read_only_component.attributes()->contains(name)) {
          attribute_exists = true;
        }
        else {
          continue;
        }

        GeometryComponent &component = geometry_set.get_component_for_write(type);
        if (!component.attributes_for_write()->remove(name)) {
          cannot_delete = true;
        }
      }
    }
  });

  if (attribute_exists && !cannot_delete) {
    params.used_named_attribute(name, NamedAttributeUsage::Remove);
  }

  if (!attribute_exists) {
    const std::string message = fmt::format(TIP_("Attribute does not exist: \"{}\""), name);
    params.error_message_add(NodeWarningType::Warning, message);
  }
  if (cannot_delete) {
    const std::string message = fmt::format(TIP_("Cannot delete built-in attribute: \"{}\""),
                                            name);
    params.error_message_add(NodeWarningType::Warning, message);
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_REMOVE_ATTRIBUTE, "Remove Named Attribute", NODE_CLASS_ATTRIBUTE);
  ntype.declare = node_declare;
  blender::bke::node_type_size(&ntype, 170, 100, 700);
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_remove_attribute_cc
