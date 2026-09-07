/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>
#include <optional>

#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_node_operation.hh"
#include "COM_pixel_operation.hh"

namespace blender {
struct bNode;
struct bNodeSocket;
}  // namespace blender

namespace blender::compositor {

struct Schedule;

/* ------------------------------------------------------------------------------------------------
 * Node Tree Evaluator
 *
 * The node tree evaluator is a utility class used to evaluate a compositing node tree and track
 * its state during evaluation. It compiles the node tree into an operations stream, evaluating
 * the operations in the process. It should be noted that operations are eagerly evaluated as soon
 * as they are compiled, as opposed to compiling the whole operations stream and then evaluating it
 * in a separate step. This is done because the evaluator uses the evaluated results of previously
 * compiled operations to compile the operations that follow them in an optimized manner.
 *
 * Figure (1) shows a sample node tree with the execution schedule denoted by the node numbers. The
 * evaluator goes over the execution schedule in order and compiles each node into either a Node
 * Operation or a Pixel Operation, depending on the node type, see the is_pixel_node function. A
 * pixel operation is constructed from a group of nodes forming a contiguous subset of the node
 * execution schedule. For instance, in the node tree in Figure (1), nodes 3 and 4 are compiled
 * together into a pixel operation and node 5 is compiled into its own pixel operation, both of
 * which are contiguous subsets of the node execution schedule. This process is described in
 * details in the following section.
 *
 *                             Pixel Operation 1                Pixel Operation 2
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
 * '------------'                           Figure (1)
 *
 * For non pixel nodes, the compilation process is straight forward, the compiler instantiates a
 * node operation from the node, map its inputs to the results of the outputs they are linked to,
 * and evaluates the operations. However, for pixel nodes, since a group of nodes can be compiled
 * together into a pixel operation, the compilation process is a bit involved. The compiler stores
 * the so called "pixel compile unit", which is the current group of nodes that will eventually be
 * compiled together into a pixel operation. While going over the schedule, the compiler adds the
 * pixel nodes to the compile unit until it decides that the compile unit is complete and should be
 * compiled. This is typically decided when the current node is not compatible with the compile
 * unit and can't be added to it, only then it compiles the compile unit into a pixel operation and
 * resets it to ready it to track the next potential group of nodes that will form a pixel
 * operation. This decision is made based on various criteria as will be described in a following
 * section, but perhaps the most evident of which is whether the node is actually a pixel node, if
 * it isn't, then it evidently can't be added to the compile unit and the compile unit is should be
 * compiled.
 *
 * For the node tree in Figure (1), the compilation process is as follows. The compiler goes over
 * the node execution schedule in order considering each node. Nodes 1 and 2 are not pixel node so
 * they are compiled into node operations and added to the operations stream. The current compile
 * unit is empty, so it is not compiled. Node 3 is a pixel node, and since the compile unit is
 * currently empty, it is unconditionally added to it. Node 4 is a pixel node, it was decided---for
 * the sake of the demonstration---that it is compatible with the compile unit and can be added to
 * it. Node 5 is a pixel node, but it was decided---for the sake of the demonstration---that it is
 * not compatible with the compile unit, so the compile unit is considered complete and is compiled
 * first, adding the first pixel operation to the operations stream and resetting the compile
 * unit. Node 5 is then added to the now empty compile unit similar to node 3. Node 6 is not a
 * pixel node, so the compile unit is considered complete and is compiled first, adding the first
 * pixel operation to the operations stream and resetting the compile unit. Finally, node 6 is
 * compiled into a node operation similar to nodes 1 and 2 and added to the operations stream.
 *
 * During compilation, the class tracks two important pieces of information, each of which is
 * described in one of the following sections.
 *
 * First, it stores a mapping between all nodes and the operations they were compiled into. The
 * mapping are stored independently depending on the type of the operation in the node_operations_
 * and pixel_operations_ maps. So those two maps are mutually exclusive. The compiler should call
 * the map_node_to_node_operation and map_node_to_pixel_operation methods to populate those maps
 * as soon as it compiles a node or multiple nodes into an operation. Those maps are used to
 * retrieve the results of outputs linked to the inputs of operations. For more details, see the
 * get_result_from_output_socket method. For the node tree in Figure (1), nodes 1, 2, and 6 are
 * mapped to their compiled operations in the node_operation_ map. While nodes 3 and 4 are both
 * mapped to the first pixel operation, and node 5 is mapped to the second pixel operation in the
 * pixel_operations_ map.
 *
 * Second, it stores the pixel compile unit, whether is operates on single values, and its domain
 * if it was not operating on single values. The one important detail in this class is the
 * should_compile_pixel_compile_unit method, which implements the criteria of whether the compile
 * unit should be compiled given the node currently being processed as an argument. Those criteria
 * are described as follows. If the compile unit is empty as is the case when processing nodes 1,
 * 2, and 3, then it plainly shouldn't be compiled. If the given node is not a pixel node, then it
 * can't be added to the compile unit and the unit is considered complete and should be compiled,
 * as is the case when processing node 6. If the compile unit operates on single values and the
 * given node operates on non-single values or vice versa, then it can't be added to the compile
 * unit and the unit is considered complete and should be compiled, more on that in the next
 * section. If the computed domain of the given node is not compatible with the domain of the
 * compiled unit, then it can't be added to the unit and the unit is considered complete and should
 * be compiled, as is the case when processing node 5, more on this in the next section. Otherwise,
 * the given node is compatible with the compile unit and can be added to it, so the unit shouldn't
 * be compiled just yet, as is the case when processing node 4.
 *
 * Special attention should be given to the aforementioned single value and domain compatibility
 * criterion. One should first go over the discussion in COM_domain.hh for more information on
 * domains. When a compile unit gets eventually compiled to a pixel operation, that operation will
 * have a certain operation domain, and any node that gets added to the compile unit should itself
 * have a computed node domain that is compatible with that operation domain, otherwise, had the
 * node been compiled into its own operation separately, the result would have been be different.
 * For instance, consider the node tree in Figure (1) where node 1 outputs a 100x100 result, node
 * 2 outputs a 50x50 result, the first input in node 3 has the highest domain priority, and the
 * second input in node 5 has the highest domain priority. In this case, pixel operation 1 will
 * output a 100x100 result, and pixel operation 2 will output a 50x50 result, because that's the
 * computed operation domain for each of them. So node 6 will get a 50x50 result. Now consider the
 * same node tree, but where all three nodes 3, 4, and 5 were compiled into a single pixel
 * operation as shown the node tree in Figure (2). In that case, pixel operation 1 will output a
 * 100x100 result, because that's its computed operation domain. So node 6 will get a 100x100
 * result. As can be seen, the final result is different even though the node tree is the same.
 * That's why the compiler can decide to compile the compile unit early even though further nodes
 * can still be technically added to it.
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
 * '------------'                           Figure (2)
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
class NodeTreeEvaluator {
 private:
  /* The compositor context. */
  Context &context_;
  /* The node execution schedule that is being evaluated. */
  const Schedule &schedule_;
  /* The operation that uses this evaluator. */
  Operation &operation_;
  /* The compute context where the evaluation takes place. */
  const ComputeContext &compute_context_;
  /* The compiled operations stream, which contains all compiled operations so far. */
  Vector<std::unique_ptr<Operation>> operations_stream_;
  /* Those two maps associate each node with the operation it was compiled into. Each node is
   * either compiled into a node operation and added to node_operations, or compiled into a pixel
   * operation and added to pixel_operations. Those maps are used to retrieve the results of
   * outputs linked to the inputs of operations. See the get_result_from_output_socket method for
   * more information. */
  Map<const bNode *, NodeOperation *> node_operations_;
  Map<const bNode *, PixelOperation *> pixel_operations_;
  /* A contiguous subset of the node execution schedule that contains the group of nodes that will
   * be compiled together into a pixel operation. See the description of the class for more
   * information. */
  PixelCompileUnit pixel_compile_unit_;
  /* Stores whether the current pixel compile unit operates on single values. Only initialized when
   * the pixel compile unit is not empty. */
  bool is_pixel_compile_unit_single_value_;
  /* The domain of the pixel compile unit if it was not a single value. Only initialized when the
   * pixel compile unit is not empty and is not a single value. */
  std::optional<Domain> pixel_compile_unit_domain_;

