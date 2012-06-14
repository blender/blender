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
#include "BKE_node.h"
#include "COM_Converter.h"
#include <sstream>
#include "COM_NodeOperation.h"
#include "COM_ExecutionGroup.h"
#include "COM_NodeBase.h"
#include "COM_WorkScheduler.h"
#include "COM_ReadBufferOperation.h"
#include "stdio.h"
#include "COM_GroupNode.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ViewerBaseOperation.h"

Node *ExecutionSystemHelper::addbNodeTree(ExecutionSystem &system, int nodes_start, bNodeTree * tree, bNode *groupnode)
{
	vector<Node*>& nodes = system.getNodes();
	vector<SocketConnection*>& links = system.getConnections();
	Node *mainnode = NULL;
	const bNode * activeGroupNode = system.getContext().getActivegNode();
	bool isActiveGroup = activeGroupNode == groupnode;
	
	/* add all nodes of the tree to the node list */
	bNode *node = (bNode*)tree->nodes.first;
	while (node != NULL) {
		Node *execnode = addNode(nodes, node, isActiveGroup);
		if (node->type == CMP_NODE_COMPOSITE) {
			mainnode = execnode;
		}
		node = (bNode*)node->next;
	}

	NodeRange node_range(nodes.begin()+nodes_start, nodes.end());

	/* add all nodelinks of the tree to the link list */
	bNodeLink *nodelink = (bNodeLink*)tree->links.first;
	while (nodelink != NULL) {
		addNodeLink(node_range, links, nodelink);
		nodelink = (bNodeLink*)nodelink->next;
	}

	/* Expand group nodes */
	for (unsigned int i=nodes_start; i < nodes.size(); ++i) {
		Node *execnode = nodes[i];
		if (execnode->isGroupNode()) {
			GroupNode * groupNode = (GroupNode*)execnode;
			groupNode->ungroup(system);
		}
	}

	return mainnode;
}

void ExecutionSystemHelper::addNode(vector<Node*>& nodes, Node *node)
{
	nodes.push_back(node);
}

Node *ExecutionSystemHelper::addNode(vector<Node*>& nodes, bNode *bNode, bool inActiveGroup)
{
	Converter converter;
	Node * node;
	node = converter.convert(bNode);
	node->setIsInActiveGroup(inActiveGroup);
	if (node != NULL) {
		addNode(nodes, node);
		return node;
	}
	return NULL;
}
void ExecutionSystemHelper::addOperation(vector<NodeOperation*>& operations, NodeOperation *operation)
{
	operations.push_back(operation);
}

void ExecutionSystemHelper::addExecutionGroup(vector<ExecutionGroup*>& executionGroups, ExecutionGroup *executionGroup)
{
	executionGroups.push_back(executionGroup);
}

void ExecutionSystemHelper::findOutputNodeOperations(vector<NodeOperation*>* result, vector<NodeOperation*>& operations, bool rendering)
{
	unsigned int index;

	for (index = 0 ; index < operations.size() ; index ++) {
		NodeOperation *operation = operations[index];
		if (operation->isOutputOperation(rendering)) {
			result->push_back(operation);
		}
	}
}

static InputSocket *find_input(NodeRange &node_range, bNode *bnode, bNodeSocket *bsocket)
{
	if (bnode != NULL) {
		for (NodeIterator it=node_range.first; it!=node_range.second; ++it) {
			Node *node = *it;
			if (node->getbNode() == bnode)
				return node->findInputSocketBybNodeSocket(bsocket);
		}
	}
	else {
		for (NodeIterator it=node_range.first; it!=node_range.second; ++it) {
			Node *node = *it;
			if (node->isProxyNode()) {
				InputSocket *proxySocket = node->getInputSocket(0);
				if (proxySocket->getbNodeSocket()==bsocket)
					return proxySocket;
			}
		}
	}
	return NULL;
}
static OutputSocket *find_output(NodeRange &node_range, bNode *bnode, bNodeSocket *bsocket)
{
	if (bnode != NULL) {
		for (NodeIterator it=node_range.first; it!=node_range.second; ++it) {
			Node *node = *it;
			if (node->getbNode() == bnode)
				return node->findOutputSocketBybNodeSocket(bsocket);
		}
	}
	else {
		for (NodeIterator it=node_range.first; it!=node_range.second; ++it) {
			Node *node = *it;
			if (node->isProxyNode()) {
				OutputSocket *proxySocket = node->getOutputSocket(0);
				if (proxySocket->getbNodeSocket()==bsocket)
					return proxySocket;
			}
		}
	}
	return NULL;
}
SocketConnection *ExecutionSystemHelper::addNodeLink(NodeRange &node_range, vector<SocketConnection*>& links, bNodeLink *bNodeLink)
{
	/// @note: cyclic lines will be ignored. This has been copied from node.c
	if (bNodeLink->tonode != 0 && bNodeLink->fromnode != 0) {
		if (!(bNodeLink->fromnode->level >= bNodeLink->tonode->level && bNodeLink->tonode->level!=0xFFF)) { // only add non cyclic lines! so execution will procede
			return NULL;
		}
	}

	InputSocket *inputSocket = find_input(node_range, bNodeLink->tonode, bNodeLink->tosock);
	OutputSocket *outputSocket = find_output(node_range, bNodeLink->fromnode, bNodeLink->fromsock);
	if (inputSocket == NULL || outputSocket == NULL) {
		return NULL;
	}
	if (inputSocket->isConnected()) {
		return NULL;
	}
	SocketConnection *connection = addLink(links, outputSocket, inputSocket);
	return connection;
}

SocketConnection *ExecutionSystemHelper::addLink(vector<SocketConnection*>& links, OutputSocket *fromSocket, InputSocket *toSocket)
{
	SocketConnection * newconnection = new SocketConnection();
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
	for (int i = 0 ; i < tot ; i ++) {
		node = system->getNodes()[i];
		printf("// NODE: %s\r\n", node->getbNode()->typeinfo->name);
	}
	tot = system->getOperations().size();
	for (int i = 0 ; i < tot ; i ++) {
		operation = system->getOperations()[i];
		printf("// OPERATION: %p\r\n", operation);
		printf("\t\"O_%p\"", operation);
		printf(" [shape=record,label=\"{");
		tot2 = operation->getNumberOfInputSockets();
		if (tot2 != 0) {
			printf("{");
			for (int j = 0 ; j < tot2 ; j ++) {
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
			ViewerBaseOperation * viewer = (ViewerBaseOperation*)operation;
			if (viewer->isActiveViewerOutput()) {
				printf("Active viewer");
			} else {
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
			for (int j = 0 ; j < tot2 ; j ++) {
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
	for (int i = 0 ; i < tot ; i ++) {
		group = system->getExecutionGroups()[i];
		printf("// GROUP: %d\r\n", i);
		printf("subgraph {\r\n");
		printf("//  OUTPUTOPERATION: %p\r\n", group->getOutputNodeOperation());
		printf(" O_%p\r\n", group->getOutputNodeOperation());
		printf("}\r\n");
	}
	tot = system->getOperations().size();
	for (int i = 0 ; i < tot ; i ++) {
		operation = system->getOperations()[i];
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation * read = (ReadBufferOperation*)operation;
			WriteBufferOperation * write = read->getMemoryProxy()->getWriteBufferOperation();
			printf("\t\"O_%p\" -> \"O_%p\" [style=dotted]\r\n", write, read);
		}
	}
	tot = system->getConnections().size();
	for (int i = 0 ; i < tot ; i ++) {
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
