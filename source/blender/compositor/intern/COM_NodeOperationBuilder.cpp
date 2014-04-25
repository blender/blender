/*
 * Copyright 2013, Blender Foundation.
 *
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
 * Contributor: 
 *		Lukas Toenne
 */

extern "C" {
#include "BLI_utildefines.h"
}

#include "COM_NodeConverter.h"
#include "COM_Converter.h"
#include "COM_Debug.h"
#include "COM_ExecutionSystem.h"
#include "COM_Node.h"
#include "COM_SocketProxyNode.h"

#include "COM_NodeOperation.h"
#include "COM_PreviewOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SocketProxyOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WriteBufferOperation.h"

#include "COM_NodeOperationBuilder.h" /* own include */

NodeOperationBuilder::NodeOperationBuilder(const CompositorContext *context, bNodeTree *b_nodetree) :
    m_context(context),
    m_current_node(NULL)
{
	m_graph.from_bNodeTree(*context, b_nodetree);
}

NodeOperationBuilder::~NodeOperationBuilder()
{
}

void NodeOperationBuilder::convertToOperations(ExecutionSystem *system)
{
	/* interface handle for nodes */
	NodeConverter converter(this);
	
	for (int index = 0; index < m_graph.nodes().size(); index++) {
		Node *node = (Node *)m_graph.nodes()[index];
		
		m_current_node = node;
		
		DebugInfo::node_to_operations(node);
		node->convertToOperations(converter, *m_context);
	}
	
	m_current_node = NULL;
	
	/* The input map constructed by nodes maps operation inputs to node inputs.
	 * Inverting yields a map of node inputs to all connected operation inputs,
	 * so multiple operations can use the same node input.
	 */
	OpInputInverseMap inverse_input_map;
	for (InputSocketMap::const_iterator it = m_input_map.begin(); it != m_input_map.end(); ++it)
		inverse_input_map[it->second].push_back(it->first);
	
	for (NodeGraph::Links::const_iterator it = m_graph.links().begin(); it != m_graph.links().end(); ++it) {
		const NodeGraph::Link &link = *it;
		NodeOutput *from = link.getFromSocket();
		NodeInput *to = link.getToSocket();
		
		NodeOperationOutput *op_from = find_operation_output(m_output_map, from);
		const OpInputs &op_to_list = find_operation_inputs(inverse_input_map, to);
		if (!op_from || op_to_list.empty()) {
			/* XXX allow this? error/debug message? */
			//BLI_assert(false);
			/* XXX note: this can happen with certain nodes (e.g. OutputFile)
			 * which only generate operations in certain circumstances (rendering)
			 * just let this pass silently for now ...
			 */
			continue;
		}
		
		for (OpInputs::const_iterator it = op_to_list.begin(); it != op_to_list.end(); ++it) {
			NodeOperationInput *op_to = *it;
			addLink(op_from, op_to);
		}
	}
	
	add_datatype_conversions();
	
	add_operation_input_constants();
	
	resolve_proxies();
	
	determineResolutions();
	
	/* surround complex ops with read/write buffer */
	add_complex_operation_buffers();
	
	/* links not available from here on */
	/* XXX make m_links a local variable to avoid confusion! */
	m_links.clear();
	
	prune_operations();
	
	/* ensure topological (link-based) order of nodes */
	/*sort_operations();*/ /* not needed yet */
	
	/* create execution groups */
	group_operations();
	
	/* transfer resulting operations to the system */
	system->set_operations(m_operations, m_groups);
}

void NodeOperationBuilder::addOperation(NodeOperation *operation)
{
	m_operations.push_back(operation);
}

void NodeOperationBuilder::mapInputSocket(NodeInput *node_socket, NodeOperationInput *operation_socket)
{
	BLI_assert(m_current_node);
	BLI_assert(node_socket->getNode() == m_current_node);
	
	/* note: this maps operation sockets to node sockets.
	 * for resolving links the map will be inverted first in convertToOperations,
	 * to get a list of links for each node input socket.
	 */
	m_input_map[operation_socket] = node_socket;
}

