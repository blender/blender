/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_string_utf8.h"

#include "NOD_geo_closure.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "BKE_compute_context_cache.hh"

#include "BLO_read_write.hh"

namespace blender::nodes::node_geo_closure_cc {

/** Shared between closure input and output node. */
static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *current_node_ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(current_node_ptr->owner_id);
  bNode *current_node = static_cast<bNode *>(current_node_ptr->data);

  const bke::bNodeTreeZones *zones = ntree.zones();
  if (!zones) {
    return;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_node(current_node->identifier);
  if (!zone) {
    return;
  }
  if (!zone->output_node) {
    return;
  }
  bNode &output_node = const_cast<bNode &>(*zone->output_node);

  if (current_node->type_legacy == GEO_NODE_CLOSURE_INPUT) {
    if (uiLayout *panel = uiLayoutPanel(C, layout, "input_items", false, TIP_("Input Items"))) {
      socket_items::ui::draw_items_list_with_operators<ClosureInputItemsAccessor>(
          C, panel, ntree, output_node);
      socket_items::ui::draw_active_item_props<ClosureInputItemsAccessor>(
          ntree, output_node, [&](PointerRNA *item_ptr) {
            uiItemR(panel, item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          });
    }
  }
  else {
    if (uiLayout *panel = uiLayoutPanel(C, layout, "output_items", false, TIP_("Output Items"))) {
      socket_items::ui::draw_items_list_with_operators<ClosureOutputItemsAccessor>(
          C, panel, ntree, output_node);
      socket_items::ui::draw_active_item_props<ClosureOutputItemsAccessor>(
          ntree, output_node, [&](PointerRNA *item_ptr) {
            uiItemR(panel, item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          });
    }
  }
}

namespace input_node {

NODE_STORAGE_FUNCS(NodeGeometryClosureInput);

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();
  if (node && tree) {
    const NodeGeometryClosureInput &storage = node_storage(*node);
    const bNode *output_node = tree->node_by_id(storage.output_node_id);
    if (output_node) {
      const auto &output_storage = *static_cast<const NodeGeometryClosureOutput *>(
          output_node->storage);
      for (const int i : IndexRange(output_storage.input_items.items_num)) {
        const NodeGeometryClosureInputItem &item = output_storage.input_items.items[i];
        const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
        const std::string identifier = ClosureInputItemsAccessor::socket_identifier_for_item(item);
        b.add_output(socket_type, item.name, identifier);
      }
    }
  }
  b.add_output<decl::Extend>("", "__extend__");
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode * /*node*/,
                       char *label,
                       const int label_maxncpy)
{
  BLI_strncpy_utf8(label, IFACE_("Closure"), label_maxncpy);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryClosureInput *data = MEM_callocN<NodeGeometryClosureInput>(__func__);
  node->storage = data;
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  bNode *output_node = ntree->node_by_id(node_storage(*node).output_node_id);
  if (!output_node) {
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<ClosureInputItemsAccessor>(
      *ntree, *node, *output_node, *link);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeClosureInput", GEO_NODE_CLOSURE_INPUT);
  ntype.ui_name = "Closure Input";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = nullptr;
  ntype.initfunc = node_init;
  ntype.labelfunc = node_label;
  ntype.no_muting = true;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryClosureInput", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace input_node

namespace output_node {

NODE_STORAGE_FUNCS(NodeGeometryClosureOutput);

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (node && tree) {
    const NodeGeometryClosureOutput &storage = node_storage(*node);
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeGeometryClosureOutputItem &item = storage.output_items.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const std::string identifier = ClosureOutputItemsAccessor::socket_identifier_for_item(item);
      b.add_input(socket_type, item.name, identifier);
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Closure>("Closure");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryClosureOutput *data = MEM_callocN<NodeGeometryClosureOutput>(__func__);
  node->storage = data;
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryClosureOutput &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeGeometryClosureOutput>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<ClosureInputItemsAccessor>(*src_node, *dst_node);
  socket_items::copy_array<ClosureOutputItemsAccessor>(*src_node, *dst_node);
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ClosureInputItemsAccessor>(*node);
  socket_items::destruct_array<ClosureOutputItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<ClosureOutputItemsAccessor>(
      *ntree, *node, *node, *link);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<ClosureInputItemsAccessor>();
  socket_items::ops::make_common_operators<ClosureOutputItemsAccessor>();
}

static void try_initialize_closure_from_evaluator(SpaceNode &snode, bNode &closure_output_node)
{
  snode.edittree->ensure_topology_cache();
  bNodeSocket &closure_socket = closure_output_node.output_socket(0);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, closure_socket);
  if (!current_context) {
    /* The current tree does not have a known context, e.g. it is pinned but the modifier has been
     * removed. */
    return;
  }
  const ComputeContext *evaluate_context_generic =
      ed::space_node::compute_context_for_closure_evaluation(
          current_context, closure_socket, compute_context_cache, std::nullopt);
  if (!evaluate_context_generic) {
    /* No evaluation of the closure found. */
    return;
  }
  const auto *evaluate_context = dynamic_cast<const bke::EvaluateClosureComputeContext *>(
      evaluate_context_generic);
  if (!evaluate_context) {
    return;
  }
  const bNode *evaluate_node = evaluate_context->evaluate_node();
  if (!evaluate_node) {
    return;
  }
  const auto *storage = static_cast<const NodeGeometryEvaluateClosure *>(evaluate_node->storage);

  for (const int i : IndexRange(storage->input_items.items_num)) {
    const NodeGeometryEvaluateClosureInputItem &evaluate_item = storage->input_items.items[i];
    socket_items::add_item_with_socket_type_and_name<ClosureInputItemsAccessor>(
        closure_output_node, eNodeSocketDatatype(evaluate_item.socket_type), evaluate_item.name);
  }
  for (const int i : IndexRange(storage->output_items.items_num)) {
    const NodeGeometryEvaluateClosureOutputItem &evaluate_item = storage->output_items.items[i];
    socket_items::add_item_with_socket_type_and_name<ClosureOutputItemsAccessor>(
        closure_output_node, eNodeSocketDatatype(evaluate_item.socket_type), evaluate_item.name);
  }
  BKE_ntree_update_tag_node_property(snode.edittree, &closure_output_node);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (other_socket.type != SOCK_CLOSURE) {
    return;
  }
  if (other_socket.in_out == SOCK_OUT) {
    return;
  }
  params.add_item_full_name(IFACE_("Closure"), [](LinkSearchOpParams &params) {
    bNode &input_node = params.add_node("GeometryNodeClosureInput");
    bNode &output_node = params.add_node("GeometryNodeClosureOutput");
    output_node.location[0] = 300;

    auto &input_storage = *static_cast<NodeGeometryClosureInput *>(input_node.storage);
    input_storage.output_node_id = output_node.identifier;

    params.connect_available_socket(output_node, "Closure");

    SpaceNode &snode = *CTX_wm_space_node(&params.C);
    try_initialize_closure_from_evaluator(snode, output_node);
  });
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeClosureOutput", GEO_NODE_CLOSURE_OUTPUT);
  ntype.ui_name = "Closure Output";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.labelfunc = input_node::node_label;
  ntype.no_muting = true;
  ntype.register_operators = node_operators;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  bke::node_type_storage(ntype, "NodeGeometryClosureOutput", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace output_node

}  // namespace blender::nodes::node_geo_closure_cc

namespace blender::nodes {

StructRNA *ClosureInputItemsAccessor::item_srna = &RNA_NodeGeometryClosureInputItem;
int ClosureInputItemsAccessor::node_type = GEO_NODE_CLOSURE_OUTPUT;
int ClosureInputItemsAccessor::item_dna_type = SDNA_TYPE_FROM_STRUCT(NodeGeometryClosureInputItem);

void ClosureInputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void ClosureInputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

StructRNA *ClosureOutputItemsAccessor::item_srna = &RNA_NodeGeometryClosureOutputItem;
int ClosureOutputItemsAccessor::node_type = GEO_NODE_CLOSURE_OUTPUT;
int ClosureOutputItemsAccessor::item_dna_type = SDNA_TYPE_FROM_STRUCT(
    NodeGeometryClosureOutputItem);

void ClosureOutputItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void ClosureOutputItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes
