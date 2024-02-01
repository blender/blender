/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * For evaluation, geometry node groups are converted to a lazy-function graph. The generated graph
 * is cached per node group, so it only has to be generated once after a change.
 *
 * Node groups are *not* inlined into the lazy-function graph. This could be added in the future as
 * it might improve performance in some cases, but generally does not seem necessary. Inlining node
 * groups also has disadvantages like making per-node-group caches less useful, resulting in more
 * overhead.
 *
 * Instead, group nodes are just like all other nodes in the lazy-function graph. What makes them
 * special is that they reference the lazy-function graph of the group they reference.
 *
 * During lazy-function graph generation, a mapping between the #bNodeTree and
 * #lazy_function::Graph is build that can be used when evaluating the graph (e.g. for logging).
 */

#include <variant>

#include "FN_lazy_function_graph.hh"
#include "FN_lazy_function_graph_executor.hh"

#include "NOD_geometry_nodes_log.hh"
#include "NOD_multi_function.hh"

#include "BLI_compute_context.hh"

#include "BKE_bake_items.hh"
#include "BKE_node_tree_zones.hh"

struct Object;
struct Depsgraph;
struct Scene;

namespace blender::nodes {

using lf::LazyFunction;
using mf::MultiFunction;

/** The structs in here describe the different possible behaviors of a simulation input node. */
namespace sim_input {

/**
 * The data is just passed through the node. Data that is incompatible with simulations (like
 * anonymous attributes), is removed though.
 */
struct PassThrough {};

/**
 * The input is not evaluated, instead the values provided here are output by the node.
 */
struct OutputCopy {
  float delta_time;
  bke::bake::BakeStateRef state;
};

/**
 * Same as above, but the values can be output by move, instead of copy. This can reduce the amount
 * of unnecessary copies, when the old simulation state is not needed anymore.
 */
struct OutputMove {
  float delta_time;
  bke::bake::BakeState state;
};

using Behavior = std::variant<PassThrough, OutputCopy, OutputMove>;

}  // namespace sim_input

/** The structs in here describe the different possible behaviors of a simulation output node. */
namespace sim_output {

/**
 * Output the data that comes from the corresponding simulation input node, ignoring the nodes in
 * the zone.
 */
struct PassThrough {};

/**
 * Computes the simulation step and calls the given function to cache the new simulation state.
 * The new simulation state is the output of the node.
 */
struct StoreNewState {
  std::function<void(bke::bake::BakeState state)> store_fn;
};

/**
 * The inputs are not evaluated, instead the given cached items are output directly.
 */
struct ReadSingle {
  bke::bake::BakeStateRef state;
};

/**
 * The inputs are not evaluated, instead of a mix of the two given states is output.
 */
struct ReadInterpolated {
  /** Factor between 0 and 1 that determines the influence of the two simulation states. */
  float mix_factor;
  bke::bake::BakeStateRef prev_state;
  bke::bake::BakeStateRef next_state;
};

/**
 * Used when there was some issue loading the baked data from disk.
 */
struct ReadError {
  std::string message;
};

using Behavior = std::variant<PassThrough, StoreNewState, ReadSingle, ReadInterpolated, ReadError>;

}  // namespace sim_output

/** Controls the behavior of one simulation zone. */
struct SimulationZoneBehavior {
  sim_input::Behavior input;
  sim_output::Behavior output;
  bke::bake::BakeDataBlockMap *data_block_map = nullptr;
};

class GeoNodesSimulationParams {
 public:
  /**
   * Get the expected behavior for the simulation zone with the given id (see #bNestedNodeRef).
   * It's possible that this method called multiple times for the same id. In this case, the same
   * pointer should be returned in each call.
   */
  virtual SimulationZoneBehavior *get(const int zone_id) const = 0;
};

struct BakeNodeBehavior {
  /** The set of possible behaviors are the same for both of these nodes currently. */
  sim_output::Behavior behavior;
  bke::bake::BakeDataBlockMap *data_block_map = nullptr;
};

class GeoNodesBakeParams {
 public:
  virtual BakeNodeBehavior *get(const int id) const = 0;
};

struct GeoNodesSideEffectNodes {
  MultiValueMap<ComputeContextHash, const lf::FunctionNode *> nodes_by_context;
  /**
   * The repeat zone is identified by the compute context of the parent and the identifier of the
   * repeat output node.
   */
  MultiValueMap<std::pair<ComputeContextHash, int32_t>, int> iterations_by_repeat_zone;
};

/**
 * Data that is passed into geometry nodes evaluation from the modifier.
 */
struct GeoNodesModifierData {
  /** Object that is currently evaluated. */
  const Object *self_object = nullptr;
  /** Depsgraph that is evaluating the modifier. */
  Depsgraph *depsgraph = nullptr;
};

struct GeoNodesOperatorData {
  eObjectMode mode;
  /** The object currently effected by the operator. */
  const Object *self_object = nullptr;
  /** Current evaluated depsgraph. */
  Depsgraph *depsgraph = nullptr;
  Scene *scene = nullptr;
};

struct GeoNodesCallData {
  /**
   * Top-level node tree of the current evaluation.
   */
  const bNodeTree *root_ntree = nullptr;
  /**
   * Optional logger that keeps track of data generated during evaluation to allow for better
   * debugging afterwards.
   */
  geo_eval_log::GeoModifierLog *eval_log = nullptr;
  /**
   * Optional injected behavior for simulations.
   */
  GeoNodesSimulationParams *simulation_params = nullptr;
  /**
   * Optional injected behavior for bake nodes.
   */
  GeoNodesBakeParams *bake_params = nullptr;
  /**
   * Some nodes should be executed even when their output is not used (e.g. active viewer nodes and
   * the node groups they are contained in).
   */
  const GeoNodesSideEffectNodes *side_effect_nodes = nullptr;
  /**
   * Controls in which compute contexts we want to log socket values. Logging them in all contexts
   * can result in slowdowns. In the majority of cases, the logged socket values are freed without
   * being looked at anyway.
   *
   * If this is null, all socket values will be logged.
   */
  const Set<ComputeContextHash> *socket_log_contexts = nullptr;

