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

#include "BKE_material.h"

#include "DNA_material_types.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_attribute_mix_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Factor")},
    {SOCK_FLOAT, N_("Factor"), 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, PROP_FACTOR},
    {SOCK_STRING, N_("A")},
    {SOCK_FLOAT, N_("A"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("A"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {SOCK_RGBA, N_("A"), 0.5, 0.5, 0.5, 1.0},
    {SOCK_STRING, N_("B")},
    {SOCK_FLOAT, N_("B"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {SOCK_VECTOR, N_("B"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX},
    {SOCK_RGBA, N_("B"), 0.5, 0.5, 0.5, 1.0},
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mix_attribute_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static void do_mix_operation_float(const int blend_mode,
                                   const FloatReadAttribute &factors,
                                   const FloatReadAttribute &inputs_a,
                                   const FloatReadAttribute &inputs_b,
                                   FloatWriteAttribute &results)
{
  const int size = results.size();
  for (const int i : IndexRange(size)) {
    const float factor = factors[i];
    float3 a{inputs_a[i]};
    const float3 b{inputs_b[i]};
    ramp_blend(blend_mode, a, factor, b);
    const float result = a.length();
    results.set(i, result);
  }
}

static void do_mix_operation_float3(const int blend_mode,
                                    const FloatReadAttribute &factors,
                                    const Float3ReadAttribute &inputs_a,
                                    const Float3ReadAttribute &inputs_b,
                                    Float3WriteAttribute &results)
{
  const int size = results.size();
  for (const int i : IndexRange(size)) {
    const float factor = factors[i];
    float3 a = inputs_a[i];
    const float3 b = inputs_b[i];
    ramp_blend(blend_mode, a, factor, b);
    results.set(i, a);
  }
}

static void do_mix_operation_color4f(const int blend_mode,
                                     const FloatReadAttribute &factors,
                                     const Color4fReadAttribute &inputs_a,
                                     const Color4fReadAttribute &inputs_b,
                                     Color4fWriteAttribute &results)
{
  const int size = results.size();
  for (const int i : IndexRange(size)) {
    const float factor = factors[i];
    Color4f a = inputs_a[i];
    const Color4f b = inputs_b[i];
    ramp_blend(blend_mode, a, factor, b);
    results.set(i, a);
  }
}

static void do_mix_operation(const CustomDataType result_type,
                             int blend_mode,
                             const FloatReadAttribute &attribute_factor,
                             ReadAttributePtr attribute_a,
                             ReadAttributePtr attribute_b,
                             WriteAttributePtr attribute_result)
{
  if (result_type == CD_PROP_FLOAT) {
    FloatReadAttribute attribute_a_float = std::move(attribute_a);
    FloatReadAttribute attribute_b_float = std::move(attribute_b);
    FloatWriteAttribute attribute_result_float = std::move(attribute_result);
    do_mix_operation_float(blend_mode,
                           attribute_factor,
                           attribute_a_float,
                           attribute_b_float,
                           attribute_result_float);
  }
  else if (result_type == CD_PROP_FLOAT3) {
    Float3ReadAttribute attribute_a_float3 = std::move(attribute_a);
    Float3ReadAttribute attribute_b_float3 = std::move(attribute_b);
    Float3WriteAttribute attribute_result_float3 = std::move(attribute_result);
    do_mix_operation_float3(blend_mode,
                            attribute_factor,
                            attribute_a_float3,
                            attribute_b_float3,
                            attribute_result_float3);
  }
  else if (result_type == CD_PROP_COLOR) {
    Color4fReadAttribute attribute_a_color4f = std::move(attribute_a);
    Color4fReadAttribute attribute_b_color4f = std::move(attribute_b);
    Color4fWriteAttribute attribute_result_color4f = std::move(attribute_result);
    do_mix_operation_color4f(blend_mode,
                             attribute_factor,
                             attribute_a_color4f,
                             attribute_b_color4f,
                             attribute_result_color4f);
  }
}

static void attribute_mix_calc(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const NodeAttributeMix *node_storage = (const NodeAttributeMix *)node.storage;

  CustomDataType result_type = CD_PROP_COLOR;
  AttributeDomain result_domain = ATTR_DOMAIN_POINT;

  /* Use type and domain from the result attribute, if it exists already. */
  const std::string result_name = params.get_input<std::string>("Result");
  const ReadAttributePtr result_attribute_read = component.attribute_try_get_for_read(result_name);
  if (result_attribute_read) {
    result_type = result_attribute_read->custom_data_type();
    result_domain = result_attribute_read->domain();
  }

  WriteAttributePtr attribute_result = component.attribute_try_ensure_for_write(
      result_name, result_domain, result_type);
  if (!attribute_result) {
    return;
  }

  FloatReadAttribute attribute_factor = params.get_input_attribute<float>(
      "Factor", component, result_domain, 0.5f);
  ReadAttributePtr attribute_a = params.get_input_attribute(
      "A", component, result_domain, result_type, nullptr);
  ReadAttributePtr attribute_b = params.get_input_attribute(
      "B", component, result_domain, result_type, nullptr);

  do_mix_operation(result_type,
                   node_storage->blend_type,
                   attribute_factor,
                   std::move(attribute_a),
                   std::move(attribute_b),
                   std::move(attribute_result));
}

static void geo_node_attribute_mix_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (geometry_set.has<MeshComponent>()) {
    attribute_mix_calc(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    attribute_mix_calc(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

static void geo_node_attribute_mix_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeMix *data = (NodeAttributeMix *)MEM_callocN(sizeof(NodeAttributeMix),
                                                           "attribute mix node");
  data->blend_type = MA_RAMP_BLEND;
  data->input_type_factor = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  data->input_type_a = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  data->input_type_b = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  node->storage = data;
}

static void geo_node_attribute_mix_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeMix *node_storage = (NodeAttributeMix *)node->storage;
  update_attribute_input_socket_availabilities(
      *node, "Factor", (GeometryNodeAttributeInputMode)node_storage->input_type_factor);
  update_attribute_input_socket_availabilities(
      *node, "A", (GeometryNodeAttributeInputMode)node_storage->input_type_a);
  update_attribute_input_socket_availabilities(
      *node, "B", (GeometryNodeAttributeInputMode)node_storage->input_type_b);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_mix()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_ATTRIBUTE_MIX, "Attribute Mix", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(&ntype, geo_node_attribute_mix_in, geo_node_mix_attribute_out);
  node_type_init(&ntype, blender::nodes::geo_node_attribute_mix_init);
  node_type_update(&ntype, blender::nodes::geo_node_attribute_mix_update);
  node_type_storage(
      &ntype, "NodeAttributeMix", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_mix_exec;
  nodeRegisterType(&ntype);
}
