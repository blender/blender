/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include <cstdlib>

#include "DNA_node_types.h"

#include "BKE_anim_data.hh"
#include "BLI_listbase.h"
#include "BLI_listbase_iterator.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_interface_convert.hh"
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

namespace blender {

const bNodeSocket &NodeAndSocket::find_socket_in_node(const bNode &other_node) const
{
  /* Don't use "by_identifier" functions of bNode because they depend on valid topology cache. */
  ListBaseT<bNodeSocket> sockets = (this->in_out == SOCK_IN) ? other_node.inputs :
                                                               other_node.outputs;
  const bNodeSocket *socket = reinterpret_cast<bNodeSocket *>(BLI_findstring(
      &sockets, this->socket_identifier.c_str(), offsetof(bNodeSocket, identifier)));
  BLI_assert(socket != nullptr);
  return *socket;
}

bNodeSocket &NodeAndSocket::find_socket_in_node(bNode &other_node) const
{
  return const_cast<bNodeSocket &>(
      this->find_socket_in_node(const_cast<const bNode &>(other_node)));
}

const bNodeSocket &MutableNodeAndSocket::find_socket_in_node(const bNode &other_node) const
{
  /* Don't use "by_identifier" functions of bNode because they depend on valid topology cache. */
  ListBaseT<bNodeSocket> sockets = (this->in_out == SOCK_IN) ? other_node.inputs :
                                                               other_node.outputs;
  const bNodeSocket *socket = reinterpret_cast<bNodeSocket *>(BLI_findstring(
      &sockets, this->socket_identifier.c_str(), offsetof(bNodeSocket, identifier)));
  BLI_assert(socket != nullptr);
  return *socket;
}

bNodeSocket &MutableNodeAndSocket::find_socket_in_node(bNode &other_node) const
{
  return const_cast<bNodeSocket &>(
      this->find_socket_in_node(const_cast<const bNode &>(other_node)));
}

namespace ed::space_node {

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
                                                           const eNodeSocketInOut in_out,
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
      tree_for_interface, node_for_io, socket_for_io, socket_for_name.name, in_out);
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
  Map<const bNodeSocket *, bNodeTreeInterfaceSocket *> data_by_socket_;

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
  const eNodeSocketInOut in_out = eNodeSocketInOut(src_socket.in_out);

  auto try_add_socket_data = [&](const bNodeSocket &key_socket) -> InterfaceSocketData * {
    InterfaceSocketData *data = io_mapping_.socket_data.lookup_ptr(
        data_by_socket_.lookup_default(&key_socket, nullptr));
    if (data) {
      return data;
    }

    bNodeTreeInterfaceSocket *io_socket = add_interface_from_socket(
        src_tree, key_socket, in_out, dst_tree_, parent);
    if (io_socket) {
      data = &io_mapping_.socket_data.lookup_or_add(io_socket, {});
      data_by_socket_.add_new(&key_socket, io_socket);

      data->hidden = key_socket.flag & SOCK_HIDDEN;
      data->collapsed = key_socket.flag & SOCK_COLLAPSED;
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
      if (InterfaceSocketData *data = try_add_socket_data(src_socket)) {
        data->internal_sockets.add({src_socket.owner_node(), src_socket});
      }
    }

    return;
  }

