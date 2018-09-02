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

#ifndef __COM_NODECONVERTER_H__
#define __COM_NODECONVERTER_H__

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

class NodeInput;
class NodeOutput;

class NodeOperation;
class NodeOperationInput;
class NodeOperationOutput;
class NodeOperationBuilder;

class ViewerOperation;

/**
 * Interface type for converting a \a Node into \a NodeOperation.
 * This is passed to \a Node::convertToOperation methods and allows them
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
	void addOperation(NodeOperation *operation);

	/**
	 * Map input socket of the node to an operation socket.
	 * Links between nodes will then generate equivalent links between
	 * the mapped operation sockets.
	 *
	 * \note A \a Node input can be mapped to multiple \a NodeOperation inputs.
	 */
	void mapInputSocket(NodeInput *node_socket, NodeOperationInput *operation_socket);
	/**
	 * Map output socket of the node to an operation socket.
	 * Links between nodes will then generate equivalent links between
	 * the mapped operation sockets.
	 *
	 * \note A \a Node output can only be mapped to one \a NodeOperation output.
	 * Any existing operation output mapping will be replaced.
	 */
	void mapOutputSocket(NodeOutput *node_socket, NodeOperationOutput *operation_socket);

	/**
	 * Create a proxy operation for a node input.
	 * This operation will be removed later and replaced
	 * by direct links between the connected operations.
	 */
	NodeOperationOutput *addInputProxy(NodeInput *input, bool use_conversion);
	/**
	 * Create a proxy operation for a node output.
	 * This operation will be removed later and replaced
	 * by direct links between the connected operations.
	 */
	NodeOperationInput *addOutputProxy(NodeOutput *output, bool use_conversion);

	/** Define a constant input value. */
	void addInputValue(NodeOperationInput *input, float value);
	/** Define a constant input color. */
	void addInputColor(NodeOperationInput *input, const float value[4]);
	/** Define a constant input vector. */
	void addInputVector(NodeOperationInput *input, const float value[3]);

	/** Define a constant output value. */
	void addOutputValue(NodeOutput *output, float value);
	/** Define a constant output color. */
	void addOutputColor(NodeOutput *output, const float value[4]);
	/** Define a constant output vector. */
	void addOutputVector(NodeOutput *output, const float value[3]);

	/** Add an explicit link between two operations. */
	void addLink(NodeOperationOutput *from, NodeOperationInput *to);

	/** Add a preview operation for a operation output. */
	void addPreview(NodeOperationOutput *output);
	/** Add a preview operation for a node input. */
	void addNodeInputPreview(NodeInput *input);

	/**
	 * When a node has no valid data
	 * \note missing image / group pointer, or missing renderlayer from EXR
	 */
	NodeOperation *setInvalidOutput(NodeOutput *output);

	/** Define a viewer operation as the active output, if possible */
	void registerViewer(ViewerOperation *viewer);
	/** The currently active viewer output operation */
	ViewerOperation *active_viewer() const;

private:
	/** The internal builder for storing the results of the graph construction. */
	NodeOperationBuilder *m_builder;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeCompiler")
#endif
};

#endif /* __COM_NODECONVERTER_H__ */
