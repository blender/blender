/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include <cstdlib>

#include "DNA_node_types.h"

#include "BLI_listbase_iterator.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"
#include "BLI_vector.hh"

#include "BKE_action.hh"
#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"

#include "ANIM_action.hh"

#include "DEG_depsgraph_build.hh"

#include "ED_node.hh"
#include "ED_node_preview.hh"
#include "ED_render.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "NOD_common.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"

#include "node_intern.hh" /* own include */

namespace blender::ed::space_node {

std::optional<Bounds<float2>> node_bounds(Span<const bNode *> nodes)
{
  std::optional<Bounds<float2>> result = std::nullopt;
  for (const bNode *node : nodes) {
    const float2 loc(node->location);
    const Bounds<float2> box(loc, loc + float2(node->width, -node->height));
    result = bounds::merge<float2>(result, box);
  }
  return result;
}

std::optional<Bounds<float2>> node_location_bounds(Span<const bNode *> nodes)
{
  std::optional<Bounds<float2>> result = std::nullopt;
  for (const bNode *node : nodes) {
    const float2 loc(node->location);
    result = bounds::min_max(result, loc);
  }
  return result;
}

/* -------------------------------------------------------------------- */
/** \name Utilities for copying node sets
 * \{ */

using NodeFilterFn = FunctionRef<bool(const bNode &)>;
static bool default_link_filter(const bNode & /*node*/)
{
  return true;
}

/* Links between nodes that should be copied.
 * Contains only links between nodes of this set. */
static Vector<const bNodeLink *> find_internal_links(
    const bNodeTree &tree,
    const Span<const bNode *> nodes,
    NodeFilterFn link_filter = default_link_filter)
{
  Vector<const bNodeLink *> internal_links;
  const Set<const bNode *> nodes_set(nodes);
  for (const bNodeLink &link : tree.links) {
    if (!link.is_available() || bke::node_link_is_hidden(link)) {
      continue;
    }
    if (!link_filter(*link.fromnode) || !link_filter(*link.tonode)) {
      continue;
    }
    if (!nodes_set.contains(link.fromnode) || !nodes_set.contains(link.tonode)) {
      continue;
    }

    internal_links.append(&link);
  }
  return internal_links;
}

static Vector<MutableNodeAndSocket> get_socket_links(
    const bNodeSocket &socket,
    const bool skip_hidden,
    NodeFilterFn link_filter = default_link_filter)
{
  Vector<MutableNodeAndSocket> result;
  for (const bNodeLink *link : socket.directly_linked_links()) {
    if (!link->is_available()) {
      continue;
    }
    if (skip_hidden && bke::node_link_is_hidden(*link)) {
      continue;
    }
    bNode *link_node = socket.is_input() ? link->fromnode : link->tonode;
    bNodeSocket *link_socket = socket.is_input() ? link->fromsock : link->tosock;
    if (!link_filter(*link_node)) {
      continue;
    }
    result.append({*link_node, *link_socket});
  }
  return result;
}

static Vector<MutableNodeAndSocket> get_internal_group_links(
    const bNodeTree &tree,
    const bNodeTreeInterfaceSocket &io_socket,
    const bool skip_hidden,
    NodeFilterFn link_filter = default_link_filter)
{
  Vector<MutableNodeAndSocket> result;
  if (io_socket.flag & NODE_INTERFACE_SOCKET_INPUT) {
    for (const bNode *group_input_node : tree.group_input_nodes()) {
      const bNodeSocket *socket = group_input_node->output_by_identifier(io_socket.identifier);
      BLI_assert(socket);
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (!link->is_available()) {
          continue;
        }
        if (skip_hidden && bke::node_link_is_hidden(*link)) {
          continue;
        }
        if (!link_filter(*link->tonode)) {
          continue;
        }
        result.append({*link->tonode, *link->tosock});
      }
    }
  }
  if (io_socket.flag & NODE_INTERFACE_SOCKET_OUTPUT) {
    if (const bNode *group_output_node = tree.group_output_node()) {
      const bNodeSocket *socket = group_output_node->input_by_identifier(io_socket.identifier);
      BLI_assert(socket);
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (!link->is_available()) {
          continue;
        }
        if (skip_hidden && bke::node_link_is_hidden(*link)) {
          continue;
        }
        if (!link_filter(*link->fromnode)) {
          continue;
        }
        result.append({*link->fromnode, *link->fromsock});
      }
    }
  }
  return result;
}

