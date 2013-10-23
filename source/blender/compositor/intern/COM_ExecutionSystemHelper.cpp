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

#include "COM_ExecutionSystemHelper.h"

#include "PIL_time.h"

#include "COM_Converter.h"
#include "COM_NodeOperation.h"
#include "COM_ExecutionGroup.h"
#include "COM_NodeBase.h"
#include "COM_WorkScheduler.h"
#include "COM_ReadBufferOperation.h"
#include "COM_GroupNode.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_Debug.h"

extern "C" {
#include "BKE_node.h"
}

void ExecutionSystemHelper::addbNodeTree(ExecutionSystem &system, int nodes_start, bNodeTree *tree, bNodeInstanceKey parent_key)
{
	vector<Node *>& nodes = system.getNodes();
	vector<SocketConnection *>& links = system.getConnections();
	
	const bNodeTree *basetree = system.getContext().getbNodeTree();
	/* update viewers in the active edittree as well the base tree (for backdrop) */
	bool is_active_group = ((parent_key.value == basetree->active_viewer_key.value) ||
	                        (tree == basetree));
	
	/* add all nodes of the tree to the node list */
	bNode *node = (bNode *)tree->nodes.first;
	while (node != NULL) {
		Node *nnode = addNode(nodes, node, is_active_group, system.getContext().isFastCalculation());
		if (nnode) {
			nnode->setbNodeTree(tree);
			nnode->setInstanceKey(BKE_node_instance_key(parent_key, tree, node));
		}
		node = node->next;
	}

	NodeRange node_range(nodes.begin() + nodes_start, nodes.end());

	/* add all nodelinks of the tree to the link list */
	bNodeLink *nodelink = (bNodeLink *)tree->links.first;
	while (nodelink != NULL) {
		addNodeLink(node_range, links, nodelink);
		nodelink = nodelink->next;
	}

	/* Expand group nodes
	 * Only go up to nodes_end, to avoid ungrouping nested node groups repeatedly.
	 */
	int nodes_end = nodes.size();
	for (unsigned int i = nodes_start; i < nodes_end; ++i) {
		Node *execnode = nodes[i];
		if (execnode->isGroupNode()) {
			GroupNode *groupNode = (GroupNode *)execnode;
			groupNode->ungroup(system);
		}
	}
}

void ExecutionSystemHelper::addNode(vector<Node *>& nodes, Node *node)
{
	nodes.push_back(node);
}

Node *ExecutionSystemHelper::addNode(vector<Node *>& nodes, bNode *b_node, bool inActiveGroup, bool fast)
{
	Node *node = Converter::convert(b_node, fast);
	if (node) {
		node->setIsInActiveGroup(inActiveGroup);
		addNode(nodes, node);
		
		DebugInfo::node_added(node);
	}
	return node;
}
void ExecutionSystemHelper::addOperation(vector<NodeOperation *>& operations, NodeOperation *operation)
{
	operations.push_back(operation);
}

void ExecutionSystemHelper::addExecutionGroup(vector<ExecutionGroup *>& executionGroups, ExecutionGroup *executionGroup)
{
	executionGroups.push_back(executionGroup);
}

void ExecutionSystemHelper::findOutputNodeOperations(vector<NodeOperation *> *result, vector<NodeOperation *>& operations, bool rendering)
{
	unsigned int index;

	for (index = 0; index < operations.size(); index++) {
		NodeOperation *operation = operations[index];
		if (operation->isOutputOperation(rendering)) {
			result->push_back(operation);
		}
	}
}

static InputSocket *find_input(NodeRange &node_range, bNode *bnode, bNodeSocket *bsocket)
{
	for (NodeIterator it = node_range.first; it != node_range.second; ++it) {
		Node *node = *it;
		InputSocket *input = node->findInputSocketBybNodeSocket(bsocket);
		if (input)
			return input;
	}
	return NULL;
}
static OutputSocket *find_output(NodeRange &node_range, bNode *bnode, bNodeSocket *bsocket)
{
	for (NodeIterator it = node_range.first; it != node_range.second; ++it) {
		Node *node = *it;
		OutputSocket *output = node->findOutputSocketBybNodeSocket(bsocket);
		if (output)
			return output;
	}
	return NULL;
}
SocketConnection *ExecutionSystemHelper::addNodeLink(NodeRange &node_range, vector<SocketConnection *>& links, bNodeLink *b_nodelink)
{
	/// @note: ignore invalid links
	if (!(b_nodelink->flag & NODE_LINK_VALID))
		return NULL;

	InputSocket *inputSocket = find_input(node_range, b_nodelink->tonode, b_nodelink->tosock);
	OutputSocket *outputSocket = find_output(node_range, b_nodelink->fromnode, b_nodelink->fromsock);
	if (inputSocket == NULL || outputSocket == NULL) {
		return NULL;
	}
	if (inputSocket->isConnected()) {
		return NULL;
	}
	SocketConnection *connection = addLink(links, outputSocket, inputSocket);
	return connection;
}

SocketConnection *ExecutionSystemHelper::addLink(vector<SocketConnection *>& links, OutputSocket *fromSocket, InputSocket *toSocket)
{
	SocketConnection *newconnection = new SocketConnection();
	newconnection->setFromSocket(fromSocket);
	newconnection->setToSocket(toSocket);
	fromSocket->addConnection(newconnection);
	toSocket->setConnection(newconnection);
	links.push_back(newconnection);
	return newconnection;
}
