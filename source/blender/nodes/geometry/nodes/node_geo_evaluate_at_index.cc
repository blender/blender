/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_attribute_math.hh"

#include "BLI_task.hh"

#include "NOD_socket_search_link.hh"

namespace blender::nodes {

EvaluateAtIndexInput::EvaluateAtIndexInput(Field<int> index_field,
                                           GField value_field,
                                           eAttrDomain value_field_domain)
    : bke::GeometryFieldInput(value_field.cpp_type(), "Evaluate at Index"),
      index_field_(std::move(index_field)),
      value_field_(std::move(value_field)),
      value_field_domain_(value_field_domain)
{
}

GVArray EvaluateAtIndexInput::get_varray_for_context(const bke::GeometryFieldContext &context,
                                                     const IndexMask &mask) const
{
  const std::optional<AttributeAccessor> attributes = context.attributes();
  if (!attributes) {
    return {};
  }

  const bke::GeometryFieldContext value_context{
      context.geometry(), context.type(), value_field_domain_};
  FieldEvaluator value_evaluator{value_context, attributes->domain_size(value_field_domain_)};
  value_evaluator.add(value_field_);
  value_evaluator.evaluate();
  const GVArray &values = value_evaluator.get_evaluated(0);

  FieldEvaluator index_evaluator{context, &mask};
  index_evaluator.add(index_field_);
  index_evaluator.evaluate();
  const VArray<int> indices = index_evaluator.get_evaluated<int>(0);

  GArray<> dst_array(values.type(), mask.min_array_size());
  copy_with_checked_indices(values, indices, mask, dst_array);
  return GVArray::ForGArray(std::move(dst_array));
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_evaluate_at_index_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Index").min(0).supports_field();

  b.add_input<decl::Float>("Value", "Value_Float").hide_value().supports_field();
  b.add_input<decl::Int>("Value", "Value_Int").hide_value().supports_field();
  b.add_input<decl::Vector>("Value", "Value_Vector").hide_value().supports_field();
  b.add_input<decl::Color>("Value", "Value_Color").hide_value().supports_field();
  b.add_input<decl::Bool>("Value", "Value_Bool").hide_value().supports_field();
  b.add_input<decl::Rotation>("Value", "Value_Rotation").hide_value().supports_field();

  b.add_output<decl::Float>("Value", "Value_Float").field_source_reference_all();
  b.add_output<decl::Int>("Value", "Value_Int").field_source_reference_all();
  b.add_output<decl::Vector>("Value", "Value_Vector").field_source_reference_all();
  b.add_output<decl::Color>("Value", "Value_Color").field_source_reference_all();
  b.add_output<decl::Bool>("Value", "Value_Bool").field_source_reference_all();
  b.add_output<decl::Rotation>("Value", "Value_Rotation").field_source_reference_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "domain", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = ATTR_DOMAIN_POINT;
  node->custom2 = CD_PROP_FLOAT;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const eCustomDataType data_type = eCustomDataType(node->custom2);

  bNodeSocket *sock_index = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *sock_in_float = sock_index->next;
  bNodeSocket *sock_in_int = sock_in_float->next;
  bNodeSocket *sock_in_vector = sock_in_int->next;
  bNodeSocket *sock_in_color = sock_in_vector->next;
  bNodeSocket *sock_in_bool = sock_in_color->next;
  bNodeSocket *sock_in_quat = sock_in_bool->next;

  bNodeSocket *sock_out_float = static_cast<bNodeSocket *>(node->outputs.first);
  bNodeSocket *sock_out_int = sock_out_float->next;
  bNodeSocket *sock_out_vector = sock_out_int->next;
  bNodeSocket *sock_out_color = sock_out_vector->next;
  bNodeSocket *sock_out_bool = sock_out_color->next;
  bNodeSocket *sock_out_quat = sock_out_bool->next;

  bke::nodeSetSocketAvailability(ntree, sock_in_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, sock_in_int, data_type == CD_PROP_INT32);
  bke::nodeSetSocketAvailability(ntree, sock_in_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, sock_in_color, data_type == CD_PROP_COLOR);
  bke::nodeSetSocketAvailability(ntree, sock_in_bool, data_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, sock_in_quat, data_type == CD_PROP_QUATERNION);

  bke::nodeSetSocketAvailability(ntree, sock_out_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, sock_out_int, data_type == CD_PROP_INT32);
  bke::nodeSetSocketAvailability(ntree, sock_out_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, sock_out_color, data_type == CD_PROP_COLOR);
  bke::nodeSetSocketAvailability(ntree, sock_out_bool, data_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, sock_out_quat, data_type == CD_PROP_QUATERNION);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeType &node_type = params.node_type();
  const std::optional<eCustomDataType> type = node_data_type_to_custom_data_type(
      eNodeSocketDatatype(params.other_socket().type));
  if (type && *type != CD_PROP_STRING) {
    params.add_item(IFACE_("Value"), [node_type, type](LinkSearchOpParams &params) {
      bNode &node = params.add_node(node_type);
      node.custom2 = *type;
      params.update_and_connect_available_socket(node, "Value");
    });
    params.add_item(
        IFACE_("Index"),
        [node_type, type](LinkSearchOpParams &params) {
          bNode &node = params.add_node(node_type);
          node.custom2 = *type;
          params.update_and_connect_available_socket(node, "Index");
        },
        -1);
  }
}

static StringRefNull identifier_suffix(eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_BOOL:
      return "Bool";
    case CD_PROP_FLOAT:
      return "Float";
    case CD_PROP_INT32:
      return "Int";
    case CD_PROP_COLOR:
      return "Color";
    case CD_PROP_FLOAT3:
      return "Vector";
    case CD_PROP_QUATERNION:
      return "Rotation";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const eAttrDomain domain = eAttrDomain(node.custom1);
  const eCustomDataType data_type = eCustomDataType(node.custom2);

  bke::attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    static const std::string identifier = "Value_" + identifier_suffix(data_type);
    Field<T> output_field{
        std::make_shared<EvaluateAtIndexInput>(params.extract_input<Field<int>>("Index"),
                                               params.extract_input<Field<T>>(identifier),
                                               domain)};
    params.set_output(identifier, std::move(output_field));
  });
}

}  // namespace blender::nodes::node_geo_evaluate_at_index_cc

void register_node_type_geo_evaluate_at_index()
{
  namespace file_ns = blender::nodes::node_geo_evaluate_at_index_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_EVALUATE_AT_INDEX, "Evaluate at Index", NODE_CLASS_CONVERTER);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