/**
 * Skip reroute nodes when finding the socket to use as an example for a new group interface
 * item. This moves "inward" into nodes selected for grouping to find properties like whether a
 * connected socket has a hidden value. It only works in trivial situations-- a single line of
 * connected reroutes with no branching.
 */
static const bNodeSocket &find_socket_to_use_for_interface(const bNodeTree &node_tree,
                                                           const bNodeSocket &socket)
{
  if (node_tree.has_available_link_cycle()) {
    return socket;
  }
  const bNode &node = socket.owner_node();
  if (!node.is_reroute()) {
    return socket;
  }
  const bNodeSocket &other_socket = socket.in_out == SOCK_IN ? node.output_socket(0) :
                                                               node.input_socket(0);
  if (!other_socket.is_logically_linked()) {
    return socket;
  }
  return *other_socket.logically_linked_sockets().first();
}

static bNodeTreeInterfaceSocket *add_interface_from_socket(const bNodeTree &original_tree,
                                                           const bNodeSocket &socket,
                                                           bNodeTree &tree_for_interface,
                                                           bNodeTreeInterfacePanel *parent)
{
  const bNode &node = socket.owner_node();
  /* The output sockets of group nodes usually have consciously given names so they have
   * precedence over socket names the link points to. */
  const bool prefer_node_for_interface_name = node.is_group() || node.is_group_input() ||
                                              node.is_group_output();

  /* The "example socket" has to have the same `in_out` status as the new interface socket. */
  const bNodeSocket &socket_for_io = find_socket_to_use_for_interface(original_tree, socket);
  const bNode &node_for_io = socket_for_io.owner_node();
  const bNodeSocket &socket_for_name = prefer_node_for_interface_name ? socket : socket_for_io;
  bNodeTreeInterfaceSocket *io_socket = bke::node_interface::add_interface_socket_from_node(
      tree_for_interface, node_for_io, socket_for_io, socket_for_io.idname, socket_for_name.name);
  if (io_socket) {
    tree_for_interface.tree_interface.move_item_to_parent(io_socket->item, parent, INT32_MAX);
  }
  return io_socket;
}

static std::string node_basepath(const bNodeTree &tree, const bNode &node)
{
  const PointerRNA ptr = RNA_pointer_create_discrete(
      &const_cast<bNodeTree &>(tree).id, RNA_Node, &const_cast<bNode &>(node));
  return *RNA_path_from_ID_to_struct(&ptr);
}

/* Maps old to new identifiers for simulation input node pairing. */
static void remap_pairing(bNodeTree &dst_tree,
                          Span<bNode *> nodes,
                          const Map<int32_t, int32_t> &identifier_map)
{
  for (bNode *dst_node : nodes) {
    if (bke::all_zone_input_node_types().contains(dst_node->type_legacy)) {
      const bke::bNodeZoneType &zone_type = *bke::zone_type_by_node_type(dst_node->type_legacy);
      int &output_node_id = zone_type.get_corresponding_output_id(*dst_node);
      if (output_node_id == 0) {
        continue;
      }
      output_node_id = identifier_map.lookup_default(output_node_id, 0);
      if (output_node_id == 0) {
        nodes::update_node_declaration_and_sockets(dst_tree, *dst_node);
      }
    }
  }
}

/* Utility for sequentially adding interface sockets and panels. */
class NodeSetInterfaceBuilder {
 private:
  using InterfaceSocketData = NodeTreeInterfaceMapping::InterfaceSocketData;
  using InterfacePanelData = NodeTreeInterfaceMapping::InterfacePanelData;

  NodeSetInterfaceParams params_;
  NodeTreeInterfaceMapping &io_mapping_;
  bNodeTree &dst_tree_;
  Set<const bNode *> src_nodes_set_;
  /* Multiple internal or external sockets may be mapped to the same interface item.
   * This map tracks unique interface items based on identifying sockets. */
  Map<const bNodeSocket *, InterfaceSocketData *> data_by_socket_;

 public:
  NodeSetInterfaceBuilder(NodeSetInterfaceParams params,
                          NodeTreeInterfaceMapping &io_mapping,
                          bNodeTree &dst_tree,
                          const Span<const bNode *> src_nodes);

  void expose_socket(const bNodeSocket &src_socket, bNodeTreeInterfacePanel *parent);
  void expose_socket(const bNode &src_node,
                     const nodes::SocketDeclaration &socket_decl,
                     bNodeTreeInterfacePanel *parent);
  bNodeTreeInterfacePanel *expose_panel(const bNode &src_node,
                                        const nodes::PanelDeclaration &panel_decl,
                                        bNodeTreeInterfacePanel *parent);
};

