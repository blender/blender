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

NODE_STORAGE_FUNCS(NodeGeometryListGetItem);

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }

  const NodeGeometryListGetItem &storage = node_storage(*node);
  const auto type = eNodeSocketDatatype(storage.socket_type);

  const auto structure_type = storage.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO ?
                                  StructureType::Dynamic :
                                  StructureType(storage.structure_type);

  b.add_input(type, "List").structure_type(StructureType::List).hide_value();

  b.add_input<decl::Int>("Index").min(0).structure_type(StructureType::Dynamic);

  b.add_output(type, "Value").dependent_field({1}).structure_type(structure_type);
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "socket_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_layout_ex(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  layout.prop(ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_new<NodeGeometryListGetItem>(__func__);
  node->storage = storage;
}

class SocketSearchOp {
 public:
  const StringRef socket_name;
  eNodeSocketDatatype socket_type;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("GeometryNodeListGetItem");
    node_storage(node).socket_type = socket_type;
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

    const IndexRange list_range(list_->size());

    IndexMaskMemory memory;
    const IndexMask valid_indices = [&]() {
      if (const std::optional<int> index = indices.get_if_single()) {
        return list_range.contains(*index) ? mask : IndexMask{};
      }
      if (indices.is_span()) {
        const Span<int> indices_span = indices.get_internal_span();
        return IndexMask::from_predicate(mask, GrainSize(4096), memory, [&](const int i) {
          return list_range.contains(indices_span[i]);
        });
      }
      return IndexMask::from_predicate(mask, GrainSize(4096), memory, [&](const int i) {
        return list_range.contains(indices[i]);
      });
    }();

    if (valid_indices.size() != mask.size()) {
      const IndexMask invalid_indices = valid_indices.complement(mask, memory);
      list_->cpp_type().fill_construct_indices(
          list_->cpp_type().default_value(), dst.data(), invalid_indices);
    }

    const List::DataVariant &data = list_->data();
    if (const auto *array_data = std::get_if<nodes::List::ArrayData>(&data)) {
      const GSpan src(list_->cpp_type(), array_data->data, list_->size());
      valid_indices.foreach_index([&](const int i, const int mask) {
        list_->cpp_type().copy_construct(src[indices[i]], dst[mask]);
      });
    }
    else if (const auto *single_data = std::get_if<nodes::List::SingleData>(&data)) {
      list_->cpp_type().fill_construct_indices(single_data->value, dst.data(), valid_indices);
    }
  }
};

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(
      srna,
      "socket_type",
      "Socket Type",
      "Value may be implicitly converted if the type does not match",
      rna_enum_node_socket_data_type_items,
      NOD_storage_enum_accessors(socket_type),
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
  RNA_def_node_enum(srna,
                    "structure_type",
                    "Structure Type",
                    "What kind of higher order types are expected to flow through this socket",
                    rna_enum_node_socket_structure_type_items,
                    NOD_storage_enum_accessors(structure_type));
}

/**
 * Needed because #execute_multi_function_on_value_variant does not support types that can't be
 * processed as fields.
 */
static bke::SocketValueVariant get_list_value_at_index(const ListPtr &list,
                                                       const eNodeSocketDatatype socket_type,
                                                       const int64_t index)
{
  const CPPType &list_type = list->cpp_type();
  bke::SocketValueVariant value;
  void *dst = value.allocate_single(socket_type);
  if (const auto *data = std::get_if<List::ArrayData>(&list->data())) {
    if (list->is_mutable() && data->sharing_info->is_mutable()) {
      list_type.move_construct(POINTER_OFFSET(data->data, list_type.size * index), dst);
    }
    else {
      list_type.copy_construct(POINTER_OFFSET(data->data, list_type.size * index), dst);
    }
  }
  if (const auto *data = std::get_if<List::SingleData>(&list->data())) {
    if (list->is_mutable() && data->sharing_info->is_mutable()) {
      list_type.move_construct(data->value, dst);
    }
    else {
      list_type.copy_construct(data->value, dst);
    }
  }
  BLI_assert_unreachable();
  return {};
}

static void node_geo_exec(GeoNodeExecParams params)
{
  bke::SocketValueVariant index = params.extract_input<bke::SocketValueVariant>("Index");
  ListPtr list = params.extract_input<ListPtr>("List");
  if (!list) {
    params.set_default_remaining_outputs();
    return;
  }
  const CPPType &list_type = list->cpp_type();
  const std::optional<eNodeSocketDatatype> socket_type =
      bke::geo_nodes_base_cpp_type_to_socket_type(list_type);
  if (!socket_type) {
    BLI_assert_unreachable();
    params.set_default_remaining_outputs();
    return;
  }

  if (!socket_type_supports_fields(*socket_type)) {
    if (!index.is_single()) {
      params.error_message_add(NodeWarningType::Error,
                               "Index must be a single value for socket type");
      params.set_default_remaining_outputs();
      return;
    }
    index.convert_to_single();
    const int index_int = index.get<int>();
    params.set_output("Value", get_list_value_at_index(list, *socket_type, index_int));
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
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeListGetItem");
  ntype.ui_name = "Get List Item";
  ntype.ui_description = "Retrieve a value from a list";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_searches;
  bke::node_type_storage(
      ntype, "NodeGeometryListGetItem", node_free_standard_storage, node_copy_standard_storage);
  bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_list_get_item_cc
