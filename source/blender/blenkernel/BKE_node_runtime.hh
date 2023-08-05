/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>
#include <mutex>

#include "BLI_cache_mutex.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_resource_scope.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"

struct bNode;
struct bNodeSocket;
struct bNodeTree;
struct bNodeType;

namespace blender::nodes {
struct FieldInferencingInterface;
class NodeDeclaration;
struct GeometryNodesLazyFunctionGraphInfo;
namespace anonymous_attribute_lifetime {
struct RelationsInNode;
}
namespace aal = anonymous_attribute_lifetime;
}  // namespace blender::nodes
namespace blender::bke {
class bNodeTreeZones;
}
namespace blender::bke::anonymous_attribute_inferencing {
struct AnonymousAttributeInferencingResult;
};

namespace blender {

struct NodeIDHash {
  uint64_t operator()(const bNode *node) const
  {
    return node->identifier;
  }
  uint64_t operator()(const int32_t id) const
  {
    return id;
  }
};

struct NodeIDEquality {
  bool operator()(const bNode *a, const bNode *b) const
  {
    return a->identifier == b->identifier;
  }
  bool operator()(const bNode *a, const int32_t b) const
  {
    return a->identifier == b;
  }
  bool operator()(const int32_t a, const bNode *b) const
  {
    return this->operator()(b, a);
  }
};

}  // namespace blender

namespace blender::bke {

using NodeIDVectorSet = VectorSet<bNode *, DefaultProbingStrategy, NodeIDHash, NodeIDEquality>;

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

  /**
   * Storage of nodes based on their identifier. Also used as a contiguous array of nodes to
   * allow simpler and more cache friendly iteration. Supports lookup by integer or by node.
   * Unlike other caches, this is maintained eagerly while changing the tree.
   */
  NodeIDVectorSet nodes_by_id;

  /**
   * Execution data.
   *
   * XXX It would be preferable to completely move this data out of the underlying node tree,
   * so node tree execution could finally run independent of the tree itself.
   * This would allow node trees to be merely linked by other data (materials, textures, etc.),
   * as ID data is supposed to.
   * Execution data is generated from the tree once at execution start and can then be used
   * as long as necessary, even while the tree is being modified.
   */
  bNodeTreeExec *execdata = nullptr;

  /* Callbacks. */
  void (*progress)(void *, float progress) = nullptr;
  /** \warning may be called by different threads */
  void (*stats_draw)(void *, const char *str) = nullptr;
  bool (*test_break)(void *) = nullptr;
  void (*update_draw)(void *) = nullptr;
  void *tbh = nullptr, *prh = nullptr, *sdh = nullptr, *udh = nullptr;

  /** Information about how inputs and outputs of the node group interact with fields. */
  std::unique_ptr<nodes::FieldInferencingInterface> field_inferencing_interface;
  /** Information about usage of anonymous attributes within the group. */
  std::unique_ptr<anonymous_attribute_inferencing::AnonymousAttributeInferencingResult>
      anonymous_attribute_inferencing;

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
  CacheMutex topology_cache_mutex;
  std::atomic<bool> topology_cache_exists = false;
  /**
   * Under some circumstances, it can be useful to use the cached data while editing the
   * #bNodeTree. By default, this is protected against using an assert.
   */
  mutable std::atomic<int> allow_use_dirty_topology_cache = 0;

  CacheMutex tree_zones_cache_mutex;
  std::unique_ptr<bNodeTreeZones> tree_zones;

  /** Only valid when #topology_cache_is_dirty is false. */
  Vector<bNodeLink *> links;
  Vector<bNodeSocket *> sockets;
  Vector<bNodeSocket *> input_sockets;
  Vector<bNodeSocket *> output_sockets;
  MultiValueMap<const bNodeType *, bNode *> nodes_by_type;
  Vector<bNode *> toposort_left_to_right;
  Vector<bNode *> toposort_right_to_left;
  Vector<bNode *> group_nodes;
  bool has_available_link_cycle = false;
  bool has_undefined_nodes_or_sockets = false;
  bNode *group_output_node = nullptr;
  Vector<bNode *> root_frames;
  Vector<bNodeSocket *> interface_inputs;
  Vector<bNodeSocket *> interface_outputs;
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

  /**
   * Runtime-only cache of the number of input links, for multi-input sockets,
   * including dragged node links that aren't actually in the tree.
   */
  short total_inputs = 0;

  /**
   * The location of the socket in the tree, calculated while drawing the nodes and invalid if the
   * node tree hasn't been drawn yet. In the node tree's "world space" (the same as
   * #bNode::runtime::totr).
   */
  float2 location;

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

  /** Used as a boolean for execution. */
  uint8_t need_exec = 0;

  /** The original node in the tree (for localized tree). */
  bNode *original = nullptr;

  /**
   * XXX TODO
   * Node totr size depends on the prvr size, which in turn is determined from preview size.
   * In earlier versions bNodePreview was stored directly in nodes, but since now there can be
   * multiple instances using different preview images it is possible that required node size
   * varies between instances. preview_xsize, preview_ysize defines a common reserved size for
   * preview rect for now, could be replaced by more accurate node instance drawing,
   * but that requires removing totr from DNA and replacing all uses with per-instance data.
   */
  /** Reserved size of the preview rect. */
  short preview_xsize, preview_ysize = 0;
  /** Entire bound-box (world-space). */
  rctf totr{};

  /** Used at runtime when going through the tree. Initialize before use. */
  short tmp_flag = 0;

  /** Used at runtime when iterating over node branches. */
  char iter_flag = 0;

  /** Update flags. */
  int update = 0;

  /** Initial locx for insert offset animation. */
  float anim_init_locx;
  /** Offset that will be added to locx for insert offset animation. */
  float anim_ofsx;