  /**
   * Data from the modifier that is being evaluated.
   */
  GeoNodesModifierData *modifier_data = nullptr;
  /**
   * Data from execution as operator in 3D viewport.
   */
  GeoNodesOperatorData *operator_data = nullptr;

  /**
   * Self object has slightly different semantics depending on how geometry nodes is called.
   * Therefor, it is not stored directly in the global data.
   */
  const Object *self_object() const;
};

/**
 * Custom user data that is passed to every geometry nodes related lazy-function evaluation.
 */
struct GeoNodesLFUserData : public lf::UserData {
  /**
   * Data provided by the root caller of geometry nodes.
   */
  const GeoNodesCallData *call_data = nullptr;
  /**
   * Current compute context. This is different depending in the (nested) node group that is being
   * evaluated.
   */
  const ComputeContext *compute_context = nullptr;
  /**
   * Log socket values in the current compute context. Child contexts might use logging again.
   */
  bool log_socket_values = true;

  destruct_ptr<lf::LocalUserData> get_local(LinearAllocator<> &allocator) override;
};

struct GeoNodesLFLocalUserData : public lf::LocalUserData {
 private:
  /**
   * Thread-local logger for the current node tree in the current compute context. It is only
   * instantiated when it is actually used and then cached for the current thread.
   */
  mutable std::optional<geo_eval_log::GeoTreeLogger *> tree_logger_;

 public:
  GeoNodesLFLocalUserData(GeoNodesLFUserData & /*user_data*/) {}

  /**
   * Get the current tree logger. This method is not thread-safe, each thread is supposed to have
   * a separate logger.
   */
  geo_eval_log::GeoTreeLogger *try_get_tree_logger(const GeoNodesLFUserData &user_data) const
  {
    if (!tree_logger_.has_value()) {
      this->ensure_tree_logger(user_data);
    }
    return *tree_logger_;
  }

 private:
  void ensure_tree_logger(const GeoNodesLFUserData &user_data) const;
};

/**
 * In the general case, this is #DynamicSocket. That means that to determine if a node group will
 * use a particular input, it has to be partially executed.
 *
 * In other cases, it's not necessary to look into the node group to determine if an input is
 * necessary.
 */
enum class InputUsageHintType {
  /** The input socket is never used. */
  Never,
  /** The input socket is used when a subset of the outputs is used. */
  DependsOnOutput,
  /** Can't determine statically if the input is used, check the corresponding output socket. */
  DynamicSocket,
};

struct InputUsageHint {
  InputUsageHintType type = InputUsageHintType::DependsOnOutput;
  /** Used in depends-on-output mode. */
  Vector<int> output_dependencies;
};

/**
 * Contains the mapping between the #bNodeTree and the corresponding lazy-function graph.
 * This is *not* a one-to-one mapping.
 */
struct GeometryNodeLazyFunctionGraphMapping {
  /**
   * This is an optimization to avoid partially evaluating a node group just to figure out which
   * inputs are needed.
   */
  Vector<InputUsageHint> group_input_usage_hints;
  /**
   * A mapping used for logging intermediate values.
   */
  MultiValueMap<const lf::Socket *, const bNodeSocket *> bsockets_by_lf_socket_map;
  /**
   * Mappings for some special node types. Generally, this mapping does not exist for all node
   * types, so better have more specialized mappings for now.
   */
  Map<const bNode *, const lf::FunctionNode *> group_node_map;
  Map<const bNode *, const lf::FunctionNode *> possible_side_effect_node_map;
  Map<const bke::bNodeTreeZone *, const lf::FunctionNode *> zone_node_map;