NodeSetInterfaceBuilder::NodeSetInterfaceBuilder(NodeSetInterfaceParams params,
                                                 NodeTreeInterfaceMapping &io_mapping,
                                                 bNodeTree &dst_tree,
                                                 const Span<const bNode *> src_nodes)
    : params_(std::move(params)),
      io_mapping_(io_mapping),
      dst_tree_(dst_tree),
      src_nodes_set_(src_nodes)
{
}

void NodeSetInterfaceBuilder::expose_socket(const bNodeSocket &src_socket,
                                            bNodeTreeInterfacePanel *parent)
{
  const bNodeTree &src_tree = src_socket.owner_tree();
  if (!src_socket.is_available()) {
    return;
  }
  if (params_.skip_hidden && !src_socket.is_visible()) {
    return;
  }

  auto try_add_socket_data = [&](const bNodeSocket &key,
                                 const bNodeSocket &template_socket) -> InterfaceSocketData * {
    InterfaceSocketData *data = data_by_socket_.lookup_default(&key, nullptr);
    if (data) {
      return data;
    }
    bNodeTreeInterfaceSocket *io_socket = add_interface_from_socket(
        src_tree, template_socket, dst_tree_, parent);
    if (io_socket) {
      data = &io_mapping_.socket_data.lookup_or_add(io_socket, {});
      data_by_socket_.add_new(&key, data);

      data->hidden = template_socket.flag & SOCK_HIDDEN;
      data->collapsed = template_socket.flag & SOCK_COLLAPSED;
      return data;
    }
    return nullptr;
  };

  Vector<MutableNodeAndSocket> external_links;
  if (params_.add_external_links) {
    external_links = get_socket_links(src_socket, params_.skip_hidden, [&](const bNode &node) {
      return !src_nodes_set_.contains(&node);
    });
  }

  if (external_links.is_empty()) {
    if (!params_.skip_unconnected) {
      if (InterfaceSocketData *data = try_add_socket_data(src_socket, src_socket)) {
        data->internal_sockets.add({src_socket.owner_node(), src_socket});
      }
    }

    return;
  }

  const bool use_external_socket_key = src_socket.is_input() ? !params_.use_unique_input :
                                                               params_.use_unique_output;
  if (use_external_socket_key) {
    /* Create a unique interface socket for each external link. */
    /* TODO This creates some problems:
     * - Input sockets with the same external link still use the internal socket as the interface
     *   template. The first input defines the interface type, which can lead to incorrect type
     *   conversion for the remaining sockets. Interface state is also based on the first internal
     *   socket.
     *   The external link should define be the interface template here.
     * - Output sockets with multiple external links are redundant because the internal socket is
     *   used as the interface template.
     *   Outputs should not create unique interface sockets for each link.
     */
    for (const MutableNodeAndSocket &external_socket : external_links) {
      if (InterfaceSocketData *data = try_add_socket_data(external_socket.socket, src_socket)) {
        data->internal_sockets.add({src_socket.owner_node(), src_socket});
        data->external_sockets.add(external_socket);
      }
    }
  }
  else {
    /* Create interface based on the internal socket. */
    if (InterfaceSocketData *data = try_add_socket_data(src_socket, src_socket)) {
      data->internal_sockets.add({src_socket.owner_node(), src_socket});
      data->external_sockets.add_multiple(external_links);
    }
  }
}

void NodeSetInterfaceBuilder::expose_socket(const bNode &src_node,
                                            const nodes::SocketDeclaration &socket_decl,
                                            bNodeTreeInterfacePanel *parent)
{
  const bNodeSocket &socket = src_node.socket_by_decl(socket_decl);
  this->expose_socket(socket, parent);
}

bNodeTreeInterfacePanel *NodeSetInterfaceBuilder::expose_panel(
    const bNode &src_node,
    const nodes::PanelDeclaration &panel_decl,
    bNodeTreeInterfacePanel *parent)
{
  NodeTreeInterfacePanelFlag flag{};
  if (panel_decl.default_collapsed) {
    flag |= NODE_INTERFACE_PANEL_DEFAULT_CLOSED;
  }
  bNodeTreeInterfacePanel *io_panel = dst_tree_.tree_interface.add_panel(
      panel_decl.name, panel_decl.description, flag, parent);
  InterfacePanelData &data = io_mapping_.panel_data.lookup_or_add(io_panel, {});

  const Span<bNodePanelState> panel_states = src_node.panel_states();
  for (const bNodePanelState panel_state : panel_states) {
    if (panel_state.identifier == panel_decl.identifier) {
      data.collapsed = panel_state.is_collapsed();
      break;
    }
  }

  return io_panel;
}

