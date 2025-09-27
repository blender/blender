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
#include "NOD_nested_node_id.hh"

#include "BLI_compute_context.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_multi_value_map.hh"

#include "BKE_bake_items.hh"
#include "BKE_node_tree_zones.hh"

struct Object;
struct Depsgraph;
struct Scene;

namespace blender::nodes {

using lf::LazyFunction;
using mf::MultiFunction;
using ReferenceSetIndex = int;

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
 * Same as #OutputCopy, but the values can be output by move, instead of copy.
 * This can reduce the amount of unnecessary copies,
 * when the old simulation state is not needed anymore.
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
   * The repeat/foreach zone is identified by the compute context of the parent and the identifier
   * of the repeat output node.
   */
  MultiValueMap<std::pair<ComputeContextHash, int32_t>, int> iterations_by_iteration_zone;
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

struct GeoNodesOperatorDepsgraphs {
  /** Current evaluated depsgraph from the viewport. Shouldn't be null. */
  const Depsgraph *active = nullptr;
  /**
   * Depsgraph containing IDs referenced by the node tree and the node tree itself and from node
   * group inputs (the redo panel).
   */
  Depsgraph *extra = nullptr;

  ~GeoNodesOperatorDepsgraphs();

  /**
   * The evaluated data-block might be in the scene's active depsgraph, in that case we should use
   * it directly. Otherwise retrieve it from the extra depsgraph that was built for all other
   * data-blocks. Return null if it isn't found, generally geometry nodes can handle null ID
   * pointers.
   */
  const ID *get_evaluated_id(const ID &id_orig) const;
};

struct GeoNodesOperatorData {
  eObjectMode mode;
  /** The object currently effected by the operator. */
  const Object *self_object_orig = nullptr;
  const GeoNodesOperatorDepsgraphs *depsgraphs = nullptr;
  Scene *scene_orig = nullptr;
  int2 mouse_position;
  int2 region_size;

  float3 cursor_position;
  math::Quaternion cursor_rotation;

  float4x4 viewport_winmat;
  float4x4 viewport_viewmat;
  bool viewport_is_perspective;

  int active_point_index = -1;
  int active_edge_index = -1;
  int active_face_index = -1;
  int active_layer_index = -1;
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
  geo_eval_log::GeoNodesLog *eval_log = nullptr;
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
 * Custom user data that can be passed to every geometry nodes related evaluation.
 */
struct GeoNodesUserData : public fn::UserData {
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

  destruct_ptr<fn::LocalUserData> get_local(LinearAllocator<> &allocator) override;
};

struct GeoNodesLocalUserData : public fn::LocalUserData {
 private:
  /**
   * Thread-local logger for the current node tree in the current compute context. It is only
   * instantiated when it is actually used and then cached for the current thread.
   */
  mutable std::optional<geo_eval_log::GeoTreeLogger *> tree_logger_;

 public:
  GeoNodesLocalUserData(GeoNodesUserData & /*user_data*/) {}

  /**
   * Get the current tree logger. This method is not thread-safe, each thread is supposed to have
   * a separate logger.
   */
  geo_eval_log::GeoTreeLogger *try_get_tree_logger(const GeoNodesUserData &user_data) const
  {
    if (!tree_logger_.has_value()) {
      this->ensure_tree_logger(user_data);
    }
    return *tree_logger_;
  }

 private:
  void ensure_tree_logger(const GeoNodesUserData &user_data) const;
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
  Array<int> lf_input_index_for_reference_set_for_output;
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
    } references_to_propagate;
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
  Map<int, const lf::Graph *> debug_zone_body_graphs;
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
std::unique_ptr<LazyFunction> get_warning_node_lazy_function(const bNode &node);
std::unique_ptr<LazyFunction> get_enable_output_node_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info);

/**
 * Outputs the default value of each output socket that has not been output yet. This needs the
 * #bNode because otherwise the default values for the outputs are not known. The lazy-function
 * parameters do not differentiate between e.g. float and vector sockets. The #SocketValueVariant
 * type is used for both.
 */
void set_default_remaining_node_outputs(lf::Params &params, const bNode &node);
void set_default_value_for_output_socket(lf::Params &params,
                                         const int lf_index,
                                         const bNodeSocket &bsocket);
void construct_socket_default_value(const bke::bNodeSocketType &stype, void *r_value);

std::string make_anonymous_attribute_socket_inspection_string(const bNodeSocket &socket);
std::string make_anonymous_attribute_socket_inspection_string(StringRef node_name,
                                                              StringRef socket_name);

std::optional<FoundNestedNodeID> find_nested_node_id(const GeoNodesUserData &user_data,
                                                     const int node_id);

/**
 * Main function that converts a #bNodeTree into a lazy-function graph. If the graph has been
 * generated already, nothing is done. Under some circumstances a valid graph cannot be created. In
 * those cases null is returned.
 */
const GeometryNodesLazyFunctionGraphInfo *ensure_geometry_nodes_lazy_function_graph(
    const bNodeTree &btree);

/**
 * Utility to measure the time that is spend in a specific compute context during geometry nodes
 * evaluation.
 */
class ScopedComputeContextTimer {
 private:
  lf::Context &context_;
  geo_eval_log::TimePoint start_;

 public:
  ScopedComputeContextTimer(lf::Context &entered_context) : context_(entered_context)
  {
    start_ = geo_eval_log::Clock::now();
  }

