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

#include "BLI_math_base_safe.h"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_attribute_map_range_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Attribute"));
  b.add_input<decl::String>(N_("Result"));
  b.add_input<decl::Float>(N_("From Min"));
  b.add_input<decl::Float>(N_("From Max")).default_value(1.0f);
  b.add_input<decl::Float>(N_("To Min"));
  b.add_input<decl::Float>(N_("To Max")).default_value(1.0f);
  b.add_input<decl::Float>(N_("Steps")).default_value(4.0f);
  b.add_input<decl::Vector>(N_("From Min"), "From Min_001");
  b.add_input<decl::Vector>(N_("From Max"), "From Max_001").default_value({1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>(N_("To Min"), "To Min_001");
  b.add_input<decl::Vector>(N_("To Max"), "To Max_001").default_value({1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>(N_("Steps"), "Steps_001").default_value({4.0f, 4.0f, 4.0f});
  b.add_input<decl::Bool>(N_("Clamp"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void fn_attribute_map_range_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "interpolation_type", 0, "", ICON_NONE);
}

static void geo_node_attribute_map_range_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeMapRange *data = (NodeAttributeMapRange *)MEM_callocN(sizeof(NodeAttributeMapRange),
                                                                     __func__);
  data->data_type = CD_PROP_FLOAT;
  data->interpolation_type = NODE_MAP_RANGE_LINEAR;

  node->storage = data;
}
static void geo_node_attribute_map_range_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeMapRange &node_storage = *(NodeAttributeMapRange *)node->storage;

  bNodeSocket *sock_from_min_float = (bNodeSocket *)BLI_findlink(&node->inputs, 3);
  bNodeSocket *sock_from_max_float = sock_from_min_float->next;
  bNodeSocket *sock_to_min_float = sock_from_max_float->next;
  bNodeSocket *sock_to_max_float = sock_to_min_float->next;
  bNodeSocket *sock_steps_float = sock_to_max_float->next;

  bNodeSocket *sock_from_min_vector = sock_steps_float->next;
  bNodeSocket *sock_from_max_vector = sock_from_min_vector->next;
  bNodeSocket *sock_to_min_vector = sock_from_max_vector->next;
  bNodeSocket *sock_to_max_vector = sock_to_min_vector->next;
  bNodeSocket *sock_steps_vector = sock_to_max_vector->next;

  bNodeSocket *sock_clamp = sock_steps_vector->next;

  const CustomDataType data_type = static_cast<CustomDataType>(node_storage.data_type);

  nodeSetSocketAvailability(sock_clamp,
                            node_storage.interpolation_type == NODE_MAP_RANGE_LINEAR ||
                                node_storage.interpolation_type == NODE_MAP_RANGE_STEPPED);

  nodeSetSocketAvailability(sock_from_min_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_from_max_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_to_min_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_to_max_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_steps_float,
                            data_type == CD_PROP_FLOAT &&
                                node_storage.interpolation_type == NODE_MAP_RANGE_STEPPED);

  nodeSetSocketAvailability(sock_from_min_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_from_max_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_to_min_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_to_max_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_steps_vector,
                            data_type == CD_PROP_FLOAT3 &&
                                node_storage.interpolation_type == NODE_MAP_RANGE_STEPPED);
}

static float map_linear(const float value,
                        const float min_from,
                        const float max_from,
                        const float min_to,
                        const float max_to)
{
  /* First we calculate a fraction that measures how far along
   * the [min_from, max_from] interval the value lies.
   *
   *                value
   * min_from [------>|------------------------] max_from
   *               factor (e.g. 0.25)
   *
   * Then to find where the value is mapped, we add the same fraction
   * of the [min_to, max_to] interval to min_to.
   *
   * min_to [--->|-----------] max_to
   *             v
   *      min_to + (max_to - min_to) * factor
   */
  const float factor = safe_divide(value - min_from, max_from - min_from);
  return min_to + factor * (max_to - min_to);
}

static float map_stepped(const float value,
                         const float min_from,
                         const float max_from,
                         const float min_to,
                         const float max_to,
                         const float steps)
{
  /* First the factor is calculated here in the same way as for the linear mapping.
   *
   * Then the factor is mapped to multiples of 1.0 / steps.
   * This is best understood with a few examples. Assume steps == 3.
   * ____________________________________
   * | factor | * 4.0 | floor() | / 3.0 |
   * |--------|-------|---------|-------|
   * | 0.0    | 0.0   | 0.0     | 0.0   |
   * | 0.1    | 0.4   | 0.0     | 0.0   |
   * | 0.25   | 1.0   | 1.0     | 0.333 |
   * | 0.45   | 1.8   | 1.0     | 0.333 |
   * | 0.5    | 2.0   | 2.0     | 0.666 |
   * | 0.55   | 2.2   | 2.0     | 0.666 |
   * | 0.999  | 3.999 | 3.0     | 1.0   |
   * | 1.0    | 4.0   | 4.0     | 1.333 |
   * ------------------------------------
   * Note that the factor is not always mapped the closest multiple of 1.0 /steps.
   */
  const float factor = safe_divide(value - min_from, max_from - min_from);
  const float factor_mapped = safe_divide(floorf(factor * (steps + 1.0f)), steps);
  return min_to + factor_mapped * (max_to - min_to);
}

static float smoothstep_polynomial(float x)
{
  /* This polynomial is only meant to be used for the [0, 1] range. */
  return (3.0f - 2.0f * x) * (x * x);
}

static float map_smoothstep(const float value,
                            const float min_from,
                            const float max_from,
                            const float min_to,
                            const float max_to)
{
  const float factor = safe_divide(value - min_from, max_from - min_from);
  const float factor_clamped = std::clamp(factor, 0.0f, 1.0f);
  const float factor_mapped = smoothstep_polynomial(factor_clamped);
  return min_to + factor_mapped * (max_to - min_to);
}

static float smootherstep_polynomial(float x)
{
  /* This polynomial is only meant to be used for the [0, 1] range. */
  return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

static float map_smootherstep(const float value,
                              const float min_from,
                              const float max_from,
                              const float min_to,
                              const float max_to)
{
  const float factor = safe_divide(value - min_from, max_from - min_from);
  const float factor_clamped = std::clamp(factor, 0.0f, 1.0f);
  const float factor_mapped = smootherstep_polynomial(factor_clamped);
  return min_to + factor_mapped * (max_to - min_to);
}

static void map_range_float(const VArray<float> &attribute_input,
                            MutableSpan<float> results,
                            const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  NodeAttributeMapRange &node_storage = *(NodeAttributeMapRange *)node.storage;
  const int interpolation_type = node_storage.interpolation_type;
  const float min_from = params.get_input<float>("From Min");
  const float max_from = params.get_input<float>("From Max");
  const float min_to = params.get_input<float>("To Min");
  const float max_to = params.get_input<float>("To Max");

  VArray_Span<float> span{attribute_input};

  switch (interpolation_type) {
    case NODE_MAP_RANGE_LINEAR: {
      threading::parallel_for(span.index_range(), 2048, [&](IndexRange range) {
        for (const int i : range) {
          results[i] = map_linear(span[i], min_from, max_from, min_to, max_to);
        }
      });
      break;
    }
    case NODE_MAP_RANGE_STEPPED: {
      const float steps = params.get_input<float>("Steps");
      threading::parallel_for(span.index_range(), 1024, [&](IndexRange range) {
        for (const int i : range) {
          results[i] = map_stepped(span[i], min_from, max_from, min_to, max_to, steps);
        }
      });
      break;
    }
    case NODE_MAP_RANGE_SMOOTHSTEP: {
      threading::parallel_for(span.index_range(), 1024, [&](IndexRange range) {
        for (const int i : range) {
          results[i] = map_smoothstep(span[i], min_from, max_from, min_to, max_to);
        }
      });
      break;
    }
    case NODE_MAP_RANGE_SMOOTHERSTEP: {
      threading::parallel_for(span.index_range(), 1024, [&](IndexRange range) {
        for (const int i : range) {
          results[i] = map_smootherstep(span[i], min_from, max_from, min_to, max_to);
        }
      });
      break;
    }
  }

  if (ELEM(interpolation_type, NODE_MAP_RANGE_LINEAR, NODE_MAP_RANGE_STEPPED) &&
      params.get_input<bool>("Clamp")) {
    /* Users can specify min_to > max_to, but clamping expects min < max. */
    const float clamp_min = min_to < max_to ? min_to : max_to;
    const float clamp_max = min_to < max_to ? max_to : min_to;

    threading::parallel_for(results.index_range(), 2048, [&](IndexRange range) {
      for (const int i : range) {
        results[i] = std::clamp(results[i], clamp_min, clamp_max);
      }
    });
  }
}

static void map_range_float3(const VArray<float3> &attribute_input,
                             const MutableSpan<float3> results,
                             const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  NodeAttributeMapRange &node_storage = *(NodeAttributeMapRange *)node.storage;
  const int interpolation_type = node_storage.interpolation_type;
  const float3 min_from = params.get_input<float3>("From Min_001");
  const float3 max_from = params.get_input<float3>("From Max_001");
  const float3 min_to = params.get_input<float3>("To Min_001");
  const float3 max_to = params.get_input<float3>("To Max_001");

  VArray_Span<float3> span{attribute_input};

  switch (interpolation_type) {
    case NODE_MAP_RANGE_LINEAR: {
      threading::parallel_for(span.index_range(), 1024, [&](IndexRange range) {
        for (const int i : range) {
          results[i].x = map_linear(span[i].x, min_from.x, max_from.x, min_to.x, max_to.x);
          results[i].y = map_linear(span[i].y, min_from.y, max_from.y, min_to.y, max_to.y);
          results[i].z = map_linear(span[i].z, min_from.z, max_from.z, min_to.z, max_to.z);
        }
      });
      break;
    }
    case NODE_MAP_RANGE_STEPPED: {
      const float3 steps = params.get_input<float3>("Steps_001");
      threading::parallel_for(span.index_range(), 1024, [&](IndexRange range) {
        for (const int i : range) {
          results[i].x = map_stepped(
              span[i].x, min_from.x, max_from.x, min_to.x, max_to.x, steps.x);
          results[i].y = map_stepped(
              span[i].y, min_from.y, max_from.y, min_to.y, max_to.y, steps.y);
          results[i].z = map_stepped(
              span[i].z, min_from.z, max_from.z, min_to.z, max_to.z, steps.z);
        }
      });
      break;
    }
    case NODE_MAP_RANGE_SMOOTHSTEP: {
      threading::parallel_for(span.index_range(), 1024, [&](IndexRange range) {
        for (const int i : range) {
          results[i].x = map_smoothstep(span[i].x, min_from.x, max_from.x, min_to.x, max_to.x);
          results[i].y = map_smoothstep(span[i].y, min_from.y, max_from.y, min_to.y, max_to.y);
          results[i].z = map_smoothstep(span[i].z, min_from.z, max_from.z, min_to.z, max_to.z);
        }
      });
      break;
    }
    case NODE_MAP_RANGE_SMOOTHERSTEP: {
      threading::parallel_for(span.index_range(), 1024, [&](IndexRange range) {
        for (const int i : range) {
          results[i].x = map_smootherstep(span[i].x, min_from.x, max_from.x, min_to.x, max_to.x);
          results[i].y = map_smootherstep(span[i].y, min_from.y, max_from.y, min_to.y, max_to.y);
          results[i].z = map_smootherstep(span[i].z, min_from.z, max_from.z, min_to.z, max_to.z);
        }
      });
      break;
    }
  }

  if (ELEM(interpolation_type, NODE_MAP_RANGE_LINEAR, NODE_MAP_RANGE_STEPPED) &&
      params.get_input<bool>("Clamp")) {
    /* Users can specify min_to > max_to, but clamping expects min < max. */
    float3 clamp_min;
    float3 clamp_max;
    clamp_min.x = min_to.x < max_to.x ? min_to.x : max_to.x;
    clamp_max.x = min_to.x < max_to.x ? max_to.x : min_to.x;
    clamp_min.y = min_to.y < max_to.y ? min_to.y : max_to.y;
    clamp_max.y = min_to.y < max_to.y ? max_to.y : min_to.y;
    clamp_min.z = min_to.z < max_to.z ? min_to.z : max_to.z;
    clamp_max.z = min_to.z < max_to.z ? max_to.z : min_to.z;

    for (int i : results.index_range()) {
      clamp_v3_v3v3(results[i], clamp_min, clamp_max);
    }
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

static void map_range_attribute(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const std::string input_name = params.get_input<std::string>("Attribute");
  const std::string result_name = params.get_input<std::string>("Result");

  if (input_name.empty() || result_name.empty()) {
    return;
  }

  const bNode &node = params.node();
  NodeAttributeMapRange &node_storage = *(NodeAttributeMapRange *)node.storage;
  const CustomDataType data_type = static_cast<CustomDataType>(node_storage.data_type);

  const AttributeDomain domain = get_result_domain(component, input_name, result_name);

  GVArrayPtr attribute_input = component.attribute_try_get_for_read(input_name, domain, data_type);

  if (!attribute_input) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("No attribute with name \"") + input_name + "\"");
    return;
  }

  OutputAttribute attribute_result = component.attribute_try_get_for_output_only(
      result_name, domain, data_type);
  if (!attribute_result) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Could not find or create attribute with name \"") +
                                 result_name + "\"");
    return;
  }

  switch (data_type) {
    case CD_PROP_FLOAT: {
      map_range_float(attribute_input->typed<float>(), attribute_result.as_span<float>(), params);
      break;
    }
    case CD_PROP_FLOAT3: {
      map_range_float3(
          attribute_input->typed<float3>(), attribute_result.as_span<float3>(), params);
      break;
    }
    default:
      BLI_assert_unreachable();
  }

  attribute_result.save();
}

static void geo_node_attribute_map_range_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (geometry_set.has<MeshComponent>()) {
    map_range_attribute(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    map_range_attribute(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }
  if (geometry_set.has<CurveComponent>()) {
    map_range_attribute(geometry_set.get_component_for_write<CurveComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_map_range()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_ATTRIBUTE_MAP_RANGE, "Attribute Map Range", NODE_CLASS_ATTRIBUTE, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_map_range_exec;
  node_type_init(&ntype, blender::nodes::geo_node_attribute_map_range_init);
  node_type_update(&ntype, blender::nodes::geo_node_attribute_map_range_update);
  node_type_storage(
      &ntype, "NodeAttributeMapRange", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_attribute_map_range_declare;
  ntype.draw_buttons = blender::nodes::fn_attribute_map_range_layout;
  nodeRegisterType(&ntype);
}
