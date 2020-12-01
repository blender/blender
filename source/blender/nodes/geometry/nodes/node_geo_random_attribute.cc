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

static bNodeSocketTemplate geo_node_random_attribute_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute")},
    {SOCK_VECTOR, N_("Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("Max"), 1.0f, 1.0f, 1.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("Max"), 1.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_INT, N_("Seed"), 0, 0, 0, 0, -10000, 10000},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_random_attribute_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_random_attribute_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void geo_node_random_attribute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_min_vector = (bNodeSocket *)BLI_findlink(&node->inputs, 2);
  bNodeSocket *sock_max_vector = sock_min_vector->next;
  bNodeSocket *sock_min_float = sock_max_vector->next;
  bNodeSocket *sock_max_float = sock_min_float->next;

  const int data_type = node->custom1;

  nodeSetSocketAvailability(sock_min_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_max_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_min_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_max_float, data_type == CD_PROP_FLOAT);
}

namespace blender::nodes {

static void randomize_attribute(FloatWriteAttribute &attribute,
                                float min,
                                float max,
                                RandomNumberGenerator &rng)
{
  MutableSpan<float> attribute_span = attribute.get_span();
  for (const int i : IndexRange(attribute.size())) {
    const float value = rng.get_float() * (max - min) + min;
    attribute_span[i] = value;
  }
  attribute.apply_span();
}

static void randomize_attribute(Float3WriteAttribute &attribute,
                                float3 min,
                                float3 max,
                                RandomNumberGenerator &rng)
{
  MutableSpan<float3> attribute_span = attribute.get_span();
  for (const int i : IndexRange(attribute.size())) {
    const float x = rng.get_float();
    const float y = rng.get_float();
    const float z = rng.get_float();
    const float3 value = float3(x, y, z) * (max - min) + min;
    attribute_span[i] = value;
  }
  attribute.apply_span();
}

static void randomize_attribute(GeometryComponent &component,
                                const GeoNodeExecParams &params,
                                RandomNumberGenerator &rng)
{
  const bNode &node = params.node();
  const CustomDataType data_type = static_cast<CustomDataType>(node.custom1);
  const AttributeDomain domain = static_cast<AttributeDomain>(node.custom2);
  const std::string attribute_name = params.get_input<std::string>("Attribute");
  if (attribute_name.empty()) {
    return;
  }

  WriteAttributePtr attribute = component.attribute_try_ensure_for_write(
      attribute_name, domain, data_type);
  if (!attribute) {
    return;
  }

  switch (data_type) {
    case CD_PROP_FLOAT: {
      FloatWriteAttribute float_attribute = std::move(attribute);
      const float min_value = params.get_input<float>("Min_001");
      const float max_value = params.get_input<float>("Max_001");
      randomize_attribute(float_attribute, min_value, max_value, rng);
      break;
    }
    case CD_PROP_FLOAT3: {
      Float3WriteAttribute float3_attribute = std::move(attribute);
      const float3 min_value = params.get_input<float3>("Min");
      const float3 max_value = params.get_input<float3>("Max");
      randomize_attribute(float3_attribute, min_value, max_value, rng);
      break;
    }
    default:
      break;
  }
}

static void geo_node_random_attribute_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const int seed = params.get_input<int>("Seed");

  if (geometry_set.has<MeshComponent>()) {
    RandomNumberGenerator rng;
    rng.seed_random(seed);
    randomize_attribute(geometry_set.get_component_for_write<MeshComponent>(), params, rng);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    RandomNumberGenerator rng;
    rng.seed_random(seed + 3245231);
    randomize_attribute(geometry_set.get_component_for_write<PointCloudComponent>(), params, rng);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_random_attribute()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_RANDOM_ATTRIBUTE, "Random Attribute", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(&ntype, geo_node_random_attribute_in, geo_node_random_attribute_out);
  node_type_init(&ntype, geo_node_random_attribute_init);
  node_type_update(&ntype, geo_node_random_attribute_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_random_attribute_exec;
  nodeRegisterType(&ntype);
}