NodeTreeInterfaceMapping build_node_set_interface(const NodeSetInterfaceParams &params,
                                                  const bNodeTree &src_tree,
                                                  const Span<bNode *> src_nodes,
                                                  bNodeTree &dst_tree)
{
  NodeTreeInterfaceMapping result;
  NodeSetInterfaceBuilder builder(params, result, dst_tree, src_nodes);

  src_tree.ensure_topology_cache();

  const Set<const bNode *> nodes_set(src_nodes);
  for (const bNode *node : src_nodes) {
    for (const bNodeSocket *socket : node->input_sockets()) {
      builder.expose_socket(*socket, nullptr);
    }
    for (const bNodeSocket *socket : node->output_sockets()) {
      builder.expose_socket(*socket, nullptr);
    }
  }
  return result;
}

static void expose_declaration_item_recursive(NodeSetInterfaceBuilder &builder,
                                              const bNode &src_node,
                                              const nodes::ItemDeclaration &item_decl,
                                              bNodeTreeInterfacePanel *parent)
{
  if (const nodes::SocketDeclaration *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(
          &item_decl))
  {
    builder.expose_socket(src_node, *socket_decl, parent);
  }
  else if (const nodes::PanelDeclaration *panel_decl =
               dynamic_cast<const nodes::PanelDeclaration *>(&item_decl))
  {
    bNodeTreeInterfacePanel *io_panel = builder.expose_panel(src_node, *panel_decl, parent);
    if (io_panel) {
      for (const nodes::ItemDeclaration *child_item_decl : panel_decl->items) {
        expose_declaration_item_recursive(builder, src_node, *child_item_decl, io_panel);
      }
    }
  }
}

NodeTreeInterfaceMapping build_node_declaration_interface(const NodeSetInterfaceParams &params,
                                                          const bNode &src_node,
                                                          bNodeTree &dst_tree)
{
  BLI_assert(src_node.declaration() != nullptr);
  const nodes::NodeDeclaration &node_decl = *src_node.declaration();

  NodeTreeInterfaceMapping result;
  NodeSetInterfaceBuilder builder(params, result, dst_tree, {&src_node});
  for (const nodes::ItemDeclaration *item_decl : node_decl.root_items) {
    expose_declaration_item_recursive(builder, src_node, *item_decl, nullptr);
  }

  return result;
}

static void map_socket(NodeTreeInterfaceMapping &io_mapping,
                       const NodeSetInterfaceParams &params,
                       const bNode &group_node,
                       const bNodeTreeInterfaceSocket &io_socket)
{
  const bNodeTree &group_tree = *id_cast<const bNodeTree *>(group_node.id);
  const bool is_input = (io_socket.flag & NODE_INTERFACE_SOCKET_INPUT);
  const bNodeSocket *group_socket = is_input ?
                                        group_node.input_by_identifier(io_socket.identifier) :
                                        group_node.output_by_identifier(io_socket.identifier);
  BLI_assert(group_socket);
  if (!group_socket->is_available()) {
    return;
  }
  if (params.skip_hidden && !group_socket->is_visible()) {
    return;
  }

  NodeTreeInterfaceMapping::InterfaceSocketData data;
  data.internal_sockets.add_multiple(
      get_internal_group_links(group_tree, io_socket, params.skip_hidden)
          .as_span()
          .cast<NodeAndSocket>());
  data.external_sockets.add_multiple(get_socket_links(*group_socket, false));
  data.hidden = group_socket->flag & SOCK_HIDDEN;
  data.collapsed = group_socket->flag & SOCK_COLLAPSED;
  io_mapping.socket_data.add(&io_socket, std::move(data));
}

static void map_panel(NodeTreeInterfaceMapping &io_mapping,
                      const NodeSetInterfaceParams & /*params*/,
                      const bNode &group_node,
                      const bNodeTreeInterfacePanel &io_panel)
{
  NodeTreeInterfaceMapping::InterfacePanelData data;
  for (const bNodePanelState &panel_state : group_node.panel_states()) {
    if (panel_state.identifier == io_panel.identifier) {
      data.collapsed = panel_state.is_collapsed();
    }
  }
  io_mapping.panel_data.add(&io_panel, std::move(data));
}

