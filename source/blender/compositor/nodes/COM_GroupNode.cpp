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
		if (strcmp(sock->getbNodeSocket()->identifier, identifier)==0) {
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
		if (strcmp(sock->getbNodeSocket()->identifier, identifier)==0) {
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
	
	/* unlink the group node itself, input links have been duplicated */
	for (int index = 0; index < this->getNumberOfInputSockets(); ++index) {
		InputSocket *sock = this->getInputSocket(index);
		sock->unlinkConnections(&system);
	}
	
	ExecutionSystemHelper::addbNodeTree(system, nodes_start, subtree, this->getInstanceKey());
}
