/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_map.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_node_operation.hh"
#include "COM_pixel_operation.hh"
#include "COM_scheduler.hh"

namespace blender::compositor {

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
 * and pixel_operations_ maps. So those two maps are mutually exclusive. The compiler should call
 * the map_node_to_node_operation and map_node_to_pixel_operation methods to populate those maps
 * as soon as it compiles a node or multiple nodes into an operation. Those maps are used to
 * retrieve the results of outputs linked to the inputs of operations. For more details, see the
 * get_result_from_output_socket method. For the node tree shown below, nodes 1, 2, and 6 are
 * mapped to their compiled operations in the node_operation_ map. While nodes 3 and 4 are both
 * mapped to the first pixel operation, and node 5 is mapped to the second pixel operation in the
 * pixel_operations_ map.
 *
 *                              Pixel Operation 1               Pixel Operation 2
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
 * Second, it stores the pixel compile unit, whether is operates on single values, and its domain
 * if it was not operating on single values. One should first go over the discussion in
 * COM_evaluator.hh for a high level description of the mechanism of the compile unit. The one
 * important detail in this class is the should_compile_pixel_compile_unit method, which implements
 * the criteria of whether the compile unit should be compiled given the node currently being
 * processed as an argument. Those criteria are described as follows. If the compile unit is empty
 * as is the case when processing nodes 1, 2, and 3, then it plainly shouldn't be compiled. If the
 * given node is not a pixel node, then it can't be added to the compile unit and the unit is
 * considered complete and should be compiled, as is the case when processing node 6. If the
 * compile unit operates on single values and the given node operates on non-single values or vice
 * versa, then it can't be added to the compile unit and the unit is considered complete and should
 * be compiled, more on that in the next section. If the computed domain of the given node is not
 * compatible with the domain of the compiled unit, then it can't be added to the unit and the unit
 * is considered complete and should be compiled, as is the case when processing node 5, more on
 * this in the next section. Otherwise, the given node is compatible with the compile unit and can
 * be added to it, so the unit shouldn't be compiled just yet, as is the case when processing
 * node 4.
 *
 * Special attention should be given to the aforementioned single value and domain compatibility
 * criterion. One should first go over the discussion in COM_domain.hh for more information on
 * domains. When a compile unit gets eventually compiled to a pixel operation, that operation will
 * have a certain operation domain, and any node that gets added to the compile unit should itself
 * have a computed node domain that is compatible with that operation domain, otherwise, had the
 * node been compiled into its own operation separately, the result would have been be different.
 * For instance, consider the above node tree where node 1 outputs a 100x100 result, node 2 outputs
 * a 50x50 result, the first input in node 3 has the highest domain priority, and the second input
 * in node 5 has the highest domain priority. In this case, pixel operation 1 will output a 100x100
 * result, and pixel operation 2 will output a 50x50 result, because that's the computed operation
 * domain for each of them. So node 6 will get a 50x50 result. Now consider the same node tree, but
 * where all three nodes 3, 4, and 5 were compiled into a single pixel operation as shown the node
 * tree below. In that case, pixel operation 1 will output a 100x100 result, because that's its
 * computed operation domain. So node 6 will get a 100x100 result. As can be seen, the final result
 * is different even though the node tree is the same. That's why the compiler can decide to
 * compile the compile unit early even though further nodes can still be technically added to it.
 *
 *                                      Pixel Operation 1
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
 * Similarly, all nodes in the compile unit should either be operating on single values or not.
 * Otherwise, assuming a node operates on single values and its output is used in 1) a non-single
 * value pixel operation and 2) another node that expects single values, if that node was added to
 * the pixel operation, its output will be non-single value, while it would have been a single
 * value if it was not added to the pixel operation.
 *
 * To check for the single value type and domain compatibility between the compile unit and the
 * node being processed, the single value type and the domain of the compile unit is assumed to be
 * the single value type and the domain of the first node added to the compile unit, noting that
 * the domain is optional, since it is not used if the compile unit is a single value one. The
 * single value type and the domain of the compile unit are computed and set in the
 * add_node_to_pixel_compile_unit method. When processing a node, the computed single value type
 * and the computed domain of node are compared to the compile unit single value type and domain in
 * the should_compile_pixel_compile_unit method. Node single value types and domains are computed
 * in the is_pixel_node_single_value and compute_pixel_node_domain methods respectively, the latter
 * of which is analogous to the Operation::compute_domain method for nodes that are not yet
 * compiled. */
