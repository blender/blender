/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <memory>

#include "BLI_cache_mutex.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_mutex.hh"
#include "BLI_set.hh"
#include "BLI_struct_equality_utils.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_tree_interface.hh"

#include "NOD_socket_usage_inference_fwd.hh"

struct bNode;
struct bNodeSocket;
struct bNodeTree;

namespace blender::nodes {
struct FieldInferencingInterface;
struct GeometryNodesEvalDependencies;
class NodeDeclaration;
struct GeometryNodesLazyFunctionGraphInfo;
struct StructureTypeInterface;
namespace anonymous_attribute_lifetime {
}
namespace aal = anonymous_attribute_lifetime;
namespace gizmos {
struct TreeGizmoPropagation;
}
}  // namespace blender::nodes
namespace blender::bke {
struct bNodeType;
class bNodeTreeZones;
}  // namespace blender::bke

namespace blender::bke::node_tree_reference_lifetimes {
struct ReferenceLifetimesInfo;
}

namespace blender::bke {

enum class FieldSocketState : int8_t {
  RequiresSingle,
  CanBeField,
  IsField,
};

struct NodeIDGetter {
  int32_t operator()(const bNode *value) const
  {
    return value->identifier;
  }
};
using NodeIDVectorSet = CustomIDVectorSet<bNode *, NodeIDGetter>;

struct NodeLinkError {
  std::string tooltip;
};

/**
 * Utility to weakly reference a link. Weak references are safer because they avoid dangling
 * references which can easily happen temporarily when editing the node tree.
 */
struct NodeLinkKey {
 private:
  int to_node_id_;
  int input_socket_index_;
  int input_link_index_;

 public:
  /** Assumes that the topology cache is up to date. */
  explicit NodeLinkKey(const bNodeLink &link);

  bNodeLink *try_find(bNodeTree &ntree) const;
  const bNodeLink *try_find(const bNodeTree &ntree) const;

  uint64_t hash() const
  {
    return get_default_hash(this->to_node_id_, this->input_socket_index_, this->input_link_index_);
  }

  BLI_STRUCT_EQUALITY_OPERATORS_3(NodeLinkKey,
                                  to_node_id_,
                                  input_socket_index_,
                                  input_link_index_);
};

struct LoggedZoneGraphs {
  Mutex mutex;
  /**
   * Technically there can be more than one graph per zone because the zone can be invoked in
   * different contexts. However, for the purpose of logging here, we only need one at a time
   * anyway.
   */
  Map<int, std::string> graph_by_zone_id;
};

/**
 * Runtime data for #bNodeTree from the perspective of execution instructions (rather than runtime
 * data from evaluation of the node tree). Evaluation data is not the responsibility of the node
 * tree and should be stored elsewhere. Evaluating a node tree should be possible without changing
 * it.
 */
class bNodeTreeRuntime : NonCopyable, NonMovable {
 public:
  /**
   * Keeps track of what changed in the node tree until the next update.
   * Should not be changed directly, instead use the functions in `BKE_node_tree_update.hh`.
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
   * Contains a number increased for each node-tree update.
   * Store a state variable in the #NestedTreePreviews structure to compare if they differ.
   */
  uint32_t previews_refresh_state = 0;

  /** Allows logging zone graphs purely for debugging purposes. */
  std::unique_ptr<LoggedZoneGraphs> logged_zone_graphs;

  /**
   * Storage of nodes based on their identifier. Also used as a contiguous array of nodes to
   * allow simpler and more cache friendly iteration. Supports lookup by integer or by node.
   * Unlike other caches, this is maintained eagerly while changing the tree.
   */
  NodeIDVectorSet nodes_by_id;

  /**
   * Legacy execution data.
   *
   * \todo Move this out of the node tree to improve semantic/physical separation between the node
   * tree execution instructions and its evaluation.
   */
  bNodeTreeExec *execdata = nullptr;
  void (*progress)(void *, float progress) = nullptr;
  /** \warning may be called by different threads */
  void (*stats_draw)(void *, const char *str) = nullptr;
  bool (*test_break)(void *) = nullptr;
  void (*update_draw)(void *) = nullptr;
  void *tbh = nullptr, *prh = nullptr, *sdh = nullptr, *udh = nullptr;

  /* End legacy execution data. */

  /** Information about how inputs and outputs of the node group interact with fields. */
  std::unique_ptr<nodes::FieldInferencingInterface> field_inferencing_interface;
  /** Field status for every socket, accessed with #bNodeSocket::index_in_tree(). */
  Array<FieldSocketState> field_states;
  /** Information about usage of anonymous attributes within the group. */
  std::unique_ptr<node_tree_reference_lifetimes::ReferenceLifetimesInfo> reference_lifetimes_info;
  std::unique_ptr<nodes::gizmos::TreeGizmoPropagation> gizmo_propagation;
  std::unique_ptr<nodes::StructureTypeInterface> structure_type_interface;

