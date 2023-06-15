/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "COM_NodeGraph.h"

namespace blender::compositor {

class CompositorContext;

class Node;
class NodeInput;
class NodeOutput;

class ExecutionSystem;
class ExecutionGroup;
class NodeOperation;
class NodeOperationInput;
class NodeOperationOutput;

class PreviewOperation;
class WriteBufferOperation;
class ViewerOperation;
class ConstantOperation;

class NodeOperationBuilder {
 public:
  class Link {
   private:
    NodeOperationOutput *from_;
    NodeOperationInput *to_;

   public:
    Link(NodeOperationOutput *from, NodeOperationInput *to) : from_(from), to_(to) {}

    NodeOperationOutput *from() const
    {
      return from_;
    }
    NodeOperationInput *to() const
    {
      return to_;
    }
  };

 private:
  const CompositorContext *context_;
  NodeGraph graph_;
  ExecutionSystem *exec_system_;

  Vector<NodeOperation *> operations_;
  Vector<Link> links_;
  Vector<ExecutionGroup *> groups_;

  /** Maps operation inputs to node inputs */
  Map<NodeOperationInput *, NodeInput *> input_map_;
  /** Maps node outputs to operation outputs */
  Map<NodeOutput *, NodeOperationOutput *> output_map_;

  Node *current_node_;

  /**
   * Operation that will be writing to the viewer image
   * Only one operation can occupy this place at a time, to avoid race conditions.
   */
  ViewerOperation *active_viewer_;

 public:
  NodeOperationBuilder(const CompositorContext *context,
                       bNodeTree *b_nodetree,
                       ExecutionSystem *system);

  const CompositorContext &context() const
  {
    return *context_;
  }

  void convert_to_operations(ExecutionSystem *system);

  void add_operation(NodeOperation *operation);
  void replace_operation_with_constant(NodeOperation *operation,
                                       ConstantOperation *constant_operation);

  /** Map input socket of the current node to an operation socket */
  void map_input_socket(NodeInput *node_socket, NodeOperationInput *operation_socket);
  /** Map output socket of the current node to an operation socket */
  void map_output_socket(NodeOutput *node_socket, NodeOperationOutput *operation_socket);

  void add_link(NodeOperationOutput *from, NodeOperationInput *to);
  void remove_input_link(NodeOperationInput *to);

  /** Add a preview operation for a operation output */
  void add_preview(NodeOperationOutput *output);
  /** Add a preview operation for a node input */
  void add_node_input_preview(NodeInput *input);

  /** Define a viewer operation as the active output, if possible */
  void register_viewer(ViewerOperation *viewer);
  /** The currently active viewer output operation */
  ViewerOperation *active_viewer() const
  {
    return active_viewer_;
  }

  const Vector<NodeOperation *> &get_operations() const
  {
    return operations_;
  }

  const Vector<Link> &get_links() const
  {
    return links_;
  }

 protected:
  /** Add datatype conversion where needed */
  void add_datatype_conversions();

  /** Construct a constant value operation for every unconnected input */
  void add_operation_input_constants();
  void add_input_constant_value(NodeOperationInput *input, const NodeInput *node_input);

  /** Replace proxy operations with direct links */
  void resolve_proxies();

  /** Calculate canvas area for each operation. */
  void determine_canvases();

  /** Helper function to store connected inputs for replacement */
  Vector<NodeOperationInput *> cache_output_links(NodeOperationOutput *output) const;
  /** Find a connected write buffer operation to an OpOutput */
  WriteBufferOperation *find_attached_write_buffer_operation(NodeOperationOutput *output) const;
  /** Add read/write buffer operations around complex operations */
  void add_complex_operation_buffers();
  void add_input_buffers(NodeOperation *operation, NodeOperationInput *input);
  void add_output_buffers(NodeOperation *operation, NodeOperationOutput *output);

  /** Remove unreachable operations */
  void prune_operations();

  /** Sort operations by link dependencies */
  void sort_operations();

  /** Create execution groups */
  void group_operations();
  ExecutionGroup *make_group(NodeOperation *op);

 private:
  PreviewOperation *make_preview_operation() const;
  void unlink_inputs_and_relink_outputs(NodeOperation *unlinked_op, NodeOperation *linked_op);
  /** Merge operations with same type, inputs and parameters that produce the same result. */
  void merge_equal_operations();
  void merge_equal_operations(NodeOperation *from, NodeOperation *into);
  void save_graphviz(StringRefNull name = "");
#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeCompilerImpl")
#endif
};

/** Create a graphviz representation of the NodeOperationBuilder. */
std::ostream &operator<<(std::ostream &os, const NodeOperationBuilder &builder);
std::ostream &operator<<(std::ostream &os, const NodeOperationBuilder::Link &link);

}  // namespace blender::compositor
