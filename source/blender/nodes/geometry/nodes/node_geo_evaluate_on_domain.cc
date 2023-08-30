/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BKE_attribute_math.hh"

#include "BLI_task.hh"

#include "RNA_enum_types.hh"

#include "NOD_socket_search_link.hh"

namespace blender::nodes::node_geo_evaluate_on_domain_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Value", "Value_Float").supports_field();
  b.add_input<decl::Int>("Value", "Value_Int").supports_field();
  b.add_input<decl::Vector>("Value", "Value_Vector").supports_field();
  b.add_input<decl::Color>("Value", "Value_Color").supports_field();
  b.add_input<decl::Bool>("Value", "Value_Bool").supports_field();
  b.add_input<decl::Rotation>("Value", "Value_Rotation").supports_field();

  b.add_output<decl::Float>("Value", "Value_Float").field_source_reference_all();
  b.add_output<decl::Int>("Value", "Value_Int").field_source_reference_all();
  b.add_output<decl::Vector>("Value", "Value_Vector").field_source_reference_all();
  b.add_output<decl::Color>("Value", "Value_Color").field_source_reference_all();
  b.add_output<decl::Bool>("Value", "Value_Bool").field_source_reference_all();
  b.add_output<decl::Rotation>("Value", "Value_Rotation").field_source_reference_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = ATTR_DOMAIN_POINT;
  node->custom2 = CD_PROP_FLOAT;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const eCustomDataType data_type = eCustomDataType(node->custom2);

  bNodeSocket *sock_in_float = static_cast<bNodeSocket *>(node->inputs.first);
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
  }
}

class EvaluateOnDomainInput final : public bke::GeometryFieldInput {
 private:
  GField src_field_;
  eAttrDomain src_domain_;

 public:
  EvaluateOnDomainInput(GField field, eAttrDomain domain)
      : bke::GeometryFieldInput(field.cpp_type(), "Evaluate on Domain"),
        src_field_(std::move(field)),
        src_domain_(domain)
  {
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask & /*mask*/) const final
  {
    const bke::AttributeAccessor attributes = *context.attributes();

    const bke::GeometryFieldContext other_domain_context{
        context.geometry(), context.type(), src_domain_};
    const int64_t src_domain_size = attributes.domain_size(src_domain_);
    GArray<> values(src_field_.cpp_type(), src_domain_size);
    FieldEvaluator value_evaluator{other_domain_context, src_domain_size};
    value_evaluator.add_with_destination(src_field_, values.as_mutable_span());
    value_evaluator.evaluate();
    return attributes.adapt_domain(
        GVArray::ForGArray(std::move(values)), src_domain_, context.domain());
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    src_field_.node().for_each_field_input_recursive(fn);
  }

  std::optional<eAttrDomain> preferred_domain(
      const GeometryComponent & /*component*/) const override
  {
    return src_domain_;
  }
};

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
    Field<T> src_field = params.extract_input<Field<T>>(identifier);
    Field<T> dst_field{std::make_shared<EvaluateOnDomainInput>(std::move(src_field), domain)};
    params.set_output(identifier, std::move(dst_field));
  });
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Domain the field is evaluated in",
                    rna_enum_attribute_domain_items,
                    NOD_inline_enum_accessors(custom1),
                    ATTR_DOMAIN_POINT);

  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "",
                    rna_enum_attribute_type_items,
                    NOD_inline_enum_accessors(custom2),
                    CD_PROP_FLOAT,
                    enums::attribute_type_type_with_socket_fn);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_EVALUATE_ON_DOMAIN, "Evaluate on Domain", NODE_CLASS_CONVERTER);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.updatefunc = node_update;
  ntype.gather_link_search_ops = node_gather_link_searches;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_evaluate_on_domain_cc
