
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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_attribute_clamp_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Attribute"));
  b.add_input<decl::String>(N_("Result"));
  b.add_input<decl::Vector>(N_("Min"));
  b.add_input<decl::Vector>(N_("Max")).default_value({1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Min"), "Min_001");
  b.add_input<decl::Float>(N_("Max"), "Max_001").default_value(1.0f);
  b.add_input<decl::Int>(N_("Min"), "Min_002").min(-100000).max(100000);
  b.add_input<decl::Int>(N_("Max"), "Max_002").default_value(100).min(-100000).max(100000);
  b.add_input<decl::Color>(N_("Min"), "Min_003").default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_input<decl::Color>(N_("Max"), "Max_003").default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_attribute_clamp_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

static void geo_node_attribute_clamp_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeClamp *data = (NodeAttributeClamp *)MEM_callocN(sizeof(NodeAttributeClamp),
                                                               __func__);
  data->data_type = CD_PROP_FLOAT;
  data->operation = NODE_CLAMP_MINMAX;
  node->storage = data;
}

static void geo_node_attribute_clamp_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock_min_vector = (bNodeSocket *)BLI_findlink(&node->inputs, 3);
  bNodeSocket *sock_max_vector = sock_min_vector->next;
  bNodeSocket *sock_min_float = sock_max_vector->next;
  bNodeSocket *sock_max_float = sock_min_float->next;
  bNodeSocket *sock_min_int = sock_max_float->next;
  bNodeSocket *sock_max_int = sock_min_int->next;
  bNodeSocket *sock_min_color = sock_max_int->next;
  bNodeSocket *sock_max_color = sock_min_color->next;

  const NodeAttributeClamp &storage = *(const NodeAttributeClamp *)node->storage;
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);
  nodeSetSocketAvailability(sock_min_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_max_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_min_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_max_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_min_int, data_type == CD_PROP_INT32);
  nodeSetSocketAvailability(sock_max_int, data_type == CD_PROP_INT32);
  nodeSetSocketAvailability(sock_min_color, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(sock_max_color, data_type == CD_PROP_COLOR);
}

template<typename T> T clamp_value(const T val, const T min, const T max);

template<> inline float clamp_value(const float val, const float min, const float max)
{
  return std::min(std::max(val, min), max);
}

template<> inline int clamp_value(const int val, const int min, const int max)
{
  return std::min(std::max(val, min), max);
}

template<> inline float3 clamp_value(const float3 val, const float3 min, const float3 max)
{
  float3 tmp;
  tmp.x = std::min(std::max(val.x, min.x), max.x);
  tmp.y = std::min(std::max(val.y, min.y), max.y);
  tmp.z = std::min(std::max(val.z, min.z), max.z);
  return tmp;
}

template<>
inline ColorGeometry4f clamp_value(const ColorGeometry4f val,
                                   const ColorGeometry4f min,
                                   const ColorGeometry4f max)
{
  ColorGeometry4f tmp;
  tmp.r = std::min(std::max(val.r, min.r), max.r);
  tmp.g = std::min(std::max(val.g, min.g), max.g);
  tmp.b = std::min(std::max(val.b, min.b), max.b);
  tmp.a = std::min(std::max(val.a, min.a), max.a);
  return tmp;
}

template<typename T>
static void clamp_attribute(const VArray<T> &inputs,
                            const MutableSpan<T> outputs,
                            const T min,
                            const T max)
{
  for (const int i : IndexRange(outputs.size())) {
    outputs[i] = clamp_value<T>(inputs[i], min, max);
  }
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         StringRef source_name,
                                         StringRef result_name)
{
  std::optional<AttributeMetaData> result_info = component.attribute_get_meta_data(result_name);
  if (result_info) {
    return result_info->domain;
  }
  std::optional<AttributeMetaData> source_info = component.attribute_get_meta_data(source_name);
  if (source_info) {
    return source_info->domain;
  }
  return ATTR_DOMAIN_POINT;
}

