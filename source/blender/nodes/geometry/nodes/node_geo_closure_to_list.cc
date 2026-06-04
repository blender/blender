/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_compute_contexts.hh"
#include "BKE_volume_grid.hh"

#include "BLO_read_write.hh"

#include "NOD_geo_closure_to_list.hh"
#include "NOD_geometry_nodes_closure_eval.hh"
#include "NOD_geometry_nodes_closure_location.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_prototypes.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_closure_to_list_cc {

NODE_STORAGE_FUNCS(GeometryNodeClosureToList)

using ItemsAccessor = ClosureToListItemsAccessor;

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const GeometryNodeClosureToList &storage = node_storage(*node);
  const Span<GeometryNodeClosureToListItem> items(storage.items, storage.items_num);

  for (const int i : items.index_range()) {
    const GeometryNodeClosureToListItem &item = items[i];
    const UString output_identifier{ItemsAccessor::output_socket_identifier_for_item(item)};
    const UString name{item.name};
    const eNodeSocketDatatype type = item.socket_type;
    b.add_output(type, name, output_identifier)
        .structure_type(StructureType::List)
        .socket_name_ptr(&tree->id, *ItemsAccessor::item_srna, &item, "name")
        .propagate_all()
        .references_other_outputs();
  }

  b.add_output<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::List)
      .custom_draw(socket_items::ui::draw_extend_socket_fn<ItemsAccessor>());

  b.add_input<decl::Int>("Count"_ustr)
      .default_value(1)
      .min(0)
      .description("The number of elements in the list");
  b.add_input<decl::Closure>("Closure"_ustr).create_signature([](const bNode &node) {
    return ClosureSignature::from_closure_to_list_node(node);
  });
}