  /**
   * Indexed by #bNodeSocket::index_in_tree(). Contains information about whether the socket is
   * used or visible.
   */
  blender::Array<nodes::socket_usage_inference::SocketUsage> inferenced_socket_usage;
  CacheMutex inferenced_input_socket_usage_mutex;

  /**
   * For geometry nodes, a lazy function graph with some additional info is cached. This is used to
   * evaluate the node group. Caching it here allows us to reuse the preprocessed node tree in case
   * its used multiple times.
   */
  CacheMutex geometry_nodes_lazy_function_graph_info_mutex;
  std::unique_ptr<nodes::GeometryNodesLazyFunctionGraphInfo>
      geometry_nodes_lazy_function_graph_info;

  /**
   * Stores information about invalid links. This information is then displayed to the user. This
   * is updated in #update_link_validation and is valid during drawing code.
   */
  MultiValueMap<NodeLinkKey, NodeLinkError> link_errors;

  /**
   * Error messages for shading nodes. Those don't have more contextual information yet. Maps
   * #bNode::identifier to error messages.
   */
  Map<int32_t, VectorSet<std::string>> shader_node_errors;
  Mutex shader_node_errors_mutex;

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
  std::shared_ptr<bNodeTreeZones> tree_zones;

  /**
   * Same as #tree_zones, but may not be valid anymore. This is used for drawing errors when the
   * zone detection failed.
   */
  std::shared_ptr<bNodeTreeZones> last_valid_zones;
  Set<int> invalid_zone_output_node_ids;

  /**
   * The stored sockets are drawn using a special link to indicate that there is a gizmo. This is
   * only valid during node editor drawing.
   */
  Set<const bNodeSocket *> sockets_on_active_gizmo_paths;

  /**
   * Cache of dependencies used by the node tree itself. Does not account for data that's passed
   * into the node tree from the outside.
   * NOTE: The node tree may reference additional data-blocks besides the ones included here. But
   * those are not used when the node tree is evaluated by Geometry Nodes.
   */
  std::unique_ptr<nodes::GeometryNodesEvalDependencies> geometry_nodes_eval_dependencies;

  /**
   * Node previews for the compositor.
   * Only available in base node trees (e.g. scene->compositing_node_group).
   */
  Map<bNodeInstanceKey, bNodePreview> previews;

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
  const nodes::SocketDeclaration *declaration = nullptr;

  /** #eNodeTreeChangedFlag. */
  uint32_t changed_flag = 0;

  /**
   * Runtime-only cache of the number of input links, for multi-input sockets,
   * including dragged node links that aren't actually in the tree.
   */
  short total_inputs = 0;

  /**
   * Inferred structure type of the socket. This is not necessarily the same as the structure type
   * that is displayed in the UI. For example, it would be #StructureType::Single for an unlinked
   * input of the Math node, but the socket is displayed as #StructureType::Dynamic.
   *
   * This is stored on the socket instead of as array in #bNodeTreeRuntime because the data needs
   * to stay attached to the socket even when the node tree changes. This is used when e.g. syncing
   * a newly created Separate Bundle node to an existing Combine Bundle node.
   */
  nodes::StructureType inferred_structure_type = nodes::StructureType::Dynamic;

  /**
   * The location of the socket in the tree, calculated while drawing the nodes and invalid if the
   * node tree hasn't been drawn yet. In the node tree's "world space" (the same as
   * #bNode::runtime::draw_bounds).
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

struct bNodePanelExtent {
  float min_y;
  float max_y;
  bool fill_node_end = false;
};

class bNodePanelRuntime : NonCopyable, NonMovable {
 public:
  /**
   * The vertical location of the panel in the tree, calculated while drawing the nodes and invalid
   * if the node tree hasn't been drawn yet. In the node tree's "world space" (the same as
   * #bNode::runtime::draw_bounds).
   */
  std::optional<float> header_center_y;
  std::optional<bNodePanelExtent> content_extent;
  /** Optional socket that is part of the panel header. */
  bNodeSocket *input_socket = nullptr;
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
   * call #node_declaration_ensure before using it.
   *
   * Currently, the declaration is the same for every node of the same type. Going forward, that is
   * intended to change though. Especially when nodes become more dynamic with respect to how many
   * sockets they have.
   */
  nodes::NodeDeclaration *declaration = nullptr;

  /** #eNodeTreeChangedFlag. */
  uint32_t changed_flag = 0;

