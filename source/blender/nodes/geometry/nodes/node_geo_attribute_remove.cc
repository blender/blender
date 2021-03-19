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

static bNodeSocketTemplate geo_node_attribute_remove_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING,
     N_("Attribute"),
     0.0f,
     0.0f,
     0.0f,
     1.0f,
     -1.0f,
     1.0f,
     PROP_NONE,
     SOCK_MULTI_INPUT},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_remove_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

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

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    remove_attribute(
        geometry_set.get_component_for_write<MeshComponent>(), params, attribute_names);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    remove_attribute(
        geometry_set.get_component_for_write<PointCloudComponent>(), params, attribute_names);
  }

  params.set_output("Geometry", geometry_set);
}
}  // namespace blender::nodes

void register_node_type_geo_attribute_remove()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_REMOVE, "Attribute Remove", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(&ntype, geo_node_attribute_remove_in, geo_node_attribute_remove_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_remove_exec;
  nodeRegisterType(&ntype);
}