NodeTreeInterfaceMapping map_group_node_interface(const NodeSetInterfaceParams &params,
                                                  const bNode &group_node)
{
  BLI_assert(group_node.is_group());
  const bNodeTree &group_tree = *id_cast<const bNodeTree *>(group_node.id);

  NodeTreeInterfaceMapping result;
  for (const bNodeTreeInterfaceItem *io_item : group_tree.interface_items()) {
    switch (io_item->item_type) {
      case NODE_INTERFACE_PANEL: {
        const auto *io_panel = reinterpret_cast<const bNodeTreeInterfacePanel *>(io_item);
        map_panel(result, params, group_node, *io_panel);
        break;
      }
      case NODE_INTERFACE_SOCKET: {
        const auto *io_socket = reinterpret_cast<const bNodeTreeInterfaceSocket *>(io_item);
        map_socket(result, params, group_node, *io_socket);
      }
    }
  }
  return result;
}

bNodeTree &NodeSetCopy::dst_tree() const
{
  return dst_tree_;
}

const Map<const bNode *, bNode *> &NodeSetCopy::node_map() const
{
  return node_map_;
}

const Map<const bNodeSocket *, bNodeSocket *> &NodeSetCopy::socket_map() const
{
  return socket_map_;
}

const Map<int32_t, int32_t> &NodeSetCopy::node_identifier_map() const
{
  return node_identifier_map_;
}

NodeSetCopy NodeSetCopy::from_nodes(Main &bmain,
                                    const bNodeTree &src_tree,
                                    const Span<const bNode *> src_nodes,
                                    bNodeTree &dst_tree)
{
  node_deselect_all(dst_tree);

  NodeSetCopy result(dst_tree);
  Vector<AnimationBasePathChange> anim_basepaths;
  for (const bNode *src_node : src_nodes) {
    bNode *dst_node = bke::node_copy_with_mapping(
        &dst_tree, *src_node, LIB_ID_COPY_DEFAULT, std::nullopt, std::nullopt, result.socket_map_);

    result.node_map_.add(src_node, dst_node);
    result.node_identifier_map_.add(src_node->identifier, dst_node->identifier);

    anim_basepaths.append(
        {node_basepath(src_tree, *src_node), node_basepath(dst_tree, *dst_node)});
  }

  /* Recreate parent/child relationship of nodes. */
  for (const auto &item : result.node_map_.items()) {
    const bNode *node = item.key;
    bNode *new_node = item.value;
    if (node->parent) {
      if (bNode *new_parent = result.node_map_.lookup_default(node->parent, nullptr)) {
        bke::node_attach_node(dst_tree, *new_node, *new_parent);
      }
      else {
        bke::node_detach_node(dst_tree, *new_node);
      }
    }
  }

  /* Recreate internal links. */
  const Vector<const bNodeLink *> internal_links = find_internal_links(src_tree, src_nodes);
  for (const bNodeLink *src_link : internal_links) {
    bke::node_add_link(dst_tree,
                       *result.node_map_.lookup(src_link->fromnode),
                       *result.socket_map_.lookup(src_link->fromsock),
                       *result.node_map_.lookup(src_link->tonode),
                       *result.socket_map_.lookup(src_link->tosock));
  }

  /* Recreate zone pairing between new nodes. */
  const Vector<bNode *> new_nodes(result.node_map_.values().begin(),
                                  result.node_map_.values().end());
  remap_pairing(dst_tree, new_nodes, result.node_identifier_map_);

  /* Copy animation data of source nodes. */
  BKE_animdata_copy_by_basepath(bmain, src_tree.id, dst_tree.id, anim_basepaths);

  /* Move nodes in the group to the center */
  if (const std::optional<Bounds<float2>> bounds = node_location_bounds(src_nodes)) {
    const float2 center = bounds->center();
    for (bNode *node : new_nodes) {
      node->location[0] -= center[0];
      node->location[1] -= center[1];
    }
  }

  return result;
}

NodeSetCopy NodeSetCopy::from_predicate(Main &bmain,
                                        const bNodeTree &src_tree,
                                        FunctionRef<bool(const bNode &node)> node_predicate,
                                        bNodeTree &dst_tree)
{
  Vector<const bNode *> nodes;
  for (const bNode &node : src_tree.nodes) {
    if (node_predicate(node)) {
      nodes.append(&node);
    }
  }
  return from_nodes(bmain, src_tree, nodes, dst_tree);
}