  /** Used as a boolean for execution. */
  uint8_t need_exec = 0;

  /** The original node in the tree (for localized tree). */
  bNode *original = nullptr;

  /** Calculated bounding box of node in the view space of the node editor (including UI scale). */
  rctf draw_bounds{};

  /** Used at runtime when going through the tree. Initialize before use. */
  short tmp_flag = 0;

  /** Used at runtime when iterating over node branches. */
  char iter_flag = 0;

  /** Update flags. */
  int update = 0;

  /** Offset that will be added to #bNode::locx for insert offset animation. */
  float anim_ofsx;

  /** List of cached internal links (input to output), for muted nodes and operators. */
  Vector<bNodeLink> internal_links;

  /** Eagerly maintained cache of the node's index in the tree. */
  int index_in_tree = -1;

  /** Used to avoid running forward compatibility code more often than necessary. */
  bool forward_compatible_versioning_done = false;

  /**
   * If this node is reroute and this reroute is not logically linked with any source except other
   * reroute, this will be true.
   */
  bool is_dangling_reroute = false;

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

  /** Panel runtime state. */
  Array<bNodePanelRuntime> panels;
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

/* If result is not true then this means that the last node tree editing operation was not covered
 * by the topology cache update ensure call. All derivative information about topology is not
 * available. You should call "tree.ensure_topology_cache();" first.
 */
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

namespace node_structure_type_inferencing {
bool update_structure_type_interface(bNodeTree &tree);
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
  return this->runtime->nodes_by_type.lookup(blender::bke::node_type_find(type_idname.c_str()));
}

inline blender::Span<const bNode *> bNodeTree::nodes_by_type(
    const blender::StringRefNull type_idname) const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->nodes_by_type.lookup(blender::bke::node_type_find(type_idname.c_str()));
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

inline blender::Span<bNode *> bNodeTree::group_input_nodes()
{
  return this->nodes_by_type("NodeGroupInput");
}

inline blender::Span<const bNode *> bNodeTree::group_input_nodes() const
{
  return this->nodes_by_type("NodeGroupInput");
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

inline blender::MutableSpan<bNestedNodeRef> bNodeTree::nested_node_refs_span()
{
  return {this->nested_node_refs, this->nested_node_refs_num};
}

inline blender::Span<bNestedNodeRef> bNodeTree::nested_node_refs_span() const
{
  return {this->nested_node_refs, this->nested_node_refs_num};
}

inline void bNodeTree::ensure_interface_cache() const
{
  this->tree_interface.ensure_items_cache();
}

inline blender::Span<bNodeTreeInterfaceSocket *> bNodeTree::interface_inputs()
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->inputs_;
}

inline blender::Span<const bNodeTreeInterfaceSocket *> bNodeTree::interface_inputs() const
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->inputs_.as_span();
}

inline blender::Span<bNodeTreeInterfaceSocket *> bNodeTree::interface_outputs()
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->outputs_;
}

inline blender::Span<const bNodeTreeInterfaceSocket *> bNodeTree::interface_outputs() const
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->outputs_.as_span();
}

inline blender::Span<bNodeTreeInterfaceItem *> bNodeTree::interface_items()
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->items_;
}

inline blender::Span<const bNodeTreeInterfaceItem *> bNodeTree::interface_items() const
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->items_.as_span();
}

inline int bNodeTree::interface_input_index(const bNodeTreeInterfaceSocket &io_socket) const
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->inputs_.index_of_as(&io_socket);
}

inline int bNodeTree::interface_output_index(const bNodeTreeInterfaceSocket &io_socket) const
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->outputs_.index_of_as(&io_socket);
}

inline int bNodeTree::interface_item_index(const bNodeTreeInterfaceItem &io_item) const
{
  BLI_assert(this->tree_interface.items_cache_is_available());
  return this->tree_interface.runtime->items_.index_of_as(&io_item);
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

inline blender::IndexRange bNode::input_socket_indices_in_tree() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  const int num_inputs = this->runtime->inputs.size();
  if (num_inputs == 0) {
    return {};
  }
  return blender::IndexRange::from_begin_size(this->input_socket(0).index_in_tree(), num_inputs);
}

inline blender::IndexRange bNode::output_socket_indices_in_tree() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  const int num_outputs = this->runtime->outputs.size();
  if (num_outputs == 0) {
    return {};
  }
  return blender::IndexRange::from_begin_size(this->output_socket(0).index_in_tree(), num_outputs);
}

inline blender::IndexRange bNode::input_socket_indices_in_all_inputs() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  const int num_inputs = this->runtime->inputs.size();
  if (num_inputs == 0) {
    return {};
  }
  return blender::IndexRange::from_begin_size(this->input_socket(0).index_in_all_inputs(),
                                              num_inputs);
}

