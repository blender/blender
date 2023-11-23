/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BKE_attribute_math.hh"
#include "BKE_grease_pencil.hh"

#include "BLI_task.hh"

#include "RNA_enum_types.hh"

#include "NOD_socket_search_link.hh"

namespace blender::nodes::node_geo_evaluate_on_domain_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom2);
    b.add_input(data_type, "Value").supports_field();

    b.add_output(data_type, "Value").field_source_reference_all();
  }
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

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeType &node_type = params.node_type();
  const std::optional<eCustomDataType> type = bke::socket_type_to_custom_data_type(
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
    const eAttrDomain dst_domain = context.domain();
    const int dst_domain_size = context.attributes()->domain_size(dst_domain);
    const CPPType &cpp_type = src_field_.cpp_type();

    if (context.type() == GeometryComponent::Type::GreasePencil &&
        (src_domain_ == ATTR_DOMAIN_LAYER) != (dst_domain == ATTR_DOMAIN_LAYER))
    {
      /* Evaluate field just for the current layer. */
      if (src_domain_ == ATTR_DOMAIN_LAYER) {
        const bke::GeometryFieldContext src_domain_context{context, ATTR_DOMAIN_LAYER};
        const int layer_index = context.grease_pencil_layer_index();

        const IndexMask single_layer_mask = IndexRange(layer_index, 1);
        FieldEvaluator value_evaluator{src_domain_context, &single_layer_mask};
        value_evaluator.add(src_field_);
        value_evaluator.evaluate();

        const GVArray &values = value_evaluator.get_evaluated(0);

        BUFFER_FOR_CPP_TYPE_VALUE(cpp_type, value);
        BLI_SCOPED_DEFER([&]() { cpp_type.destruct(value); });
        values.get_to_uninitialized(layer_index, value);
        return GVArray::ForSingle(cpp_type, dst_domain_size, value);
      }
      /* We don't adapt from curve to layer domain currently. */
      return GVArray::ForSingleDefault(cpp_type, dst_domain_size);
    }

    const bke::AttributeAccessor attributes = *context.attributes();

    const bke::GeometryFieldContext other_domain_context{context, src_domain_};
    const int64_t src_domain_size = attributes.domain_size(src_domain_);
    GArray<> values(cpp_type, src_domain_size);
    FieldEvaluator value_evaluator{other_domain_context, src_domain_size};
    value_evaluator.add_with_destination(src_field_, values.as_mutable_span());
    value_evaluator.evaluate();
    return attributes.adapt_domain(GVArray::ForGArray(std::move(values)), src_domain_, dst_domain);
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

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const eAttrDomain domain = eAttrDomain(node.custom1);

  GField src_field = params.extract_input<GField>("Value");
  GField dst_field{std::make_shared<EvaluateOnDomainInput>(std::move(src_field), domain)};
  params.set_output<GField>("Value", std::move(dst_field));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Domain the field is evaluated in",
                    rna_enum_attribute_domain_items,
                    NOD_inline_enum_accessors(custom1),
                    ATTR_DOMAIN_POINT,
                    enums::domain_experimental_grease_pencil_version3_fn);

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
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_searches;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_evaluate_on_domain_cc
