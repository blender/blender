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

#include "BLI_hash.h"
#include "BLI_rand.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

static bNodeSocketTemplate geo_node_attribute_randomize_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute")},
    {SOCK_VECTOR, N_("Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("Max"), 1.0f, 1.0f, 1.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("Max"), 1.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_INT, N_("Seed"), 0, 0, 0, 0, -10000, 10000},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_randomize_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_attribute_randomize_init(bNodeTree *UNUSED(tree), bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void geo_node_attribute_randomize_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_min_vector = (bNodeSocket *)BLI_findlink(&node->inputs, 2);
  bNodeSocket *sock_max_vector = sock_min_vector->next;
  bNodeSocket *sock_min_float = sock_max_vector->next;
  bNodeSocket *sock_max_float = sock_min_float->next;

  const CustomDataType data_type = static_cast<CustomDataType>(node->custom1);
  nodeSetSocketAvailability(sock_min_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_max_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_min_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_max_float, data_type == CD_PROP_FLOAT);
}

namespace blender::nodes {

/** Rehash to combine the seed with the "id" hash and a mutator for each dimension. */
static float noise_from_index_and_mutator(const int seed, const int hash, const int mutator)
{
  const int combined_hash = BLI_hash_int_3d(seed, hash, mutator);
  return BLI_hash_int_01(combined_hash);
}

/** Rehash to combine the seed with the "id" hash. */
static float noise_from_index(const int seed, const int hash)
{
  const int combined_hash = BLI_hash_int_2d(seed, hash);
  return BLI_hash_int_01(combined_hash);
}

static void randomize_attribute_bool(BooleanWriteAttribute attribute,
                                     Span<uint32_t> hashes,
                                     const int seed)
{
  MutableSpan<bool> attribute_span = attribute.get_span();
  for (const int i : IndexRange(attribute.size())) {
    const bool value = noise_from_index(seed, (int)hashes[i]) > 0.5f;
    attribute_span[i] = value;
  }
  attribute.apply_span();
}

static void randomize_attribute_float(FloatWriteAttribute attribute,
                                      const float min,
                                      const float max,
                                      Span<uint32_t> hashes,
                                      const int seed)
{
  MutableSpan<float> attribute_span = attribute.get_span();
  for (const int i : IndexRange(attribute.size())) {
    const float value = noise_from_index(seed, (int)hashes[i]) * (max - min) + min;
    attribute_span[i] = value;
  }
  attribute.apply_span();
}

static void randomize_attribute_float3(Float3WriteAttribute attribute,
                                       const float3 min,
                                       const float3 max,
                                       Span<uint32_t> hashes,
                                       const int seed)
{
  MutableSpan<float3> attribute_span = attribute.get_span();
  for (const int i : IndexRange(attribute.size())) {
    const float x = noise_from_index_and_mutator(seed, (int)hashes[i], 47);
    const float y = noise_from_index_and_mutator(seed, (int)hashes[i], 8);
    const float z = noise_from_index_and_mutator(seed, (int)hashes[i], 64);
    const float3 value = float3(x, y, z) * (max - min) + min;
    attribute_span[i] = value;
  }
  attribute.apply_span();
}

Array<uint32_t> get_geometry_element_ids_as_uints(const GeometryComponent &component,
                                                  const AttributeDomain domain)
{
  const int domain_size = component.attribute_domain_size(domain);

  /* Hash the reserved name attribute "id" as a (hopefully) stable seed for each point. */
  ReadAttributePtr hash_attribute = component.attribute_try_get_for_read("id", domain);
  Array<uint32_t> hashes(domain_size);
  if (hash_attribute) {
    BLI_assert(hashes.size() == hash_attribute->size());
    const CPPType &cpp_type = hash_attribute->cpp_type();
    fn::GSpan items = hash_attribute->get_span();
    for (const int i : hashes.index_range()) {
      hashes[i] = cpp_type.hash(items[i]);
    }
  }
  else {
    /* If there is no "id" attribute for per-point variation, just create it here. */
    RandomNumberGenerator rng(0);
    for (const int i : hashes.index_range()) {
      hashes[i] = rng.get_uint32();
    }
  }

  return hashes;
}

static void randomize_attribute(GeometryComponent &component,
                                const GeoNodeExecParams &params,
                                const int seed)
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

  Array<uint32_t> hashes = get_geometry_element_ids_as_uints(component, domain);

  switch (data_type) {
    case CD_PROP_FLOAT: {
      const float min_value = params.get_input<float>("Min_001");
      const float max_value = params.get_input<float>("Max_001");
      randomize_attribute_float(*attribute, min_value, max_value, hashes, seed);
      break;
    }
    case CD_PROP_FLOAT3: {
      const float3 min_value = params.get_input<float3>("Min");
      const float3 max_value = params.get_input<float3>("Max");
      randomize_attribute_float3(*attribute, min_value, max_value, hashes, seed);
      break;
    }
    case CD_PROP_BOOL: {
      randomize_attribute_bool(*attribute, hashes, seed);
      break;
    }
    default:
      break;
  }

  attribute.save();
}

static void geo_node_random_attribute_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const int seed = params.get_input<int>("Seed");

  if (geometry_set.has<MeshComponent>()) {
    randomize_attribute(geometry_set.get_component_for_write<MeshComponent>(), params, seed);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    randomize_attribute(geometry_set.get_component_for_write<PointCloudComponent>(), params, seed);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_randomize()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_RANDOMIZE, "Attribute Randomize", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(
      &ntype, geo_node_attribute_randomize_in, geo_node_attribute_randomize_out);
  node_type_init(&ntype, geo_node_attribute_randomize_init);
  node_type_update(&ntype, geo_node_attribute_randomize_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_random_attribute_exec;
  nodeRegisterType(&ntype);
}
