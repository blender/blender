/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_domain.hh"
#include "COM_node_operation.hh"
#include "COM_scheduler.hh"
#include "COM_shader_operation.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

/* ------------------------------------------------------------------------------------------------
 * Compile State
 *
 * The compile state is a utility class used to track the state of compilation when compiling the
 * node tree. In particular, it tracks two important pieces of information, each of which is
 * described in one of the following sections.
 *
 * First, it stores a mapping between all nodes and the operations they were compiled into. The
 * mapping are stored independently depending on the type of the operation in the node_operations_
 * and shader_operations_ maps. So those two maps are mutually exclusive. The compiler should call
 * the map_node_to_node_operation and map_node_to_shader_operation methods to populate those maps
 * as soon as it compiles a node or multiple nodes into an operation. Those maps are used to
 * retrieve the results of outputs linked to the inputs of operations. For more details, see the
 * get_result_from_output_socket method. For the node tree shown below, nodes 1, 2, and 6 are
 * mapped to their compiled operations in the node_operation_ map. While nodes 3 and 4 are both
 * mapped to the first shader operation, and node 5 is mapped to the second shader operation in the
 * shader_operations_ map.
 *
 *                             Shader Operation 1               Shader Operation 2
 *                   +-----------------------------------+     +------------------+
 * .------------.    |  .------------.  .------------.   |     |  .------------.  |  .------------.
 * |   Node 1   |    |  |   Node 3   |  |   Node 4   |   |     |  |   Node 5   |  |  |   Node 6   |
 * |            |----|--|            |--|            |---|-----|--|            |--|--|            |
 * |            |  .-|--|            |  |            |   |  .--|--|            |  |  |            |
 * '------------'  | |  '------------'  '------------'   |  |  |  '------------'  |  '------------'
 *                 | +-----------------------------------+  |  +------------------+
 * .------------.  |                                        |
 * |   Node 2   |  |                                        |
 * |            |--'----------------------------------------'
 * |            |
 * '------------'
 *
 * Second, it stores the shader compile unit as well as its domain. One should first go over the
 * discussion in COM_evaluator.hh for a high level description of the mechanism of the compile
 * unit. The one important detail in this class is the should_compile_shader_compile_unit method,
 * which implements the criteria of whether the compile unit should be compiled given the node
 * currently being processed as an argument. Those criteria are described as follows. If the
 * compile unit is empty as is the case when processing nodes 1, 2, and 3, then it plainly
 * shouldn't be compiled. If the given node is not a shader node, then it can't be added to the
 * compile unit and the unit is considered complete and should be compiled, as is the case when
 * processing node 6. If the computed domain of the given node is not compatible with the domain of
 * the compiled unit, then it can't be added to the unit and the unit is considered complete and
 * should be compiled, as is the case when processing node 5, more on this in the next section.
 * Otherwise, the given node is compatible with the compile unit and can be added to it, so the
 * unit shouldn't be compiled just yet, as is the case when processing node 4.
 *
 * Special attention should be given to the aforementioned domain compatibility criterion. One
 * should first go over the discussion in COM_domain.hh for more information on domains. When a
 * compile unit gets eventually compiled to a shader operation, that operation will have a certain
 * operation domain, and any node that gets added to the compile unit should itself have a computed
 * node domain that is compatible with that operation domain, otherwise, had the node been compiled
 * into its own operation separately, the result would have been be different. For instance,
 * consider the above node tree where node 1 outputs a 100x100 result, node 2 outputs a 50x50
 * result, the first input in node 3 has the highest domain priority, and the second input in node
 * 5 has the highest domain priority. In this case, shader operation 1 will output a 100x100
 * result, and shader operation 2 will output a 50x50 result, because that's the computed operation
 * domain for each of them. So node 6 will get a 50x50 result. Now consider the same node tree, but
 * where all three nodes 3, 4, and 5 were compiled into a single shader operation as shown the node
 * tree below. In that case, shader operation 1 will output a 100x100 result, because that's its
 * computed operation domain. So node 6 will get a 100x100 result. As can be seen, the final result
 * is different even though the node tree is the same. That's why the compiler can decide to
 * compile the compile unit early even though further nodes can still be technically added to it.
 *
 *                                      Shader Operation 1
 *                   +------------------------------------------------------+
 * .------------.    |  .------------.  .------------.      .------------.  |  .------------.
 * |   Node 1   |    |  |   Node 3   |  |   Node 4   |      |   Node 5   |  |  |   Node 6   |
 * |            |----|--|            |--|            |------|            |--|--|            |
 * |            |  .-|--|            |  |            |  .---|            |  |  |            |
 * '------------'  | |  '------------'  '------------'  |   '------------'  |  '------------'
 *                 | +----------------------------------|-------------------+
 * .------------.  |                                    |
 * |   Node 2   |  |                                    |
 * |            |--'------------------------------------'
 * |            |
 * '------------'
 *
 * To check for the domain compatibility between the compile unit and the node being processed,
 * the domain of the compile unit is assumed to be the domain of the first node whose computed
 * domain is not an identity domain. Identity domains corresponds to single value results, so those
 * are always compatible with any domain. The domain of the compile unit is computed and set in
 * the add_node_to_shader_compile_unit method. When processing a node, the computed domain of node
 * is compared to the compile unit domain in the should_compile_shader_compile_unit method, noting
 * that identity domains are always compatible. Node domains are computed in the
 * compute_shader_node_domain method, which is analogous to Operation::compute_domain for nodes
 * that are not yet compiled. */
