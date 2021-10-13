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

#include <set>

#include "BLI_multi_value_map.hh"

#include "COM_Converter.h"
#include "COM_Debug.h"

#include "COM_ExecutionGroup.h"
#include "COM_PreviewOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_WriteBufferOperation.h"

#include "COM_ConstantFolder.h"
#include "COM_NodeOperationBuilder.h" /* own include */

namespace blender::compositor {

NodeOperationBuilder::NodeOperationBuilder(const CompositorContext *context,
                                           bNodeTree *b_nodetree,
                                           ExecutionSystem *system)
    : context_(context), exec_system_(system), current_node_(nullptr), active_viewer_(nullptr)
{
  graph_.from_bNodeTree(*context, b_nodetree);
}

void NodeOperationBuilder::convertToOperations(ExecutionSystem *system)
{
  /* interface handle for nodes */
  NodeConverter converter(this);

  for (Node *node : graph_.nodes()) {
    current_node_ = node;

    DebugInfo::node_to_operations(node);
    node->convertToOperations(converter, *context_);
  }

  current_node_ = nullptr;

  /* The input map constructed by nodes maps operation inputs to node inputs.
   * Inverting yields a map of node inputs to all connected operation inputs,
   * so multiple operations can use the same node input.
   */
  blender::MultiValueMap<NodeInput *, NodeOperationInput *> inverse_input_map;
  for (Map<NodeOperationInput *, NodeInput *>::MutableItem item : input_map_.items()) {
    inverse_input_map.add(item.value, item.key);
  }

  for (const NodeGraph::Link &link : graph_.links()) {
    NodeOutput *from = link.from;
    NodeInput *to = link.to;

    NodeOperationOutput *op_from = output_map_.lookup_default(from, nullptr);

    const blender::Span<NodeOperationInput *> op_to_list = inverse_input_map.lookup(to);
    if (!op_from || op_to_list.is_empty()) {
      /* XXX allow this? error/debug message? */
      // BLI_assert(false);
      /* XXX NOTE: this can happen with certain nodes (e.g. OutputFile)
       * which only generate operations in certain circumstances (rendering)
       * just let this pass silently for now ...
       */
      continue;
    }

    for (NodeOperationInput *op_to : op_to_list) {
      addLink(op_from, op_to);
    }
  }

  add_operation_input_constants();

  resolve_proxies();

  add_datatype_conversions();

  if (context_->get_execution_model() == eExecutionModel::FullFrame) {
    save_graphviz("compositor_prior_folding");
    ConstantFolder folder(*this);
    folder.fold_operations();
  }

  determine_canvases();

  save_graphviz("compositor_prior_merging");
  merge_equal_operations();

  if (context_->get_execution_model() == eExecutionModel::Tiled) {
    /* surround complex ops with read/write buffer */
    add_complex_operation_buffers();
  }

  /* links not available from here on */
  /* XXX make links_ a local variable to avoid confusion! */
  links_.clear();

  prune_operations();

  /* ensure topological (link-based) order of nodes */
  /*sort_operations();*/ /* not needed yet */

  if (context_->get_execution_model() == eExecutionModel::Tiled) {
    /* create execution groups */
    group_operations();
  }

  /* transfer resulting operations to the system */
  system->set_operations(operations_, groups_);
}

void NodeOperationBuilder::addOperation(NodeOperation *operation)
{
  operation->set_id(operations_.size());
  operations_.append(operation);
  if (current_node_) {
    operation->set_name(current_node_->getbNode()->name);
  }
  operation->set_execution_model(context_->get_execution_model());
  operation->set_execution_system(exec_system_);
}

void NodeOperationBuilder::replace_operation_with_constant(NodeOperation *operation,
                                                           ConstantOperation *constant_operation)
{
  BLI_assert(constant_operation->getNumberOfInputSockets() == 0);
  unlink_inputs_and_relink_outputs(operation, constant_operation);
  addOperation(constant_operation);
}

void NodeOperationBuilder::unlink_inputs_and_relink_outputs(NodeOperation *unlinked_op,
                                                            NodeOperation *linked_op)
{
  int i = 0;
  while (i < links_.size()) {
    Link &link = links_[i];
    if (&link.to()->getOperation() == unlinked_op) {
      link.to()->setLink(nullptr);
      links_.remove(i);
      continue;
    }

    if (&link.from()->getOperation() == unlinked_op) {
      link.to()->setLink(linked_op->getOutputSocket());
      links_[i] = Link(linked_op->getOutputSocket(), link.to());
    }
    i++;
  }
}

void NodeOperationBuilder::mapInputSocket(NodeInput *node_socket,
                                          NodeOperationInput *operation_socket)
{
  BLI_assert(current_node_);
  BLI_assert(node_socket->getNode() == current_node_);

  /* NOTE: this maps operation sockets to node sockets.
   * for resolving links the map will be inverted first in convertToOperations,
   * to get a list of links for each node input socket.
   */
  input_map_.add_new(operation_socket, node_socket);
}

void NodeOperationBuilder::mapOutputSocket(NodeOutput *node_socket,
                                           NodeOperationOutput *operation_socket)
{
  BLI_assert(current_node_);
  BLI_assert(node_socket->getNode() == current_node_);

  output_map_.add_new(node_socket, operation_socket);
}

void NodeOperationBuilder::addLink(NodeOperationOutput *from, NodeOperationInput *to)
{
  if (to->isConnected()) {
    return;
  }

  links_.append(Link(from, to));

  /* register with the input */
  to->setLink(from);
}

void NodeOperationBuilder::removeInputLink(NodeOperationInput *to)
{
  int index = 0;
  for (Link &link : links_) {
    if (link.to() == to) {
      /* unregister with the input */
      to->setLink(nullptr);

      links_.remove(index);
      return;
    }
    index++;
  }
}

PreviewOperation *NodeOperationBuilder::make_preview_operation() const
{
  BLI_assert(current_node_);

  if (!(current_node_->getbNode()->flag & NODE_PREVIEW)) {
    return nullptr;
  }
  /* previews only in the active group */
  if (!current_node_->isInActiveGroup()) {
    return nullptr;
  }
  /* do not calculate previews of hidden nodes */
  if (current_node_->getbNode()->flag & NODE_HIDDEN) {
    return nullptr;
  }

  bNodeInstanceHash *previews = context_->getPreviewHash();
  if (previews) {
    PreviewOperation *operation = new PreviewOperation(context_->getViewSettings(),
                                                       context_->getDisplaySettings(),
                                                       current_node_->getbNode()->preview_xsize,
                                                       current_node_->getbNode()->preview_ysize);
    operation->setbNodeTree(context_->getbNodeTree());
    operation->verifyPreview(previews, current_node_->getInstanceKey());
    return operation;
  }

  return nullptr;
}

void NodeOperationBuilder::addPreview(NodeOperationOutput *output)
{
  PreviewOperation *operation = make_preview_operation();
  if (operation) {
    addOperation(operation);

    addLink(output, operation->getInputSocket(0));
  }
}

void NodeOperationBuilder::addNodeInputPreview(NodeInput *input)
{
  PreviewOperation *operation = make_preview_operation();
  if (operation) {
    addOperation(operation);

    mapInputSocket(input, operation->getInputSocket(0));
  }
}

void NodeOperationBuilder::registerViewer(ViewerOperation *viewer)
{
  if (active_viewer_) {
    if (current_node_->isInActiveGroup()) {
      /* deactivate previous viewer */
      active_viewer_->setActive(false);

      active_viewer_ = viewer;
      viewer->setActive(true);
    }
  }
  else {
    if (current_node_->getbNodeTree() == context_->getbNodeTree()) {
      active_viewer_ = viewer;
      viewer->setActive(true);
    }
  }
}

/****************************
 **** Optimization Steps ****
 ****************************/

void NodeOperationBuilder::add_datatype_conversions()
{
  Vector<Link> convert_links;
  for (const Link &link : links_) {
    /* proxy operations can skip data type conversion */
    NodeOperation *from_op = &link.from()->getOperation();
    NodeOperation *to_op = &link.to()->getOperation();
    if (!(from_op->get_flags().use_datatype_conversion ||
          to_op->get_flags().use_datatype_conversion)) {
      continue;
    }

    if (link.from()->getDataType() != link.to()->getDataType()) {
      convert_links.append(link);
    }
  }
  for (const Link &link : convert_links) {
    NodeOperation *converter = COM_convert_data_type(*link.from(), *link.to());
    if (converter) {
      addOperation(converter);

      removeInputLink(link.to());
      addLink(link.from(), converter->getInputSocket(0));
      addLink(converter->getOutputSocket(0), link.to());
    }
  }
}

void NodeOperationBuilder::add_operation_input_constants()
{
  /* NOTE: unconnected inputs cached first to avoid modifying
   *       operations_ while iterating over it
   */
  Vector<NodeOperationInput *> pending_inputs;
  for (NodeOperation *op : operations_) {
    for (int k = 0; k < op->getNumberOfInputSockets(); ++k) {
      NodeOperationInput *input = op->getInputSocket(k);
      if (!input->isConnected()) {
        pending_inputs.append(input);
      }
    }
  }
  for (NodeOperationInput *input : pending_inputs) {
    add_input_constant_value(input, input_map_.lookup_default(input, nullptr));
  }
}

void NodeOperationBuilder::add_input_constant_value(NodeOperationInput *input,
                                                    const NodeInput *node_input)
{
  switch (input->getDataType()) {
    case DataType::Value: {
      float value;
      if (node_input && node_input->getbNodeSocket()) {
        value = node_input->getEditorValueFloat();
      }
      else {
        value = 0.0f;
      }

      SetValueOperation *op = new SetValueOperation();
      op->setValue(value);
      addOperation(op);
      addLink(op->getOutputSocket(), input);
      break;
    }
    case DataType::Color: {
      float value[4];
      if (node_input && node_input->getbNodeSocket()) {
        node_input->getEditorValueColor(value);
      }
      else {
        zero_v4(value);
      }

      SetColorOperation *op = new SetColorOperation();
      op->setChannels(value);
      addOperation(op);
      addLink(op->getOutputSocket(), input);
      break;
    }
    case DataType::Vector: {
      float value[3];
      if (node_input && node_input->getbNodeSocket()) {
        node_input->getEditorValueVector(value);
      }
      else {
        zero_v3(value);
      }

      SetVectorOperation *op = new SetVectorOperation();
      op->setVector(value);
      addOperation(op);
      addLink(op->getOutputSocket(), input);
      break;
    }
  }
}

void NodeOperationBuilder::resolve_proxies()
{
  Vector<Link> proxy_links;
  for (const Link &link : links_) {
    /* don't replace links from proxy to proxy, since we may need them for replacing others! */
    if (link.from()->getOperation().get_flags().is_proxy_operation &&
        !link.to()->getOperation().get_flags().is_proxy_operation) {
      proxy_links.append(link);
    }
  }

  for (const Link &link : proxy_links) {
    NodeOperationInput *to = link.to();
    NodeOperationOutput *from = link.from();
    do {
      /* walk upstream bypassing the proxy operation */
      from = from->getOperation().getInputSocket(0)->getLink();
    } while (from && from->getOperation().get_flags().is_proxy_operation);

    removeInputLink(to);
    /* we may not have a final proxy input link,
     * in that case it just gets dropped
     */
    if (from) {
      addLink(from, to);
    }
  }
}

void NodeOperationBuilder::determine_canvases()
{
  /* Determine all canvas areas of the operations. */
  const rcti &preferred_area = COM_AREA_NONE;
  for (NodeOperation *op : operations_) {
    if (op->isOutputOperation(context_->isRendering()) && !op->get_flags().is_preview_operation) {
      rcti canvas = COM_AREA_NONE;
      op->determine_canvas(preferred_area, canvas);
      op->set_canvas(canvas);
    }
  }

  for (NodeOperation *op : operations_) {
    if (op->isOutputOperation(context_->isRendering()) && op->get_flags().is_preview_operation) {
      rcti canvas = COM_AREA_NONE;
      op->determine_canvas(preferred_area, canvas);
      op->set_canvas(canvas);
    }
  }

  /* Convert operation canvases when needed. */
  {
    Vector<Link> convert_links;
    for (const Link &link : links_) {
      if (link.to()->getResizeMode() != ResizeMode::None) {
        const rcti &from_canvas = link.from()->getOperation().get_canvas();
        const rcti &to_canvas = link.to()->getOperation().get_canvas();

        bool needs_conversion;
        if (link.to()->getResizeMode() == ResizeMode::Align) {
          needs_conversion = from_canvas.xmin != to_canvas.xmin ||
                             from_canvas.ymin != to_canvas.ymin;
        }
        else {
          needs_conversion = !BLI_rcti_compare(&from_canvas, &to_canvas);
        }

        if (needs_conversion) {
          convert_links.append(link);
        }
      }
    }
    for (const Link &link : convert_links) {
      COM_convert_canvas(*this, link.from(), link.to());
    }
  }
}

static Vector<NodeOperationHash> generate_hashes(Span<NodeOperation *> operations)
{
  Vector<NodeOperationHash> hashes;
  for (NodeOperation *op : operations) {
    std::optional<NodeOperationHash> hash = op->generate_hash();
    if (hash) {
      hashes.append(std::move(*hash));
    }
  }
  return hashes;
}

/** Merge operations with same type, inputs and parameters that produce the same result. */
void NodeOperationBuilder::merge_equal_operations()
{
  bool check_for_next_merge = true;
  while (check_for_next_merge) {
    /* Re-generate hashes with any change. */
    Vector<NodeOperationHash> hashes = generate_hashes(operations_);

    /* Make hashes be consecutive when they are equal. */
    std::sort(hashes.begin(), hashes.end());

    bool any_merged = false;
    const NodeOperationHash *prev_hash = nullptr;
    for (const NodeOperationHash &hash : hashes) {
      if (prev_hash && *prev_hash == hash) {
        merge_equal_operations(prev_hash->get_operation(), hash.get_operation());
        any_merged = true;
      }
      prev_hash = &hash;
    }

    check_for_next_merge = any_merged;
  }
}

void NodeOperationBuilder::merge_equal_operations(NodeOperation *from, NodeOperation *into)
{
  unlink_inputs_and_relink_outputs(from, into);
  operations_.remove_first_occurrence_and_reorder(from);
  delete from;
}

Vector<NodeOperationInput *> NodeOperationBuilder::cache_output_links(
    NodeOperationOutput *output) const
{
  Vector<NodeOperationInput *> inputs;
  for (const Link &link : links_) {
    if (link.from() == output) {
      inputs.append(link.to());
    }
  }
  return inputs;
}

WriteBufferOperation *NodeOperationBuilder::find_attached_write_buffer_operation(
    NodeOperationOutput *output) const
{
  for (const Link &link : links_) {
    if (link.from() == output) {
      NodeOperation &op = link.to()->getOperation();
      if (op.get_flags().is_write_buffer_operation) {
        return (WriteBufferOperation *)(&op);
      }
    }
  }
  return nullptr;
}

void NodeOperationBuilder::add_input_buffers(NodeOperation * /*operation*/,
                                             NodeOperationInput *input)
{
  if (!input->isConnected()) {
    return;
  }

  NodeOperationOutput *output = input->getLink();
  if (output->getOperation().get_flags().is_read_buffer_operation) {
    /* input is already buffered, no need to add another */
    return;
  }

  /* this link will be replaced below */
  removeInputLink(input);

  /* check of other end already has write operation, otherwise add a new one */
  WriteBufferOperation *writeoperation = find_attached_write_buffer_operation(output);
  if (!writeoperation) {
    writeoperation = new WriteBufferOperation(output->getDataType());
    writeoperation->setbNodeTree(context_->getbNodeTree());
    addOperation(writeoperation);

    addLink(output, writeoperation->getInputSocket(0));

    writeoperation->readResolutionFromInputSocket();
  }

  /* add readbuffer op for the input */
  ReadBufferOperation *readoperation = new ReadBufferOperation(output->getDataType());
  readoperation->setMemoryProxy(writeoperation->getMemoryProxy());
  this->addOperation(readoperation);

  addLink(readoperation->getOutputSocket(), input);

  readoperation->readResolutionFromWriteBuffer();
}

void NodeOperationBuilder::add_output_buffers(NodeOperation *operation,
                                              NodeOperationOutput *output)
{
  /* cache connected sockets, so we can safely remove links first before replacing them */
  Vector<NodeOperationInput *> targets = cache_output_links(output);
  if (targets.is_empty()) {
    return;
  }

  WriteBufferOperation *writeOperation = nullptr;
  for (NodeOperationInput *target : targets) {
    /* try to find existing write buffer operation */
    if (target->getOperation().get_flags().is_write_buffer_operation) {
      BLI_assert(writeOperation == nullptr); /* there should only be one write op connected */
      writeOperation = (WriteBufferOperation *)(&target->getOperation());
    }
    else {
      /* remove all links to other nodes */
      removeInputLink(target);
    }
  }

  /* if no write buffer operation exists yet, create a new one */
  if (!writeOperation) {
    writeOperation = new WriteBufferOperation(operation->getOutputSocket()->getDataType());
    writeOperation->setbNodeTree(context_->getbNodeTree());
    addOperation(writeOperation);

    addLink(output, writeOperation->getInputSocket(0));
  }

  writeOperation->readResolutionFromInputSocket();

  /* add readbuffer op for every former connected input */
  for (NodeOperationInput *target : targets) {
    if (&target->getOperation() == writeOperation) {
      continue; /* skip existing write op links */
    }

    ReadBufferOperation *readoperation = new ReadBufferOperation(
        operation->getOutputSocket()->getDataType());
    readoperation->setMemoryProxy(writeOperation->getMemoryProxy());
    addOperation(readoperation);

    addLink(readoperation->getOutputSocket(), target);

    readoperation->readResolutionFromWriteBuffer();
  }
}

void NodeOperationBuilder::add_complex_operation_buffers()
{
  /* NOTE: complex ops and get cached here first, since adding operations
   * will invalidate iterators over the main operations_
   */
  Vector<NodeOperation *> complex_ops;
  for (NodeOperation *operation : operations_) {
    if (operation->get_flags().complex) {
      complex_ops.append(operation);
    }
  }

  for (NodeOperation *op : complex_ops) {
    DebugInfo::operation_read_write_buffer(op);

    for (int index = 0; index < op->getNumberOfInputSockets(); index++) {
      add_input_buffers(op, op->getInputSocket(index));
    }

    for (int index = 0; index < op->getNumberOfOutputSockets(); index++) {
      add_output_buffers(op, op->getOutputSocket(index));
    }
  }
}

using Tags = std::set<NodeOperation *>;

static void find_reachable_operations_recursive(Tags &reachable, NodeOperation *op)
{
  if (reachable.find(op) != reachable.end()) {
    return;
  }
  reachable.insert(op);

  for (int i = 0; i < op->getNumberOfInputSockets(); i++) {
    NodeOperationInput *input = op->getInputSocket(i);
    if (input->isConnected()) {
      find_reachable_operations_recursive(reachable, &input->getLink()->getOperation());
    }
  }

  /* associated write-buffer operations are executed as well */
  if (op->get_flags().is_read_buffer_operation) {
    ReadBufferOperation *read_op = (ReadBufferOperation *)op;
    MemoryProxy *memproxy = read_op->getMemoryProxy();
    find_reachable_operations_recursive(reachable, memproxy->getWriteBufferOperation());
  }
}

void NodeOperationBuilder::prune_operations()
{
  Tags reachable;
  for (NodeOperation *op : operations_) {
    /* output operations are primary executed operations */
    if (op->isOutputOperation(context_->isRendering())) {
      find_reachable_operations_recursive(reachable, op);
    }
  }

  /* delete unreachable operations */
  Vector<NodeOperation *> reachable_ops;
  for (NodeOperation *op : operations_) {
    if (reachable.find(op) != reachable.end()) {
      reachable_ops.append(op);
    }
    else {
      delete op;
    }
  }
  /* finally replace the operations list with the pruned list */
  operations_ = reachable_ops;
}

/* topological (depth-first) sorting of operations */
static void sort_operations_recursive(Vector<NodeOperation *> &sorted,
                                      Tags &visited,
                                      NodeOperation *op)
{
  if (visited.find(op) != visited.end()) {
    return;
  }
  visited.insert(op);

  for (int i = 0; i < op->getNumberOfInputSockets(); i++) {
    NodeOperationInput *input = op->getInputSocket(i);
    if (input->isConnected()) {
      sort_operations_recursive(sorted, visited, &input->getLink()->getOperation());
    }
  }

  sorted.append(op);
}

void NodeOperationBuilder::sort_operations()
{
  Vector<NodeOperation *> sorted;
  sorted.reserve(operations_.size());
  Tags visited;

  for (NodeOperation *operation : operations_) {
    sort_operations_recursive(sorted, visited, operation);
  }

  operations_ = sorted;
}

static void add_group_operations_recursive(Tags &visited, NodeOperation *op, ExecutionGroup *group)
{
  if (visited.find(op) != visited.end()) {
    return;
  }
  visited.insert(op);

  if (!group->addOperation(op)) {
    return;
  }

  /* add all eligible input ops to the group */
  for (int i = 0; i < op->getNumberOfInputSockets(); i++) {
    NodeOperationInput *input = op->getInputSocket(i);
    if (input->isConnected()) {
      add_group_operations_recursive(visited, &input->getLink()->getOperation(), group);
    }
  }
}

ExecutionGroup *NodeOperationBuilder::make_group(NodeOperation *op)
{
  ExecutionGroup *group = new ExecutionGroup(groups_.size());
  groups_.append(group);

  Tags visited;
  add_group_operations_recursive(visited, op, group);

  return group;
}

void NodeOperationBuilder::group_operations()
{
  for (NodeOperation *op : operations_) {
    if (op->isOutputOperation(context_->isRendering())) {
      ExecutionGroup *group = make_group(op);
      group->setOutputExecutionGroup(true);
    }

    /* add new groups for associated memory proxies where needed */
    if (op->get_flags().is_read_buffer_operation) {
      ReadBufferOperation *read_op = (ReadBufferOperation *)op;
      MemoryProxy *memproxy = read_op->getMemoryProxy();

      if (memproxy->getExecutor() == nullptr) {
        ExecutionGroup *group = make_group(memproxy->getWriteBufferOperation());
        memproxy->setExecutor(group);
      }
    }
  }
}

void NodeOperationBuilder::save_graphviz(StringRefNull name)
{
  if (COM_EXPORT_GRAPHVIZ) {
    exec_system_->set_operations(operations_, groups_);
    DebugInfo::graphviz(exec_system_, name);
  }
}

/** Create a graphviz representation of the NodeOperationBuilder. */
std::ostream &operator<<(std::ostream &os, const NodeOperationBuilder &builder)
{
  os << "# Builder start\n";
  os << "digraph  G {\n";
  os << "    rankdir=LR;\n";
  os << "    node [shape=box];\n";
  for (const NodeOperation *operation : builder.get_operations()) {
    os << "    op" << operation->get_id() << " [label=\"" << *operation << "\"];\n";
  }

  os << "\n";
  for (const NodeOperationBuilder::Link &link : builder.get_links()) {
    os << "    op" << link.from()->getOperation().get_id() << " -> op"
       << link.to()->getOperation().get_id() << ";\n";
  }
  for (const NodeOperation *operation : builder.get_operations()) {
    if (operation->get_flags().is_read_buffer_operation) {
      const ReadBufferOperation &read_operation = static_cast<const ReadBufferOperation &>(
          *operation);
      const WriteBufferOperation &write_operation =
          *read_operation.getMemoryProxy()->getWriteBufferOperation();
      os << "    op" << write_operation.get_id() << " -> op" << read_operation.get_id() << ";\n";
    }
  }

  os << "}\n";
  os << "# Builder end\n";
  return os;
}

std::ostream &operator<<(std::ostream &os, const NodeOperationBuilder::Link &link)
{
  os << link.from()->getOperation().get_id() << " -> " << link.to()->getOperation().get_id();
  return os;
}

}  // namespace blender::compositor