  const bool use_external_socket_key = src_socket.is_input() ? !params_.use_unique_input :
                                                               params_.use_unique_output;
  if (use_external_socket_key) {
    /* Create a unique interface socket for each external link. */
    for (const MutableNodeAndSocket &external_socket : external_links) {
      if (InterfaceSocketData *data = try_add_socket_data(external_socket.find_socket())) {
        data->internal_sockets.add({src_socket.owner_node(), src_socket});
        data->external_sockets.add(external_socket);
      }
    }
  }
  else {
    /* Create interface based on the internal socket. */
    if (InterfaceSocketData *data = try_add_socket_data(src_socket)) {
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
      panel_decl.name.ref(), panel_decl.description, flag, parent);
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

static void make_panel_visible(bNode &node, const nodes::PanelDeclaration &panel_decl)
{
  for (bNodePanelState &panel_state : node.panel_states()) {
    if (panel_state.identifier == panel_decl.identifier) {
      panel_state.flag &= ~NODE_PANEL_COLLAPSED;
      return;
    }
  }
  BLI_assert_unreachable();
}

static bool make_parent_panels_visible_recursive(bNode &node,
                                                 StringRef socket_identifier,
                                                 const Span<nodes::ItemDeclaration *> item_decls)
{
  for (const nodes::ItemDeclaration *item_decl : item_decls) {
    if (const auto *socket_decl = dynamic_cast<const nodes::SocketDeclaration *>(item_decl)) {
      if (socket_identifier == socket_decl->identifier) {
        return true;
      }
    }
    if (const auto *child_panel_decl = dynamic_cast<const nodes::PanelDeclaration *>(item_decl)) {
      if (make_parent_panels_visible_recursive(node, socket_identifier, child_panel_decl->items)) {
        make_panel_visible(node, *child_panel_decl);
        return true;
      }
    }
  }
  return false;
}

static void make_socket_visible(bNode &node, bNodeSocket &socket)
{
  socket.flag &= ~SOCK_HIDDEN;
  if (const nodes::NodeDeclaration *node_decl = node.declaration()) {
    make_parent_panels_visible_recursive(node, socket.identifier, node_decl->root_items);
  }
}

/* Add a link to a node tree. If one of the sockets is visible the other will be visible too. */
static bNodeLink &add_link_and_make_visible(bNodeTree &tree,
                                            bNode &from_node,
                                            bNodeSocket &from_socket,
                                            bNode &to_node,
                                            bNodeSocket &to_socket)
{
  BLI_assert(from_socket.is_available());
  BLI_assert(to_socket.is_available());
  if (!from_socket.is_user_hidden() || !to_socket.is_user_hidden()) {
    if (from_socket.is_user_hidden() || from_socket.is_panel_collapsed()) {
      make_socket_visible(from_node, from_socket);
    }
    if (to_socket.is_user_hidden() || to_socket.is_panel_collapsed()) {
      make_socket_visible(to_node, to_socket);
    }
  }

  return bke::node_add_link(tree, from_node, from_socket, to_node, to_socket);
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

/* Make sure the target node tree uses a different action than the source.
 * Otherwise the target action is cleared to ensure a new action is created. */
static void ensure_separate_actions(Main &bmain, const bNodeTree &src, bNodeTree &dst)
{
  const AnimData *src_adt = BKE_animdata_from_id(&src.id);
  AnimData *dst_adt = BKE_animdata_from_id(&dst.id);
  if (!src_adt || !dst_adt) {
    /* Nothing to do:
     * If the source has no animdata nothing will be copied.
     * If the target has no animdata a new action will be created anyway. */
    return;
  }

  if (dst_adt->action == src_adt->action) {
    const bool unassign_ok = animrig::unassign_action({dst.id, *dst_adt});
    BLI_assert_msg(unassign_ok, "Expected Action unassignment to work");
    UNUSED_VARS_NDEBUG(unassign_ok);

    DEG_relations_tag_update(&bmain);
  }
}

NodeTreeInterfaceMapping map_group_node_interface(const NodeSetInterfaceParams &params,
                                                  const bNodeTree &tree,
                                                  const bNode &group_node)
{
  BLI_assert(group_node.is_group());
  const bNodeTree &group_tree = *id_cast<const bNodeTree *>(group_node.id);

  tree.ensure_topology_cache();

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

const Map<int32_t, int32_t> &NodeSetCopy::node_identifier_map() const
{
  return node_identifier_map_;
}

NodeSetCopy NodeSetCopy::from_nodes(Main &bmain,
                                    const bNodeTree &src_tree,
                                    const Span<const bNode *> src_nodes,
                                    bNodeTree &dst_tree)
{
  NodeSetCopy result(dst_tree);
  Vector<AnimationBasePathChange> anim_basepaths;
  /* Note: socket map is not stored in NodeSetCopy because socket pointers are easily invalidated
   * by adding links to the tree. This should only be used locally. */
  Map<const bNodeSocket *, bNodeSocket *> socket_map;
  for (const bNode *src_node : src_nodes) {
    bNode *dst_node = bke::node_copy_with_mapping(
        &dst_tree, *src_node, LIB_ID_COPY_DEFAULT, std::nullopt, std::nullopt, socket_map);

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
    add_link_and_make_visible(dst_tree,
                              *result.node_map_.lookup(src_link->fromnode),
                              *socket_map.lookup(src_link->fromsock),
                              *result.node_map_.lookup(src_link->tonode),
                              *socket_map.lookup(src_link->tosock));
  }

  /* Recreate zone pairing between new nodes. */
  const Vector<bNode *> new_nodes(result.node_map_.values().begin(),
                                  result.node_map_.values().end());
  remap_pairing(dst_tree, new_nodes, result.node_identifier_map_);

  /* Copy animation data of source nodes. */
  if (&src_tree != &dst_tree) {
    ensure_separate_actions(bmain, src_tree, dst_tree);
  }
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
      bNode &new_node = *copied_nodes.node_map().lookup(&origin.node);
      bNodeSocket &new_socket = origin.find_socket_in_node(new_node);
      if (new_socket.is_input()) {
        bNodeSocket *group_input_socket = node_group_input_find_socket(io_nodes.input_node,
                                                                       item.key->identifier);
        BLI_assert(group_input_socket);
        add_link_and_make_visible(
            tree, *io_nodes.input_node, *group_input_socket, new_node, new_socket);
      }
      else {
        bNodeSocket *group_output_socket = node_group_output_find_socket(io_nodes.output_node,
                                                                         item.key->identifier);
        BLI_assert(group_output_socket);
        add_link_and_make_visible(
            tree, new_node, new_socket, *io_nodes.output_node, *group_output_socket);
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
    io_nodes.output_node->location[0] = bounds->max[0] + 50.0f;
  }
  /* Ignore node dimensions for vertical placement. */
  if (const std::optional<Bounds<float2>> bounds = node_location_bounds(nodes_vec)) {
    io_nodes.input_node->location[1] = bounds->center()[1];
    io_nodes.output_node->location[1] = bounds->center()[1];
  }

  return io_nodes;
}

static bool socket_types_need_conversion(const StringRef from_type, const StringRef to_type)
{
  const bke::bNodeSocketType *from_typeinfo = bke::node_socket_type_find(from_type);
  const bke::bNodeSocketType *to_typeinfo = bke::node_socket_type_find(to_type);
  const bool from_static = node_is_static_socket_type(*from_typeinfo);
  const bool to_static = node_is_static_socket_type(*to_typeinfo);
  if (from_typeinfo == nullptr || to_typeinfo == nullptr) {
    return true;
  }
  if (from_static != to_static) {
    return true;
  }
  /* Dynamic socket types need conversion if type idnames are different. */
  if (!from_static && !to_static) {
    return from_type != to_type;
  }
  /* Static socket types need conversion only if the base types are different. */
  return from_typeinfo->type != to_typeinfo->type;
}

static bool any_link_need_conversion(const Span<MutableNodeAndSocket> links,
                                     const bNodeTreeInterfaceSocket &io_socket)
{
  /* A proxy is needed if any internal or external connection has a different type and therefore
   * cannot directly be connected without loss of conversion. */
  for (const MutableNodeAndSocket &in_link : links) {
    const bNodeSocket &in_socket = in_link.find_socket();
    if (socket_types_need_conversion(in_socket.idname, io_socket.socket_type)) {
      return true;
    }
  }
  return false;
}

static std::pair<std::optional<MutableNodeAndSocket>, std::optional<MutableNodeAndSocket>>
find_proxy_node_sockets(bNode &proxy_node)
{
  std::optional<MutableNodeAndSocket> in, out;
  for (bNodeSocket &socket : proxy_node.inputs) {
    if (socket.is_available() && !socket.is_user_hidden()) {
      in.emplace(MutableNodeAndSocket{proxy_node, socket});
      break;
    }
  }
  for (bNodeSocket &socket : proxy_node.outputs) {
    if (socket.is_available() && !socket.is_user_hidden()) {
      out.emplace(MutableNodeAndSocket{proxy_node, socket});
      break;
    }
  }
  return {in, out};
}

using UniqueLinkSet = Set<std::pair<MutableNodeAndSocket, MutableNodeAndSocket>>;

/* Create a constant value or field input proxy node, using the source socket value. */
static bNode *create_proxy_input_node(const bNodeTreeInterfaceSocket &io_socket,
                                      const bNodeTree &src_tree,
                                      const bNodeSocket &src_socket,
                                      bContext &C,
                                      bNodeTree &dst_tree,
                                      Vector<AnimationBasePathChange> &anim_basepaths)
{
  const eNodeSocketDatatype socket_type = bke::node_socket_type_find(io_socket.socket_type)->type;
  const NodeDefaultInputType default_input_type = NodeDefaultInputType(io_socket.default_input);
  /* TODO Inferred structure type isn't fully working yet, use declared structure type for now. */
  // const nodes::StructureType structure_type = nodes::StructureType(
  //     src_socket.runtime->inferred_structure_type);
  const nodes::StructureType structure_type = io_socket.structure_type ==
                                                      NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO ?
                                                  nodes::StructureType::Dynamic :
                                                  nodes::StructureType(io_socket.structure_type);

  if (ELEM(structure_type, nodes::StructureType::Grid, nodes::StructureType::List)) {
    /* Grids and Lists don't have input value nodes or implicit field inputs. */
    return nullptr;
  }

  if (bNode *proxy_node = bke::node_interface::create_proxy_implicit_input_node(
          socket_type, default_input_type, C, dst_tree))
  {
    return proxy_node;
  }

  if (!src_socket.default_value) {
    /* No proxy needed if the socket type does not have input values. */
    return nullptr;
  }
  if (bNode *proxy_node = bke::node_interface::create_proxy_const_input_node(
          socket_type, src_tree, src_socket, C, dst_tree, anim_basepaths))
  {
    return proxy_node;
  }

  /* Fall back to reroute node. */
  return bke::node_add_static_node(&C, dst_tree, NODE_REROUTE);
}

/* Create a converter proxy node for the interface socket. */
static bNode *create_proxy_converter_node(const bNodeTreeInterfaceSocket &io_socket,
                                          const bNodeTree &src_tree,
                                          const bNodeSocket *src_socket,
                                          bContext &C,
                                          bNodeTree &dst_tree,
                                          Vector<AnimationBasePathChange> &anim_basepaths)
{
  const eNodeSocketDatatype socket_type = bke::node_socket_type_find(io_socket.socket_type)->type;

  if (bNode *proxy_node = bke::node_interface::create_proxy_converter_node(
          socket_type, src_tree, src_socket, C, dst_tree, anim_basepaths))
  {
    return proxy_node;
  }

  /* Fall back to reroute node. */
  return bke::node_add_static_node(&C, dst_tree, NODE_REROUTE);
}

/* Replace an interface socket by adding proxy nodes. */
static void replace_interface_socket(
    bContext &C,
    bNodeTree &dst_tree,
    const bNodeTreeInterfaceSocket &io_socket,
    const Span<MutableNodeAndSocket> incoming_links,
    const Span<MutableNodeAndSocket> outgoing_links,
    const bNode *group_node,
    InterfaceProxyNodes &interface_proxies,
    float2 &input_location,
    float2 &output_location,
    UniqueLinkSet &unique_links,
    Vector<AnimationBasePathChange> &anim_basepaths_for_dst_tree,
    Vector<AnimationBasePathChange> &anim_basepaths_for_group_tree)
{
  /* If there are no output connections the socket is unused and can be discarded. */
  if (outgoing_links.is_empty()) {
    return;
  }

  /* Find the socket input value to use, if available. */
  const bNodeTree &group_tree = *id_cast<bNodeTree *>(group_node->id);
  const bNode *group_output_node = group_tree.group_output_node();
  /* The source for inputs is the group node, for constant outputs is the group output node. */
  const bool is_input = io_socket.flag & NODE_INTERFACE_SOCKET_INPUT;
  Vector<AnimationBasePathChange> &anim_basepaths = is_input ? anim_basepaths_for_dst_tree :
                                                               anim_basepaths_for_group_tree;
  const bNodeTree &src_tree = is_input ? dst_tree : group_tree;
  const bNode *src_node = is_input ? group_node : group_output_node;
  const bNodeSocket *src_socket = src_node ? bke::node_find_socket(
                                                 *src_node, SOCK_IN, io_socket.identifier) :
                                             nullptr;

  /* Create a proxy node if necessary. */
  bNode *proxy_node = nullptr;
  if (incoming_links.is_empty()) {
    /* The socket has no incoming links. A proxy is needed if the socket value needs to be stored
     * or if the socket uses a default input field. */
    if (src_socket) {
      proxy_node = create_proxy_input_node(
          io_socket, src_tree, *src_socket, C, dst_tree, anim_basepaths);
    }
  }
  else {
    /* A proxy is needed if any internal or external connection has a different type
     * and therefore cannot directly be connected without loss of conversion. */
    if (any_link_need_conversion(incoming_links, io_socket) &&
        any_link_need_conversion(outgoing_links, io_socket))
    {
      proxy_node = create_proxy_converter_node(
          io_socket, src_tree, src_socket, C, dst_tree, anim_basepaths);
    }
  }

  if (proxy_node) {
    BLI_assert(proxy_node);
    BLI_strncpy(proxy_node->label, io_socket.name, sizeof(proxy_node->label));

    const float width = (proxy_node->is_reroute() ? 0.0f : proxy_node->width);
    const float height = (proxy_node->is_reroute() ? 0.0f : proxy_node->height);
    if (is_input) {
      proxy_node->location[0] = input_location.x - width;
      proxy_node->location[1] = input_location.y;
      input_location.y -= height + 20.0f;
    }
    else {
      proxy_node->location[0] = output_location.x;
      proxy_node->location[1] = output_location.y;
      output_location.y -= height + 20.0f;
    }

    interface_proxies.add_new(io_socket.identifier, proxy_node);

    /* Connect incoming to outgoing sockets via the proxy node. */
    const auto [proxy_input, proxy_output] = find_proxy_node_sockets(*proxy_node);
    BLI_assert(proxy_input || incoming_links.is_empty());
    BLI_assert(proxy_output || outgoing_links.is_empty());
    for (const MutableNodeAndSocket &from_socket : incoming_links) {
      unique_links.add({from_socket, *proxy_input});
    }
    for (const MutableNodeAndSocket &to_socket : outgoing_links) {
      unique_links.add({*proxy_output, to_socket});
    }
  }
  else {
    /* Connect incoming to outgoing sockets directly.
     * Creates N * M links, in practice N is usually 1. */
    for (const MutableNodeAndSocket &from_socket : incoming_links) {
      for (const MutableNodeAndSocket &to_socket : outgoing_links) {
        unique_links.add({from_socket, to_socket});
      }
    }
  }
}

InterfaceProxyNodes connect_copied_nodes_to_external_sockets(
    bContext &C,
    const bNodeTree &src_tree,
    const NodeSetCopy &copied_nodes,
    const NodeTreeInterfaceMapping &io_mapping,
    const bNode *group_node)
{
  bNodeTree &dst_tree = copied_nodes.dst_tree();

  /* Set location for proxy nodes, based on drawing order of interface items. */
  float2 input_location = {-50, 0}, output_location = {50, 0};
  Vector<const bNode *> nodes_vec;
  for (const bNode *node : copied_nodes.node_map().values()) {
    nodes_vec.append(node);
  }
  if (const std::optional<Bounds<float2>> bounds = node_bounds(nodes_vec)) {
    const Bounds<float2> loc_bounds = *node_location_bounds(nodes_vec);
    /* Move outputs to the edge of copied nodes, which are centered at zero. */
    input_location.x += -loc_bounds.size().x * 0.5f;
    output_location.x += -loc_bounds.size().x * 0.5f + bounds->size().x;
  }

  /* Deduplicate links in case multiple connections get merged. This can happen because both
   * input and output sockets are connected, potentially adding redundant links. This can
   * theoretically create N * M links but in practice either N or M is usually 1. */
  UniqueLinkSet unique_links;
  /* Animdata paths for group sockets for copying animdata to proxy nodes. */
  Vector<AnimationBasePathChange> anim_basepaths_for_dst_tree;
  Vector<AnimationBasePathChange> anim_basepaths_for_group_tree;

  InterfaceProxyNodes interface_proxies;
  /* Loop over mapped items based on the interface socket order. */
  for (const bNodeTreeInterfaceItem *io_item : src_tree.interface_items()) {
    if (io_item->item_type != NODE_INTERFACE_SOCKET) {
      continue;
    }
    const auto &io_socket = bke::node_interface::get_item_as<bNodeTreeInterfaceSocket>(*io_item);
    const std::optional<NodeTreeInterfaceMapping::InterfaceSocketData> socket_data =
        io_mapping.socket_data.lookup_try(&io_socket);
    if (!socket_data) {
      continue;
    }
    const bool is_input = io_socket.flag & NODE_INTERFACE_SOCKET_INPUT;

    /* Gather all new links to and from the interface socket that must be added. */
    Vector<MutableNodeAndSocket> incoming_links, outgoing_links;
    if (is_input) {
      incoming_links.extend(socket_data->external_sockets);
    }
    else {
      outgoing_links.extend(socket_data->external_sockets);
    }
    for (const NodeAndSocket &origin : socket_data->internal_sockets) {
      if (origin.node.is_group_output()) {
        /* Directly connect to external output links. */
        BLI_assert(is_input);
        const bNodeTreeInterfaceSocket *io_socket = bke::node_find_interface_output_by_identifier(
            src_tree, origin.socket_identifier);
        outgoing_links.extend(
            io_mapping.socket_data.lookup_default(io_socket, {}).external_sockets);
        continue;
      }
      if (origin.node.is_group_input()) {
        /* Directly connect to external input links. */
        BLI_assert(!is_input);
        const bNodeTreeInterfaceSocket *io_socket = bke::node_find_interface_input_by_identifier(
            src_tree, origin.socket_identifier);
        incoming_links.extend(
            io_mapping.socket_data.lookup_default(io_socket, {}).external_sockets);
        continue;
      }

      /* Map the old internal to the new socket in the target tree. */
      bNode &new_node = *copied_nodes.node_map().lookup_default(&origin.node, nullptr);
      bNodeSocket &new_socket = origin.find_socket_in_node(new_node);
      if (is_input) {
        outgoing_links.append({new_node, new_socket});
      }
      else {
        incoming_links.append({new_node, new_socket});
      }
    }

    replace_interface_socket(C,
                             dst_tree,
                             io_socket,
                             incoming_links,
                             outgoing_links,
                             group_node,
                             interface_proxies,
                             input_location,
                             output_location,
                             unique_links,
                             anim_basepaths_for_dst_tree,
                             anim_basepaths_for_group_tree);
  }

  /* Actually add deduplicated links to the tree. */
  for (const std::pair<MutableNodeAndSocket, MutableNodeAndSocket> &item : unique_links) {
    add_link_and_make_visible(dst_tree,
                              item.first.node,
                              item.first.find_socket(),
                              item.second.node,
                              item.second.find_socket());
  }

  BKE_animdata_copy_by_basepath(
      *CTX_data_main(&C), dst_tree.id, dst_tree.id, anim_basepaths_for_dst_tree);
  BKE_animdata_copy_by_basepath(
      *CTX_data_main(&C), *group_node->id, dst_tree.id, anim_basepaths_for_group_tree);

  return interface_proxies;
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
    if (!data) {
      continue;
    }
    for (const MutableNodeAndSocket &link : data->external_sockets) {
      BLI_assert(owner_tree.all_nodes().contains(&link.node));
      add_link_and_make_visible(
          owner_tree, link.node, link.find_socket(), group_node, *group_node_input);
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
    if (!data) {
      continue;
    }
    for (const MutableNodeAndSocket &link : data->external_sockets) {
      BLI_assert(owner_tree.all_nodes().contains(&link.node));
      add_link_and_make_visible(
          owner_tree, group_node, *group_node_output, link.node, link.find_socket());
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
  bNestedNodeRef *new_nested_node_refs = MEM_new_array<bNestedNodeRef>(new_nested_node_refs_num,
                                                                       __func__);
  uninitialized_copy_n(ntree.nested_node_refs, ntree.nested_node_refs_num, new_nested_node_refs);
  uninitialized_copy_n(nested_node_refs.data(),
                       nested_node_refs.size(),
                       new_nested_node_refs + ntree.nested_node_refs_num);

  MEM_SAFE_DELETE(ntree.nested_node_refs);
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

}  // namespace ed::space_node

}  // namespace blender
