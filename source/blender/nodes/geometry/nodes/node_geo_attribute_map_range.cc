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

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_attribute_map_range_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Attribute")},
    {SOCK_STRING, N_("Result")},
    {SOCK_FLOAT, N_("From Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("From Max"), 1.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("To Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("To Max"), 1.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_FLOAT, N_("Steps"), 4.0f, 4.0f, 4.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("From Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("From Max"), 1.0f, 1.0f, 1.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("To Min"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("To Max"), 1.0f, 1.0f, 1.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("Steps"), 4.0f, 4.0f, 4.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_BOOLEAN, N_("Clamp")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_map_range_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

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

namespace blender::nodes {

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

static void map_range_float(FloatReadAttribute attribute_input,
                            FloatWriteAttribute attribute_result,
                            const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  NodeAttributeMapRange &node_storage = *(NodeAttributeMapRange *)node.storage;
  const int interpolation_type = node_storage.interpolation_type;
  const float min_from = params.get_input<float>("From Min");
  const float max_from = params.get_input<float>("From Max");
  const float min_to = params.get_input<float>("To Min");
  const float max_to = params.get_input<float>("To Max");

  Span<float> span = attribute_input.get_span();
  MutableSpan<float> result_span = attribute_result.get_span();

  switch (interpolation_type) {
    case NODE_MAP_RANGE_LINEAR: {
      for (int i : span.index_range()) {
        result_span[i] = map_linear(span[i], min_from, max_from, min_to, max_to);
      }
      break;
    }
    case NODE_MAP_RANGE_STEPPED: {
      const float steps = params.get_input<float>("Steps");
      for (int i : span.index_range()) {
        result_span[i] = map_stepped(span[i], min_from, max_from, min_to, max_to, steps);
      }
      break;
    }
    case NODE_MAP_RANGE_SMOOTHSTEP: {
      for (int i : span.index_range()) {
        result_span[i] = map_smoothstep(span[i], min_from, max_from, min_to, max_to);
      }
      break;
    }
    case NODE_MAP_RANGE_SMOOTHERSTEP: {
      for (int i : span.index_range()) {
        result_span[i] = map_smootherstep(span[i], min_from, max_from, min_to, max_to);
      }
      break;
    }
  }

  if (ELEM(interpolation_type, NODE_MAP_RANGE_LINEAR, NODE_MAP_RANGE_STEPPED) &&
      params.get_input<bool>("Clamp")) {
    /* Users can specify min_to > max_to, but clamping expects min < max. */
    const float clamp_min = min_to < max_to ? min_to : max_to;
    const float clamp_max = min_to < max_to ? max_to : min_to;

    for (int i : result_span.index_range()) {
      result_span[i] = std::clamp(result_span[i], clamp_min, clamp_max);
    }
  }
}

static void map_range_float3(Float3ReadAttribute attribute_input,
                             Float3WriteAttribute attribute_result,
                             const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  NodeAttributeMapRange &node_storage = *(NodeAttributeMapRange *)node.storage;
  const int interpolation_type = node_storage.interpolation_type;
  const float3 min_from = params.get_input<float3>("From Min_001");
  const float3 max_from = params.get_input<float3>("From Max_001");
  const float3 min_to = params.get_input<float3>("To Min_001");
  const float3 max_to = params.get_input<float3>("To Max_001");

  Span<float3> span = attribute_input.get_span();
  MutableSpan<float3> result_span = attribute_result.get_span();

  switch (interpolation_type) {
    case NODE_MAP_RANGE_LINEAR: {
      for (int i : span.index_range()) {
        result_span[i].x = map_linear(span[i].x, min_from.x, max_from.x, min_to.x, max_to.x);
        result_span[i].y = map_linear(span[i].y, min_from.y, max_from.y, min_to.y, max_to.y);
        result_span[i].z = map_linear(span[i].z, min_from.z, max_from.z, min_to.z, max_to.z);
      }
      break;
    }
    case NODE_MAP_RANGE_STEPPED: {
      const float3 steps = params.get_input<float3>("Steps_001");
      for (int i : span.index_range()) {
        result_span[i].x = map_stepped(
            span[i].x, min_from.x, max_from.x, min_to.x, max_to.x, steps.x);
        result_span[i].y = map_stepped(
            span[i].y, min_from.y, max_from.y, min_to.y, max_to.y, steps.y);
        result_span[i].z = map_stepped(
            span[i].z, min_from.z, max_from.z, min_to.z, max_to.z, steps.z);
      }
      break;
    }
    case NODE_MAP_RANGE_SMOOTHSTEP: {
      for (int i : span.index_range()) {
        result_span[i].x = map_smoothstep(span[i].x, min_from.x, max_from.x, min_to.x, max_to.x);
        result_span[i].y = map_smoothstep(span[i].y, min_from.y, max_from.y, min_to.y, max_to.y);
        result_span[i].z = map_smoothstep(span[i].z, min_from.z, max_from.z, min_to.z, max_to.z);
      }
      break;
    }
    case NODE_MAP_RANGE_SMOOTHERSTEP: {
      for (int i : span.index_range()) {
        result_span[i].x = map_smootherstep(span[i].x, min_from.x, max_from.x, min_to.x, max_to.x);
        result_span[i].y = map_smootherstep(span[i].y, min_from.y, max_from.y, min_to.y, max_to.y);
        result_span[i].z = map_smootherstep(span[i].z, min_from.z, max_from.z, min_to.z, max_to.z);
      }
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

    for (int i : result_span.index_range()) {
      clamp_v3_v3v3(result_span[i], clamp_min, clamp_max);
    }
  }
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         StringRef source_name,
                                         StringRef result_name)
{
  ReadAttributePtr result_attribute = component.attribute_try_get_for_read(result_name);
  if (result_attribute) {
    return result_attribute->domain();
  }
  ReadAttributePtr source_attribute = component.attribute_try_get_for_read(source_name);
  if (source_attribute) {
    return source_attribute->domain();
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

  ReadAttributePtr attribute_input = component.attribute_try_get_for_read(
      input_name, domain, data_type);

  if (!attribute_input) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("No attribute with name \"") + input_name + "\"");
    return;
  }

  OutputAttributePtr attribute_result = component.attribute_try_get_for_output(
      result_name, domain, data_type);
  if (!attribute_result) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Could not find or create attribute with name \"") +
                                 result_name + "\"");
    return;
  }

  switch (data_type) {
    case CD_PROP_FLOAT: {
      map_range_float(*attribute_input, *attribute_result, params);
      break;
    }
    case CD_PROP_FLOAT3: {
      map_range_float3(*attribute_input, *attribute_result, params);
      break;
    }
    default:
      BLI_assert_unreachable();
  }

  attribute_result.apply_span_and_save();
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

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_map_range()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_ATTRIBUTE_MAP_RANGE, "Attribute Map Range", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(
      &ntype, geo_node_attribute_map_range_in, geo_node_attribute_map_range_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_map_range_exec;
  node_type_init(&ntype, geo_node_attribute_map_range_init);
  node_type_update(&ntype, geo_node_attribute_map_range_update);
  node_type_storage(
      &ntype, "NodeAttributeMapRange", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = fn_attribute_map_range_layout;
  nodeRegisterType(&ntype);
}
