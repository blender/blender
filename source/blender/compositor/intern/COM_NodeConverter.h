/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

class NodeInput;
class NodeOutput;

class NodeOperation;
class NodeOperationInput;
class NodeOperationOutput;
class NodeOperationBuilder;

class ViewerOperation;

/**
 * Interface type for converting a \a Node into \a NodeOperation.
 * This is passed to \a Node::convert_to_operation methods and allows them
 * to register any number of operations, create links between them,
 * and map original node sockets to their inputs or outputs.
 */
class NodeConverter {
 public:
  NodeConverter(NodeOperationBuilder *builder);

  /**
   * Insert a new operation into the operations graph.
   * The operation must be created by the node.
   */
  void add_operation(NodeOperation *operation);

  /**
   * Map input socket of the node to an operation socket.
   * Links between nodes will then generate equivalent links between
   * the mapped operation sockets.
   *
   * \note A \a Node input can be mapped to multiple \a NodeOperation inputs.
   */
  void map_input_socket(NodeInput *node_socket, NodeOperationInput *operation_socket);
  /**
   * Map output socket of the node to an operation socket.
   * Links between nodes will then generate equivalent links between
   * the mapped operation sockets.
   *
   * \note A \a Node output can only be mapped to one \a NodeOperation output.
   * Any existing operation output mapping will be replaced.
   */
  void map_output_socket(NodeOutput *node_socket, NodeOperationOutput *operation_socket);

  /**
   * Create a proxy operation for a node input.
   * This operation will be removed later and replaced
   * by direct links between the connected operations.
   */
  NodeOperationOutput *add_input_proxy(NodeInput *input, bool use_conversion);
  /**
   * Create a proxy operation for a node output.
   * This operation will be removed later and replaced
   * by direct links between the connected operations.
   */
  NodeOperationInput *add_output_proxy(NodeOutput *output, bool use_conversion);

  /** Define a constant input value. */
  void add_input_value(NodeOperationInput *input, float value);
  /** Define a constant input color. */
  void add_input_color(NodeOperationInput *input, const float value[4]);
  /** Define a constant input vector. */
  void add_input_vector(NodeOperationInput *input, const float value[3]);

  /** Define a constant output value. */
  void add_output_value(NodeOutput *output, float value);
  /** Define a constant output color. */
  void add_output_color(NodeOutput *output, const float value[4]);
  /** Define a constant output vector. */
  void add_output_vector(NodeOutput *output, const float value[3]);

  /** Add an explicit link between two operations. */
  void add_link(NodeOperationOutput *from, NodeOperationInput *to);

  /** Add a preview operation for a operation output. */
  void add_preview(NodeOperationOutput *output);
  /** Add a preview operation for a node input. */
  void add_node_input_preview(NodeInput *input);

  /**
   * When a node has no valid data
   * \note missing image / group pointer, or missing render-layer from EXR.
   */
  NodeOperation *set_invalid_output(NodeOutput *output);

  /** Define a viewer operation as the active output, if possible */
  void register_viewer(ViewerOperation *viewer);
  /** The currently active viewer output operation */
  ViewerOperation *active_viewer() const;

 private:
  /** The internal builder for storing the results of the graph construction. */
  NodeOperationBuilder *builder_;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeCompiler")
#endif
};

}  // namespace blender::compositor
