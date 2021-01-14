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

#include "BLI_rand.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

static bNodeSocketTemplate geo_node_attribute_fill_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute")},
    {SOCK_VECTOR, N_("Value"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("Value"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_RGBA, N_("Value"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_BOOLEAN, N_("Value"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_fill_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_attribute_fill_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void geo_node_attribute_fill_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *socket_value_vector = (bNodeSocket *)BLI_findlink(&node->inputs, 2);
  bNodeSocket *socket_value_float = socket_value_vector->next;
  bNodeSocket *socket_value_color4f = socket_value_float->next;
  bNodeSocket *socket_value_boolean = socket_value_color4f->next;

  const CustomDataType data_type = static_cast<CustomDataType>(node->custom1);

  nodeSetSocketAvailability(socket_value_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_value_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_value_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(socket_value_boolean, data_type == CD_PROP_BOOL);
}

namespace blender::nodes {

static void fill_attribute(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const CustomDataType data_type = static_cast<CustomDataType>(node.custom1);
  const AttributeDomain domain = static_cast<AttributeDomain>(node.custom2);
  const std::string attribute_name = params.get_input<std::string>("Attribute");
  if (attribute_name.empty()) {
    return;
  }

  OutputAttributePtr attribute = component.attribute_try_get_for_output(
      attribute_name, domain, data_type);
  if (!attribute) {
    return;
  }

  switch (data_type) {
    case CD_PROP_FLOAT: {
      const float value = params.get_input<float>("Value_001");
      MutableSpan<float> attribute_span = attribute->get_span_for_write_only<float>();
      attribute_span.fill(value);
      break;
    }
    case CD_PROP_FLOAT3: {
      const float3 value = params.get_input<float3>("Value");
      MutableSpan<float3> attribute_span = attribute->get_span_for_write_only<float3>();
      attribute_span.fill(value);
      break;
    }
    case CD_PROP_COLOR: {
      const Color4f value = params.get_input<Color4f>("Value_002");
      MutableSpan<Color4f> attribute_span = attribute->get_span_for_write_only<Color4f>();
      attribute_span.fill(value);
      break;
    }
    case CD_PROP_BOOL: {
      const bool value = params.get_input<bool>("Value_003");
      MutableSpan<bool> attribute_span = attribute->get_span_for_write_only<bool>();
      attribute_span.fill(value);
      break;
    }
    default:
      break;
  }

  attribute.apply_span_and_save();
}

static void geo_node_attribute_fill_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (geometry_set.has<MeshComponent>()) {
    fill_attribute(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    fill_attribute(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_fill()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_ATTRIBUTE_FILL, "Attribute Fill", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(&ntype, geo_node_attribute_fill_in, geo_node_attribute_fill_out);
  node_type_init(&ntype, geo_node_attribute_fill_init);
  node_type_update(&ntype, geo_node_attribute_fill_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_fill_exec;
  nodeRegisterType(&ntype);
}
