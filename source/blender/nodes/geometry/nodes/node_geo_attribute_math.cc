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

static bNodeSocketTemplate geo_node_attribute_math_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("A")},
    {SOCK_FLOAT, N_("A"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_STRING, N_("B")},
    {SOCK_FLOAT, N_("B"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_STRING, N_("C")},
    {SOCK_FLOAT, N_("C"), 0.0f, 0.0f, 0.0f, 0.0f, -FLT_MAX, FLT_MAX},
    {SOCK_STRING, N_("Result")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_attribute_math_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static bool operation_use_input_c(const NodeMathOperation operation)
{
  return ELEM(operation,
              NODE_MATH_MULTIPLY_ADD,
              NODE_MATH_SMOOTH_MIN,
              NODE_MATH_SMOOTH_MAX,
              NODE_MATH_WRAP,
              NODE_MATH_COMPARE);
}

static bool operation_use_input_b(const NodeMathOperation operation)
{
  switch (operation) {
    case NODE_MATH_ADD:
    case NODE_MATH_SUBTRACT:
    case NODE_MATH_MULTIPLY:
    case NODE_MATH_DIVIDE:
    case NODE_MATH_POWER:
    case NODE_MATH_LOGARITHM:
    case NODE_MATH_MINIMUM:
    case NODE_MATH_MAXIMUM:
    case NODE_MATH_LESS_THAN:
    case NODE_MATH_GREATER_THAN:
    case NODE_MATH_MODULO:
    case NODE_MATH_ARCTAN2:
    case NODE_MATH_SNAP:
    case NODE_MATH_WRAP:
    case NODE_MATH_COMPARE:
    case NODE_MATH_MULTIPLY_ADD:
    case NODE_MATH_PINGPONG:
    case NODE_MATH_SMOOTH_MIN:
    case NODE_MATH_SMOOTH_MAX:
      return true;
    case NODE_MATH_SINE:
    case NODE_MATH_COSINE:
    case NODE_MATH_TANGENT:
    case NODE_MATH_ARCSINE:
    case NODE_MATH_ARCCOSINE:
    case NODE_MATH_ARCTANGENT:
    case NODE_MATH_ROUND:
    case NODE_MATH_ABSOLUTE:
    case NODE_MATH_FLOOR:
    case NODE_MATH_CEIL:
    case NODE_MATH_FRACTION:
    case NODE_MATH_SQRT:
    case NODE_MATH_INV_SQRT:
    case NODE_MATH_SIGN:
    case NODE_MATH_EXPONENT:
    case NODE_MATH_RADIANS:
    case NODE_MATH_DEGREES:
    case NODE_MATH_SINH:
    case NODE_MATH_COSH:
    case NODE_MATH_TANH:
    case NODE_MATH_TRUNC:
      return false;
  }
  BLI_assert(false);
  return false;
}

static void geo_node_attribute_math_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  NodeAttributeMath *node_storage = (NodeAttributeMath *)node->storage;
  NodeMathOperation operation = (NodeMathOperation)node_storage->operation;

  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "input_type_a", 0, IFACE_("A"), ICON_NONE);
  if (operation_use_input_b(operation)) {
    uiItemR(layout, ptr, "input_type_b", 0, IFACE_("B"), ICON_NONE);
  }
  if (operation_use_input_c(operation)) {
    uiItemR(layout, ptr, "input_type_c", 0, IFACE_("C"), ICON_NONE);
  }
}

static void geo_node_attribute_math_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeMath *data = (NodeAttributeMath *)MEM_callocN(sizeof(NodeAttributeMath), __func__);

  data->operation = NODE_MATH_ADD;
  data->input_type_a = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  data->input_type_b = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  data->input_type_c = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  node->storage = data;
}

namespace blender::nodes {

static void geo_node_attribute_math_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeAttributeMath &node_storage = *(NodeAttributeMath *)node->storage;
  NodeMathOperation operation = static_cast<NodeMathOperation>(node_storage.operation);

  update_attribute_input_socket_availabilities(
      *node, "A", (GeometryNodeAttributeInputMode)node_storage.input_type_a);
  update_attribute_input_socket_availabilities(
      *node,
      "B",
      (GeometryNodeAttributeInputMode)node_storage.input_type_b,
      operation_use_input_b(operation));
  update_attribute_input_socket_availabilities(
      *node,
      "C",
      (GeometryNodeAttributeInputMode)node_storage.input_type_c,
      operation_use_input_c(operation));
}