  ~ScopedComputeContextTimer()
  {
    const geo_eval_log::TimePoint end = geo_eval_log::Clock::now();
    auto &user_data = static_cast<GeoNodesUserData &>(*context_.user_data);
    auto &local_user_data = static_cast<GeoNodesLocalUserData &>(*context_.local_user_data);
    if (geo_eval_log::GeoTreeLogger *tree_logger = local_user_data.try_get_tree_logger(user_data))
    {
      tree_logger->execution_time += (end - start_);
    }
  }
};

/**
 * Utility to measure the time that is spend in a specific node during geometry nodes evaluation.
 */
class ScopedNodeTimer {
 private:
  const lf::Context &context_;
  const bNode &node_;
  geo_eval_log::TimePoint start_;

 public:
  ScopedNodeTimer(const lf::Context &context, const bNode &node) : context_(context), node_(node)
  {
    start_ = geo_eval_log::Clock::now();
  }

  ~ScopedNodeTimer()
  {
    const geo_eval_log::TimePoint end = geo_eval_log::Clock::now();
    auto &user_data = static_cast<GeoNodesUserData &>(*context_.user_data);
    auto &local_user_data = static_cast<GeoNodesLocalUserData &>(*context_.local_user_data);
    if (geo_eval_log::GeoTreeLogger *tree_logger = local_user_data.try_get_tree_logger(user_data))
    {
      tree_logger->node_execution_times.append(*tree_logger->allocator,
                                               {node_.identifier, start_, end});
    }
  }
};

bool should_log_socket_values_for_context(const GeoNodesUserData &user_data,
                                          const ComputeContextHash hash);

/**
 * Computes the logical or of the inputs and supports short-circuit evaluation (i.e. if the first
 * input is true already, the other inputs are not checked).
 */
class LazyFunctionForLogicalOr : public lf::LazyFunction {
 public:
  LazyFunctionForLogicalOr(const int inputs_num);

  void execute_impl(lf::Params &params, const lf::Context &context) const override;
};

struct ZoneFunctionIndices {
  struct {
    Vector<int> main;
    Vector<int> border_links;
    Vector<int> output_usages;
    Map<ReferenceSetIndex, int> reference_sets;
  } inputs;
  struct {
    Vector<int> main;
    Vector<int> border_link_usages;
    Vector<int> input_usages;
  } outputs;
};

struct ZoneBuildInfo {
  /** The lazy function that contains the zone. */
  const LazyFunction *lazy_function = nullptr;

  /** Information about what the various inputs and outputs of the lazy-function are. */
  ZoneFunctionIndices indices;
};

/**
 * Contains the lazy-function for the "body" of a zone. It contains all the nodes inside of the
 * zone. The "body" function is wrapped by another lazy-function which represents the zone as a
 * hole. The wrapper function might invoke the zone body multiple times (like for repeat zones).
 */
struct ZoneBodyFunction {
  const LazyFunction *function = nullptr;
  ZoneFunctionIndices indices;
};

LazyFunction &build_repeat_zone_lazy_function(ResourceScope &scope,
                                              const bNodeTree &btree,
                                              const bke::bNodeTreeZone &zone,
                                              ZoneBuildInfo &zone_info,
                                              const ZoneBodyFunction &body_fn);

LazyFunction &build_foreach_geometry_element_zone_lazy_function(ResourceScope &scope,
                                                                const bNodeTree &btree,
                                                                const bke::bNodeTreeZone &zone,
                                                                ZoneBuildInfo &zone_info,
                                                                const ZoneBodyFunction &body_fn);

LazyFunction &build_closure_zone_lazy_function(ResourceScope &scope,
                                               const bNodeTree &btree,
                                               const bke::bNodeTreeZone &zone,
                                               ZoneBuildInfo &zone_info,
                                               const ZoneBodyFunction &body_fn);

struct EvaluateClosureFunctionIndices {
  struct {
    Vector<int> main;
    Vector<int> output_usages;
    Map<int, int> reference_set_by_output;
  } inputs;
  struct {
    Vector<int> main;
    Vector<int> input_usages;
  } outputs;
};

struct EvaluateClosureFunction {
  const LazyFunction *lazy_function = nullptr;
  EvaluateClosureFunctionIndices indices;
};

EvaluateClosureFunction build_evaluate_closure_node_lazy_function(ResourceScope &scope,
                                                                  const bNode &bnode);

void initialize_zone_wrapper(const bke::bNodeTreeZone &zone,
                             ZoneBuildInfo &zone_info,
                             const ZoneBodyFunction &body_fn,
                             bool expose_all_reference_sets,
                             Vector<lf::Input> &r_inputs,
                             Vector<lf::Output> &r_outputs);

std::string zone_wrapper_input_name(const ZoneBuildInfo &zone_info,
                                    const bke::bNodeTreeZone &zone,
                                    const Span<lf::Input> inputs,
                                    const int lf_socket_i);

std::string zone_wrapper_output_name(const ZoneBuildInfo &zone_info,
                                     const bke::bNodeTreeZone &zone,
                                     const Span<lf::Output> outputs,
                                     const int lf_socket_i);

/**
 * Report an error from a multi-function evaluation within a Geometry Nodes evaluation.
 *
 * NOTE: Currently, this the error is only actually reported under limited circumstances. It's
 * still safe to call this function from any multi-function though.
 */
void report_from_multi_function(const mf::Context &context,
                                NodeWarningType type,
                                std::string message);

}  // namespace blender::nodes
