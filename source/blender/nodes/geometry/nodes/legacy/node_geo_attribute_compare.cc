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

#include "NOD_math_functions.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_attribute_compare_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("A"));
  b.add_input<decl::Float>(N_("A"), "A_001");
  b.add_input<decl::Vector>(N_("A"), "A_002");
  b.add_input<decl::Color>(N_("A"), "A_003").default_value({0.5, 0.5, 0.5, 1.0});
  b.add_input<decl::String>(N_("B"));
  b.add_input<decl::Float>(N_("B"), "B_001");
  b.add_input<decl::Vector>(N_("B"), "B_002");
  b.add_input<decl::Color>(N_("B"), "B_003").default_value({0.5, 0.5, 0.5, 1.0});
  b.add_input<decl::Float>(N_("Threshold")).default_value(0.01f).min(0.0f);
  b.add_input<decl::String>(N_("Result"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_attribute_compare_layout(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "input_type_a", 0, IFACE_("A"), ICON_NONE);
  uiItemR(layout, ptr, "input_type_b", 0, IFACE_("B"), ICON_NONE);
}

static void geo_node_attribute_compare_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeCompare *data = (NodeAttributeCompare *)MEM_callocN(sizeof(NodeAttributeCompare),
                                                                   __func__);
  data->operation = NODE_FLOAT_COMPARE_GREATER_THAN;
  data->input_type_a = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  data->input_type_b = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  node->storage = data;
}

static bool operation_tests_equality(const NodeAttributeCompare &node_storage)
{
  return ELEM(node_storage.operation, NODE_FLOAT_COMPARE_EQUAL, NODE_FLOAT_COMPARE_NOT_EQUAL);
}

static void geo_node_attribute_compare_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeCompare *node_storage = (NodeAttributeCompare *)node->storage;
  update_attribute_input_socket_availabilities(
      *node, "A", (GeometryNodeAttributeInputMode)node_storage->input_type_a);
  update_attribute_input_socket_availabilities(
      *node, "B", (GeometryNodeAttributeInputMode)node_storage->input_type_b);

  bNodeSocket *socket_threshold = (bNodeSocket *)BLI_findlink(&node->inputs, 9);
  nodeSetSocketAvailability(socket_threshold, operation_tests_equality(*node_storage));
}

static void do_math_operation(const VArray<float> &input_a,
                              const VArray<float> &input_b,
                              const FloatCompareOperation operation,
                              MutableSpan<bool> span_result)
{
  const int size = input_a.size();

  if (try_dispatch_float_math_fl_fl_to_bool(
          operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
            for (const int i : IndexRange(size)) {
              const float a = input_a[i];
              const float b = input_b[i];
              const bool out = math_function(a, b);
              span_result[i] = out;
            }
          })) {
    return;
  }

  /* The operation is not supported by this node currently. */
  BLI_assert(false);
}

static void do_equal_operation_float(const VArray<float> &input_a,
                                     const VArray<float> &input_b,
                                     const float threshold,
                                     MutableSpan<bool> span_result)
{
  const int size = input_a.size();
  for (const int i : IndexRange(size)) {
    const float a = input_a[i];
    const float b = input_b[i];
    span_result[i] = compare_ff(a, b, threshold);
  }
}

static void do_equal_operation_float3(const VArray<float3> &input_a,
                                      const VArray<float3> &input_b,
                                      const float threshold,
                                      MutableSpan<bool> span_result)
{
  const float threshold_squared = pow2f(threshold);
  const int size = input_a.size();
  for (const int i : IndexRange(size)) {
    const float3 a = input_a[i];
    const float3 b = input_b[i];
    span_result[i] = len_squared_v3v3(a, b) < threshold_squared;
  }
}

static void do_equal_operation_color4f(const VArray<ColorGeometry4f> &input_a,
                                       const VArray<ColorGeometry4f> &input_b,
                                       const float threshold,
                                       MutableSpan<bool> span_result)
{
  const float threshold_squared = pow2f(threshold);
  const int size = input_a.size();
  for (const int i : IndexRange(size)) {
    const ColorGeometry4f a = input_a[i];
    const ColorGeometry4f b = input_b[i];
    span_result[i] = len_squared_v4v4(a, b) < threshold_squared;
  }
}

static void do_equal_operation_bool(const VArray<bool> &input_a,
                                    const VArray<bool> &input_b,
                                    const float UNUSED(threshold),
                                    MutableSpan<bool> span_result)
{
  const int size = input_a.size();
  for (const int i : IndexRange(size)) {
    const bool a = input_a[i];
    const bool b = input_b[i];
    span_result[i] = a == b;
  }
}

static void do_not_equal_operation_float(const VArray<float> &input_a,
                                         const VArray<float> &input_b,
                                         const float threshold,
                                         MutableSpan<bool> span_result)
{
  const int size = input_a.size();
  for (const int i : IndexRange(size)) {
    const float a = input_a[i];
    const float b = input_b[i];
    span_result[i] = !compare_ff(a, b, threshold);
  }
}

static void do_not_equal_operation_float3(const VArray<float3> &input_a,
                                          const VArray<float3> &input_b,
                                          const float threshold,
                                          MutableSpan<bool> span_result)
{
  const float threshold_squared = pow2f(threshold);
  const int size = input_a.size();
  for (const int i : IndexRange(size)) {
    const float3 a = input_a[i];
    const float3 b = input_b[i];
    span_result[i] = len_squared_v3v3(a, b) >= threshold_squared;
  }
}

