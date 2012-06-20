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

#include "COM_GroupNode.h"
#include "COM_SocketProxyNode.h"
#include "COM_ExecutionSystemHelper.h"

GroupNode::GroupNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void GroupNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	/* pass */
}

void GroupNode::ungroup(ExecutionSystem &system)
{
	bNode *bnode = this->getbNode();
	bNodeTree *subtree = (bNodeTree *)bnode->id;
	vector<InputSocket *> &inputsockets = this->getInputSockets();
	vector<OutputSocket *> &outputsockets = this->getOutputSockets();
	unsigned int index;

	/* get the node list size _before_ adding proxy nodes, so they are available for linking */
	int nodes_start = system.getNodes().size();

	/* missing node group datablock can happen with library linking */
	if(!subtree)
		return;

	for (index = 0; index < inputsockets.size(); index++) {
		InputSocket *inputSocket = inputsockets[index];
		bNodeSocket *editorInput = inputSocket->getbNodeSocket();
		if (editorInput->groupsock) {
			SocketProxyNode *proxy = new SocketProxyNode(bnode, editorInput, editorInput->groupsock);
			inputSocket->relinkConnections(proxy->getInputSocket(0), index, &system);
			ExecutionSystemHelper::addNode(system.getNodes(), proxy);
		}
	}

	for (index = 0; index < outputsockets.size(); index++) {
		OutputSocket *outputSocket = outputsockets[index];
		bNodeSocket *editorOutput = outputSocket->getbNodeSocket();
		if (editorOutput->groupsock) {
			SocketProxyNode *proxy = new SocketProxyNode(bnode, editorOutput->groupsock, editorOutput);
			outputSocket->relinkConnections(proxy->getOutputSocket(0));
			ExecutionSystemHelper::addNode(system.getNodes(), proxy);
		}
	}

	ExecutionSystemHelper::addbNodeTree(system, nodes_start, subtree, bnode);
}
