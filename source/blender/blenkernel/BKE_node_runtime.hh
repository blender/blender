/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>
#include <mutex>

#include "BLI_multi_value_map.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "BKE_node.h"

struct bNode;
struct bNodeSocket;
struct bNodeTree;
struct bNodeType;

namespace blender::nodes {
struct FieldInferencingInterface;
class NodeDeclaration;
struct GeometryNodesLazyFunctionGraphInfo;
}  // namespace blender::nodes

namespace blender::bke {

class bNodeTreeRuntime : NonCopyable, NonMovable {
 public:
  /**
   * Keeps track of what changed in the node tree until the next update.
   * Should not be changed directly, instead use the functions in `BKE_node_tree_update.h`.
   * #eNodeTreeChangedFlag.
   */
  uint32_t changed_flag = 0;
  /**
   * A hash of the topology of the node tree leading up to the outputs. This is used to determine
   * of the node tree changed in a way that requires updating geometry nodes or shaders.
   */
  uint32_t output_topology_hash = 0;

  /**
   * Used to cache run-time information of the node tree.
   * #eNodeTreeRuntimeFlag.
   */
  uint8_t runtime_flag = 0;

  /** Information about how inputs and outputs of the node group interact with fields. */
  std::unique_ptr<nodes::FieldInferencingInterface> field_inferencing_interface;

  /**
   * For geometry nodes, a lazy function graph with some additional info is cached. This is used to
   * evaluate the node group. Caching it here allows us to reuse the preprocessed node tree in case
   * its used multiple times.
   */
  std::mutex geometry_nodes_lazy_function_graph_info_mutex;
  std::unique_ptr<nodes::GeometryNodesLazyFunctionGraphInfo>
      geometry_nodes_lazy_function_graph_info;

  /**
   * Protects access to all topology cache variables below. This is necessary so that the cache can
   * be updated on a const #bNodeTree.
   */
  std::mutex topology_cache_mutex;
  bool topology_cache_is_dirty = true;
  bool topology_cache_exists = false;
  /**
   * Under some circumstances, it can be useful to use the cached data while editing the
   * #bNodeTree. By default, this is protected against using an assert.
   */
  mutable std::atomic<int> allow_use_dirty_topology_cache = 0;

  /** Only valid when #topology_cache_is_dirty is false. */
  Vector<bNode *> nodes;
  Vector<bNodeLink *> links;
  Vector<bNodeSocket *> sockets;
  Vector<bNodeSocket *> input_sockets;
  Vector<bNodeSocket *> output_sockets;
  MultiValueMap<const bNodeType *, bNode *> nodes_by_type;
  Vector<bNode *> toposort_left_to_right;
  Vector<bNode *> toposort_right_to_left;
  Vector<bNode *> group_nodes;
  bool has_link_cycle = false;
  bool has_undefined_nodes_or_sockets = false;
  bNode *group_output_node = nullptr;
};

/**
 * Run-time data for every socket. This should only contain data that is somewhat persistent (i.e.
 * data that lives longer than a single depsgraph evaluation + redraw). Data that's only used in
 * smaller scopes should generally be stored in separate arrays and/or maps.
 */
class bNodeSocketRuntime : NonCopyable, NonMovable {
 public:
  /**
   * References a socket declaration that is owned by `node->declaration`. This is only runtime
   * data. It has to be updated when the node declaration changes. Access can be allowed by using
   * #AllowUsingOutdatedInfo.
   */
  const SocketDeclarationHandle *declaration = nullptr;

  /** #eNodeTreeChangedFlag. */
  uint32_t changed_flag = 0;

  /** Only valid when #topology_cache_is_dirty is false. */
  Vector<bNodeLink *> directly_linked_links;
  Vector<bNodeSocket *> directly_linked_sockets;
  Vector<bNodeSocket *> logically_linked_sockets;
  Vector<bNodeSocket *> logically_linked_skipped_sockets;
  bNode *owner_node = nullptr;
  bNodeSocket *internal_link_input = nullptr;
  int index_in_node = -1;
  int index_in_all_sockets = -1;
  int index_in_inout_sockets = -1;
};

/**
 * Run-time data for every node. This should only contain data that is somewhat persistent (i.e.
 * data that lives longer than a single depsgraph evaluation + redraw). Data that's only used in
 * smaller scopes should generally be stored in separate arrays and/or maps.
 */
class bNodeRuntime : NonCopyable, NonMovable {
 public:
  /**
   * Describes the desired interface of the node. This is run-time data only.
   * The actual interface of the node may deviate from the declaration temporarily.
   * It's possible to sync the actual state of the node to the desired state. Currently, this is
   * only done when a node is created or loaded.
   *
   * In the future, we may want to keep more data only in the declaration, so that it does not have
   * to be synced to other places that are stored in files. That especially applies to data that
   * can't be edited by users directly (e.g. min/max values of sockets, tooltips, ...).
   *
   * The declaration of a node can be recreated at any time when it is used. Caching it here is
   * just a bit more efficient when it is used a lot. To make sure that the cache is up-to-date,
   * call #nodeDeclarationEnsure before using it.
   *
   * Currently, the declaration is the same for every node of the same type. Going forward, that is
   * intended to change though. Especially when nodes become more dynamic with respect to how many
   * sockets they have.
   */
  NodeDeclarationHandle *declaration = nullptr;

