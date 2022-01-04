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

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_math_functions.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_legacy_attribute_vector_math_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("A"));
  b.add_input<decl::Vector>(N_("A"), "A_001");
  b.add_input<decl::String>(N_("B"));
  b.add_input<decl::Vector>(N_("B"), "B_001");
  b.add_input<decl::Float>(N_("B"), "B_002");
  b.add_input<decl::String>(N_("C"));
  b.add_input<decl::Vector>(N_("C"), "C_001");
  b.add_input<decl::Float>(N_("C"), "C_002");
  b.add_input<decl::String>(N_("Result"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static bool operation_use_input_b(const NodeVectorMathOperation operation)
{
  return !ELEM(operation,
               NODE_VECTOR_MATH_NORMALIZE,
               NODE_VECTOR_MATH_FLOOR,
               NODE_VECTOR_MATH_CEIL,
               NODE_VECTOR_MATH_FRACTION,
               NODE_VECTOR_MATH_ABSOLUTE,
               NODE_VECTOR_MATH_SINE,
               NODE_VECTOR_MATH_COSINE,
               NODE_VECTOR_MATH_TANGENT,
               NODE_VECTOR_MATH_LENGTH);
}

static bool operation_use_input_c(const NodeVectorMathOperation operation)
{
  return ELEM(operation,
              NODE_VECTOR_MATH_WRAP,
              NODE_VECTOR_MATH_REFRACT,
              NODE_VECTOR_MATH_FACEFORWARD,
              NODE_VECTOR_MATH_MULTIPLY_ADD);
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  const NodeAttributeVectorMath &node_storage = *(NodeAttributeVectorMath *)node->storage;
  const NodeVectorMathOperation operation = (const NodeVectorMathOperation)node_storage.operation;

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

static CustomDataType operation_get_read_type_b(const NodeVectorMathOperation operation)
{
  if (operation == NODE_VECTOR_MATH_SCALE) {
    return CD_PROP_FLOAT;
  }
  return CD_PROP_FLOAT3;
}

static CustomDataType operation_get_read_type_c(const NodeVectorMathOperation operation)
{
  if (operation == NODE_VECTOR_MATH_REFRACT) {
    return CD_PROP_FLOAT;
  }
  return CD_PROP_FLOAT3;
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeAttributeVectorMath *data = MEM_cnew<NodeAttributeVectorMath>(__func__);

  data->operation = NODE_VECTOR_MATH_ADD;
  data->input_type_a = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  data->input_type_b = GEO_NODE_ATTRIBUTE_INPUT_ATTRIBUTE;
  node->storage = data;
}

static CustomDataType operation_get_result_type(const NodeVectorMathOperation operation)
{
  switch (operation) {
    case NODE_VECTOR_MATH_ADD:
    case NODE_VECTOR_MATH_SUBTRACT:
    case NODE_VECTOR_MATH_MULTIPLY:
    case NODE_VECTOR_MATH_DIVIDE:
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
    case NODE_VECTOR_MATH_PROJECT:
    case NODE_VECTOR_MATH_REFLECT:
    case NODE_VECTOR_MATH_SCALE:
    case NODE_VECTOR_MATH_NORMALIZE:
    case NODE_VECTOR_MATH_SNAP:
    case NODE_VECTOR_MATH_FLOOR:
    case NODE_VECTOR_MATH_CEIL:
    case NODE_VECTOR_MATH_MODULO:
    case NODE_VECTOR_MATH_FRACTION:
    case NODE_VECTOR_MATH_ABSOLUTE:
    case NODE_VECTOR_MATH_MINIMUM:
    case NODE_VECTOR_MATH_MAXIMUM:
    case NODE_VECTOR_MATH_WRAP:
    case NODE_VECTOR_MATH_SINE:
    case NODE_VECTOR_MATH_COSINE:
    case NODE_VECTOR_MATH_TANGENT:
    case NODE_VECTOR_MATH_REFRACT:
    case NODE_VECTOR_MATH_FACEFORWARD:
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      return CD_PROP_FLOAT3;
    case NODE_VECTOR_MATH_DOT_PRODUCT:
    case NODE_VECTOR_MATH_DISTANCE:
    case NODE_VECTOR_MATH_LENGTH:
      return CD_PROP_FLOAT;
  }

  BLI_assert(false);
  return CD_PROP_FLOAT3;
}

static void geo_node_vector_math_label(const bNodeTree *UNUSED(ntree),
                                       const bNode *node,
                                       char *label,
                                       int maxlen)
{
  NodeAttributeMath &node_storage = *(NodeAttributeMath *)node->storage;
  const char *name;
  bool enum_label = RNA_enum_name(rna_enum_node_vec_math_items, node_storage.operation, &name);
  if (!enum_label) {
    name = "Unknown";
  }
  BLI_snprintf(label, maxlen, IFACE_("Vector %s"), IFACE_(name));
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeAttributeVectorMath *node_storage = (NodeAttributeVectorMath *)node->storage;
  const NodeVectorMathOperation operation = (const NodeVectorMathOperation)node_storage->operation;

  update_attribute_input_socket_availabilities(
      *ntree, *node, "A", (GeometryNodeAttributeInputMode)node_storage->input_type_a);
  update_attribute_input_socket_availabilities(
      *ntree,
      *node,
      "B",
      (GeometryNodeAttributeInputMode)node_storage->input_type_b,
      operation_use_input_b(operation));
  update_attribute_input_socket_availabilities(
      *ntree,
      *node,
      "C",
      (GeometryNodeAttributeInputMode)node_storage->input_type_c,
      operation_use_input_c(operation));
}

static void do_math_operation_fl3_fl3_to_fl3(const VArray<float3> &input_a,
                                             const VArray<float3> &input_b,
                                             const VMutableArray<float3> &result,
                                             const NodeVectorMathOperation operation)
{
  const int size = input_a.size();

  VArray_Span<float3> span_a{input_a};
  VArray_Span<float3> span_b{input_b};
  VMutableArray_Span<float3> span_result{result, false};

  bool success = try_dispatch_float_math_fl3_fl3_to_fl3(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
          for (const int i : range) {
            const float3 a = span_a[i];
            const float3 b = span_b[i];
            const float3 out = math_function(a, b);
            span_result[i] = out;
          }
        });
      });

  span_result.save();

  /* The operation is not supported by this node currently. */
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static void do_math_operation_fl3_fl3_fl3_to_fl3(const VArray<float3> &input_a,
                                                 const VArray<float3> &input_b,
                                                 const VArray<float3> &input_c,
                                                 const VMutableArray<float3> &result,
                                                 const NodeVectorMathOperation operation)
{
  const int size = input_a.size();

  VArray_Span<float3> span_a{input_a};
  VArray_Span<float3> span_b{input_b};
  VArray_Span<float3> span_c{input_c};
  VMutableArray_Span<float3> span_result{result};

  bool success = try_dispatch_float_math_fl3_fl3_fl3_to_fl3(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
          for (const int i : range) {
            const float3 a = span_a[i];
            const float3 b = span_b[i];
            const float3 c = span_c[i];
            const float3 out = math_function(a, b, c);
            span_result[i] = out;
          }
        });
      });

  span_result.save();

  /* The operation is not supported by this node currently. */
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static void do_math_operation_fl3_fl3_fl_to_fl3(const VArray<float3> &input_a,
                                                const VArray<float3> &input_b,
                                                const VArray<float> &input_c,
                                                const VMutableArray<float3> &result,
                                                const NodeVectorMathOperation operation)
{
  const int size = input_a.size();

  VArray_Span<float3> span_a{input_a};
  VArray_Span<float3> span_b{input_b};
  VArray_Span<float> span_c{input_c};
  VMutableArray_Span<float3> span_result{result, false};

  bool success = try_dispatch_float_math_fl3_fl3_fl_to_fl3(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
          for (const int i : range) {
            const float3 a = span_a[i];
            const float3 b = span_b[i];
            const float c = span_c[i];
            const float3 out = math_function(a, b, c);
            span_result[i] = out;
          }
        });
      });

  span_result.save();

  /* The operation is not supported by this node currently. */
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static void do_math_operation_fl3_fl3_to_fl(const VArray<float3> &input_a,
                                            const VArray<float3> &input_b,
                                            const VMutableArray<float> &result,
                                            const NodeVectorMathOperation operation)
{
  const int size = input_a.size();

  VArray_Span<float3> span_a{input_a};
  VArray_Span<float3> span_b{input_b};
  VMutableArray_Span<float> span_result{result, false};

  bool success = try_dispatch_float_math_fl3_fl3_to_fl(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
          for (const int i : range) {
            const float3 a = span_a[i];
            const float3 b = span_b[i];
            const float out = math_function(a, b);
            span_result[i] = out;
          }
        });
      });

  span_result.save();

  /* The operation is not supported by this node currently. */
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static void do_math_operation_fl3_fl_to_fl3(const VArray<float3> &input_a,
                                            const VArray<float> &input_b,
                                            const VMutableArray<float3> &result,
                                            const NodeVectorMathOperation operation)
{
  const int size = input_a.size();

  VArray_Span<float3> span_a{input_a};
  VArray_Span<float> span_b{input_b};
  VMutableArray_Span<float3> span_result{result, false};

  bool success = try_dispatch_float_math_fl3_fl_to_fl3(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
          for (const int i : range) {
            const float3 a = span_a[i];
            const float b = span_b[i];
            const float3 out = math_function(a, b);
            span_result[i] = out;
          }
        });
      });

  span_result.save();

  /* The operation is not supported by this node currently. */
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static void do_math_operation_fl3_to_fl3(const VArray<float3> &input_a,
                                         const VMutableArray<float3> &result,
                                         const NodeVectorMathOperation operation)
{
  const int size = input_a.size();

  VArray_Span<float3> span_a{input_a};
  VMutableArray_Span<float3> span_result{result, false};

  bool success = try_dispatch_float_math_fl3_to_fl3(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
          for (const int i : range) {
            const float3 in = span_a[i];
            const float3 out = math_function(in);
            span_result[i] = out;
          }
        });
      });

  span_result.save();

  /* The operation is not supported by this node currently. */
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static void do_math_operation_fl3_to_fl(const VArray<float3> &input_a,
                                        const VMutableArray<float> &result,
                                        const NodeVectorMathOperation operation)
{
  const int size = input_a.size();

  VArray_Span<float3> span_a{input_a};
  VMutableArray_Span<float> span_result{result, false};

  bool success = try_dispatch_float_math_fl3_to_fl(
      operation, [&](auto math_function, const FloatMathOperationInfo &UNUSED(info)) {
        threading::parallel_for(IndexRange(size), 512, [&](IndexRange range) {
          for (const int i : range) {
            const float3 in = span_a[i];
            const float out = math_function(in);
            span_result[i] = out;
          }
        });
      });

  span_result.save();

  /* The operation is not supported by this node currently. */
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);
}

