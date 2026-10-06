/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_geo_closure.hh"
#include "NOD_geometry_nodes_closure_location.hh"
#include "NOD_geometry_nodes_closure_signature.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_sync_sockets.hh"

#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_idprop.hh"
#include "BKE_node_tree_reference_lifetimes.hh"

#include "BLO_read_write.hh"

#include "COM_closure.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"
#include "COM_zone_tree_operation.hh"

#include "node_geometry_util.hh"
#include "shader/node_shader_util.hh"

namespace blender {

namespace nodes::node_geo_evaluate_closure_cc {

NODE_STORAGE_FUNCS(NodeEvaluateClosure)

static void create_all_reference_lifetime_relations(NodeDeclarationBuilder &b)
{
  using bke::node_tree_reference_lifetimes::can_contain_reference;
  using bke::node_tree_reference_lifetimes::can_contain_referenced_data;
  rl::RelationsInNode &relations = b.get_reference_lifetime_relations();
  const NodeDeclaration &node_decl = b.declaration();
  for (const SocketDeclaration *input : node_decl.inputs) {
    if (can_contain_reference(input->socket_type)) {
      for (const SocketDeclaration *other_input : node_decl.inputs) {
        if (can_contain_referenced_data(other_input->socket_type)) {
          relations.use_relations.append({input->index, other_input->index});
        }
      }
      for (const SocketDeclaration *output : node_decl.outputs) {
        if (can_contain_reference(output->socket_type)) {
          relations.reference_propagations.append({input->index, output->index});
        }
      }
    }
    if (can_contain_referenced_data(input->socket_type)) {
      for (const SocketDeclaration *output : node_decl.outputs) {
        if (can_contain_referenced_data(output->socket_type)) {
          relations.data_propagations.append({input->index, output->index});
        }
      }
    }
  }
  for (const SocketDeclaration *output : node_decl.outputs) {
    if (can_contain_reference(output->socket_type)) {
      for (const SocketDeclaration *other_output : node_decl.outputs) {
        if (can_contain_referenced_data(other_output->socket_type)) {
          relations.available_relations.append({output->index, other_output->index});
        }
      }
    }
  }
}

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Closure>("Closure"_ustr).create_signature([](const bNode &node) {
    const auto &storage = node_storage(node);
    return nodes::ClosureSignature::from_evaluate_closure_node(
        node, storage.flag & NODE_EVALUATE_CLOSURE_FLAG_DEFINE_SIGNATURE);
  });

  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();
  auto &panel = b.add_panel("Interface"_ustr);
  if (node) {
    const auto &storage = node_storage(*node);
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeEvaluateClosureOutputItem &item = storage.output_items.items[i];
      const eNodeSocketDatatype socket_type = item.socket_type;
      const UString identifier(
          EvaluateClosureOutputItemsAccessor::socket_identifier_for_item(item));
      auto &decl = panel.add_output(socket_type, UString(item.name), identifier);
      decl.socket_name_ptr(
          &tree->id, *EvaluateClosureOutputItemsAccessor::item_srna, &item, "name");
      if (item.structure_type != NodeSocketInterfaceStructureType::Auto) {
        decl.structure_type(StructureType(item.structure_type));
      }
      else {
        decl.structure_type(StructureType::Dynamic);
      }
    }
    panel.add_output<decl::Extend>(""_ustr, "__extend__"_ustr)
        .custom_draw(
            socket_items::ui::draw_extend_socket_fn<EvaluateClosureOutputItemsAccessor>());
    for (const int i : IndexRange(storage.input_items.items_num)) {
      const NodeEvaluateClosureInputItem &item = storage.input_items.items[i];
      const eNodeSocketDatatype socket_type = item.socket_type;
      const UString identifier(
          EvaluateClosureInputItemsAccessor::socket_identifier_for_item(item));
      auto &decl = panel.add_input(socket_type, UString(item.name), identifier);
      decl.socket_name_ptr(
          &tree->id, *EvaluateClosureInputItemsAccessor::item_srna, &item, "name");
      if (item.structure_type != NodeSocketInterfaceStructureType::Auto) {
        decl.structure_type(StructureType(item.structure_type));
      }
      else {
        decl.structure_type(StructureType::Dynamic);
      }
      decl.compositor_realization_mode(CompositorInputRealizationMode::None);
    }
    panel.add_input<decl::Extend>(""_ustr, "__extend__"_ustr)
        .custom_draw(socket_items::ui::draw_extend_socket_fn<EvaluateClosureInputItemsAccessor>());