  /** #eNodeTreeChangedFlag. */
  uint32_t changed_flag = 0;

  /** Only valid if #topology_cache_is_dirty is false. */
  Vector<bNodeSocket *> inputs;
  Vector<bNodeSocket *> outputs;
  Vector<bNodeLink *> internal_links;
  Map<StringRefNull, bNodeSocket *> inputs_by_identifier;
  Map<StringRefNull, bNodeSocket *> outputs_by_identifier;
  int index_in_tree = -1;
  bool has_linked_inputs = false;
  bool has_linked_outputs = false;
  bNodeTree *owner_tree = nullptr;
};

namespace node_tree_runtime {

/**
 * Is executed when the depsgraph determines that something in the node group changed that will
 * affect the output.
 */
void handle_node_tree_output_changed(bNodeTree &tree_cow);

class AllowUsingOutdatedInfo : NonCopyable, NonMovable {
 private:
  const bNodeTree &tree_;

 public:
  AllowUsingOutdatedInfo(const bNodeTree &tree) : tree_(tree)
  {
    tree_.runtime->allow_use_dirty_topology_cache.fetch_add(1);
  }

  ~AllowUsingOutdatedInfo()
  {
    tree_.runtime->allow_use_dirty_topology_cache.fetch_sub(1);
  }
};

inline bool topology_cache_is_available(const bNodeTree &tree)
{
  if (!tree.runtime->topology_cache_exists) {
    return false;
  }
  if (tree.runtime->allow_use_dirty_topology_cache.load() > 0) {
    return true;
  }
  if (tree.runtime->topology_cache_is_dirty) {
    return false;
  }
  return true;
}

inline bool topology_cache_is_available(const bNode &node)
{
  const bNodeTree *ntree = node.runtime->owner_tree;
  if (ntree == nullptr) {
    return false;
  }
  return topology_cache_is_available(*ntree);
}

inline bool topology_cache_is_available(const bNodeSocket &socket)
{
  const bNode *node = socket.runtime->owner_node;
  if (node == nullptr) {
    return false;
  }
  return topology_cache_is_available(*node);
}

}  // namespace node_tree_runtime

}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name #bNodeTree Inline Methods
 * \{ */

inline blender::Span<bNode *> bNodeTree::nodes_by_type(const blender::StringRefNull type_idname)
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->nodes_by_type.lookup(nodeTypeFind(type_idname.c_str()));
}

inline blender::Span<const bNode *> bNodeTree::nodes_by_type(
    const blender::StringRefNull type_idname) const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->nodes_by_type.lookup(nodeTypeFind(type_idname.c_str()));
}

inline blender::Span<const bNode *> bNodeTree::toposort_left_to_right() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->toposort_left_to_right;
}

inline blender::Span<const bNode *> bNodeTree::toposort_right_to_left() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->toposort_right_to_left;
}

inline blender::Span<const bNode *> bNodeTree::all_nodes() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->nodes;
}

inline blender::Span<bNode *> bNodeTree::all_nodes()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->nodes;
}

inline blender::Span<const bNode *> bNodeTree::group_nodes() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->group_nodes;
}

inline blender::Span<bNode *> bNodeTree::group_nodes()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->group_nodes;
}

inline bool bNodeTree::has_link_cycle() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->has_link_cycle;
}

inline bool bNodeTree::has_undefined_nodes_or_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->has_undefined_nodes_or_sockets;
}

inline const bNode *bNodeTree::group_output_node() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->group_output_node;
}

inline blender::Span<const bNodeSocket *> bNodeTree::all_input_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->input_sockets;
}

inline blender::Span<bNodeSocket *> bNodeTree::all_input_sockets()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->input_sockets;
}

inline blender::Span<const bNodeSocket *> bNodeTree::all_output_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->output_sockets;
}