class CompileState {
 private:
  /* A reference to the compositor context. */
  const Context &context_;
  /* A reference to the node execution schedule that is being compiled. */
  const Schedule &schedule_;
  /* Those two maps associate each node with the operation it was compiled into. Each node is
   * either compiled into a node operation and added to node_operations, or compiled into a pixel
   * operation and added to pixel_operations. Those maps are used to retrieve the results of
   * outputs linked to the inputs of operations. See the get_result_from_output_socket method for
   * more information. */
  Map<DNode, NodeOperation *> node_operations_;
  Map<DNode, PixelOperation *> pixel_operations_;
  /* A contiguous subset of the node execution schedule that contains the group of nodes that will
   * be compiled together into a pixel operation. See the discussion in COM_evaluator.hh for more
   * information. */
  PixelCompileUnit pixel_compile_unit_;
  /* Stores whether the current pixel compile unit operates on single values. Only initialized when
   * the pixel compile unit is not empty. */
  bool is_pixel_compile_unit_single_value_;
  /* The domain of the pixel compile unit if it was not a single value. Only initialized when the
   * pixel compile unit is not empty and is not a single value. */
  std::optional<Domain> pixel_compile_unit_domain_;

 public:
  /* Construct a compile state from the node execution schedule being compiled. */
  CompileState(const Context &context, const Schedule &schedule);

  /* Get a reference to the node execution schedule being compiled. */
  const Schedule &get_schedule();

  /* Add an association between the given node and the give node operation that the node was
   * compiled into in the node_operations_ map. */
  void map_node_to_node_operation(DNode node, NodeOperation *operation);

  /* Add an association between the given node and the give pixel operation that the node was
   * compiled into in the pixel_operations_ map. */
  void map_node_to_pixel_operation(DNode node, PixelOperation *operation);

  /* Returns a reference to the result of the operation corresponding to the given output that the
   * given output's node was compiled to. */
  Result &get_result_from_output_socket(DOutputSocket output);

  /* Add the given node to the compile unit. And if the domain of the compile unit is not yet
   * determined or was determined to be an identity domain, update it to the computed domain for
   * the give node. */
  void add_node_to_pixel_compile_unit(DNode node);

  /* Get a reference to the pixel compile unit. */
  PixelCompileUnit &get_pixel_compile_unit();

  /* Returns true if the pixel compile unit operates on single values. */
  bool is_pixel_compile_unit_single_value();

  /* Clear the compile unit. This should be called once the compile unit is compiled to ready it to
   * track the next potential compile unit. */
  void reset_pixel_compile_unit();

  /* Determines if the compile unit should be compiled based on a number of criteria give the node
   * currently being processed. See the class description for a description of the method. */
  bool should_compile_pixel_compile_unit(DNode node);

  /* Computes the number of pixel operation outputs that will be added for this node in the current
   * pixel compile unit. This is essentially the number of outputs that will be added for the node
   * in PixelOperation::populate_results_for_node. */
  int compute_pixel_node_operation_outputs_count(DNode node);

 private:
  /* Determines if the given pixel node operates on single values or not. The node operates on
   * single values if all its inputs are single values, and consequently will also output single
   * values. */
  bool is_pixel_node_single_value(DNode node);

  /* Compute the node domain of the given pixel node. This is analogous to the
   * Operation::compute_domain method, except it is computed from the node itself as opposed to a
   * compiled operation. See the discussion in COM_domain.hh for more information. */
  Domain compute_pixel_node_domain(DNode node);
};

}  // namespace blender::compositor
