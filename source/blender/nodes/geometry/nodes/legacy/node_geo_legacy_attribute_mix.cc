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

#include "BLI_task.hh"

#include "BKE_material.h"

#include "DNA_material_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_legacy_attribute_mix_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Factor"));
  b.add_input<decl::Float>(N_("Factor"), "Factor_001")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::String>(N_("A"));
  b.add_input<decl::Float>(N_("A"), "A_001");
  b.add_input<decl::Vector>(N_("A"), "A_002");
  b.add_input<decl::Color>(N_("A"), "A_003").default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_input<decl::String>(N_("B"));
  b.add_input<decl::Float>(N_("B"), "B_001");
  b.add_input<decl::Vector>(N_("B"), "B_002");
  b.add_input<decl::Color>(N_("B"), "B_003").default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_input<decl::String>(N_("Result"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "blend_type", 0, "", ICON_NONE);
  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "input_type_factor", 0, IFACE_("Factor"), ICON_NONE);
  uiItemR(col, ptr, "input_type_a", 0, IFACE_("A"), ICON_NONE);
  uiItemR(col, ptr, "input_type_b", 0, IFACE_("B"), ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeMix *data = MEM_cnew<NodeAttributeMix>("attribute mix node");
  data->blend_type = MA_RAMP_BLEND;
  data->input_type_factor = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  data->input_type_a = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  data->input_type_b = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  NodeAttributeMix *node_storage = (NodeAttributeMix *)node->storage;
  update_attribute_input_socket_availabilities(
      *ntree, *node, "Factor", (GeometryNodeAttributeInputMode)node_storage->input_type_factor);
  update_attribute_input_socket_availabilities(
      *ntree, *node, "A", (GeometryNodeAttributeInputMode)node_storage->input_type_a);
  update_attribute_input_socket_availabilities(
      *ntree, *node, "B", (GeometryNodeAttributeInputMode)node_storage->input_type_b);
}

static void do_mix_operation_float(const int blend_mode,
                                   const VArray<float> &factors,
                                   const VArray<float> &inputs_a,
                                   const VArray<float> &inputs_b,
                                   VMutableArray<float> &results)
{
  const int size = results.size();
  threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
    for (const int i : range) {
      const float factor = factors[i];
      float3 a{inputs_a[i]};
      const float3 b{inputs_b[i]};
      ramp_blend(blend_mode, a, factor, b);
      const float result = a.x;
      results.set(i, result);
    }
  });
}

static void do_mix_operation_float3(const int blend_mode,
                                    const VArray<float> &factors,
                                    const VArray<float3> &inputs_a,
                                    const VArray<float3> &inputs_b,
                                    VMutableArray<float3> &results)
{
  const int size = results.size();
  threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
    for (const int i : range) {
      const float factor = factors[i];
      float3 a = inputs_a[i];
      const float3 b = inputs_b[i];
      ramp_blend(blend_mode, a, factor, b);
      results.set(i, a);
    }
  });
}

static void do_mix_operation_color4f(const int blend_mode,
                                     const VArray<float> &factors,
                                     const VArray<ColorGeometry4f> &inputs_a,
                                     const VArray<ColorGeometry4f> &inputs_b,
                                     VMutableArray<ColorGeometry4f> &results)
{
  const int size = results.size();
  threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
    for (const int i : range) {
      const float factor = factors[i];
      ColorGeometry4f a = inputs_a[i];
      const ColorGeometry4f b = inputs_b[i];
      ramp_blend(blend_mode, a, factor, b);
      results.set(i, a);
    }
  });
}

static void do_mix_operation(const CustomDataType result_type,
                             int blend_mode,
                             const VArray<float> &attribute_factor,
                             const GVArray &attribute_a,
                             const GVArray &attribute_b,
                             GVMutableArray &attribute_result)
{
  if (result_type == CD_PROP_FLOAT) {
    VMutableArray<float> result = attribute_result.typed<float>();
    do_mix_operation_float(blend_mode,
                           attribute_factor,
                           attribute_a.typed<float>(),
                           attribute_b.typed<float>(),
                           result);
  }
  else if (result_type == CD_PROP_FLOAT3) {
    VMutableArray<float3> result = attribute_result.typed<float3>();
    do_mix_operation_float3(blend_mode,
                            attribute_factor,
                            attribute_a.typed<float3>(),
                            attribute_b.typed<float3>(),
                            result);
  }
  else if (result_type == CD_PROP_COLOR) {
    VMutableArray<ColorGeometry4f> result = attribute_result.typed<ColorGeometry4f>();
    do_mix_operation_color4f(blend_mode,
                             attribute_factor,
                             attribute_a.typed<ColorGeometry4f>(),
                             attribute_b.typed<ColorGeometry4f>(),
                             result);
  }
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         const GeoNodeExecParams &params,
                                         StringRef result_name)
{
  /* Use the domain of the result attribute if it already exists. */
  std::optional<AttributeMetaData> result_info = component.attribute_get_meta_data(result_name);
  if (result_info) {
    return result_info->domain;
  }

  /* Otherwise use the highest priority domain from existing input attributes, or the default. */
  return params.get_highest_priority_input_domain({"A", "B"}, component, ATTR_DOMAIN_POINT);
}

static void attribute_mix_calc(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const NodeAttributeMix *node_storage = (const NodeAttributeMix *)node.storage;
  const std::string result_name = params.get_input<std::string>("Result");

  /* Use the highest complexity data type among the inputs and outputs, that way the node will
   * never "remove information". Use CD_PROP_BOOL as the lowest complexity data type, but in any
   * real situation it won't be returned. */
  const CustomDataType result_type = bke::attribute_data_type_highest_complexity({
      params.get_input_attribute_data_type("A", component, CD_PROP_BOOL),
      params.get_input_attribute_data_type("B", component, CD_PROP_BOOL),
      params.get_input_attribute_data_type("Result", component, CD_PROP_BOOL),
  });

  const AttributeDomain result_domain = get_result_domain(component, params, result_name);

  OutputAttribute attribute_result = component.attribute_try_get_for_output_only(
      result_name, result_domain, result_type);
  if (!attribute_result) {
    return;
  }

  VArray<float> attribute_factor = params.get_input_attribute<float>(
      "Factor", component, result_domain, 0.5f);
  GVArray attribute_a = params.get_input_attribute(
      "A", component, result_domain, result_type, nullptr);
  GVArray attribute_b = params.get_input_attribute(
      "B", component, result_domain, result_type, nullptr);

  do_mix_operation(result_type,
                   node_storage->blend_type,
                   attribute_factor,
                   attribute_a,
                   attribute_b,
                   attribute_result.varray());
  attribute_result.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    attribute_mix_calc(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    attribute_mix_calc(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }
  if (geometry_set.has<CurveComponent>()) {
    attribute_mix_calc(geometry_set.get_component_for_write<CurveComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes::node_geo_legacy_attribute_mix_cc

void register_node_type_geo_attribute_mix()
{
  namespace file_ns = blender::nodes::node_geo_legacy_attribute_mix_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_LEGACY_ATTRIBUTE_MIX, "Attribute Mix", NODE_CLASS_ATTRIBUTE);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeAttributeMix", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
