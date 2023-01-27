/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_vector.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

#include "COM_compile_state.hh"
#include "COM_context.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_shader_operation.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

/* ------------------------------------------------------------------------------------------------
 * Evaluator
 *
 * The evaluator is the main class of the compositor and the entry point of its execution. The
 * evaluator compiles the compositor node tree and evaluates it to compute its output. It is
 * constructed from a compositor node tree and a compositor context. Upon calling the evaluate
 * method, the evaluator will check if the node tree is already compiled into an operations stream,
 * and if it is, it will go over it and evaluate the operations in order. It is then the
 * responsibility of the caller to call the reset method when the node tree changes to invalidate
 * the operations stream. A reset is also required if the resources used by the node tree change,
 * for instances, when the dimensions of an image used by the node tree changes. This is necessary
 * because the evaluator compiles the node tree into an operations stream that is specifically
 * optimized for the structure of the resources used by the node tree.
 *
 * Otherwise, if the node tree is not yet compiled, the evaluator will compile it into an
 * operations stream, evaluating the operations in the process. It should be noted that operations
 * are evaluated as soon as they are compiled, as opposed to compiling the whole operations stream
 * and then evaluating it in a separate step. This is important because, as mentioned before, the
 * operations stream is optimized specifically for the structure of the resources used by the node
 * tree, which is only known after the operations are evaluated. In other words, the evaluator uses
 * the evaluated results of previously compiled operations to compile the operations that follow
 * them in an optimized manner.
 *
 * Compilation starts by computing an optimized node execution schedule by calling the
 * compute_schedule function, see the discussion in COM_scheduler.hh for more details. For the node
 * tree shown below, the execution schedule is denoted by the node numbers. The compiler then goes
 * over the execution schedule in order and compiles each node into either a Node Operation or a
 * Shader Operation, depending on the node type, see the is_shader_node function. A Shader
 * operation is constructed from a group of nodes forming a contiguous subset of the node execution
 * schedule. For instance, in the node tree shown below, nodes 3 and 4 are compiled together into a
 * shader operation and node 5 is compiled into its own shader operation, both of which are
 * contiguous subsets of the node execution schedule. This process is described in details in the
 * following section.
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
 * For non shader nodes, the compilation process is straight forward, the compiler instantiates a
 * node operation from the node, map its inputs to the results of the outputs they are linked to,
 * and evaluates the operations. However, for shader nodes, since a group of nodes can be compiled
 * together into a shader operation, the compilation process is a bit involved. The compiler uses
 * an instance of the Compile State class to keep track of the compilation process. The compiler
 * state stores the so called "shader compile unit", which is the current group of nodes that will
 * eventually be compiled together into a shader operation. While going over the schedule, the
 * compiler adds the shader nodes to the compile unit until it decides that the compile unit is
 * complete and should be compiled. This is typically decided when the current node is not
 * compatible with the compile unit and can't be added to it, only then it compiles the compile
 * unit into a shader operation and resets it to ready it to track the next potential group of
 * nodes that will form a shader operation. This decision is made based on various criteria in the
 * should_compile_shader_compile_unit function. See the discussion in COM_compile_state.hh for more
 * details of those criteria, but perhaps the most evident of which is whether the node is actually
 * a shader node, if it isn't, then it evidently can't be added to the compile unit and the compile
 * unit is should be compiled.
 *
 * For the node tree above, the compilation process is as follows. The compiler goes over the node
 * execution schedule in order considering each node. Nodes 1 and 2 are not shader node so they are
 * compiled into node operations and added to the operations stream. The current compile unit is
 * empty, so it is not compiled. Node 3 is a shader node, and since the compile unit is currently
 * empty, it is unconditionally added to it. Node 4 is a shader node, it was decided---for the sake
 * of the demonstration---that it is compatible with the compile unit and can be added to it. Node
 * 5 is a shader node, but it was decided---for the sake of the demonstration---that it is not
 * compatible with the compile unit, so the compile unit is considered complete and is compiled
 * first, adding the first shader operation to the operations stream and resetting the compile
 * unit. Node 5 is then added to the now empty compile unit similar to node 3. Node 6 is not a
 * shader node, so the compile unit is considered complete and is compiled first, adding the first
 * shader operation to the operations stream and resetting the compile unit. Finally, node 6 is
 * compiled into a node operation similar to nodes 1 and 2 and added to the operations stream. */
class Evaluator {
 private:
  /* A reference to the compositor context. */
  Context &context_;
  /* A derived node tree representing the compositor node tree. This is constructed when the node
   * tree is compiled and reset when the evaluator is reset, so it gets reconstructed every time
   * the node tree changes. */
  std::unique_ptr<DerivedNodeTree> derived_node_tree_;
  /* The compiled operations stream. This contains ordered pointers to the operations that were
   * compiled. This is initialized when the node tree is compiled and freed when the evaluator
   * resets. The is_compiled_ member indicates whether the operation stream can be used or needs to
   * be compiled first. Note that the operations stream can be empty even when compiled, this can
   * happen when the node tree is empty or invalid for instance. */
  Vector<std::unique_ptr<Operation>> operations_stream_;
  /* True if the node tree is already compiled into an operations stream that can be evaluated
   * directly. False if the node tree is not compiled yet and needs to be compiled. */
  bool is_compiled_ = false;

 public:
  /* Construct an evaluator from a context. */
  Evaluator(Context &context);

  /* Evaluate the compositor node tree. If the node tree is already compiled into an operations
   * stream, that stream will be evaluated directly. Otherwise, the node tree will be compiled and
   * evaluated. */
  void evaluate();

  /* Invalidate the operations stream that was compiled for the node tree. This should be called
   * when the node tree changes or the structure of any of the resources used by it changes. By
   * structure, we mean things like the dimensions of the used images, while changes to their
   * contents do not necessitate a reset. */
  void reset();

 private:
  /* Check if the compositor node tree is valid by checking if it has:
   * - Cyclic links.
   * - Undefined nodes or sockets.
   * - Unsupported nodes.
   * If the node tree is valid, true is returned. Otherwise, false is returned, and an appropriate
   * error message is set by calling the context's set_info_message method. */
  bool validate_node_tree();

  /* Compile the node tree into an operations stream and evaluate it. */
  void compile_and_evaluate();

  /* Compile the given node into a node operation, map each input to the result of the output
   * linked to it, update the compile state, add the newly created operation to the operations
   * stream, and evaluate the operation. */
  void compile_and_evaluate_node(DNode node, CompileState &compile_state);

  /* Map each input of the node operation to the result of the output linked to it. Unlinked inputs
   * are mapped to the result of a newly created Input Single Value Operation, which is added to
   * the operations stream and evaluated. Since this method might add operations to the operations
   * stream, the actual node operation should only be added to the stream once this method is
   * called. */
  void map_node_operation_inputs_to_their_results(DNode node,
                                                  NodeOperation *operation,
                                                  CompileState &compile_state);

  /* Compile the shader compile unit into a shader operation, map each input of the operation to
   * the result of the output linked to it, update the compile state, add the newly created
   * operation to the operations stream, evaluate the operation, and finally reset the shader
   * compile unit. */
  void compile_and_evaluate_shader_compile_unit(CompileState &compile_state);

  /* Map each input of the shader operation to the result of the output linked to it. */
  void map_shader_operation_inputs_to_their_results(ShaderOperation *operation,
                                                    CompileState &compile_state);
};

}  // namespace blender::realtime_compositor