static void do_not_equal_operation_color4f(const VArray<ColorGeometry4f> &input_a,
                                           const VArray<ColorGeometry4f> &input_b,
                                           const float threshold,
                                           MutableSpan<bool> span_result)
{
  const float threshold_squared = pow2f(threshold);
  const int size = input_a.size();
  for (const int i : IndexRange(size)) {
    const ColorGeometry4f a = input_a[i];
    const ColorGeometry4f b = input_b[i];
    span_result[i] = len_squared_v4v4(a, b) >= threshold_squared;
  }
}

static void do_not_equal_operation_bool(const VArray<bool> &input_a,
                                        const VArray<bool> &input_b,
                                        const float UNUSED(threshold),
                                        MutableSpan<bool> span_result)
{
  const int size = input_a.size();
  for (const int i : IndexRange(size)) {
    const bool a = input_a[i];
    const bool b = input_b[i];
    span_result[i] = a != b;
  }
}

static CustomDataType get_data_type(GeometryComponent &component,
                                    const GeoNodeExecParams &params,
                                    const NodeAttributeCompare &node_storage)
{
  if (operation_tests_equality(node_storage)) {
    /* Convert the input attributes to the same data type for the equality tests. Use the higher
     * complexity attribute type, otherwise information necessary to the comparison may be lost. */
    return bke::attribute_data_type_highest_complexity({
        params.get_input_attribute_data_type("A", component, CD_PROP_FLOAT),
        params.get_input_attribute_data_type("B", component, CD_PROP_FLOAT),
    });
  }

  /* Use float compare for every operation besides equality. */
  return CD_PROP_FLOAT;
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

static void attribute_compare_calc(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  NodeAttributeCompare *node_storage = (NodeAttributeCompare *)node.storage;
  const FloatCompareOperation operation = static_cast<FloatCompareOperation>(
      node_storage->operation);
  const std::string result_name = params.get_input<std::string>("Result");

  const AttributeDomain result_domain = get_result_domain(component, params, result_name);

  OutputAttribute_Typed<bool> attribute_result = component.attribute_try_get_for_output_only<bool>(
      result_name, result_domain);
  if (!attribute_result) {
    return;
  }

  const CustomDataType input_data_type = get_data_type(component, params, *node_storage);

  GVArrayPtr attribute_a = params.get_input_attribute(
      "A", component, result_domain, input_data_type, nullptr);
  GVArrayPtr attribute_b = params.get_input_attribute(
      "B", component, result_domain, input_data_type, nullptr);

  if (!attribute_a || !attribute_b) {
    /* Attribute wasn't found. */
    return;
  }

  MutableSpan<bool> result_span = attribute_result.as_span();

  /* Use specific types for correct equality operations, but for other operations we use implicit
   * conversions and float comparison. In other words, the comparison is not element-wise. */
  if (operation_tests_equality(*node_storage)) {
    const float threshold = params.get_input<float>("Threshold");
    if (operation == NODE_FLOAT_COMPARE_EQUAL) {
      if (input_data_type == CD_PROP_FLOAT) {
        do_equal_operation_float(
            attribute_a->typed<float>(), attribute_b->typed<float>(), threshold, result_span);
      }
      else if (input_data_type == CD_PROP_FLOAT3) {
        do_equal_operation_float3(
            attribute_a->typed<float3>(), attribute_b->typed<float3>(), threshold, result_span);
      }
      else if (input_data_type == CD_PROP_COLOR) {
        do_equal_operation_color4f(attribute_a->typed<ColorGeometry4f>(),
                                   attribute_b->typed<ColorGeometry4f>(),
                                   threshold,
                                   result_span);
      }
      else if (input_data_type == CD_PROP_BOOL) {
        do_equal_operation_bool(
            attribute_a->typed<bool>(), attribute_b->typed<bool>(), threshold, result_span);
      }
    }
    else if (operation == NODE_FLOAT_COMPARE_NOT_EQUAL) {
      if (input_data_type == CD_PROP_FLOAT) {
        do_not_equal_operation_float(
            attribute_a->typed<float>(), attribute_b->typed<float>(), threshold, result_span);
      }
      else if (input_data_type == CD_PROP_FLOAT3) {
        do_not_equal_operation_float3(
            attribute_a->typed<float3>(), attribute_b->typed<float3>(), threshold, result_span);
      }
      else if (input_data_type == CD_PROP_COLOR) {
        do_not_equal_operation_color4f(attribute_a->typed<ColorGeometry4f>(),
                                       attribute_b->typed<ColorGeometry4f>(),
                                       threshold,
                                       result_span);
      }
      else if (input_data_type == CD_PROP_BOOL) {
        do_not_equal_operation_bool(
            attribute_a->typed<bool>(), attribute_b->typed<bool>(), threshold, result_span);
      }
    }
  }
  else {
    do_math_operation(
        attribute_a->typed<float>(), attribute_b->typed<float>(), operation, result_span);
  }

  attribute_result.save();
}

static void geo_node_attribute_compare_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    attribute_compare_calc(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    attribute_compare_calc(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }
  if (geometry_set.has<CurveComponent>()) {
    attribute_compare_calc(geometry_set.get_component_for_write<CurveComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_compare()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_ATTRIBUTE_COMPARE, "Attribute Compare", NODE_CLASS_ATTRIBUTE, 0);
  ntype.declare = blender::nodes::geo_node_attribute_compare_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_compare_exec;
  ntype.draw_buttons = blender::nodes::geo_node_attribute_compare_layout;
  node_type_update(&ntype, blender::nodes::geo_node_attribute_compare_update);
  node_type_storage(
      &ntype, "NodeAttributeCompare", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, blender::nodes::geo_node_attribute_compare_init);
  nodeRegisterType(&ntype);
}