 public:
  /* Construct a node tree evaluator for the given node tree execution schedule. The evaluation is
   * assumed to happen in the given compute context for the given operation. */
  NodeTreeEvaluator(Context &context,
                    const Schedule &schedule,
                    Operation &operation,
                    const ComputeContext &compute_context);

  /* Evaluates the node tree. */
  void evaluate();

  /* Returns a reference to the result of the operation corresponding to the given output that the
   * given output's node was compiled to. */
  Result &get_result_from_output_socket(const bNodeSocket &output);

  /* Get a reference to the pixel compile unit. */
  PixelCompileUnit &pixel_compile_unit();

  /* Get a reference to the node execution schedule being compiled. */
  const Schedule &schedule();

 private:
  /* Compile the given node into a node operation, map each input to the result of the output
   * linked to it, add the newly created operation to the operations stream, and evaluate the
   * operation. */
  void evaluate_node(const bNode &node);

  /* Constructs and returns a node operation that represents to the given node. */
  NodeOperation *create_node_operation(const bNode &node);

  /* Map each input of the node operation to the result of the output linked to it. Unlinked inputs
   * are mapped to the result of a newly created Input Single Value Operation, which is added to
   * the operations stream and evaluated. Since this method might add operations to the operations
   * stream, the actual node operation should only be added to the stream once this method is
   * called. */
  void map_node_operation_inputs_to_their_results(const bNode &node, NodeOperation *operation);