class CompileState {
 private:
  /* A reference to the node execution schedule that is being compiled. */
  const Schedule &schedule_;
  /* Those two maps associate each node with the operation it was compiled into. Each node is
   * either compiled into a node operation and added to node_operations, or compiled into a shader
   * operation and added to shader_operations. Those maps are used to retrieve the results of
   * outputs linked to the inputs of operations. See the get_result_from_output_socket method for
   * more information. */
  Map<DNode, NodeOperation *> node_operations_;
  Map<DNode, ShaderOperation *> shader_operations_;
  /* A contiguous subset of the node execution schedule that contains the group of nodes that will
   * be compiled together into a Shader Operation. See the discussion in COM_evaluator.hh for
   * more information. */
  ShaderCompileUnit shader_compile_unit_;
  /* The domain of the shader compile unit. */
  Domain shader_compile_unit_domain_ = Domain::identity();

 public:
  /* Construct a compile state from the node execution schedule being compiled. */
  CompileState(const Schedule &schedule);

  /* Get a reference to the node execution schedule being compiled. */
  const Schedule &get_schedule();

  /* Add an association between the given node and the give node operation that the node was
   * compiled into in the node_operations_ map. */
  void map_node_to_node_operation(DNode node, NodeOperation *operation);

  /* Add an association between the given node and the give shader operation that the node was
   * compiled into in the shader_operations_ map. */
  void map_node_to_shader_operation(DNode node, ShaderOperation *operation);

  /* Returns a reference to the result of the operation corresponding to the given output that the
   * given output's node was compiled to. */
  Result &get_result_from_output_socket(DOutputSocket output);

  /* Add the given node to the compile unit. And if the domain of the compile unit is not yet
   * determined or was determined to be an identity domain, update it to the computed domain for
   * the give node. */
  void add_node_to_shader_compile_unit(DNode node);

  /* Get a reference to the shader compile unit. */
  ShaderCompileUnit &get_shader_compile_unit();

  /* Clear the compile unit. This should be called once the compile unit is compiled to ready it to
   * track the next potential compile unit. */
  void reset_shader_compile_unit();

  /* Determines if the compile unit should be compiled based on a number of criteria give the node
   * currently being processed. Those criteria are as follows:
   * - If compile unit is empty, then it can't and shouldn't be compiled.
   * - If the given node is not a shader node, then it can't be added to the compile unit
   *   and the unit is considered complete and should be compiled.
   * - If the computed domain of the given node is not compatible with the domain of the compile
   *   unit, then it can't be added to it and the unit is considered complete and should be
   *   compiled. */
  bool should_compile_shader_compile_unit(DNode node);

 private:
  /* Compute the node domain of the given shader node. This is analogous to the
   * Operation::compute_domain method, except it is computed from the node itself as opposed to a
   * compiled operation. See the discussion in COM_domain.hh for more information. */
  Domain compute_shader_node_domain(DNode node);
};

}  // namespace blender::realtime_compositor
