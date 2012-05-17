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

GroupNode::GroupNode(bNode *editorNode): Node(editorNode)
{
}

void GroupNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
}

void GroupNode::ungroup(ExecutionSystem &system)
{
	vector<InputSocket*> &inputsockets = this->getInputSockets();
	vector<OutputSocket*> &outputsockets = this->getOutputSockets();
	unsigned int index;

	/* get the node list size _before_ adding proxy nodes, so they are available for linking */
	int nodes_start = system.getNodes().size();

	for (index = 0 ; index < inputsockets.size();index ++) {
		InputSocket * inputSocket = inputsockets[index];
		bNodeSocket *editorInput = inputSocket->getbNodeSocket();
		if (editorInput->groupsock) {
			if (inputSocket->isConnected()) {
				SocketProxyNode * proxy = new SocketProxyNode(this->getbNode(), editorInput, editorInput->groupsock);
				inputSocket->relinkConnections(proxy->getInputSocket(0), true, index, &system);
				ExecutionSystemHelper::addNode(system.getNodes(), proxy);
			}
			else {
				OutputSocketProxyNode * proxy = new OutputSocketProxyNode(this->getbNode(), editorInput, editorInput->groupsock);
				inputSocket->relinkConnections(proxy->getInputSocket(0), true, index, &system);
				ExecutionSystemHelper::addNode(system.getNodes(), proxy);
			}
		}
	}

	for (index = 0 ; index < outputsockets.size();index ++) {
		OutputSocket * outputSocket = outputsockets[index];
		bNodeSocket *editorOutput = outputSocket->getbNodeSocket();
		if (outputSocket->isConnected() && editorOutput->groupsock) {
			SocketProxyNode * proxy = new SocketProxyNode(this->getbNode(), editorOutput->groupsock, editorOutput);
			outputSocket->relinkConnections(proxy->getOutputSocket(0));
			ExecutionSystemHelper::addNode(system.getNodes(), proxy);
		}
	}

	bNodeTree *subtree = (bNodeTree*)this->getbNode()->id;
	ExecutionSystemHelper::addbNodeTree(system, nodes_start, subtree);
}
