/* SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "FN_lazy_function_graph.hh"
#include "FN_lazy_function_graph_executor.hh"

#include "NOD_geometry_nodes_log.hh"
#include "NOD_multi_function.hh"

#include "BLI_compute_context.hh"

struct Object;
struct Depsgraph;

namespace blender::nodes {

namespace lf = fn::lazy_function;
using lf::LazyFunction;

/**
 * Data that is passed into geometry nodes evaluation from the modifier.
 */
struct GeoNodesModifierData {
  /** Object that is currently evaluated. */
  const Object *self_object = nullptr;
  /** Depsgraph that is evaluating the modifier. */
  Depsgraph *depsgraph = nullptr;
  /** Optional logger. */
  geo_eval_log::GeoModifierLog *eval_log = nullptr;
  /**
   * Some nodes should be executed even when their output is not used (e.g. active viewer nodes and
   * the node groups they are contained in).
   */
  const MultiValueMap<ComputeContextHash, const lf::FunctionNode *> *side_effect_nodes;
};

/**
 * Custom user data that is passed to every geometry nodes related lazy-function evaluation.
 */
struct GeoNodesLFUserData : public lf::UserData {
  /**
   * Data from the modifier that is being evaluated.
   */
  GeoNodesModifierData *modifier_data = nullptr;
  /**
   * Current compute context. This is different depending in the (nested) node group that is being
   * evaluated.
   */
  const ComputeContext *compute_context = nullptr;
};

/**
 * Contains the mapping between the #bNodeTree and the corresponding lazy-function graph.
 * This is *not* a one-to-one mapping.
 */
struct GeometryNodeLazyFunctionGraphMapping {
  /**
   * Contains mapping of sockets for special nodes like group input and group output.
   */
  Map<const bNodeSocket *, lf::Socket *> dummy_socket_map;
  /**
   * The inputs sockets in the graph. Multiple group input nodes are combined into one in the
   * lazy-function graph.
   */
  Vector<lf::OutputSocket *> group_input_sockets;
  /**
   * A mapping used for logging intermediate values.
   */
  MultiValueMap<const lf::Socket *, const bNodeSocket *> bsockets_by_lf_socket_map;
  /**
   * Mappings for some special node types. Generally, this mapping does not exist for all node
   * types, so better have more specialized mappings for now.
   */
  Map<const bNode *, const lf::FunctionNode *> group_node_map;
  Map<const bNode *, const lf::FunctionNode *> viewer_node_map;
};

/**
 * Data that is cached for every #bNodeTree.
 */
struct GeometryNodesLazyFunctionGraphInfo {
  /**
   * Allocator used for many things contained in this struct.
   */
  LinearAllocator<> allocator;
  /**
   * Many nodes are implemented as multi-functions. So this contains a mapping from nodes to their
   * corresponding multi-functions.
   */
  std::unique_ptr<NodeMultiFunctions> node_multi_functions;
  /**
   * Many lazy-functions are build for the lazy-function graph. Since the graph does not own them,
   * we have to keep track of them separately.
   */
  Vector<std::unique_ptr<LazyFunction>> functions;
  /**
   * Debug info that has to be destructed when the graph is not used anymore.
   */
  Vector<std::unique_ptr<lf::DummyDebugInfo>> dummy_debug_infos_;
  /**
   * Many sockets have default values. Since those are not owned by the lazy-function graph, we
   * have to keep track of them separately. This only owns the values, the memory is owned by the
   * allocator above.
   */
  Vector<GMutablePointer> values_to_destruct;
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

  GeometryNodesLazyFunctionGraphInfo();
  ~GeometryNodesLazyFunctionGraphInfo();
};

/**
 * Logs intermediate values from the lazy-function graph evaluation into #GeoModifierLog based on
 * the mapping between the lazy-function graph and the corresponding #bNodeTree.
 */
class GeometryNodesLazyFunctionLogger : public fn::lazy_function::GraphExecutor::Logger {
 private:
  const GeometryNodesLazyFunctionGraphInfo &lf_graph_info_;

 public:
  GeometryNodesLazyFunctionLogger(const GeometryNodesLazyFunctionGraphInfo &lf_graph_info);
  void log_socket_value(const fn::lazy_function::Socket &lf_socket,
                        GPointer value,
                        const fn::lazy_function::Context &context) const override;
  void dump_when_outputs_are_missing(const lf::FunctionNode &node,
                                     Span<const lf::OutputSocket *> missing_sockets,
                                     const lf::Context &context) const override;
  void dump_when_input_is_set_twice(const lf::InputSocket &target_socket,
                                    const lf::OutputSocket &from_socket,
                                    const lf::Context &context) const override;
  void log_before_node_execute(const lf::FunctionNode &node,
                               const lf::Params &params,
                               const lf::Context &context) const override;
};

/**
 * Tells the lazy-function graph evaluator which nodes have side effects based on the current
 * context. For example, the same viewer node can have side effects in one context, but not in
 * another (depending on e.g. which tree path is currently viewed in the node editor).
 */
class GeometryNodesLazyFunctionSideEffectProvider
    : public fn::lazy_function::GraphExecutor::SideEffectProvider {
 public:
  Vector<const lf::FunctionNode *> get_nodes_with_side_effects(
      const lf::Context &context) const override;
};

/**
 * Main function that converts a #bNodeTree into a lazy-function graph. If the graph has been
 * generated already, nothing is done. Under some circumstances a valid graph cannot be created. In
 * those cases null is returned.
 */
const GeometryNodesLazyFunctionGraphInfo *ensure_geometry_nodes_lazy_function_graph(
    const bNodeTree &btree);

}  // namespace blender::nodes