  /* Indexed by #bNodeSocket::index_in_all_outputs. */
  Array<int> lf_input_index_for_output_bsocket_usage;
  /* Indexed by #bNodeSocket::index_in_all_outputs. */
  Array<int> lf_input_index_for_attribute_propagation_to_output;
  /* Indexed by #bNodeSocket::index_in_tree. */
  Array<int> lf_index_by_bsocket;
};

/**
 * Contains the information that is necessary to execute a geometry node tree.
 */
struct GeometryNodesGroupFunction {
  /**
   * The lazy-function that does what the node group does. Its inputs and outputs are described
   * below.
   */
  const LazyFunction *function = nullptr;

  struct {
    /**
     * Main input values that come out of the Group Input node.
     */
    IndexRange main;
    /**
     * A boolean for every group output that indicates whether that output is needed. It's ok if
     * those are set to true even when an output is not used, but the other way around will lead to
     * bugs. The node group uses those values to compute the lifetimes of anonymous attributes.
     */
    IndexRange output_usages;
    /**
     * Some node groups can propagate attributes from a geometry input to a geometry output. In
     * those cases, the caller of the node group has to decide which anonymous attributes have to
     * be kept alive on the geometry because the caller requires them.
     */
    struct {
      IndexRange range;
      Vector<int> geometry_outputs;
    } attributes_to_propagate;
  } inputs;

  struct {
    /**
     * Main output values that are passed into the Group Output node.
     */
    IndexRange main;
    /**
     * A boolean for every group input that indicates whether this input will be used. Oftentimes
     * this can be determined without actually computing much. This is used to compute anonymous
     * attribute lifetimes.
     */
    IndexRange input_usages;
  } outputs;
};

/**
 * Data that is cached for every #bNodeTree.
 */
struct GeometryNodesLazyFunctionGraphInfo {
  /**
   * Contains resources that need to be freed when the graph is not needed anymore.
   */
  ResourceScope scope;
  GeometryNodesGroupFunction function;
  /**
   * The actual lazy-function graph.
   */
  lf::Graph graph;
  /**
   * Mappings between the lazy-function graph and the #bNodeTree.
   */
  GeometryNodeLazyFunctionGraphMapping mapping;
  /**
   * Approximate number of nodes in the graph if all sub-graphs were inlined.
   * This can be used as a simple heuristic for the complexity of the node group.
   */
  int num_inline_nodes_approximate = 0;
};

std::unique_ptr<LazyFunction> get_simulation_output_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info);
std::unique_ptr<LazyFunction> get_simulation_input_lazy_function(
    const bNodeTree &node_tree,
    const bNode &node,
    GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info);
std::unique_ptr<LazyFunction> get_switch_node_lazy_function(const bNode &node);
std::unique_ptr<LazyFunction> get_index_switch_node_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &lf_graph_info);
std::unique_ptr<LazyFunction> get_bake_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info);
std::unique_ptr<LazyFunction> get_menu_switch_node_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &lf_graph_info);
std::unique_ptr<LazyFunction> get_menu_switch_node_socket_usage_lazy_function(const bNode &node);

/**
 * Outputs the default value of each output socket that has not been output yet. This needs the
 * #bNode because otherwise the default values for the outputs are not known. The lazy-function
 * parameters do not differentiate between e.g. float and vector sockets. The #SocketValueVariant
 * type is used for both.
 */
void set_default_remaining_node_outputs(lf::Params &params, const bNode &node);

struct FoundNestedNodeID {
  int id;
  bool is_in_simulation = false;
  bool is_in_loop = false;
};

std::optional<FoundNestedNodeID> find_nested_node_id(const GeoNodesLFUserData &user_data,
                                                     const int node_id);

/**
 * An anonymous attribute created by a node.
 */
class NodeAnonymousAttributeID : public bke::AnonymousAttributeID {
  std::string long_name_;
  std::string socket_name_;

 public:
  NodeAnonymousAttributeID(const Object &object,
                           const ComputeContext &compute_context,
                           const bNode &bnode,
                           const StringRef identifier,
                           const StringRef name);

  std::string user_name() const override;
};

/**
 * Main function that converts a #bNodeTree into a lazy-function graph. If the graph has been
 * generated already, nothing is done. Under some circumstances a valid graph cannot be created. In
 * those cases null is returned.
 */
const GeometryNodesLazyFunctionGraphInfo *ensure_geometry_nodes_lazy_function_graph(
    const bNodeTree &btree);

}  // namespace blender::nodes