inline blender::IndexRange bNode::output_socket_indices_in_all_outputs() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  const int num_outputs = this->runtime->outputs.size();
  if (num_outputs == 0) {
    return {};
  }
  return blender::IndexRange::from_begin_size(this->output_socket(0).index_in_all_outputs(),
                                              num_outputs);
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

inline const bNodeSocket *bNode::input_by_identifier(blender::StringRef identifier) const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->inputs_by_identifier.lookup_default_as(identifier, nullptr);
}

inline const bNodeSocket *bNode::output_by_identifier(blender::StringRef identifier) const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->outputs_by_identifier.lookup_default_as(identifier, nullptr);
}

inline bNodeSocket *bNode::input_by_identifier(blender::StringRef identifier)
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->inputs_by_identifier.lookup_default_as(identifier, nullptr);
}

inline bNodeSocket *bNode::output_by_identifier(blender::StringRef identifier)
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->outputs_by_identifier.lookup_default_as(identifier, nullptr);
}

inline const bNodeTree &bNode::owner_tree() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return *this->runtime->owner_tree;
}

inline bNodeTree &bNode::owner_tree()
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
  return this->type_legacy == NODE_REROUTE;
}

inline bool bNode::is_frame() const
{
  return this->type_legacy == NODE_FRAME;
}

inline bool bNode::is_group() const
{
  return ELEM(this->type_legacy, NODE_GROUP, NODE_CUSTOM_GROUP);
}

inline bool bNode::is_custom_group() const
{
  return this->type_legacy == NODE_CUSTOM_GROUP;
}

inline bool bNode::is_group_input() const
{
  return this->type_legacy == NODE_GROUP_INPUT;
}

inline bool bNode::is_group_output() const
{
  return this->type_legacy == NODE_GROUP_OUTPUT;
}

inline bool bNode::is_undefined() const
{
  return this->typeinfo == &blender::bke::NodeTypeUndefined;
}

inline bool bNode::is_type(const blender::StringRef query_idname) const
{
  return this->typeinfo->is_type(query_idname);
}

inline blender::Span<bNodeLink> bNode::internal_links() const
{
  return this->runtime->internal_links;
}

inline bool bNode::is_dangling_reroute() const
{
  BLI_assert(blender::bke::node_tree_runtime::topology_cache_is_available(*this));
  return this->runtime->is_dangling_reroute;
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

inline blender::Span<bNodePanelState> bNode::panel_states() const
{
  return {panel_states_array, num_panel_states};
}

inline blender::MutableSpan<bNodePanelState> bNode::panel_states()
{
  return {panel_states_array, num_panel_states};
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

inline bool bNodeSocket::is_user_hidden() const
{
  return (this->flag & SOCK_HIDDEN) != 0;
}

inline bool bNodeSocket::is_inactive() const
{
  /* Gray out inputs that do not affect the output of the node currently.
   * Don't gray out any inputs if the node has no outputs (in which case no input can affect the
   * output). Otherwise, viewer node inputs would be inactive. */
  return this->is_input() && !this->affects_node_output() &&
         !this->owner_node().output_sockets().is_empty();
}

inline bool bNodeSocket::is_available() const
{
  return (this->flag & SOCK_UNAVAIL) == 0;
}

inline bool bNodeSocket::is_panel_collapsed() const
{
  return (this->flag & SOCK_PANEL_COLLAPSED) != 0;
}

inline bool bNodeSocket::is_visible() const
{
  return !this->is_user_hidden() && this->is_available() && this->inferred_socket_visibility();
}

inline bool bNodeSocket::is_icon_visible() const
{
  return this->is_visible() &&
         (this->owner_node().flag & NODE_COLLAPSED || !this->is_panel_collapsed());
}

inline bool bNodeSocket::may_be_field() const
{
  return ELEM(this->runtime->inferred_structure_type,
              blender::nodes::StructureType::Field,
              blender::nodes::StructureType::Dynamic);
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

inline bNodeTree &bNodeSocket::owner_tree()
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

/* -------------------------------------------------------------------- */
/** \name #bNode Inline Methods
 * \{ */

inline bool bNodePanelState::is_collapsed() const
{
  return flag & NODE_PANEL_COLLAPSED;
}

inline bool bNodePanelState::is_parent_collapsed() const
{
  return flag & NODE_PANEL_PARENT_COLLAPSED;
}

inline bool bNodePanelState::has_visible_content() const
{
  return flag & NODE_PANEL_CONTENT_VISIBLE;
}

/** \} */