GroupInputOutputNodes connect_copied_nodes_to_interface(const bContext &C,
                                                        const NodeSetCopy &copied_nodes,
                                                        const NodeTreeInterfaceMapping &io_mapping)
{
  Main &bmain = *CTX_data_main(&C);
  bNodeTree &tree = copied_nodes.dst_tree();
  tree.ensure_topology_cache();

  GroupInputOutputNodes io_nodes;
  io_nodes.output_node = tree.group_output_node();
  if (!io_nodes.output_node) {
    io_nodes.output_node = bke::node_add_static_node(&C, tree, NODE_GROUP_OUTPUT);
  }
  io_nodes.input_node = bke::node_add_static_node(&C, tree, NODE_GROUP_INPUT);

  /* This makes sure that all nodes have the correct sockets so that we can link. */
  BKE_main_ensure_invariants(bmain, tree.id);

  for (const auto &item : io_mapping.socket_data.items()) {
    for (const NodeAndSocket &origin : item.value.internal_sockets) {
      bNode *new_node = copied_nodes.node_map().lookup(&origin.node);
      bNodeSocket *new_socket = copied_nodes.socket_map().lookup(&origin.socket);
      if (new_socket->is_input()) {
        bNodeSocket *group_input_socket = node_group_input_find_socket(io_nodes.input_node,
                                                                       item.key->identifier);
        BLI_assert(group_input_socket);
        bke::node_add_link(
            tree, *io_nodes.input_node, *group_input_socket, *new_node, *new_socket);
      }
      else {
        bNodeSocket *group_output_socket = node_group_output_find_socket(io_nodes.output_node,
                                                                         item.key->identifier);
        BLI_assert(group_output_socket);
        bke::node_add_link(
            tree, *new_node, *new_socket, *io_nodes.output_node, *group_output_socket);
      }
    }
  }

  /* Make sure group input/output node sockets match the tree interface. */
  nodes::update_node_declaration_and_sockets(tree, *io_nodes.input_node);
  nodes::update_node_declaration_and_sockets(tree, *io_nodes.output_node);

  /* Move group input/output nodes to the edges of the bounding box. */
  Vector<const bNode *> nodes_vec;
  for (const bNode *node : copied_nodes.node_map().values()) {
    nodes_vec.append(node);
  }
  if (const std::optional<Bounds<float2>> bounds = node_bounds(nodes_vec)) {
    io_nodes.input_node->location[0] = bounds->min[0] - 200.0f;
    io_nodes.input_node->location[1] = bounds->center()[1];
    io_nodes.output_node->location[0] = bounds->max[0] + 50.0f;
    io_nodes.output_node->location[1] = bounds->center()[1];
  }

  return io_nodes;
}