  /* Create one of the concrete subclasses of the PixelOperation based on the context and currently
   * active pixel compile unit. Deleting the operation is the caller's responsibility. */
  PixelOperation *create_pixel_operation();

  /* Compile the pixel compile unit into a pixel operation, map each input of the operation to
   * the result of the output linked to it, add the newly created operation to the operations
   * stream, evaluate the operation, and finally reset the pixel compile unit. */
  void evaluate_pixel_compile_unit();

  /* Map each input of the pixel operation to the result of the output linked to it. This might
   * also correct the reference counts of the results, see the implementation for more details. */
  void map_pixel_operation_inputs_to_their_results(PixelOperation *operation);

  /* Add an association between the given node and the given node operation that the node was
   * compiled into in the node_operations_ map. */
  void map_node_to_node_operation(const bNode &node, NodeOperation *operation);

  /* Add an association between the given node and the give pixel operation that the node was
   * compiled into in the pixel_operations_ map. */
  void map_node_to_pixel_operation(const bNode &node, PixelOperation *operation);

  /* Add the given node to the compile unit. And if the domain of the compile unit is not yet
   * determined or was determined to be an identity domain, update it to the computed domain for
   * the given node. */
  void add_node_to_pixel_compile_unit(const bNode &node);

  /* Returns true if the pixel compile unit operates on single values. */
  bool is_pixel_compile_unit_single_value();

  /* Clear the compile unit. This should be called once the compile unit is compiled to ready it to
   * track the next potential compile unit. */
  void reset_pixel_compile_unit();

  /* Determines if the compile unit should be compiled based on a number of criteria give the node
   * currently being processed. See the class description for a description of the method. */
  bool should_compile_pixel_compile_unit(const bNode &node);

  /* Determines if the given pixel node operates on single values or not. The node operates on
   * single values if all its inputs are single values, and consequently will also output single
   * values. */
  bool is_pixel_node_single_value(const bNode &node);

  /* Compute the node domain of the given pixel node. This is analogous to the
   * Operation::compute_domain method, except it is computed from the node itself as opposed to a
   * compiled operation. See the discussion in COM_domain.hh for more information. */
  Domain compute_pixel_node_domain(const bNode &node);

  /* Identify if the number of outputs of the pixel compile unit surpass what is possible. This is
   * essentially the number of outputs that will be added for the nodes in the pixel compile unit
   * in ShaderOperation::populate_results_for_node. */
  bool pixel_compile_unit_has_too_many_outputs(const bool are_node_previews_needed);

  /* Identify if the number of inputs of the pixel compile unit surpass what is possible. This is
   * essentially the number of inputs that will be added for the nodes in the pixel compile unit in
   * ShaderOperation::link_node_inputs. */
  bool pixel_compile_unit_has_too_many_inputs();

  /* Cancels the evaluation by freeing the results of the operations that were already evaluated,
   * that's because later operations that use the already allocated results will not be evaluated,
   * so they consequently will not release the results that they use and we need to free them
   * manually. */
  void cancel_evaluation();

  /* Get a reference to the compositor context. */
  Context &context();

  /* Get a reference to the operation that uses the evaluator. */
  Operation &operation();
};

}  // namespace blender::compositor
