/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_list.hh"
#include "NOD_geometry_nodes_values.hh"
#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_list_get_item_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  if (node != nullptr) {
    const eNodeSocketDatatype type = eNodeSocketDatatype(node->custom1);
    b.add_input(type, "List").structure_type(StructureType::List).hide_value();
  }

  b.add_input<decl::Int>("Index").min(0).structure_type(StructureType::Dynamic);

  if (node != nullptr) {
    const eNodeSocketDatatype type = eNodeSocketDatatype(node->custom1);
    b.add_output(type, "Value").dependent_field({1});
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

class SocketSearchOp {
 public:
  const StringRef socket_name;
  eNodeSocketDatatype socket_type;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("GeometryNodeListGetItem");
    node.custom1 = socket_type;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!U.experimental.use_geometry_nodes_lists) {
    return;
  }
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(params.other_socket().type);
  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(socket_type, SOCK_INT)) {
      params.add_item(IFACE_("Index"), SocketSearchOp{"Index", SOCK_INT});
    }
    params.add_item(IFACE_("List"), SocketSearchOp{"List", socket_type});
  }
  else {
    params.add_item(IFACE_("Value"), SocketSearchOp{"Value", socket_type});
  }
}

class SampleIndexFunction : public mf::MultiFunction {
  ListPtr list_;
  mf::Signature signature_;

 public:
  SampleIndexFunction(ListPtr list) : list_(std::move(list))
  {
    mf::SignatureBuilder builder{"Sample Index", signature_};
    builder.single_input<int>("Index");
    builder.single_output("Value", list_->cpp_type());
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<int> &indices = params.readonly_single_input<int>(0, "Index");
    GMutableSpan dst = params.uninitialized_single_output(1, "Value");
    const List::DataVariant &data = list_->data();
    if (const auto *array_data = std::get_if<nodes::List::ArrayData>(&data)) {
      const GSpan span(list_->cpp_type(), array_data->data, list_->size());
      bke::copy_with_checked_indices(GVArray::from_span(span), indices, mask, dst);
    }
    else if (const auto *single_data = std::get_if<nodes::List::SingleData>(&data)) {
      list_->cpp_type().fill_construct_indices(single_data->value, dst.data(), mask);
    }
  }
};

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(
      srna,
      "data_type",
      "Data Type",
      "",
      rna_enum_node_socket_data_type_items,
      NOD_inline_enum_accessors(custom1),
      SOCK_GEOMETRY,
      [](bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free) {
        *r_free = true;
        return enum_items_filter(
            rna_enum_node_socket_data_type_items, [](const EnumPropertyItem &item) -> bool {
              return socket_type_supports_fields(eNodeSocketDatatype(item.value));
            });
      });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  bke::SocketValueVariant index = params.extract_input<bke::SocketValueVariant>("Index");
  ListPtr list = params.extract_input<ListPtr>("List");
  if (!list) {
    params.set_default_remaining_outputs();
    return;
  }

  std::string error_message;
  bke::SocketValueVariant output_value;
  if (!execute_multi_function_on_value_variant(
          std::make_shared<SampleIndexFunction>(std::move(list)),
          {&index},
          {&output_value},
          params.user_data(),
          error_message))
  {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, std::move(error_message));
    return;
  }

  params.set_output("Value", std::move(output_value));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeListGetItem");
  ntype.ui_name = "Get List Item";
  ntype.ui_description = "Retrieve a value from a list";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_list_get_item_cc