void connect_copied_nodes_to_external_sockets(const bNodeTree &src_tree,
                                              const NodeSetCopy &copied_nodes,
                                              const NodeTreeInterfaceMapping &io_mapping)
{
  using InterfaceSocketData = NodeTreeInterfaceMapping::InterfaceSocketData;

  bNodeTree &dst_tree = copied_nodes.dst_tree();

  /* Deduplicate links in case multiple connects get merged. This can happen because input and
   * output sockets are connected, potentially adding redundant links. */
  Set<std::pair<MutableNodeAndSocket, MutableNodeAndSocket>> unique_links;
  /* Connect one set of sockets to all sockets in the other set.
   * One set must contain inputs and the other outputs, it doesn't matter which is which.
   * In principle can add N * M links.
   * In practice the "from_sockets" list is generally limited to a single socket. */
  auto connect_socket_lists = [&](const Span<MutableNodeAndSocket> sockets1,
                                  const Span<MutableNodeAndSocket> sockets2) {
    for (const MutableNodeAndSocket &socket1 : sockets1) {
      for (const MutableNodeAndSocket &socket2 : sockets2) {
        if (socket1.socket.is_input()) {
          BLI_assert(socket2.socket.is_output());
          unique_links.add({socket2, socket1});
        }
        else {
          BLI_assert(socket2.socket.is_input());
          unique_links.add({socket1, socket2});
        }
      }
    }
  };

  for (const auto &item : io_mapping.socket_data.items()) {
    for (const NodeAndSocket &origin : item.value.internal_sockets) {
      if (origin.node.is_group_input()) {
        /* Directly connect external inputs to external outputs. */
        const bNodeTreeInterfaceSocket *io_socket = bke::node_find_interface_input_by_identifier(
            src_tree, origin.socket.identifier);
        BLI_assert(io_socket);
        if (const InterfaceSocketData *data = io_mapping.socket_data.lookup_ptr(io_socket)) {
          connect_socket_lists(data->external_sockets, item.value.external_sockets);
        }
        continue;
      }
      if (origin.node.is_group_output()) {
        /* Directly connect external inputs to external outputs. */
        const bNodeTreeInterfaceSocket *io_socket = bke::node_find_interface_output_by_identifier(
            src_tree, origin.socket.identifier);
        BLI_assert(io_socket);
        if (const InterfaceSocketData *data = io_mapping.socket_data.lookup_ptr(io_socket)) {
          connect_socket_lists(data->external_sockets, item.value.external_sockets);
        }
        continue;
      }

      bNode *new_node = copied_nodes.node_map().lookup(&origin.node);
      bNodeSocket *new_socket = copied_nodes.socket_map().lookup(&origin.socket);
      MutableNodeAndSocket new_target = {*new_node, *new_socket};
      connect_socket_lists({new_target}, item.value.external_sockets);
    }
  }

  /* Actually add deduplicated links to the tree. */
  for (const std::pair<MutableNodeAndSocket, MutableNodeAndSocket> &item : unique_links) {
    bke::node_add_link(
        dst_tree, item.first.node, item.first.socket, item.second.node, item.second.socket);
  }
}

void connect_group_node_to_external_sockets(bNode &group_node,
                                            const NodeTreeInterfaceMapping &io_mapping)
{
  using InterfaceSocketData = NodeTreeInterfaceMapping::InterfaceSocketData;

  bNodeTree &owner_tree = group_node.owner_tree();
  const bNodeTree &group_tree = *id_cast<bNodeTree *>(group_node.id);

  /* Cache node socket lists to avoid invalid topology cache after linking. */
  owner_tree.ensure_topology_cache();
  const Span<bNodeSocket *> group_node_inputs = group_node.input_sockets();
  const Span<bNodeSocket *> group_node_outputs = group_node.output_sockets();

  for (bNodeSocket *group_node_input : group_node_inputs) {
    const bNodeTreeInterfaceSocket *interface = bke::node_find_interface_input_by_identifier(
        group_tree, group_node_input->identifier);
    if (!interface) {
      continue;
    }
    const InterfaceSocketData *data = io_mapping.socket_data.lookup_ptr(interface);
    BLI_assert(data);
    for (const MutableNodeAndSocket &link : data->external_sockets) {
      BLI_assert(owner_tree.all_nodes().contains(&link.node));
      bke::node_add_link(owner_tree, link.node, link.socket, group_node, *group_node_input);
    }
    /* Keep old socket visibility. */
    SET_FLAG_FROM_TEST(group_node_input->flag, data->hidden, SOCK_HIDDEN);
    SET_FLAG_FROM_TEST(group_node_input->flag, data->collapsed, SOCK_COLLAPSED);
  }
  for (bNodeSocket *group_node_output : group_node_outputs) {
    const bNodeTreeInterfaceSocket *interface = bke::node_find_interface_output_by_identifier(
        group_tree, group_node_output->identifier);
    if (!interface) {
      continue;
    }
    const InterfaceSocketData *data = io_mapping.socket_data.lookup_ptr(interface);
    BLI_assert(data);
    for (const MutableNodeAndSocket &link : data->external_sockets) {
      BLI_assert(owner_tree.all_nodes().contains(&link.node));
      bke::node_add_link(owner_tree, group_node, *group_node_output, link.node, link.socket);
    }
    /* Keep old socket visibility. */
    SET_FLAG_FROM_TEST(group_node_output->flag, data->hidden, SOCK_HIDDEN);
    SET_FLAG_FROM_TEST(group_node_output->flag, data->collapsed, SOCK_COLLAPSED);
  }

  /* Keep old panel collapse status. */
  MutableSpan<bNodePanelState> panel_states = group_node.panel_states();
  for (const auto &item : io_mapping.panel_data.items()) {
    for (bNodePanelState &new_panel_state : panel_states) {
      if (new_panel_state.identifier == item.key->identifier) {
        SET_FLAG_FROM_TEST(new_panel_state.flag, item.value.collapsed, NODE_PANEL_COLLAPSED);
      }
    }
  }
}