static void do_math_operation(Span<float> span_a,
                              Span<float> span_b,
                              Span<float> span_c,
                              MutableSpan<float> span_result,
                              const NodeMathOperation operation)
{
  bool success = try_dispatch_float_math_fl_fl_fl_to_fl(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        for (const int i : IndexRange(span_result.size())) {
          span_result[i] = math_function(span_a[i], span_b[i], span_c[i]);
        }
      });
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static void do_math_operation(Span<float> span_a,
                              Span<float> span_b,
                              MutableSpan<float> span_result,
                              const NodeMathOperation operation)
{
  bool success = try_dispatch_float_math_fl_fl_to_fl(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        for (const int i : IndexRange(span_result.size())) {
          span_result[i] = math_function(span_a[i], span_b[i]);
        }
      });
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static void do_math_operation(Span<float> span_input,
                              MutableSpan<float> span_result,
                              const NodeMathOperation operation)
{
  bool success = try_dispatch_float_math_fl_to_fl(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        for (const int i : IndexRange(span_result.size())) {
          span_result[i] = math_function(span_input[i]);
        }
      });
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         const GeoNodeExecParams &params,
                                         const NodeMathOperation operation,
                                         StringRef result_name)
{
  /* Use the domain of the result attribute if it already exists. */
  ReadAttributePtr result_attribute = component.attribute_try_get_for_read(result_name);
  if (result_attribute) {
    return result_attribute->domain();
  }

  /* Otherwise use the highest priority domain from existing input attributes, or the default. */
  const AttributeDomain default_domain = ATTR_DOMAIN_POINT;
  if (operation_use_input_b(operation)) {
    if (operation_use_input_c(operation)) {
      return params.get_highest_priority_input_domain({"A", "B", "C"}, component, default_domain);
    }
    return params.get_highest_priority_input_domain({"A", "B"}, component, default_domain);
  }
  return params.get_highest_priority_input_domain({"A"}, component, default_domain);
}

static void attribute_math_calc(GeometryComponent &component, const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const NodeAttributeMath *node_storage = (const NodeAttributeMath *)node.storage;
  const NodeMathOperation operation = static_cast<NodeMathOperation>(node_storage->operation);
  const std::string result_name = params.get_input<std::string>("Result");

  /* The result type of this node is always float. */
  const CustomDataType result_type = CD_PROP_FLOAT;
  const AttributeDomain result_domain = get_result_domain(
      component, params, operation, result_name);

  OutputAttributePtr attribute_result = component.attribute_try_get_for_output(
      result_name, result_domain, result_type);
  if (!attribute_result) {
    return;
  }

  ReadAttributePtr attribute_a = params.get_input_attribute(
      "A", component, result_domain, result_type, nullptr);
  if (!attribute_a) {
    return;
  }

  /* Note that passing the data with `get_span<float>()` works
   * because the attributes were accessed with #CD_PROP_FLOAT. */
  if (operation_use_input_b(operation)) {
    ReadAttributePtr attribute_b = params.get_input_attribute(
        "B", component, result_domain, result_type, nullptr);
    if (!attribute_b) {
      return;
    }
    if (operation_use_input_c(operation)) {
      ReadAttributePtr attribute_c = params.get_input_attribute(
          "C", component, result_domain, result_type, nullptr);
      if (!attribute_c) {
        return;
      }
      do_math_operation(attribute_a->get_span<float>(),
                        attribute_b->get_span<float>(),
                        attribute_c->get_span<float>(),
                        attribute_result->get_span_for_write_only<float>(),
                        operation);
    }
    else {
      do_math_operation(attribute_a->get_span<float>(),
                        attribute_b->get_span<float>(),
                        attribute_result->get_span_for_write_only<float>(),
                        operation);
    }
  }
  else {
    do_math_operation(attribute_a->get_span<float>(),
                      attribute_result->get_span_for_write_only<float>(),
                      operation);
  }

  attribute_result.apply_span_and_save();
}

static void geo_node_attribute_math_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    attribute_math_calc(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    attribute_math_calc(geometry_set.get_component_for_write<PointCloudComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_attribute_math()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_ATTRIBUTE_MATH, "Attribute Math", NODE_CLASS_ATTRIBUTE, 0);
  node_type_socket_templates(&ntype, geo_node_attribute_math_in, geo_node_attribute_math_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_attribute_math_exec;
  ntype.draw_buttons = geo_node_attribute_math_layout;
  node_type_update(&ntype, blender::nodes::geo_node_attribute_math_update);
  node_type_init(&ntype, geo_node_attribute_math_init);
  node_type_storage(
      &ntype, "NodeAttributeMath", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
