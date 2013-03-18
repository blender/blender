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

#include <sstream>
#include <stdio.h>

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
#include "COM_ViewerBaseOperation.h"

extern "C" {
#include "BKE_node.h"
}

void ExecutionSystemHelper::addbNodeTree(ExecutionSystem &system, int nodes_start, bNodeTree *tree, bNodeInstanceKey parent_key)
{
	vector<Node *>& nodes = system.getNodes();
	vector<SocketConnection *>& links = system.getConnections();
	
	/* add all nodes of the tree to the node list */
	bNode *node = (bNode *)tree->nodes.first;
	while (node != NULL) {
		/* XXX TODO replace isActiveGroup by a more accurate check, all visible editors should do this! */
		bool isActiveGroup = true;
		Node *nnode = addNode(nodes, node, isActiveGroup, system.getContext().isFastCalculation());
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

	/* Expand group nodes */
	for (unsigned int i = nodes_start; i < nodes.size(); ++i) {
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

void ExecutionSystemHelper::debugDump(ExecutionSystem *system)
{
	Node *node;
	NodeOperation *operation;
	ExecutionGroup *group;
	SocketConnection *connection;
	int tot, tot2;
	printf("-- BEGIN COMPOSITOR DUMP --\r\n");
	printf("digraph compositorexecution {\r\n");
	tot = system->getNodes().size();
	for (int i = 0; i < tot; i++) {
		node = system->getNodes()[i];
		printf("// NODE: %s\r\n", node->getbNode()->typeinfo->ui_name);
	}
	tot = system->getOperations().size();
	for (int i = 0; i < tot; i++) {
		operation = system->getOperations()[i];
		printf("// OPERATION: %p\r\n", operation);
		printf("\t\"O_%p\"", operation);
		printf(" [shape=record,label=\"{");
		tot2 = operation->getNumberOfInputSockets();
		if (tot2 != 0) {
			printf("{");
			for (int j = 0; j < tot2; j++) {
				InputSocket *socket = operation->getInputSocket(j);
				if (j != 0) {
					printf("|");
				}
				printf("<IN_%p>", socket);
				switch (socket->getDataType()) {
					case COM_DT_VALUE:
						printf("Value");
						break;
					case COM_DT_VECTOR:
						printf("Vector");
						break;
					case COM_DT_COLOR:
						printf("Color");
						break;
				}
			}
			printf("}");
			printf("|");
		}
		if (operation->isViewerOperation()) {
			ViewerBaseOperation *viewer = (ViewerBaseOperation *)operation;
			if (viewer->isActiveViewerOutput()) {
				printf("Active viewer");
			}
			else {
				printf("Viewer");
			}
		}
		else if (operation->isOutputOperation(system->getContext().isRendering())) {
			printf("Output");
		}
		else if (operation->isSetOperation()) {
			printf("Set");
		}
		else if (operation->isReadBufferOperation()) {
			printf("ReadBuffer");
		}
		else if (operation->isWriteBufferOperation()) {
			printf("WriteBuffer");
		}
		else {
			printf("O_%p", operation);
		}
		printf(" (%d,%d)", operation->getWidth(), operation->getHeight());
		tot2 = operation->getNumberOfOutputSockets();
		if (tot2 != 0) {
			printf("|");
			printf("{");
			for (int j = 0; j < tot2; j++) {
				OutputSocket *socket = operation->getOutputSocket(j);
				if (j != 0) {
					printf("|");
				}
				printf("<OUT_%p>", socket);
				switch (socket->getDataType()) {
					case COM_DT_VALUE:
						printf("Value");
						break;
					case COM_DT_VECTOR:
						printf("Vector");
						break;
					case COM_DT_COLOR:
						printf("Color");
						break;
				}
			}
			printf("}");
		}
		printf("}\"]");
		printf("\r\n");
	}
	tot = system->getExecutionGroups().size();
	for (int i = 0; i < tot; i++) {
		group = system->getExecutionGroups()[i];
		printf("// GROUP: %d\r\n", i);
		printf("subgraph {\r\n");
		printf("//  OUTPUTOPERATION: %p\r\n", group->getOutputNodeOperation());
		printf(" O_%p\r\n", group->getOutputNodeOperation());
		printf("}\r\n");
	}
	tot = system->getOperations().size();
	for (int i = 0; i < tot; i++) {
		operation = system->getOperations()[i];
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation *read = (ReadBufferOperation *)operation;
			WriteBufferOperation *write = read->getMemoryProxy()->getWriteBufferOperation();
			printf("\t\"O_%p\" -> \"O_%p\" [style=dotted]\r\n", write, read);
		}
	}
	tot = system->getConnections().size();
	for (int i = 0; i < tot; i++) {
		connection = system->getConnections()[i];
		printf("// CONNECTION: %p.%p -> %p.%p\r\n", connection->getFromNode(), connection->getFromSocket(), connection->getToNode(), connection->getToSocket());
		printf("\t\"O_%p\":\"OUT_%p\" -> \"O_%p\":\"IN_%p\"", connection->getFromNode(), connection->getFromSocket(), connection->getToNode(), connection->getToSocket());
		if (!connection->isValid()) {
			printf(" [color=red]");
		}
		else {
			switch (connection->getFromSocket()->getDataType()) {
				case COM_DT_VALUE:
					printf(" [color=grey]");
					break;
				case COM_DT_VECTOR:
					printf(" [color=blue]");
					break;
				case COM_DT_COLOR:
					printf(" [color=orange]");
					break;
			}
		}
		printf("\r\n");
	}
	printf("}\r\n");
	printf("-- END COMPOSITOR DUMP --\r\n");
}
