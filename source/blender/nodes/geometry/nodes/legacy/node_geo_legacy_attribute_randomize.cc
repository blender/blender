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

#include "BLI_hash.h"
#include "BLI_rand.hh"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

Array<uint32_t> get_geometry_element_ids_as_uints(const GeometryComponent &component,
                                                  const AttributeDomain domain)
{
  const int domain_size = component.attribute_domain_size(domain);

  /* Hash the reserved name attribute "id" as a (hopefully) stable seed for each point. */
  GVArray hash_attribute = component.attribute_try_get_for_read("id", domain);
  Array<uint32_t> hashes(domain_size);
  if (hash_attribute) {
    BLI_assert(hashes.size() == hash_attribute.size());
    const CPPType &cpp_type = hash_attribute.type();
    BLI_assert(cpp_type.is_hashable());
    GVArray_GSpan items{hash_attribute};
    threading::parallel_for(hashes.index_range(), 512, [&](IndexRange range) {
      for (const int i : range) {
        hashes[i] = cpp_type.hash(items[i]);
      }
    });
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

}  // namespace blender::nodes

namespace blender::nodes::node_geo_legacy_attribute_randomize_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Attribute"));
  b.add_input<decl::Vector>(N_("Min"));
  b.add_input<decl::Vector>(N_("Max")).default_value({1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Min"), "Min_001");
  b.add_input<decl::Float>(N_("Max"), "Max_001").default_value(1.0f);
  b.add_input<decl::Int>(N_("Min"), "Min_002").min(-100000).max(100000);
  b.add_input<decl::Int>(N_("Max"), "Max_002").default_value(100).min(-100000).max(100000);
  b.add_input<decl::Int>(N_("Seed")).min(-10000).max(10000);
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeRandomize *data = MEM_cnew<NodeAttributeRandomize>(__func__);
  data->data_type = CD_PROP_FLOAT;
  data->domain = ATTR_DOMAIN_POINT;
  data->operation = GEO_NODE_ATTRIBUTE_RANDOMIZE_REPLACE_CREATE;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock_min_vector = (bNodeSocket *)BLI_findlink(&node->inputs, 2);
  bNodeSocket *sock_max_vector = sock_min_vector->next;
  bNodeSocket *sock_min_float = sock_max_vector->next;
  bNodeSocket *sock_max_float = sock_min_float->next;
  bNodeSocket *sock_min_int = sock_max_float->next;
  bNodeSocket *sock_max_int = sock_min_int->next;

  const NodeAttributeRandomize &storage = *(const NodeAttributeRandomize *)node->storage;
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);
  nodeSetSocketAvailability(ntree, sock_min_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, sock_max_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, sock_min_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, sock_max_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, sock_min_int, data_type == CD_PROP_INT32);
  nodeSetSocketAvailability(ntree, sock_max_int, data_type == CD_PROP_INT32);
}

template<typename T>
T random_value_in_range(const uint32_t id, const uint32_t seed, const T min, const T max);

template<>
inline float random_value_in_range(const uint32_t id,
                                   const uint32_t seed,
                                   const float min,
                                   const float max)
{
  return BLI_hash_int_2d_to_float(id, seed) * (max - min) + min;
}

template<>
inline int random_value_in_range(const uint32_t id,
                                 const uint32_t seed,
                                 const int min,
                                 const int max)
{
  return round_fl_to_int(
      random_value_in_range<float>(id, seed, static_cast<float>(min), static_cast<float>(max)));
}

template<>
inline float3 random_value_in_range(const uint32_t id,
                                    const uint32_t seed,
                                    const float3 min,
                                    const float3 max)
{
  const float x = BLI_hash_int_3d_to_float(seed, id, 435109);
  const float y = BLI_hash_int_3d_to_float(seed, id, 380867);
  const float z = BLI_hash_int_3d_to_float(seed, id, 1059217);

  return float3(x, y, z) * (max - min) + min;
}

template<typename T>
static void randomize_attribute(MutableSpan<T> span,
                                const T min,
                                const T max,
                                Span<uint32_t> ids,
                                const uint32_t seed,
                                const GeometryNodeAttributeRandomizeMode operation)
{
  /* The operations could be templated too, but it doesn't make the code much shorter. */
  switch (operation) {
    case GEO_NODE_ATTRIBUTE_RANDOMIZE_REPLACE_CREATE:
      threading::parallel_for(span.index_range(), 512, [&](IndexRange range) {
        for (const int i : range) {
          const T random_value = random_value_in_range<T>(ids[i], seed, min, max);
          span[i] = random_value;
        }
      });
      break;
    case GEO_NODE_ATTRIBUTE_RANDOMIZE_ADD:
      threading::parallel_for(span.index_range(), 512, [&](IndexRange range) {
        for (const int i : range) {
          const T random_value = random_value_in_range<T>(ids[i], seed, min, max);
          span[i] = span[i] + random_value;
        }
      });
      break;
    case GEO_NODE_ATTRIBUTE_RANDOMIZE_SUBTRACT:
      threading::parallel_for(span.index_range(), 512, [&](IndexRange range) {
        for (const int i : range) {
          const T random_value = random_value_in_range<T>(ids[i], seed, min, max);
          span[i] = span[i] - random_value;
        }
      });
      break;
    case GEO_NODE_ATTRIBUTE_RANDOMIZE_MULTIPLY:
      threading::parallel_for(span.index_range(), 512, [&](IndexRange range) {
        for (const int i : range) {
          const T random_value = random_value_in_range<T>(ids[i], seed, min, max);
          span[i] = span[i] * random_value;
        }
      });
      break;
    default:
      BLI_assert(false);
      break;
  }
}

static void randomize_attribute_bool(MutableSpan<bool> span,
                                     Span<uint32_t> ids,
                                     const uint32_t seed,
                                     const GeometryNodeAttributeRandomizeMode operation)
{
  BLI_assert(operation == GEO_NODE_ATTRIBUTE_RANDOMIZE_REPLACE_CREATE);
  UNUSED_VARS_NDEBUG(operation);
  threading::parallel_for(span.index_range(), 512, [&](IndexRange range) {
    for (const int i : range) {
      const bool random_value = BLI_hash_int_2d_to_float(ids[i], seed) > 0.5f;
      span[i] = random_value;
    }
  });
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         const GeoNodeExecParams &params,
                                         const StringRef name)
{
  /* Use the domain of the result attribute if it already exists. */
  std::optional<AttributeMetaData> result_info = component.attribute_get_meta_data(name);
  if (result_info) {
    return result_info->domain;
  }

  /* Otherwise use the input domain chosen in the interface. */
  const bNode &node = params.node();
  return static_cast<AttributeDomain>(node.custom2);
}

static void randomize_attribute_on_component(GeometryComponent &component,
                                             const GeoNodeExecParams &params,
                                             StringRef attribute_name,
                                             const CustomDataType data_type,
                                             const GeometryNodeAttributeRandomizeMode operation,
                                             const int seed)
{
  /* If the node is not in "replace / create" mode and the attribute
   * doesn't already exist, don't do the operation. */
  if (operation != GEO_NODE_ATTRIBUTE_RANDOMIZE_REPLACE_CREATE) {
    if (!component.attribute_exists(attribute_name)) {
      params.error_message_add(NodeWarningType::Error,
                               TIP_("No attribute with name \"") + attribute_name + "\"");
      return;
    }
  }

  const AttributeDomain domain = get_result_domain(component, params, attribute_name);

  OutputAttribute attribute = component.attribute_try_get_for_output(
      attribute_name, domain, data_type);
  if (!attribute) {
    return;
  }

  GMutableSpan span = attribute.as_span();

  Array<uint32_t> hashes = get_geometry_element_ids_as_uints(component, domain);

  switch (data_type) {
    case CD_PROP_FLOAT3: {
      const float3 min = params.get_input<float3>("Min");
      const float3 max = params.get_input<float3>("Max");
      randomize_attribute<float3>(span.typed<float3>(), min, max, hashes, seed, operation);
      break;
    }
    case CD_PROP_FLOAT: {
      const float min = params.get_input<float>("Min_001");
      const float max = params.get_input<float>("Max_001");
      randomize_attribute<float>(span.typed<float>(), min, max, hashes, seed, operation);
      break;
    }
    case CD_PROP_BOOL: {
      randomize_attribute_bool(span.typed<bool>(), hashes, seed, operation);
      break;
    }
    case CD_PROP_INT32: {
      const int min = params.get_input<int>("Min_002");
      const int max = params.get_input<int>("Max_002");
      randomize_attribute<int>(span.typed<int>(), min, max, hashes, seed, operation);
      break;
    }
    default: {
      BLI_assert(false);
      break;
    }
  }

  attribute.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  const std::string attribute_name = params.get_input<std::string>("Attribute");
  if (attribute_name.empty()) {
    params.set_default_remaining_outputs();
    return;
  }
  const int seed = params.get_input<int>("Seed");
  const NodeAttributeRandomize &storage = *(const NodeAttributeRandomize *)params.node().storage;
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);
  const GeometryNodeAttributeRandomizeMode operation =
      static_cast<GeometryNodeAttributeRandomizeMode>(storage.operation);

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    randomize_attribute_on_component(geometry_set.get_component_for_write<MeshComponent>(),
                                     params,
                                     attribute_name,
                                     data_type,
                                     operation,
                                     seed);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    randomize_attribute_on_component(geometry_set.get_component_for_write<PointCloudComponent>(),
                                     params,
                                     attribute_name,
                                     data_type,
                                     operation,
                                     seed);
  }
  if (geometry_set.has<CurveComponent>()) {
    randomize_attribute_on_component(geometry_set.get_component_for_write<CurveComponent>(),
                                     params,
                                     attribute_name,
                                     data_type,
                                     operation,
                                     seed);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes::node_geo_legacy_attribute_randomize_cc

void register_node_type_geo_legacy_attribute_randomize()
{
  namespace file_ns = blender::nodes::node_geo_legacy_attribute_randomize_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_ATTRIBUTE_RANDOMIZE, "Attribute Randomize", NODE_CLASS_ATTRIBUTE);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);

  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeAttributeRandomize", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