void NodeOperationBuilder::mapOutputSocket(NodeOutput *node_socket, NodeOperationOutput *operation_socket)
{
	BLI_assert(m_current_node);
	BLI_assert(node_socket->getNode() == m_current_node);
	
	m_output_map[node_socket] = operation_socket;
}

void NodeOperationBuilder::addLink(NodeOperationOutput *from, NodeOperationInput *to)
{
	if (to->isConnected())
		return;
	
	m_links.push_back(Link(from, to));
	
	/* register with the input */
	to->setLink(from);
}

void NodeOperationBuilder::removeInputLink(NodeOperationInput *to)
{
	for (Links::iterator it = m_links.begin(); it != m_links.end(); ++it) {
		Link &link = *it;
		if (link.to() == to) {
			/* unregister with the input */
			to->setLink(NULL);
			
			m_links.erase(it);
			return;
		}
	}
}

NodeInput *NodeOperationBuilder::find_node_input(const InputSocketMap &map, NodeOperationInput *op_input)
{
	InputSocketMap::const_iterator it = map.find(op_input);
	return (it != map.end() ? it->second : NULL);
}

const NodeOperationBuilder::OpInputs &NodeOperationBuilder::find_operation_inputs(const OpInputInverseMap &map, NodeInput *node_input)
{
	static const OpInputs empty_list;
	OpInputInverseMap::const_iterator it = map.find(node_input);
	return (it != map.end() ? it->second : empty_list);
}

NodeOperationOutput *NodeOperationBuilder::find_operation_output(const OutputSocketMap &map, NodeOutput *node_output)
{
	OutputSocketMap::const_iterator it = map.find(node_output);
	return (it != map.end() ? it->second : NULL);
}

