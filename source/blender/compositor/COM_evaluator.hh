/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_vector.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_compile_state.hh"
#include "COM_context.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

/* ------------------------------------------------------------------------------------------------
 * Evaluator
 *
 * The evaluator is the main class of the compositor and the entry point of its execution. It is
 * constructed from a compositor node tree and a compositor context. It compiles the node tree into
 * an operations stream, evaluating the operations in the process. It should be noted that
 * operations are eagerly evaluated as soon as they are compiled, as opposed to compiling the whole
 * operations stream and then evaluating it in a separate step. This is done because the evaluator
 * uses the evaluated results of previously compiled operations to compile the operations that
 * follow them in an optimized manner.
 *
 * Evaluation starts by computing an optimized node execution schedule by calling the
 * compute_schedule function, see the discussion in COM_scheduler.hh for more details. For the node
 * tree shown below, the execution schedule is denoted by the node numbers. The compiler then goes
 * over the execution schedule in order and compiles each node into either a Node Operation or a
 * Pixel Operation, depending on the node type, see the is_pixel_node function. A pixel operation
 * is constructed from a group of nodes forming a contiguous subset of the node execution schedule.
 * For instance, in the node tree shown below, nodes 3 and 4 are compiled together into a pixel
 * operation and node 5 is compiled into its own pixel operation, both of which are contiguous
 * subsets of the node execution schedule. This process is described in details in the following
 * section.
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
 * '------------'
 *
 * For non pixel nodes, the compilation process is straight forward, the compiler instantiates a
 * node operation from the node, map its inputs to the results of the outputs they are linked to,
 * and evaluates the operations. However, for pixel nodes, since a group of nodes can be compiled
 * together into a pixel operation, the compilation process is a bit involved. The compiler uses
 * an instance of the Compile State class to keep track of the compilation process. The compiler
 * state stores the so called "pixel compile unit", which is the current group of nodes that will
 * eventually be compiled together into a pixel operation. While going over the schedule, the
 * compiler adds the pixel nodes to the compile unit until it decides that the compile unit is
 * complete and should be compiled. This is typically decided when the current node is not
 * compatible with the compile unit and can't be added to it, only then it compiles the compile
 * unit into a pixel operation and resets it to ready it to track the next potential group of
 * nodes that will form a pixel operation. This decision is made based on various criteria in the
 * should_compile_pixel_compile_unit function. See the discussion in COM_compile_state.hh for more
 * details of those criteria, but perhaps the most evident of which is whether the node is actually
 * a pixel node, if it isn't, then it evidently can't be added to the compile unit and the compile
 * unit is should be compiled.
 *
 * For the node tree above, the compilation process is as follows. The compiler goes over the node
 * execution schedule in order considering each node. Nodes 1 and 2 are not pixel node so they are
 * compiled into node operations and added to the operations stream. The current compile unit is
 * empty, so it is not compiled. Node 3 is a pixel node, and since the compile unit is currently
 * empty, it is unconditionally added to it. Node 4 is a pixel node, it was decided---for the sake
 * of the demonstration---that it is compatible with the compile unit and can be added to it. Node
 * 5 is a pixel node, but it was decided---for the sake of the demonstration---that it is not
 * compatible with the compile unit, so the compile unit is considered complete and is compiled
 * first, adding the first pixel operation to the operations stream and resetting the compile
 * unit. Node 5 is then added to the now empty compile unit similar to node 3. Node 6 is not a
 * pixel node, so the compile unit is considered complete and is compiled first, adding the first
 * pixel operation to the operations stream and resetting the compile unit. Finally, node 6 is
 * compiled into a node operation similar to nodes 1 and 2 and added to the operations stream. */
class Evaluator {
 private:
  /* A reference to the compositor context. */
  Context &context_;
  /* A derived node tree representing the compositor node tree. */
  std::unique_ptr<DerivedNodeTree> derived_node_tree_;
  /* The compiled operations stream, which contains all compiled operations so far. */
  Vector<std::unique_ptr<Operation>> operations_stream_;

 public:
  /* Construct an evaluator from a context. */
  Evaluator(Context &context);

  /* Evaluates the compositor node tree by compiling it into an operations stream and evaluating
   * it. */
  void evaluate();

 private:
  /* Check if the compositor node tree is valid by checking if it has things like cyclic links and
   * undefined nodes or sockets. If the node tree is valid, true is returned. Otherwise, false is
   * returned, and an appropriate error message is set by calling the context's set_info_message
   * method. */
  bool validate_node_tree();

  /* Compile the given node into a node operation, map each input to the result of the output
   * linked to it, update the compile state, add the newly created operation to the operations
   * stream, and evaluate the operation. */
  void evaluate_node(DNode node, CompileState &compile_state);

  /* Map each input of the node operation to the result of the output linked to it. Unlinked inputs
   * are mapped to the result of a newly created Input Single Value Operation, which is added to
   * the operations stream and evaluated. Since this method might add operations to the operations
   * stream, the actual node operation should only be added to the stream once this method is
   * called. */
  void map_node_operation_inputs_to_their_results(DNode node,
                                                  NodeOperation *operation,
                                                  CompileState &compile_state);

  /* Compile the pixel compile unit into a pixel operation, map each input of the operation to
   * the result of the output linked to it, update the compile state, add the newly created
   * operation to the operations stream, evaluate the operation, and finally reset the pixel
   * compile unit. */
  void evaluate_pixel_compile_unit(CompileState &compile_state);

  /* Map each input of the pixel operation to the result of the output linked to it. This might
   * also correct the reference counts of the results, see the implementation for more details. */
  void map_pixel_operation_inputs_to_their_results(PixelOperation *operation,
                                                   CompileState &compile_state);

  /* Cancels the evaluation by informing the static cache manager of the cancellation and freeing
   * the results of the operations that were already evaluated, that's because later operations
   * that use the already allocated results will not be evaluated, so they consequently will not
   * release the results that they use and we need to free them manually. */
  void cancel_evaluation();
};

}  // namespace blender::compositor
