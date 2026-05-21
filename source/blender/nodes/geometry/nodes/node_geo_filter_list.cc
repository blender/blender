/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "NOD_geometry_nodes_list.hh"
#include "NOD_geometry_nodes_values.hh"
#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "list_function_eval.hh"
#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_filter_list_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const auto type = eNodeSocketDatatype(node->custom1);
  b.add_input(type, "List"_ustr).structure_type(StructureType::List).hide_value();
  b.add_input<decl::Bool>("Selection"_ustr)
      .default_value(true)
      .hide_value()
      .description("A field or list representing the values that will not be removed")
      .structure_type(StructureType::Dynamic);
  b.add_output(type, "Selection"_ustr)
      .dependent_field({1})
      .structure_type(StructureType::List)
      .align_with_previous();
  b.add_output(type, "Inverted"_ustr)
      .dependent_field({1})
      .structure_type(StructureType::List)
      .align_with_previous();
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "socket_type", UI_ITEM_NONE, "", ICON_NONE);
}

class SocketSearchOp {
 public:
  UString socket_name;
  eNodeSocketDatatype socket_type;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("GeometryNodeFilterList"_ustr);
    node.custom1 = socket_type;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(params.other_socket().type);
  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(socket_type, SOCK_BOOLEAN)) {
      params.add_item(IFACE_("Selection"), SocketSearchOp{"Selection"_ustr, SOCK_BOOLEAN});
    }
    params.add_item(IFACE_("List"), SocketSearchOp{"List"_ustr, socket_type});
  }
  else {
    params.add_item(IFACE_("Selection"), SocketSearchOp{"Selection"_ustr, socket_type});
    params.add_item(IFACE_("Inverted"), SocketSearchOp{"Inverted"_ustr, socket_type});
  }
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(
      srna,
      "socket_type",
      "Socket Type",
      "",
      rna_enum_node_socket_data_type_items,
      NOD_inline_enum_accessors(custom1),
      SOCK_FLOAT,
      [](bContext * /*C*/, PointerRNA *ptr, PropertyRNA * /*prop*/, bool *r_free) {
        *r_free = true;
        const bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
        bke::bNodeTreeType *ntree_type = ntree.typeinfo;
        return enum_items_filter(
            rna_enum_node_socket_data_type_items, [&](const EnumPropertyItem &item) -> bool {
              bke::bNodeSocketType *socket_type = bke::node_socket_type_find_static(item.value);
              return ntree_type->valid_socket_type(ntree_type, socket_type);
            });
      });
}

static GListPtr filter_list(const GListPtr &list, const IndexMask &mask)
{
  if (mask.size() == list->size()) {
    return list;
  }
  const CPPType &list_type = list->cpp_type();
  return std::visit(
      [&]<typename T>(const T &src_data) {
        if constexpr (std::is_same_v<T, GList::ArrayData>) {
          GArray<> dst_data(list_type, mask.size(), NoInitialization());
          array_utils::gather(GSpan(list_type, src_data.data, list->size()), mask, dst_data);
          return GList::from_garray(std::move(dst_data));
        }
        else if constexpr (std::is_same_v<T, GList::SingleData>) {
          return GList::create(list_type, src_data, mask.size());
        }
      },
      list->data());
}

static void output_lists(GeoNodeExecParams &params,
                         const GListPtr &list,
                         const IndexMask &selection)
{
  if (params.output_is_required("Selection"_ustr)) {
    params.set_output("Selection"_ustr, filter_list(list, selection));
  }
  if (params.output_is_required("Inverted"_ustr)) {
    IndexMaskMemory memory;
    const IndexMask inverted = selection.complement(IndexRange(list->size()), memory);
    params.set_output("Inverted"_ustr, filter_list(list, inverted));
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GListPtr list = params.extract_input<GListPtr>("List"_ustr);
  if (!list) {
    params.set_default_remaining_outputs();
    return;
  }
  auto filter_value = params.extract_input<bke::SocketValueVariant>("Selection"_ustr);
  if (filter_value.is_single()) {
    if (filter_value.get<bool>()) {
      output_lists(params, list, IndexMask(list->size()));
    }
    else {
      output_lists(params, list, {});
    }
  }
  else if (filter_value.is_context_dependent_field()) {
    ListFieldContext field_context;
    fn::FieldEvaluator field_evaluator(field_context, list->size());
    field_evaluator.add(filter_value.extract<Field<bool>>());
    field_evaluator.evaluate();
    output_lists(params, list, field_evaluator.get_evaluated_as_mask(0));
  }
  else if (filter_value.is_list()) {
    const GListPtr keep_list = filter_value.get<GListPtr>();
    const VArray<bool> values = keep_list->varray().typed<bool>();
    if (values.size() < list->size()) {
      params.error_message_add(NodeWarningType::Error, "\"Selection\" list is too small");
      params.set_default_remaining_outputs();
      return;
    }
    IndexMaskMemory memory;
    output_lists(params, list, IndexMask::from_bools(values, memory));
  }
  else {
    params.error_message_add(NodeWarningType::Warning,
                             "\"Selection\" input must be a field or a list");
    params.set_output("Selection"_ustr, std::move(list));
    params.set_output("Inverted"_ustr, GList::from_garray(GArray<>(list->cpp_type(), 0)));
    params.set_default_remaining_outputs();
  }
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeFilterList"_ustr);
  ntype.ui_name = "Filter List";
  ntype.ui_description = "Remove items from a list";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_searches;
  bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_filter_list_cc