PreviewOperation *NodeOperationBuilder::make_preview_operation() const
{
	BLI_assert(m_current_node);
	
	if (!(m_current_node->getbNode()->flag & NODE_PREVIEW))
		return NULL;
	/* previews only in the active group */
	if (!m_current_node->isInActiveGroup())
		return NULL;
	/* do not calculate previews of hidden nodes */
	if (m_current_node->getbNode()->flag & NODE_HIDDEN)
		return NULL;
	
	bNodeInstanceHash *previews = m_context->getPreviewHash();
	if (previews) {
		PreviewOperation *operation = new PreviewOperation(m_context->getViewSettings(), m_context->getDisplaySettings());
		operation->setbNodeTree(m_context->getbNodeTree());
		operation->verifyPreview(previews, m_current_node->getInstanceKey());
		return operation;
	}
	
	return NULL;
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

/****************************
 **** Optimization Steps ****
 ****************************/

void NodeOperationBuilder::add_datatype_conversions()
{
	Links convert_links;
	for (Links::const_iterator it = m_links.begin(); it != m_links.end(); ++it) {
		const Link &link = *it;
		if (link.from()->getDataType() != link.to()->getDataType())
			convert_links.push_back(link);
	}
	for (Links::const_iterator it = convert_links.begin(); it != convert_links.end(); ++it) {
		const Link &link = *it;
		NodeOperation *converter = Converter::convertDataType(link.from(), link.to());
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
	/* Note: unconnected inputs cached first to avoid modifying
	 *       m_operations while iterating over it
	 */
	typedef std::vector<NodeOperationInput*> Inputs;
	Inputs pending_inputs;
	for (Operations::const_iterator it = m_operations.begin(); it != m_operations.end(); ++it) {
		NodeOperation *op = *it;
		for (int k = 0; k < op->getNumberOfInputSockets(); ++k) {
			NodeOperationInput *input = op->getInputSocket(k);
			if (!input->isConnected())
				pending_inputs.push_back(input);
		}
	}
	for (Inputs::const_iterator it = pending_inputs.begin(); it != pending_inputs.end(); ++it) {
		NodeOperationInput *input = *it;
		add_input_constant_value(input, find_node_input(m_input_map, input));
	}
}

void NodeOperationBuilder::add_input_constant_value(NodeOperationInput *input, NodeInput *node_input)
{
	switch (input->getDataType()) {
		case COM_DT_VALUE: {
			float value;
			if (node_input && node_input->getbNodeSocket())
				value = node_input->getEditorValueFloat();
			else
				value = 0.0f;
			
			SetValueOperation *op = new SetValueOperation();
			op->setValue(value);
			addOperation(op);
			addLink(op->getOutputSocket(), input);
			break;
		}
		case COM_DT_COLOR: {
			float value[4];
			if (node_input && node_input->getbNodeSocket())
				node_input->getEditorValueColor(value);
			else
				zero_v4(value);
			
			SetColorOperation *op = new SetColorOperation();
			op->setChannels(value);
			addOperation(op);
			addLink(op->getOutputSocket(), input);
			break;
		}
		case COM_DT_VECTOR: {
			float value[3];
			if (node_input && node_input->getbNodeSocket())
				node_input->getEditorValueVector(value);
			else
				zero_v3(value);
			
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
	Links proxy_links;
	for (Links::const_iterator it = m_links.begin(); it != m_links.end(); ++it) {
		const Link &link = *it;
		/* don't replace links from proxy to proxy, since we may need them for replacing others! */
		if (link.from()->getOperation().isProxyOperation() &&
		    !link.to()->getOperation().isProxyOperation())
		{
			proxy_links.push_back(link);
		}
	}
	
	for (Links::const_iterator it = proxy_links.begin(); it != proxy_links.end(); ++it) {
		const Link &link = *it;
		
		NodeOperationInput *to = link.to();
		NodeOperationOutput *from = link.from();
		do {
			/* walk upstream bypassing the proxy operation */
			from = from->getOperation().getInputSocket(0)->getLink();
		} while (from && from->getOperation().isProxyOperation());
		
		removeInputLink(to);
		/* we may not have a final proxy input link,
		 * in that case it just gets dropped
		 */
		if (from)
			addLink(from, to);
	}
}

void NodeOperationBuilder::determineResolutions()
{
	/* determine all resolutions of the operations (Width/Height) */
	for (Operations::const_iterator it = m_operations.begin(); it != m_operations.end(); ++it) {
		NodeOperation *op = *it;
		
		if (op->isOutputOperation(m_context->isRendering()) && !op->isPreviewOperation()) {
			unsigned int resolution[2] = {0, 0};
			unsigned int preferredResolution[2] = {0, 0};
			op->determineResolution(resolution, preferredResolution);
			op->setResolution(resolution);
		}
	}
	
	for (Operations::const_iterator it = m_operations.begin(); it != m_operations.end(); ++it) {
		NodeOperation *op = *it;
		
		if (op->isOutputOperation(m_context->isRendering()) && op->isPreviewOperation()) {
			unsigned int resolution[2] = {0, 0};
			unsigned int preferredResolution[2] = {0, 0};
			op->determineResolution(resolution, preferredResolution);
			op->setResolution(resolution);
		}
	}
	
	/* add convert resolution operations when needed */
	{
		Links convert_links;
		for (Links::const_iterator it = m_links.begin(); it != m_links.end(); ++it) {
			const Link &link = *it;
			
			if (link.to()->getResizeMode() != COM_SC_NO_RESIZE) {
				NodeOperation &from_op = link.from()->getOperation();
				NodeOperation &to_op = link.to()->getOperation();
				if (from_op.getWidth() != to_op.getWidth() || from_op.getHeight() != to_op.getHeight())
					convert_links.push_back(link);
			}
		}
		for (Links::const_iterator it = convert_links.begin(); it != convert_links.end(); ++it) {
			const Link &link = *it;
			Converter::convertResolution(*this, link.from(), link.to());
		}
	}
}

NodeOperationBuilder::OpInputs NodeOperationBuilder::cache_output_links(NodeOperationOutput *output) const
{
	OpInputs inputs;
	for (Links::const_iterator it = m_links.begin(); it != m_links.end(); ++it) {
		const Link &link = *it;
		if (link.from() == output)
			inputs.push_back(link.to());
	}
	return inputs;
}

WriteBufferOperation *NodeOperationBuilder::find_attached_write_buffer_operation(NodeOperationOutput *output) const
{
	for (Links::const_iterator it = m_links.begin(); it != m_links.end(); ++it) {
		const Link &link = *it;
		if (link.from() == output) {
			NodeOperation &op = link.to()->getOperation();
			if (op.isWriteBufferOperation())
				return (WriteBufferOperation *)(&op);
		}
	}
	return NULL;
}

void NodeOperationBuilder::add_input_buffers(NodeOperation *operation, NodeOperationInput *input)
{
	if (!input->isConnected())
		return;
	
	NodeOperationOutput *output = input->getLink();
	if (output->getOperation().isReadBufferOperation()) {
		/* input is already buffered, no need to add another */
		return;
	}
	
	/* this link will be replaced below */
	removeInputLink(input);
	
	/* check of other end already has write operation, otherwise add a new one */
	WriteBufferOperation *writeoperation = find_attached_write_buffer_operation(output);
	if (!writeoperation) {
		writeoperation = new WriteBufferOperation();
		writeoperation->setbNodeTree(m_context->getbNodeTree());
		addOperation(writeoperation);
		
		addLink(output, writeoperation->getInputSocket(0));
		
		writeoperation->readResolutionFromInputSocket();
	}
	
	/* add readbuffer op for the input */
	ReadBufferOperation *readoperation = new ReadBufferOperation();
	readoperation->setMemoryProxy(writeoperation->getMemoryProxy());
	this->addOperation(readoperation);
	
	addLink(readoperation->getOutputSocket(), input);
	
	readoperation->readResolutionFromWriteBuffer();
}

void NodeOperationBuilder::add_output_buffers(NodeOperation *operation, NodeOperationOutput *output)
{
	/* cache connected sockets, so we can safely remove links first before replacing them */
	OpInputs targets = cache_output_links(output);
	if (targets.empty())
		return;
	
	WriteBufferOperation *writeOperation = NULL;
	for (OpInputs::const_iterator it = targets.begin(); it != targets.end(); ++it) {
		NodeOperationInput *target = *it;
		
		/* try to find existing write buffer operation */
		if (target->getOperation().isWriteBufferOperation()) {
			BLI_assert(writeOperation == NULL); /* there should only be one write op connected */
			writeOperation = (WriteBufferOperation *)(&target->getOperation());
		}
		else {
			/* remove all links to other nodes */
			removeInputLink(target);
		}
	}
	
	/* if no write buffer operation exists yet, create a new one */
	if (!writeOperation) {
		writeOperation = new WriteBufferOperation();
		writeOperation->setbNodeTree(m_context->getbNodeTree());
		addOperation(writeOperation);
		
		addLink(output, writeOperation->getInputSocket(0));
	}
	
	writeOperation->readResolutionFromInputSocket();
	
	/* add readbuffer op for every former connected input */
	for (OpInputs::const_iterator it = targets.begin(); it != targets.end(); ++it) {
		NodeOperationInput *target = *it;
		if (&target->getOperation() == writeOperation)
			continue; /* skip existing write op links */
		
		ReadBufferOperation *readoperation = new ReadBufferOperation();
		readoperation->setMemoryProxy(writeOperation->getMemoryProxy());
		addOperation(readoperation);
		
		addLink(readoperation->getOutputSocket(), target);
	
		readoperation->readResolutionFromWriteBuffer();
	}
}

void NodeOperationBuilder::add_complex_operation_buffers()
{
	/* note: complex ops and get cached here first, since adding operations
	 * will invalidate iterators over the main m_operations
	 */
	Operations complex_ops;
	for (Operations::const_iterator it = m_operations.begin(); it != m_operations.end(); ++it)
		if ((*it)->isComplex())
			complex_ops.push_back(*it);
	
	for (Operations::const_iterator it = complex_ops.begin(); it != complex_ops.end(); ++it) {
		NodeOperation *op = *it;
		
		DebugInfo::operation_read_write_buffer(op);
		
		for (int index = 0; index < op->getNumberOfInputSockets(); index++)
			add_input_buffers(op, op->getInputSocket(index));
		
		for (int index = 0; index < op->getNumberOfOutputSockets(); index++)
			add_output_buffers(op, op->getOutputSocket(index));
	}
}

typedef std::set<NodeOperation*> Tags;

static void find_reachable_operations_recursive(Tags &reachable, NodeOperation *op)
{
	if (reachable.find(op) != reachable.end())
		return;
	reachable.insert(op);
	
	for (int i = 0; i < op->getNumberOfInputSockets(); ++i) {
		NodeOperationInput *input = op->getInputSocket(i);
		if (input->isConnected())
			find_reachable_operations_recursive(reachable, &input->getLink()->getOperation());
	}
	
	/* associated write-buffer operations are executed as well */
	if (op->isReadBufferOperation()) {
		ReadBufferOperation *read_op = (ReadBufferOperation *)op;
		MemoryProxy *memproxy = read_op->getMemoryProxy();
		find_reachable_operations_recursive(reachable, memproxy->getWriteBufferOperation());
	}
}

void NodeOperationBuilder::prune_operations()
{
	Tags reachable;
	for (Operations::const_iterator it = m_operations.begin(); it != m_operations.end(); ++it) {
		NodeOperation *op = *it;
		
		/* output operations are primary executed operations */
		if (op->isOutputOperation(m_context->isRendering()))
			find_reachable_operations_recursive(reachable, op);
	}
	
	/* delete unreachable operations */
	Operations reachable_ops;
	for (Operations::const_iterator it = m_operations.begin(); it != m_operations.end(); ++it) {
		NodeOperation *op = *it;
		
		if (reachable.find(op) != reachable.end())
			reachable_ops.push_back(op);
		else
			delete op;
	}
	/* finally replace the operations list with the pruned list */
	m_operations = reachable_ops;
}

/* topological (depth-first) sorting of operations */
static void sort_operations_recursive(NodeOperationBuilder::Operations &sorted, Tags &visited, NodeOperation *op)
{
	if (visited.find(op) != visited.end())
		return;
	visited.insert(op);
	
	for (int i = 0; i < op->getNumberOfInputSockets(); ++i) {
		NodeOperationInput *input = op->getInputSocket(i);
		if (input->isConnected())
			sort_operations_recursive(sorted, visited, &input->getLink()->getOperation());
	}
	
	sorted.push_back(op);
}

void NodeOperationBuilder::sort_operations()
{
	Operations sorted;
	sorted.reserve(m_operations.size());
	Tags visited;
	
	for (Operations::const_iterator it = m_operations.begin(); it != m_operations.end(); ++it)
		sort_operations_recursive(sorted, visited, *it);
	
	m_operations = sorted;
}

static void add_group_operations_recursive(Tags &visited, NodeOperation *op, ExecutionGroup *group)
{
	if (visited.find(op) != visited.end())
		return;
	visited.insert(op);
	
	if (!group->addOperation(op))
		return;
	
	/* add all eligible input ops to the group */
	for (int i = 0; i < op->getNumberOfInputSockets(); ++i) {
		NodeOperationInput *input = op->getInputSocket(i);
		if (input->isConnected())
			add_group_operations_recursive(visited, &input->getLink()->getOperation(), group);
	}
}

ExecutionGroup *NodeOperationBuilder::make_group(NodeOperation *op)
{
	ExecutionGroup *group = new ExecutionGroup();
	m_groups.push_back(group);
	
	Tags visited;
	add_group_operations_recursive(visited, op, group);
	
	return group;
}

void NodeOperationBuilder::group_operations()
{
	for (Operations::const_iterator it = m_operations.begin(); it != m_operations.end(); ++it) {
		NodeOperation *op = *it;
		
		if (op->isOutputOperation(m_context->isRendering())) {
			ExecutionGroup *group = make_group(op);
			group->setOutputExecutionGroup(true);
		}
		
		/* add new groups for associated memory proxies where needed */
		if (op->isReadBufferOperation()) {
			ReadBufferOperation *read_op = (ReadBufferOperation *)op;
			MemoryProxy *memproxy = read_op->getMemoryProxy();
			
			if (memproxy->getExecutor() == NULL) {
				ExecutionGroup *group = make_group(memproxy->getWriteBufferOperation());
				memproxy->setExecutor(group);
			}
		}
	}
}