static void clamp_attribute(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const std::string attribute_name = params.get_input<std::string>("Attribute");
  const std::string result_name = params.get_input<std::string>("Result");

  if (attribute_name.empty() || result_name.empty()) {
    return;
  }

  if (!component.attribute_exists(attribute_name)) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("No attribute with name \"") + attribute_name + "\"");
    return;
  }

  const NodeAttributeClamp &storage = *(const NodeAttributeClamp *)params.node().storage;
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);
  const AttributeDomain domain = get_result_domain(component, attribute_name, result_name);
  const int operation = static_cast<int>(storage.operation);

  GVArrayPtr attribute_input = component.attribute_try_get_for_read(
      attribute_name, domain, data_type);

  OutputAttribute attribute_result = component.attribute_try_get_for_output_only(
      result_name, domain, data_type);

  if (!attribute_result) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Could not find or create attribute with name \"") +
                                 result_name + "\"");
    return;
  }

  switch (data_type) {
    case CD_PROP_FLOAT3: {
      float3 min = params.get_input<float3>("Min");
      float3 max = params.get_input<float3>("Max");
      if (operation == NODE_CLAMP_RANGE) {
        if (min.x > max.x) {
          std::swap(min.x, max.x);
        }
        if (min.y > max.y) {
          std::swap(min.y, max.y);
        }
        if (min.z > max.z) {
          std::swap(min.z, max.z);
        }
      }
      MutableSpan<float3> results = attribute_result.as_span<float3>();
      clamp_attribute<float3>(attribute_input->typed<float3>(), results, min, max);
      break;
    }
    case CD_PROP_FLOAT: {
      const float min = params.get_input<float>("Min_001");
      const float max = params.get_input<float>("Max_001");
      MutableSpan<float> results = attribute_result.as_span<float>();
      if (operation == NODE_CLAMP_RANGE && min > max) {
        clamp_attribute<float>(attribute_input->typed<float>(), results, max, min);
      }
      else {
        clamp_attribute<float>(attribute_input->typed<float>(), results, min, max);
      }
      break;
    }
    case CD_PROP_INT32: {
      const int min = params.get_input<int>("Min_002");
      const int max = params.get_input<int>("Max_002");
      MutableSpan<int> results = attribute_result.as_span<int>();
      if (operation == NODE_CLAMP_RANGE && min > max) {
        clamp_attribute<int>(attribute_input->typed<int>(), results, max, min);
      }
      else {
        clamp_attribute<int>(attribute_input->typed<int>(), results, min, max);
      }
      break;
    }
    case CD_PROP_COLOR: {
      ColorGeometry4f min = params.get_input<ColorGeometry4f>("Min_003");
      ColorGeometry4f max = params.get_input<ColorGeometry4f>("Max_003");
      if (operation == NODE_CLAMP_RANGE) {
        if (min.r > max.r) {
          std::swap(min.r, max.r);
        }
        if (min.g > max.g) {
          std::swap(min.g, max.g);
        }
        if (min.b > max.b) {
          std::swap(min.b, max.b);
        }
        if (min.a > max.a) {
          std::swap(min.a, max.a);
        }
      }
      MutableSpan<ColorGeometry4f> results = attribute_result.as_span<ColorGeometry4f>();
      clamp_attribute<ColorGeometry4f>(
          attribute_input->typed<ColorGeometry4f>(), results, min, max);
      break;
    }
    default: {
      BLI_assert(false);
      break;
    }
  }

  attribute_result.save();
}

static void geo_node_attribute_clamp_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    clamp_attribute(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    clamp_attribute(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }
  if (geometry_set.has<CurveComponent>()) {
    clamp_attribute(geometry_set.get_component_for_write<CurveComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_clamp()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_ATTRIBUTE_CLAMP, "Attribute Clamp", NODE_CLASS_ATTRIBUTE, 0);
  node_type_init(&ntype, blender::nodes::geo_node_attribute_clamp_init);
  node_type_update(&ntype, blender::nodes::geo_node_attribute_clamp_update);
  ntype.declare = blender::nodes::geo_node_attribute_clamp_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_clamp_exec;
  ntype.draw_buttons = blender::nodes::geo_node_attribute_clamp_layout;
  node_type_storage(
      &ntype, "NodeAttributeClamp", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