struct NestedNodeRefIDGenerator {
 private:
  Set<int32_t> used_ref_ids_;
  RandomNumberGenerator rng_;

 public:
  NestedNodeRefIDGenerator(const Span<bNestedNodeRef> used_nested_node_refs)
      : rng_(RandomNumberGenerator::from_random_seed())
  {
    for (const bNestedNodeRef &ref : used_nested_node_refs) {
      used_ref_ids_.add(ref.id);
    }
  }

  int32_t operator()()
  {
    /* Find new unique identifier for the nested node ref. */
    while (true) {
      const int32_t new_id = rng_.get_int32(INT32_MAX);
      if (used_ref_ids_.add(new_id)) {
        return new_id;
      }
    }
    BLI_assert_unreachable();
    return 0;
  }
};

static void append_nested_node_refs(bNodeTree &ntree, const Span<bNestedNodeRef> nested_node_refs)
{
  const int new_nested_node_refs_num = ntree.nested_node_refs_num + nested_node_refs.size();
  bNestedNodeRef *new_nested_node_refs = MEM_new_array_for_free<bNestedNodeRef>(
      new_nested_node_refs_num, __func__);
  uninitialized_copy_n(ntree.nested_node_refs, ntree.nested_node_refs_num, new_nested_node_refs);
  uninitialized_copy_n(nested_node_refs.data(),
                       nested_node_refs.size(),
                       new_nested_node_refs + ntree.nested_node_refs_num);

  MEM_SAFE_FREE(ntree.nested_node_refs);
  ntree.nested_node_refs = new_nested_node_refs;
  ntree.nested_node_refs_num = new_nested_node_refs_num;
}

void update_nested_node_refs_after_moving_nodes_into_group(bNodeTree &src_tree,
                                                           const bNode &group_node,
                                                           const NodeSetCopy &node_set_copy)
{
  BLI_assert(group_node.is_group());
  BLI_assert(group_node.id == &node_set_copy.dst_tree().id);

  bNodeTree &dst_tree = *id_cast<bNodeTree *>(group_node.id);
  /* Update nested node references in the parent and child node tree. */
  NestedNodeRefIDGenerator ref_id_gen(dst_tree.nested_node_refs_span());

  Vector<bNestedNodeRef> new_group_refs;
  for (bNestedNodeRef &ref : src_tree.nested_node_refs_span()) {
    const std::optional<int32_t> new_node_id = node_set_copy.node_identifier_map().lookup_try(
        ref.path.node_id);
    if (!new_node_id) {
      /* The node was not moved between node groups. */
      continue;
    }
    /* Find new unique identifier for the nested node ref. */
    const int32_t new_ref_id = ref_id_gen();

    /* Add a new nested node ref inside the group. */
    bNestedNodeRef new_ref = ref;
    new_ref.id = new_ref_id;
    new_ref.path.node_id = *new_node_id;
    new_group_refs.append(new_ref);

    /* Update the nested node ref in the parent so that it points to the same node that is now
     * inside of a nested group. */
    ref.path.node_id = group_node.identifier;
    ref.path.id_in_node = new_ref_id;
  }

  append_nested_node_refs(dst_tree, new_group_refs);
}

void update_nested_node_refs_after_ungroup(bNodeTree &dst_tree,
                                           const bNode &group_node,
                                           const NodeSetCopy &node_set_copy)
{
  BLI_assert(group_node.is_group());
  BLI_assert(group_node.id != nullptr);
  BLI_assert(&dst_tree == &node_set_copy.dst_tree());

  const bNodeTree &src_tree = *id_cast<const bNodeTree *>(group_node.id);
  for (bNestedNodeRef &dst_ref : dst_tree.nested_node_refs_span()) {
    if (dst_ref.path.node_id != group_node.identifier) {
      continue;
    }
    const bNestedNodeRef *src_ref = src_tree.find_nested_node_ref(dst_ref.path.id_in_node);
    if (!src_ref) {
      continue;
    }
    constexpr int32_t missing_id = -1;
    const int32_t dst_node_id = node_set_copy.node_identifier_map().lookup_default(
        src_ref->path.node_id, missing_id);
    if (dst_node_id == missing_id) {
      continue;
    }
    dst_ref.path.node_id = dst_node_id;
    dst_ref.path.id_in_node = src_ref->path.id_in_node;
  }
}

/** \} */

}  // namespace blender::ed::space_node