static void node_layout_ex(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);
  if (ui::Layout *panel = layout.panel(C, "closure_to_list_items", false, IFACE_("Items"))) {
    socket_items::ui::draw_items_list_with_operators<ItemsAccessor>(C, panel, tree, node);
    socket_items::ui::draw_active_item_props<ItemsAccessor>(tree, node, [&](PointerRNA *item_ptr) {
      panel->use_property_split_set(true);
      panel->use_property_decorate_set(false);
      panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      panel->prop(item_ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
    });
  }
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype data_type = params.other_socket().type;
  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(data_type, SOCK_INT)) {
      params.add_item(IFACE_("Count"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeClosureToList"_ustr);
        params.update_and_connect_available_socket(node, "Count"_ustr);
      });
    }
    if (params.node_tree().typeinfo->validate_link(data_type, SOCK_CLOSURE)) {
      params.add_item(IFACE_("Closure"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeClosureToList"_ustr);
        params.update_and_connect_available_socket(node, "Closure"_ustr);
      });
    }
  }
  else {
    params.add_item(IFACE_("List"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeClosureToList"_ustr);
      socket_items::add_item_with_socket_type_and_name<ItemsAccessor>(
          params.node_tree, node, data_type, params.socket.name);
      params.update_and_connect_available_socket(node, UString(params.socket.name));
    });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const int count = params.extract_input<int>("Count"_ustr);
  if (count < 0) {
    params.error_message_add(NodeWarningType::Error, "Count must not be negative");
    params.set_default_remaining_outputs();
    return;
  }
  const bNode &node = params.node();
  const GeometryNodeClosureToList &storage = node_storage(node);
  const Span<GeometryNodeClosureToListItem> items(storage.items, storage.items_num);

  ClosurePtr closure = params.extract_input<ClosurePtr>("Closure"_ustr);
  if (!closure) {
    params.set_default_remaining_outputs();
    return;
  }

  Vector<int> required_items;
  for (const int i : items.index_range()) {
    if (params.output_is_required(
            UString(ItemsAccessor::output_socket_identifier_for_item(items[i]))))
    {
      required_items.append(i);
    }
  }

  Array<const bke::bNodeSocketType *> socket_types(required_items.size());
  for (const int required_i : required_items.index_range()) {
    const int item_i = required_items[required_i];
    const eNodeSocketDatatype type = items[item_i].socket_type;
    socket_types[required_i] = bke::node_socket_type_find_static(type);
  }
  if (socket_types.as_span().contains(nullptr)) {
    params.set_default_remaining_outputs();
    return;
  }

  Array<Array<bke::SocketValueVariant>> closure_results(required_items.size(), NoInitialization());
  for (const int i : closure_results.index_range()) {
    new (&closure_results[i]) Array<bke::SocketValueVariant>(count, NoInitialization());
  }

  const GeoNodesUserData &parent_user_data = *params.user_data();

  /* The grain size is completely arbitrary since we don't know how expensive the closure is.
   * However since the closure evaluation itself has fairly high overhead, it makes to optimize for
   * the case where each task has a relatively high cost. */
  const bke::bNodeSocketType *int_type = bke::node_socket_type_find("NodeSocketInt");
  threading::parallel_for(IndexRange(count), 8, [&](const IndexRange range) {
    ClosureEagerEvalParams closure_params;

    /* Create inputs. */
    closure_params.inputs.resize(1);
    closure_params.inputs[0].key = "Index";
    closure_params.inputs[0].type = int_type;

    /* Create outputs. */
    closure_params.outputs.resize(required_items.size());
    for (const int required_i : required_items.index_range()) {
      const int item_i = required_items[required_i];
      closure_params.outputs[required_i].key = items[item_i].name;
      closure_params.outputs[required_i].type = socket_types[required_i];
    }

    for (const int64_t list_i : range) {
      /* Create input value. */
      BLI_assert(list_i < std::numeric_limits<int>::max());
      closure_params.inputs[0].value = bke::SocketValueVariant::From(int(list_i));

      /* Set output locations. */
      for (const int required_i : required_items.index_range()) {
        closure_params.outputs[required_i].value = &closure_results[required_i][list_i];
      }

      const bke::ClosureToListComputeContext context(
          parent_user_data.compute_context, node.identifier, int(list_i));
      GeoNodesUserData user_data = parent_user_data;
      user_data.compute_context = &context;
      user_data.verbose_log = should_log_verbose_in_context(user_data, context.hash());
      closure_params.user_data = &user_data;

      evaluate_closure_eagerly(*closure, closure_params);
    }
  });

  for (const int required_i : required_items.index_range()) {
    const int item_i = required_items[required_i];
    const UString identifier{ItemsAccessor::output_socket_identifier_for_item(items[item_i])};
    Array<bke::SocketValueVariant> &values = closure_results[required_i];

    if (std::all_of(values.begin(), values.end(), [](const bke::SocketValueVariant &value) {
          return value.is_single();
        }))
    {
      const eNodeSocketDatatype socket_type = items[item_i].socket_type;
      const CPPType &type = *bke::socket_type_to_geo_nodes_base_cpp_type(socket_type);

      GArray<> array(type, count, NoInitialization());
      threading::parallel_for(IndexRange(count), 128, [&](const IndexRange range) {
        for (const int list_i : range) {
          void *closure_result = const_cast<void *>(values[list_i].get_single_ptr_raw());
          type.move_construct(closure_result, array[list_i]);
        }
      });
      params.set_output(identifier, GList::from_garray(std::move(array)));
    }
    else {
      params.set_output(identifier, GList::from_container(std::move(values)));
    }
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->storage = MEM_new<GeometryNodeClosureToList>(__func__);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ItemsAccessor>(*node);
  MEM_delete(static_cast<GeometryNodeClosureToList *>(node->storage));
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const GeometryNodeClosureToList &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<GeometryNodeClosureToList>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<ItemsAccessor>(*src_node, *dst_node);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<ItemsAccessor>();
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<ItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<ItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<ItemsAccessor>(&reader, node);
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode &node,
                                                       const bNodeSocket &output_socket)
{
  return node.input_by_identifier(output_socket.identifier_ustr());
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeClosureToList"_ustr);
  ntype.ui_name = "Closure to List";
  ntype.ui_description = "Create a list of values";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  bke::node_type_storage(ntype, "GeometryNodeClosureToList", node_free_storage, node_copy_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  ntype.insert_link = node_insert_link;
  ntype.ignore_inferred_input_socket_visibility = true;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.internally_linked_input = node_internally_linked_input;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_closure_to_list_cc

namespace blender::nodes {

StructRNA **ClosureToListItemsAccessor::item_srna = &RNA_GeometryNodeClosureToListItem;

void ClosureToListItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  writer->write_string(item.name);
}

void ClosureToListItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes
