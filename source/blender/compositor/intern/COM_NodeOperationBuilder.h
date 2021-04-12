/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2013, Blender Foundation.
 */

#pragma once

#include <map>
#include <set>
#include <vector>

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

class NodeOperationBuilder {
 public:
  class Link {
   private:
    NodeOperationOutput *m_from;
    NodeOperationInput *m_to;

   public:
    Link(NodeOperationOutput *from, NodeOperationInput *to) : m_from(from), m_to(to)
    {
    }

    NodeOperationOutput *from() const
    {
      return m_from;
    }
    NodeOperationInput *to() const
    {
      return m_to;
    }
  };

 private:
  const CompositorContext *m_context;
  NodeGraph m_graph;

  Vector<NodeOperation *> m_operations;
  Vector<Link> m_links;
  Vector<ExecutionGroup *> m_groups;

  /** Maps operation inputs to node inputs */
  Map<NodeOperationInput *, NodeInput *> m_input_map;
  /** Maps node outputs to operation outputs */
  Map<NodeOutput *, NodeOperationOutput *> m_output_map;

  Node *m_current_node;

  /** Operation that will be writing to the viewer image
   *  Only one operation can occupy this place at a time,
   *  to avoid race conditions
   */
  ViewerOperation *m_active_viewer;

 public:
  NodeOperationBuilder(const CompositorContext *context, bNodeTree *b_nodetree);

  const CompositorContext &context() const
  {
    return *m_context;
  }

  void convertToOperations(ExecutionSystem *system);

  void addOperation(NodeOperation *operation);

  /** Map input socket of the current node to an operation socket */
  void mapInputSocket(NodeInput *node_socket, NodeOperationInput *operation_socket);
  /** Map output socket of the current node to an operation socket */
  void mapOutputSocket(NodeOutput *node_socket, NodeOperationOutput *operation_socket);

  void addLink(NodeOperationOutput *from, NodeOperationInput *to);
  void removeInputLink(NodeOperationInput *to);

  /** Add a preview operation for a operation output */
  void addPreview(NodeOperationOutput *output);
  /** Add a preview operation for a node input */
  void addNodeInputPreview(NodeInput *input);

  /** Define a viewer operation as the active output, if possible */
  void registerViewer(ViewerOperation *viewer);
  /** The currently active viewer output operation */
  ViewerOperation *active_viewer() const
  {
    return m_active_viewer;
  }

  const Vector<NodeOperation *> &get_operations() const
  {
    return m_operations;
  }

  const Vector<Link> &get_links() const
  {
    return m_links;
  }

 protected:
  /** Add datatype conversion where needed */
  void add_datatype_conversions();

  /** Construct a constant value operation for every unconnected input */
  void add_operation_input_constants();
  void add_input_constant_value(NodeOperationInput *input, const NodeInput *node_input);

  /** Replace proxy operations with direct links */
  void resolve_proxies();

  /** Calculate resolution for each operation */
  void determineResolutions();

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

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeCompilerImpl")
#endif
};

std::ostream &operator<<(std::ostream &os, const NodeOperationBuilder &builder);
std::ostream &operator<<(std::ostream &os, const NodeOperationBuilder::Link &link);

}  // namespace blender::compositor
