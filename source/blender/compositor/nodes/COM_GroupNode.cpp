/*
 * Copyright 2011, Blender Foundation.
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
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "BKE_node.h"

#include "COM_GroupNode.h"
#include "COM_SocketProxyNode.h"
#include "COM_SetColorOperation.h"
#include "COM_ExecutionSystemHelper.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"

extern "C" {
#include "RNA_access.h"
}

GroupNode::GroupNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void GroupNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	if (this->getbNode()->id == NULL) {
		convertToOperations_invalid(graph, context);
	}
}

static int find_group_input(GroupNode *gnode, const char *identifier, InputSocket **r_sock)
{
	int index;
	for (index = 0; index < gnode->getNumberOfInputSockets(); ++index) {
		InputSocket *sock = gnode->getInputSocket(index);
		if (STREQ(sock->getbNodeSocket()->identifier, identifier)) {
			*r_sock = sock;
			return index;
		}
	}
	*r_sock = NULL;
	return -1;
}

static int find_group_output(GroupNode *gnode, const char *identifier, OutputSocket **r_sock)
{
	int index;
	for (index = 0; index < gnode->getNumberOfOutputSockets(); ++index) {
		OutputSocket *sock = gnode->getOutputSocket(index);
		if (STREQ(sock->getbNodeSocket()->identifier, identifier)) {
			*r_sock = sock;
			return index;
		}
	}
	*r_sock = NULL;
	return -1;
}

void GroupNode::ungroup(ExecutionSystem &system)
{
	bNode *bnode = this->getbNode();
	bNodeTree *subtree = (bNodeTree *)bnode->id;

	/* get the node list size _before_ adding proxy nodes, so they are available for linking */
	int nodes_start = system.getNodes().size();

	/* missing node group datablock can happen with library linking */
	if (!subtree) {
		/* this error case its handled in convertToOperations() so we don't get un-convertred sockets */
		return;
	}

	const bool groupnodeBuffering = system.getContext().isGroupnodeBufferEnabled();

	bool has_output = false;
	/* create proxy nodes for group input/output nodes */
	for (bNode *bionode = (bNode *)subtree->nodes.first; bionode; bionode = bionode->next) {
		if (bionode->type == NODE_GROUP_INPUT) {
			for (bNodeSocket *bsock = (bNodeSocket *)bionode->outputs.first; bsock; bsock = bsock->next) {
				InputSocket *gsock;
				int gsock_index = find_group_input(this, bsock->identifier, &gsock);
				/* ignore virtual sockets */
				if (gsock) {
					SocketProxyNode *proxy = new SocketProxyNode(bionode, gsock->getbNodeSocket(), bsock, false);
					ExecutionSystemHelper::addNode(system.getNodes(), proxy);
					
					gsock->relinkConnectionsDuplicate(proxy->getInputSocket(0), gsock_index, &system);
				}
			}
		}
		
		if (bionode->type == NODE_GROUP_OUTPUT && (bionode->flag & NODE_DO_OUTPUT)) {
			has_output = true;
			for (bNodeSocket *bsock = (bNodeSocket *)bionode->inputs.first; bsock; bsock = bsock->next) {
				OutputSocket *gsock;
				find_group_output(this, bsock->identifier, &gsock);
				/* ignore virtual sockets */
				if (gsock) {
					SocketProxyNode *proxy = new SocketProxyNode(bionode, bsock, gsock->getbNodeSocket(), groupnodeBuffering);
					ExecutionSystemHelper::addNode(system.getNodes(), proxy);
					
					gsock->relinkConnections(proxy->getOutputSocket(0));
				}
			}
		}
	}
	
	/* in case no output node exists, add input value operations using defaults */
	if (!has_output) {
		for (int index = 0; index < getNumberOfOutputSockets(); ++index) {
			OutputSocket *output = getOutputSocket(index);
			addDefaultOutputOperation(system, output);
		}
	}
	
	/* unlink the group node itself, input links have been duplicated */
	for (int index = 0; index < this->getNumberOfInputSockets(); ++index) {
		InputSocket *sock = this->getInputSocket(index);
		sock->unlinkConnections(&system);
	}
	for (int index = 0; index < this->getNumberOfOutputSockets(); ++index) {
		OutputSocket *sock = this->getOutputSocket(index);
		sock->clearConnections();
	}
	
	ExecutionSystemHelper::addbNodeTree(system, nodes_start, subtree, this->getInstanceKey());
}

bNodeSocket *GroupNode::findInterfaceInput(InputSocket *socket)
{
	bNode *bnode = this->getbNode();
	bNodeTree *subtree = (bNodeTree *)bnode->id;
	if (!subtree)
		return NULL;
	
	const char *identifier = socket->getbNodeSocket()->identifier;
	for (bNodeSocket *iosock = (bNodeSocket *)subtree->inputs.first; iosock; iosock = iosock->next)
		if (STREQ(iosock->identifier, identifier))
			return iosock;
	return NULL;
}

bNodeSocket *GroupNode::findInterfaceOutput(OutputSocket *socket)
{
	bNode *bnode = this->getbNode();
	bNodeTree *subtree = (bNodeTree *)bnode->id;
	if (!subtree)
		return NULL;
	
	const char *identifier = socket->getbNodeSocket()->identifier;
	for (bNodeSocket *iosock = (bNodeSocket *)subtree->outputs.first; iosock; iosock = iosock->next)
		if (STREQ(iosock->identifier, identifier))
			return iosock;
	return NULL;
}

void GroupNode::addDefaultOutputOperation(ExecutionSystem &system, OutputSocket *outputsocket)
{
	bNodeSocket *iosock = findInterfaceOutput(outputsocket);
	if (!iosock)
		return;
	
	PointerRNA ptr;
	RNA_pointer_create(&getbNodeTree()->id, &RNA_NodeSocket, iosock, &ptr);
	
	NodeOperation *operation = NULL;
	switch (iosock->typeinfo->type) {
		case SOCK_FLOAT:
		{
			float value = RNA_float_get(&ptr, "default_value");
			SetValueOperation *value_op = new SetValueOperation();
			value_op->setValue(value);
			operation = value_op;
			break;
		}
		case SOCK_VECTOR:
		{
			float vector[3];
			RNA_float_get_array(&ptr, "default_value", vector);
			SetVectorOperation *vector_op = new SetVectorOperation();
			vector_op->setVector(vector);
			operation = vector_op;
			break;
		}
		case SOCK_RGBA:
		{
			float color[4];
			RNA_float_get_array(&ptr, "default_value", color);
			SetColorOperation *color_op = new SetColorOperation();
			color_op->setChannels(color);
			operation = color_op;
			break;
		}
	}
	
	outputsocket->relinkConnections(operation->getOutputSocket());
	system.addOperation(operation);
}