    /* This creates all possible reference lifetime relations because the closure could do
     * anything. */
    create_all_reference_lifetime_relations(b);
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *storage = MEM_new<NodeEvaluateClosure>(__func__);
  node->storage = storage;
}

static void node_copy_storage(bNodeTree * /*tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeEvaluateClosure &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<NodeEvaluateClosure>(__func__, dna::shallow_copy(src_storage));
  dst_node->storage = dst_storage;

  socket_items::copy_array<EvaluateClosureInputItemsAccessor>(*src_node, *dst_node);
  socket_items::copy_array<EvaluateClosureOutputItemsAccessor>(*src_node, *dst_node);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<EvaluateClosureInputItemsAccessor>(*node);
  socket_items::destruct_array<EvaluateClosureOutputItemsAccessor>(*node);
  MEM_delete(static_cast<NodeEvaluateClosure *>(node->storage));
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  if (params.C && params.link.tosock == params.node.inputs.first_ &&
      params.link.fromsock->type == SOCK_CLOSURE)
  {
    const NodeEvaluateClosure &storage = node_storage(params.node);
    if (storage.input_items.items_num == 0 && storage.output_items.items_num == 0) {
      SpaceNode *snode = CTX_wm_space_node(params.C);
      if (snode && snode->edittree == &params.ntree) {
        sync_sockets_evaluate_closure(*snode, params.node, nullptr, params.link.fromsock);
      }
    }
    return true;
  }
  if (params.link.tonode == &params.node) {
    return socket_items::try_add_item_via_any_extend_socket<EvaluateClosureInputItemsAccessor>(
        params.ntree, params.node, params.node, params.link);
  }
  return socket_items::try_add_item_via_any_extend_socket<EvaluateClosureOutputItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_layout_ex(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);

  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);

  layout.op("node.sockets_sync", IFACE_("Sync"), ICON_FILE_REFRESH);
  layout.prop(ptr, "define_signature", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (ui::Layout *panel = layout.panel(C, "input_items", false, IFACE_("Input Items"))) {
    socket_items::ui::draw_items_list_with_operators<EvaluateClosureInputItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<EvaluateClosureInputItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          panel->prop(item_ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
        });
  }
  if (ui::Layout *panel = layout.panel(C, "output_items", false, IFACE_("Output Items"))) {
    socket_items::ui::draw_items_list_with_operators<EvaluateClosureOutputItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<EvaluateClosureOutputItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          panel->prop(item_ptr, "structure_type", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
        });
  }
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode & /*node*/,
                                                       const bNodeSocket &output_socket)
{
  return evaluate_closure_node_internally_linked_input(output_socket);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (other_socket.in_out == SOCK_IN) {
    params.add_item(IFACE_("Item"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("NodeEvaluateClosure"_ustr);
      const auto *item =
          socket_items::add_item_with_socket_type_and_name<EvaluateClosureOutputItemsAccessor>(
              params.node_tree, node, params.socket.typeinfo->type, params.socket.name);
      params.update_and_connect_available_socket(node, UString(item->name));
    });
    return;
  }
  if (other_socket.type == SOCK_CLOSURE) {
    params.add_item(IFACE_("Closure"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("NodeEvaluateClosure"_ustr);
      params.connect_available_socket(node, "Closure"_ustr);

      SpaceNode &snode = *CTX_wm_space_node(&params.C);
      sync_sockets_evaluate_closure(snode, node, nullptr);
    });
  }
  if (EvaluateClosureInputItemsAccessor::supports_socket_type(other_socket.typeinfo->type,
                                                              params.node_tree().type))
  {
    params.add_item(
        IFACE_("Item"),
        [](LinkSearchOpParams &params) {
          bNode &node = params.add_node("NodeEvaluateClosure"_ustr);
          const auto *item =
              socket_items::add_item_with_socket_type_and_name<EvaluateClosureInputItemsAccessor>(
                  params.node_tree, node, params.socket.typeinfo->type, params.socket.name);
          nodes::update_node_declaration_and_sockets(params.node_tree, node);
          params.connect_available_socket_by_identifier(
              node, UString(EvaluateClosureInputItemsAccessor::socket_identifier_for_item(*item)));
        },
        other_socket.type == SOCK_CLOSURE ? -1 : 0);
  }
}

static void node_operators()
{
  socket_items::ops::make_common_operators<EvaluateClosureInputItemsAccessor>();
  socket_items::ops::make_common_operators<EvaluateClosureOutputItemsAccessor>();
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<EvaluateClosureInputItemsAccessor>(&writer, node);
  socket_items::blend_write<EvaluateClosureOutputItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<EvaluateClosureInputItemsAccessor>(&reader, node);
  socket_items::blend_read_data<EvaluateClosureOutputItemsAccessor>(&reader, node);
}

using namespace blender::compositor;

class EvaluateClosureOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const compositor::ClosurePtr closure =
        this->get_input("Closure").get_single_value<compositor::ClosurePtr>();
    if (!closure) {
      this->write_default_outputs();
      return;
    }