static AttributeDomain get_result_domain(const GeometryComponent &component,
                                         const GeoNodeExecParams &params,
                                         const NodeVectorMathOperation operation,
                                         StringRef result_name)
{
  /* Use the domain of the result attribute if it already exists. */
  std::optional<AttributeMetaData> result_info = component.attribute_get_meta_data(result_name);
  if (result_info) {
    return result_info->domain;
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

static void attribute_vector_math_calc(GeometryComponent &component,
                                       const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const NodeAttributeVectorMath *node_storage = (const NodeAttributeVectorMath *)node.storage;
  const NodeVectorMathOperation operation = (NodeVectorMathOperation)node_storage->operation;
  const std::string result_name = params.get_input<std::string>("Result");

  /* The number and type of the input attribute depend on the operation. */
  const CustomDataType read_type_a = CD_PROP_FLOAT3;
  const bool use_input_b = operation_use_input_b(operation);
  const CustomDataType read_type_b = operation_get_read_type_b(operation);
  const bool use_input_c = operation_use_input_c(operation);
  const CustomDataType read_type_c = operation_get_read_type_c(operation);

  /* The result domain is always point for now. */
  const CustomDataType result_type = operation_get_result_type(operation);
  const AttributeDomain result_domain = get_result_domain(
      component, params, operation, result_name);

  GVArray attribute_a = params.get_input_attribute(
      "A", component, result_domain, read_type_a, nullptr);
  if (!attribute_a) {
    return;
  }
  GVArray attribute_b;
  GVArray attribute_c;
  if (use_input_b) {
    attribute_b = params.get_input_attribute("B", component, result_domain, read_type_b, nullptr);
    if (!attribute_b) {
      return;
    }
  }
  if (use_input_c) {
    attribute_c = params.get_input_attribute("C", component, result_domain, read_type_c, nullptr);
    if (!attribute_c) {
      return;
    }
  }

  /* Get result attribute first, in case it has to overwrite one of the existing attributes. */
  OutputAttribute attribute_result = component.attribute_try_get_for_output_only(
      result_name, result_domain, result_type);
  if (!attribute_result) {
    return;
  }

  switch (operation) {
    case NODE_VECTOR_MATH_ADD:
    case NODE_VECTOR_MATH_SUBTRACT:
    case NODE_VECTOR_MATH_MULTIPLY:
    case NODE_VECTOR_MATH_DIVIDE:
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
    case NODE_VECTOR_MATH_PROJECT:
    case NODE_VECTOR_MATH_REFLECT:
    case NODE_VECTOR_MATH_SNAP:
    case NODE_VECTOR_MATH_MODULO:
    case NODE_VECTOR_MATH_MINIMUM:
    case NODE_VECTOR_MATH_MAXIMUM:
      do_math_operation_fl3_fl3_to_fl3(attribute_a.typed<float3>(),
                                       attribute_b.typed<float3>(),
                                       attribute_result.varray().typed<float3>(),
                                       operation);
      break;
    case NODE_VECTOR_MATH_DOT_PRODUCT:
    case NODE_VECTOR_MATH_DISTANCE:
      do_math_operation_fl3_fl3_to_fl(attribute_a.typed<float3>(),
                                      attribute_b.typed<float3>(),
                                      attribute_result.varray().typed<float>(),
                                      operation);
      break;
    case NODE_VECTOR_MATH_LENGTH:
      do_math_operation_fl3_to_fl(
          attribute_a.typed<float3>(), attribute_result.varray().typed<float>(), operation);
      break;
    case NODE_VECTOR_MATH_SCALE:
      do_math_operation_fl3_fl_to_fl3(attribute_a.typed<float3>(),
                                      attribute_b.typed<float>(),
                                      attribute_result.varray().typed<float3>(),
                                      operation);
      break;
    case NODE_VECTOR_MATH_NORMALIZE:
    case NODE_VECTOR_MATH_FLOOR:
    case NODE_VECTOR_MATH_CEIL:
    case NODE_VECTOR_MATH_FRACTION:
    case NODE_VECTOR_MATH_ABSOLUTE:
    case NODE_VECTOR_MATH_SINE:
    case NODE_VECTOR_MATH_COSINE:
    case NODE_VECTOR_MATH_TANGENT:
      do_math_operation_fl3_to_fl3(
          attribute_a.typed<float3>(), attribute_result.varray().typed<float3>(), operation);
      break;
    case NODE_VECTOR_MATH_WRAP:
    case NODE_VECTOR_MATH_FACEFORWARD:
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      do_math_operation_fl3_fl3_fl3_to_fl3(attribute_a.typed<float3>(),
                                           attribute_b.typed<float3>(),
                                           attribute_c.typed<float3>(),
                                           attribute_result.varray().typed<float3>(),
                                           operation);
      break;
    case NODE_VECTOR_MATH_REFRACT:
      do_math_operation_fl3_fl3_fl_to_fl3(attribute_a.typed<float3>(),
                                          attribute_b.typed<float3>(),
                                          attribute_c.typed<float>(),
                                          attribute_result.varray().typed<float3>(),
                                          operation);
      break;
  }
  attribute_result.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    attribute_vector_math_calc(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    attribute_vector_math_calc(geometry_set.get_component_for_write<PointCloudComponent>(),
                               params);
  }
  if (geometry_set.has<CurveComponent>()) {
    attribute_vector_math_calc(geometry_set.get_component_for_write<CurveComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes::node_geo_legacy_attribute_vector_math_cc

void register_node_type_geo_attribute_vector_math()
{
  namespace file_ns = blender::nodes::node_geo_legacy_attribute_vector_math_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_LEGACY_ATTRIBUTE_VECTOR_MATH,
                     "Attribute Vector Math",
                     NODE_CLASS_ATTRIBUTE);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.labelfunc = file_ns::geo_node_vector_math_label;
  node_type_update(&ntype, file_ns::node_update);
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(
      &ntype, "NodeAttributeVectorMath", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}