inline blender::Span<bNodeSocket *> bNodeTree::all_output_sockets()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->output_sockets;
}

inline blender::Span<const bNodeSocket *> bNodeTree::all_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->sockets;
}

inline blender::Span<bNodeSocket *> bNodeTree::all_sockets()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->sockets;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #bNode Inline Methods
 * \{ */

inline blender::Span<bNodeSocket *> bNode::input_sockets()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->inputs;
}

inline blender::Span<bNodeSocket *> bNode::output_sockets()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->outputs;
}

inline blender::Span<const bNodeSocket *> bNode::input_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->inputs;
}

inline blender::Span<const bNodeSocket *> bNode::output_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->outputs;
}

inline bNodeSocket &bNode::input_socket(int index)
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->inputs[index];
}

inline bNodeSocket &bNode::output_socket(int index)
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->outputs[index];
}

inline const bNodeSocket &bNode::input_socket(int index) const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->inputs[index];
}

inline const bNodeSocket &bNode::output_socket(int index) const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->outputs[index];
}

inline const bNodeSocket &bNode::input_by_identifier(blender::StringRef identifier) const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->inputs_by_identifier.lookup_as(identifier);
}

inline const bNodeSocket &bNode::output_by_identifier(blender::StringRef identifier) const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->outputs_by_identifier.lookup_as(identifier);
}

inline const bNodeTree &bNode::owner_tree() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->owner_tree;
}

inline blender::StringRefNull bNode::label_or_name() const
{
  if (this->label[0] == '\0') {
    return this->name;
  }
  return this->label;
}

inline bool bNode::is_muted() const
{
  return this->flag & NODE_MUTED;
}

inline bool bNode::is_reroute() const
{
  return this->type == NODE_REROUTE;
}

inline bool bNode::is_frame() const
{
  return this->type == NODE_FRAME;
}

inline bool bNode::is_group() const
{
  return ELEM(this->type, NODE_GROUP, NODE_CUSTOM_GROUP);
}

inline bool bNode::is_group_input() const
{
  return this->type == NODE_GROUP_INPUT;
}

inline bool bNode::is_group_output() const
{
  return this->type == NODE_GROUP_OUTPUT;
}

inline blender::Span<const bNodeLink *> bNode::internal_links_span() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->internal_links;
}

inline const blender::nodes::NodeDeclaration *bNode::declaration() const
{
  return this->runtime->declaration;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #bNodeLink Inline Methods
 * \{ */

inline bool bNodeLink::is_muted() const
{
  return this->flag & NODE_LINK_MUTED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #bNodeSocket Inline Methods
 * \{ */

inline int bNodeSocket::index() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->index_in_node;
}

inline int bNodeSocket::index_in_tree() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->index_in_all_sockets;
}

inline bool bNodeSocket::is_available() const
{
  return (this->flag & SOCK_UNAVAIL) == 0;
}

inline bNode &bNodeSocket::owner_node()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->owner_node;
}

inline const bNodeTree &bNodeSocket::owner_tree() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->owner_node->runtime->owner_tree;
}

inline blender::Span<const bNodeSocket *> bNodeSocket::logically_linked_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->logically_linked_sockets;
}

inline blender::Span<const bNodeLink *> bNodeSocket::directly_linked_links() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->directly_linked_links;
}

inline blender::Span<bNodeLink *> bNodeSocket::directly_linked_links()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->directly_linked_links;
}

inline blender::Span<const bNodeSocket *> bNodeSocket::directly_linked_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->directly_linked_sockets;
}

inline blender::Span<bNodeSocket *> bNodeSocket::directly_linked_sockets()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->directly_linked_sockets;
}

inline bool bNodeSocket::is_directly_linked() const
{
  return !this->directly_linked_links().is_empty();
}

inline bool bNodeSocket::is_logically_linked() const
{
  return !this->logically_linked_sockets().is_empty();
}

inline const bNodeSocket *bNodeSocket::internal_link_input() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  BLI_assert(this->in_out == SOCK_OUT);
  return this->runtime->internal_link_input;
}

template<typename T> const T *bNodeSocket::default_value_typed() const
{
  return static_cast<const T *>(this->default_value);
}

inline bool bNodeSocket::is_input() const
{
  return this->in_out == SOCK_IN;
}

inline bool bNodeSocket::is_output() const
{
  return this->in_out == SOCK_OUT;
}

inline bool bNodeSocket::is_multi_input() const
{
  return this->flag & SOCK_MULTI_INPUT;
}

inline const bNode &bNodeSocket::owner_node() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->owner_node;
}

/** \} */