  /** List of cached internal links (input to output), for muted nodes and operators. */
  Vector<bNodeLink> internal_links;

  /** Eagerly maintained cache of the node's index in the tree. */
  int index_in_tree = -1;

  /** Only valid if #topology_cache_is_dirty is false. */
  Vector<bNodeSocket *> inputs;
  Vector<bNodeSocket *> outputs;
  Map<StringRefNull, bNodeSocket *> inputs_by_identifier;
  Map<StringRefNull, bNodeSocket *> outputs_by_identifier;
  bool has_available_linked_inputs = false;
  bool has_available_linked_outputs = false;
  Vector<bNode *> direct_children_in_frame;
  bNodeTree *owner_tree = nullptr;
  /** Can be used to toposort a subset of nodes. */
  int toposort_left_to_right_index = -1;
  int toposort_right_to_left_index = -1;
};

namespace node_tree_runtime {

/**
 * Is executed when the node tree changed in the depsgraph.
 */
void preprocess_geometry_node_tree_for_evaluation(bNodeTree &tree_cow);

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
  if (tree.runtime->topology_cache_mutex.is_dirty()) {
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

namespace node_field_inferencing {
bool update_field_inferencing(const bNodeTree &tree);
}
}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name #bNodeTree Inline Methods
 * \{ */

inline blender::Span<const bNode *> bNodeTree::all_nodes() const
{
  return this->runtime->nodes_by_id.as_span();
}

inline blender::Span<bNode *> bNodeTree::all_nodes()
{
  return this->runtime->nodes_by_id;
}

inline bNode *bNodeTree::node_by_id(const int32_t identifier)
{
  BLI_assert(identifier >= 0);
  bNode *const *node = this->runtime->nodes_by_id.lookup_key_ptr_as(identifier);
  return node ? *node : nullptr;
}

inline const bNode *bNodeTree::node_by_id(const int32_t identifier) const
{
  BLI_assert(identifier >= 0);
  const bNode *const *node = this->runtime->nodes_by_id.lookup_key_ptr_as(identifier);
  return node ? *node : nullptr;
}

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

inline blender::Span<bNode *> bNodeTree::toposort_left_to_right()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->toposort_left_to_right;
}

inline blender::Span<bNode *> bNodeTree::toposort_right_to_left()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->toposort_right_to_left;
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

inline bool bNodeTree::has_available_link_cycle() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->has_available_link_cycle;
}

inline bool bNodeTree::has_undefined_nodes_or_sockets() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->has_undefined_nodes_or_sockets;
}

inline bNode *bNodeTree::group_output_node()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->group_output_node;
}

inline const bNode *bNodeTree::group_output_node() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->group_output_node;
}

inline blender::Span<const bNode *> bNodeTree::group_input_nodes() const
{
  return this->nodes_by_type("NodeGroupInput");
}

inline blender::Span<const bNodeSocket *> bNodeTree::interface_inputs() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->interface_inputs;
}

inline blender::Span<const bNodeSocket *> bNodeTree::interface_outputs() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->interface_outputs;
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

inline blender::Span<bNode *> bNodeTree::root_frames() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->root_frames;
}

inline blender::Span<bNodeLink *> bNodeTree::all_links()
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->links;
}

inline blender::Span<const bNodeLink *> bNodeTree::all_links() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->links;
}

inline blender::Span<const bNodePanel *> bNodeTree::panels() const
{
  return blender::Span(panels_array, panels_num);
}

inline blender::MutableSpan<bNodePanel *> bNodeTree::panels_for_write()
{
  return blender::MutableSpan(panels_array, panels_num);
}

inline blender::MutableSpan<bNestedNodeRef> bNodeTree::nested_node_refs_span()
{
  return {this->nested_node_refs, this->nested_node_refs_num};
}

inline blender::Span<bNestedNodeRef> bNodeTree::nested_node_refs_span() const
{
  return {this->nested_node_refs, this->nested_node_refs_num};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #bNode Inline Methods
 * \{ */

inline int bNode::index() const
{
  const int index = this->runtime->index_in_tree;
  /* The order of nodes should always be consistent with the `nodes_by_id` vector. */
  BLI_assert(index ==
             this->runtime->owner_tree->runtime->nodes_by_id.index_of_as(this->identifier));
  return index;
}

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

inline bNodeSocket &bNode::input_by_identifier(blender::StringRef identifier)
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->inputs_by_identifier.lookup_as(identifier);
}

inline bNodeSocket &bNode::output_by_identifier(blender::StringRef identifier)
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

inline blender::Span<bNodeLink> bNode::internal_links() const
{
  return this->runtime->internal_links;
}

inline blender::Span<bNode *> bNode::direct_children_in_frame() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  BLI_assert(this->is_frame());
  return this->runtime->direct_children_in_frame;
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

inline bool bNodeLink::is_available() const
{
  return this->fromsock->is_available() && this->tosock->is_available();
}

inline bool bNodeLink::is_used() const
{
  return !this->is_muted() && this->is_available();
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

inline int bNodeSocket::index_in_all_inputs() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  BLI_assert(this->is_input());
  return this->runtime->index_in_inout_sockets;
}

inline int bNodeSocket::index_in_all_outputs() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  BLI_assert(this->is_output());
  return this->runtime->index_in_inout_sockets;
}

inline bool bNodeSocket::is_hidden() const
{
  return (this->flag & SOCK_HIDDEN) != 0;
}

inline bool bNodeSocket::is_available() const
{
  return (this->flag & SOCK_UNAVAIL) == 0;
}

inline bool bNodeSocket::is_visible() const
{
  return !this->is_hidden() && this->is_available();
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

template<typename T> T *bNodeSocket::default_value_typed()
{
  return static_cast<T *>(this->default_value);
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
