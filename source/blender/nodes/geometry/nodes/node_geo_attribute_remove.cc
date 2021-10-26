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

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_attribute_remove_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::String>("Attribute").multi_input();
  b.add_output<decl::Geometry>("Geometry");
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

static void geo_node_attribute_remove_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Vector<std::string> attribute_names = params.extract_multi_input<std::string>("Attribute");

  if (geometry_set.has<MeshComponent>()) {
    remove_attribute(
        geometry_set.get_component_for_write<MeshComponent>(), params, attribute_names);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    remove_attribute(
        geometry_set.get_component_for_write<PointCloudComponent>(), params, attribute_names);
  }
  if (geometry_set.has<CurveComponent>()) {
    remove_attribute(
        geometry_set.get_component_for_write<CurveComponent>(), params, attribute_names);
  }
  if (geometry_set.has<InstancesComponent>()) {
    remove_attribute(
        geometry_set.get_component_for_write<InstancesComponent>(), params, attribute_names);
  }

  params.set_output("Geometry", geometry_set);
}
}  // namespace blender::nodes

void register_node_type_geo_attribute_remove()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_REMOVE, "Attribute Remove", NODE_CLASS_ATTRIBUTE, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_remove_exec;
  ntype.declare = blender::nodes::geo_node_attribute_remove_declare;
  nodeRegisterType(&ntype);
}
