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

#ifndef __COM_NODEOPERATIONBUILDER_H__
#define __COM_NODEOPERATIONBUILDER_H__

#include <map>
#include <set>
#include <vector>

#include "COM_NodeGraph.h"

using std::vector;

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
		Link(NodeOperationOutput *from, NodeOperationInput *to) :
		    m_from(from),
		    m_to(to)
		{}

		NodeOperationOutput *from() const { return m_from; }
		NodeOperationInput *to() const { return m_to; }
	};

	typedef std::vector<NodeOperation *> Operations;
	typedef std::vector<Link> Links;
	typedef std::vector<ExecutionGroup *> Groups;

	typedef std::map<NodeOperationInput *, NodeInput *> InputSocketMap;
	typedef std::map<NodeOutput *, NodeOperationOutput *> OutputSocketMap;

	typedef std::vector<NodeOperationInput *> OpInputs;
	typedef std::map<NodeInput *, OpInputs> OpInputInverseMap;

private:
	const CompositorContext *m_context;
	NodeGraph m_graph;

	Operations m_operations;
	Links m_links;
	Groups m_groups;

	/** Maps operation inputs to node inputs */
	InputSocketMap m_input_map;
	/** Maps node outputs to operation outputs */
	OutputSocketMap m_output_map;

	Node *m_current_node;

	/** Operation that will be writing to the viewer image
	 *  Only one operation can occupy this place at a time,
	 *  to avoid race conditions
	 */
	ViewerOperation *m_active_viewer;

public:
	NodeOperationBuilder(const CompositorContext *context, bNodeTree *b_nodetree);
	~NodeOperationBuilder();

	const CompositorContext &context() const { return *m_context; }

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
	ViewerOperation *active_viewer() const { return m_active_viewer; }

protected:
	static NodeInput *find_node_input(const InputSocketMap &map, NodeOperationInput *op_input);
	static const OpInputs &find_operation_inputs(const OpInputInverseMap &map, NodeInput *node_input);
	static NodeOperationOutput *find_operation_output(const OutputSocketMap &map, NodeOutput *node_output);

	/** Add datatype conversion where needed */
	void add_datatype_conversions();

	/** Construct a constant value operation for every unconnected input */
	void add_operation_input_constants();
	void add_input_constant_value(NodeOperationInput *input, NodeInput *node_input);

	/** Replace proxy operations with direct links */
	void resolve_proxies();

	/** Calculate resolution for each operation */
	void determineResolutions();

	/** Helper function to store connected inputs for replacement */
	OpInputs cache_output_links(NodeOperationOutput *output) const;
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

#endif /* __COM_NODEOPERATIONBUILDER_H__ */