    const ClosureSourceLocation closure_source_location{&closure->zone.output_node()->owner_tree(),
                                                        closure->zone.output_node()->identifier,
                                                        closure->compute_context.hash(),
                                                        &closure->compute_context};
    const bke::EvaluateClosureComputeContext &evaluate_closure_context =
        this->context().compute_context_cache().for_evaluate_closure(&this->get_compute_context(),
                                                                     this->node().identifier,
                                                                     &this->node().owner_tree(),
                                                                     closure_source_location);
    ZoneTreeOperation zone_tree_operation = ZoneTreeOperation(
        this->context(), closure->zone, evaluate_closure_context);
    this->set_reference_counts(zone_tree_operation, closure);
    Vector<std::unique_ptr<Result>> inputs = this->map_inputs(zone_tree_operation, closure);
    Vector<std::unique_ptr<Result>> captured_values = this->map_captured_values(
        zone_tree_operation, closure);
    zone_tree_operation.evaluate();
    this->write_outputs(zone_tree_operation, closure);
  }

  /* Write a default value for each of the needed outputs, if an input with the same name and type
   * as the output exists, the output shares the value of that input, otherwise, the output is
   * default initialized. */
  void write_default_outputs()
  {
    for (const bNodeSocket *output : this->node().output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      Result &result = this->get_result(output->identifier);
      if (!result.should_compute()) {
        continue;
      }

      const bNodeSocket *matching_input = bke::node_find_enabled_input_socket(
          const_cast<bNode &>(this->node()), output->name);
      if (!matching_input) {
        continue;
      }

      Result &input = this->get_input(matching_input->identifier);
      if (input.type() != result.type()) {
        continue;
      }

      result.share_data(input);
    }

    this->allocate_default_remaining_outputs();
  }

  /* Setup the reference count of each of the outputs of the given zone tree operation of the given
   * closure. This is either 0 or 1 depending on whether a corresponding output with the same name
   * exists in the evaluate node and is needed. If no corresponding output exists, the zone output
   * will not be used. */
  void set_reference_counts(ZoneTreeOperation &zone_tree_operation,
                            const compositor::ClosurePtr &closure)
  {
    for (const bNodeSocket *zone_output : closure->zone.output_node()->input_sockets()) {
      if (!is_socket_available(zone_output)) {
        continue;
      }

      Result &zone_result = zone_tree_operation.get_result(zone_output->identifier);
      const bNodeSocket *evaluate_node_output = bke::node_find_enabled_output_socket(
          const_cast<bNode &>(this->node()), zone_output->name);
      if (!evaluate_node_output) {
        zone_result.set_reference_count(0);
        continue;
      }

      Result &evaluate_node_result = this->get_result(evaluate_node_output->identifier);
      zone_result.set_reference_count(evaluate_node_result.should_compute() ? 1 : 0);
    }
  }

  /* Map each input of the zone tree operation for the given closure to the input we get from the
   * evaluate node. If no input corresponding input with the same name in the evaluate node is
   * found, a default value is mapped. The mapped inputs are returned. */
  Vector<std::unique_ptr<Result>> map_inputs(ZoneTreeOperation &zone_tree_operation,
                                             const compositor::ClosurePtr &closure)
  {
    Vector<std::unique_ptr<Result>> temporary_inputs;
    for (const bNodeSocket *zone_input : closure->zone.input_node()->output_sockets()) {
      if (!is_socket_available(zone_input)) {
        continue;
      }

      const bNodeSocket *evaluate_node_input = bke::node_find_enabled_input_socket(
          const_cast<bNode &>(this->node()), zone_input->name);
      if (!evaluate_node_input) {
        const ResultType zone_input_type = get_node_socket_result_type(zone_input);
        std::unique_ptr<Result> temporary_input = std::make_unique<Result>(
            this->context().create_result(zone_input_type));
        temporary_input->allocate_invalid();
        temporary_inputs.append(std::move(temporary_input));
        zone_tree_operation.map_input_to_result(zone_input->identifier,
                                                temporary_inputs.last().get());
        continue;
      }

      const Result &input_result = this->get_input(evaluate_node_input->identifier);
      std::unique_ptr<Result> temporary_input = std::make_unique<Result>(
          this->context().create_result(input_result.type(), input_result.precision()));
      temporary_input->share_data(input_result);
      temporary_inputs.append(std::move(temporary_input));
      zone_tree_operation.map_input_to_result(zone_input->identifier,
                                              temporary_inputs.last().get());
    }
    return temporary_inputs;
  }

  /* Map each captured value from the closure to its corresponding input of the zone tree
   * operation. The mapped captured inputs are returned. */
  Vector<std::unique_ptr<Result>> map_captured_values(ZoneTreeOperation &zone_tree_operation,
                                                      const compositor::ClosurePtr &closure)
  {
    Vector<std::unique_ptr<Result>> temporary_inputs;
    for (const auto &item : closure->captured_values.items()) {
      const std::string &identifier = item.key;
      const Result *value = item.value;
      std::unique_ptr<Result> temporary_input = std::make_unique<Result>(
          this->context().create_result(value->type(), value->precision()));
      temporary_input->share_data(*value);
      temporary_inputs.append(std::move(temporary_input));
      zone_tree_operation.map_input_to_result(identifier, temporary_inputs.last().get());
    }
    return temporary_inputs;
  }

  /* Writes the output results of the zone tree operation to this evaluate node operation outputs
   * by sharing its data and freeing the results. */
  void write_outputs(ZoneTreeOperation &zone_tree_operation, const compositor::ClosurePtr &closure)
  {
    for (const bNodeSocket *zone_output : closure->zone.output_node()->input_sockets()) {
      if (!is_socket_available(zone_output)) {
        continue;
      }

      const bNodeSocket *evaluate_node_output = bke::node_find_enabled_output_socket(
          const_cast<bNode &>(this->node()), zone_output->name);
      if (!evaluate_node_output) {
        continue;
      }

      Result &evaluate_node_result = this->get_result(evaluate_node_output->identifier);
      if (!evaluate_node_result.should_compute()) {
        continue;
      }

      Result &zone_tree_result = zone_tree_operation.get_result(zone_output->identifier);
      if (zone_tree_result.type() != evaluate_node_result.type()) {
        zone_tree_result.release();
        continue;
      }

      evaluate_node_result.share_data(zone_tree_result);
      zone_tree_result.release();
    }

    this->allocate_default_remaining_outputs();
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new EvaluateClosureOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  sh_geo_node_type_base(&ntype, "NodeEvaluateClosure"_ustr, NODE_EVALUATE_CLOSURE);
  ntype.ui_name = "Evaluate Closure";
  ntype.ui_description = "Execute a given closure";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.internally_linked_input = node_internally_linked_input;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.register_operators = node_operators;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_type_storage(ntype, "NodeEvaluateClosure", node_free_storage, node_copy_storage);
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace nodes::node_geo_evaluate_closure_cc

namespace nodes {

StructRNA **EvaluateClosureInputItemsAccessor::item_srna = &RNA_NodeEvaluateClosureInputItem;

void EvaluateClosureInputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  writer->write_string(item.name);
}

void EvaluateClosureInputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

StructRNA **EvaluateClosureOutputItemsAccessor::item_srna = &RNA_NodeEvaluateClosureOutputItem;

void EvaluateClosureOutputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  writer->write_string(item.name);
}

void EvaluateClosureOutputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

const bNodeSocket *evaluate_closure_node_internally_linked_input(const bNodeSocket &output_socket)
{
  const bNode &node = output_socket.owner_node();
  const bNodeTree &tree = node.owner_tree();
  BLI_assert(node.is_type("NodeEvaluateClosure"_ustr));
  const auto &storage = *static_cast<const NodeEvaluateClosure *>(node.storage);
  if (output_socket.index() >= storage.output_items.items_num) {
    return nullptr;
  }
  const NodeEvaluateClosureOutputItem &output_item =
      storage.output_items.items[output_socket.index()];
  const StringRef output_key = output_item.name;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeEvaluateClosureInputItem &input_item = storage.input_items.items[i];
    const StringRef input_key = input_item.name;
    if (output_key == input_key) {
      if (!tree.typeinfo->validate_link ||
          tree.typeinfo->validate_link(input_item.socket_type, output_item.socket_type))
      {
        return &node.input_socket(i + 1);
      }
    }
  }
  return nullptr;
}

}  // namespace nodes
}  // namespace blender
