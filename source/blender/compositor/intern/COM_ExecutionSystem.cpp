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

#include "COM_ExecutionSystem.h"

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
#include "COM_ExecutionSystemHelper.h"

#include "BKE_global.h"

ExecutionSystem::ExecutionSystem(bNodeTree *editingtree, bool rendering)
{
	context.setbNodeTree(editingtree);
	bNode* gnode;
	for (gnode = (bNode*)editingtree->nodes.first ; gnode ; gnode = (bNode*)gnode->next) {
		if (gnode->type == NODE_GROUP && gnode->typeinfo->group_edit_get(gnode)) {
			context.setActivegNode(gnode);
			break;
		}
	}

	/* initialize the CompositorContext */
	if (rendering) {
		context.setQuality((CompositorQuality)editingtree->render_quality);
	}
	else {
		context.setQuality((CompositorQuality)editingtree->edit_quality);
	}
	context.setRendering(rendering);
	context.setHasActiveOpenCLDevices(WorkScheduler::hasGPUDevices() && (editingtree->flag & NTREE_COM_OPENCL));

	Node *mainOutputNode=NULL;

	mainOutputNode = ExecutionSystemHelper::addbNodeTree(*this, 0, editingtree, NULL);

	if (mainOutputNode) {
		context.setScene((Scene*)mainOutputNode->getbNode()->id);
		this->convertToOperations();
		this->groupOperations(); /* group operations in ExecutionGroups */
		unsigned int index;
		unsigned int resolution[2];
		for (index = 0 ; index < this->groups.size(); index ++) {
			resolution[0]=0;
			resolution[1]=0;
			ExecutionGroup *executionGroup = groups[index];
			executionGroup->determineResolution(resolution);
		}
	}

#ifdef COM_DEBUG
	ExecutionSystemHelper::debugDump(this);
#endif
}

ExecutionSystem::~ExecutionSystem()
{
	unsigned int index;
	for (index = 0; index < this->connections.size(); index++) {
		SocketConnection *connection = this->connections[index];
		delete connection;
	}
	this->connections.clear();
	for (index = 0; index < this->nodes.size(); index++) {
		Node *node = this->nodes[index];
		delete node;
	}
	this->nodes.clear();
	for (index = 0; index < this->operations.size(); index++) {
		NodeOperation *operation = this->operations[index];
		delete operation;
	}
	this->operations.clear();
	for (index = 0; index < this->groups.size(); index++) {
		ExecutionGroup *group = this->groups[index];
		delete group;
	}
	this->groups.clear();
}

void ExecutionSystem::execute()
{
	unsigned int order = 0;
	for (vector<NodeOperation*>::iterator iter = this->operations.begin(); iter != operations.end(); ++iter) {
		NodeBase *node = *iter;
		NodeOperation *operation = (NodeOperation*) node;
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation * readOperation = (ReadBufferOperation*)operation;
			readOperation->setOffset(order);
			order ++;
		}
	}
	unsigned int index;

	for (index = 0 ; index < this->operations.size() ; index ++) {
		NodeOperation * operation = this->operations[index];
		operation->initExecution();
	}
	for (index = 0 ; index < this->groups.size() ; index ++) {
		ExecutionGroup * executionGroup = this->groups[index];
		executionGroup->setChunksize(context.getChunksize());
		executionGroup->initExecution();
	}

	WorkScheduler::start(this->context);

	executeGroups(COM_PRIORITY_HIGH);
	executeGroups(COM_PRIORITY_MEDIUM);
	executeGroups(COM_PRIORITY_LOW);

	WorkScheduler::finish();
	WorkScheduler::stop();

	for (index = 0 ; index < this->operations.size() ; index ++) {
		NodeOperation * operation = this->operations[index];
		operation->deinitExecution();
	}
	for (index = 0 ; index < this->groups.size() ; index ++) {
		ExecutionGroup * executionGroup = this->groups[index];
		executionGroup->deinitExecution();
	}
}

void ExecutionSystem::executeGroups(CompositorPriority priority)
{
	int index;
	vector<ExecutionGroup*> executionGroups;
	this->findOutputExecutionGroup(&executionGroups, priority);

	for (index = 0 ; index < executionGroups.size(); index ++) {
		ExecutionGroup *group = executionGroups[index];
		group->execute(this);
	}
}

void ExecutionSystem::addOperation(NodeOperation *operation)
{
	ExecutionSystemHelper::addOperation(this->operations, operation);
}

void ExecutionSystem::addReadWriteBufferOperations(NodeOperation *operation)
{
	// for every input add write and read operation if input is not a read operation
	// only add read operation to other links when they are attached to buffered operations.
	unsigned int index;
	for (index = 0 ; index < operation->getNumberOfInputSockets();index++) {
		InputSocket *inputsocket = operation->getInputSocket(index);
		if (inputsocket->isConnected()) {
			SocketConnection *connection = inputsocket->getConnection();
			NodeOperation *otherEnd = (NodeOperation*)connection->getFromNode();
			if (!otherEnd->isReadBufferOperation()) {
				// check of other end already has write operation
				OutputSocket *fromsocket = connection->getFromSocket();
				WriteBufferOperation * writeoperation = fromsocket->findAttachedWriteBufferOperation();
				if (writeoperation == NULL) {
					writeoperation = new WriteBufferOperation();
					writeoperation->setbNodeTree(this->getContext().getbNodeTree());
					this->addOperation(writeoperation);
					ExecutionSystemHelper::addLink(this->getConnections(), fromsocket, writeoperation->getInputSocket(0));
					writeoperation->readResolutionFromInputSocket();
				}
				ReadBufferOperation *readoperation = new ReadBufferOperation();
				readoperation->setMemoryProxy(writeoperation->getMemoryProxy());
				connection->setFromSocket(readoperation->getOutputSocket());
				readoperation->getOutputSocket()->addConnection(connection);
				readoperation->readResolutionFromWriteBuffer();
				this->addOperation(readoperation);
			}
		}
	}
	/*
		link the outputsocket to a write operation
		link the writeoperation to a read operation
		link the read operation to the next node.
	*/
	OutputSocket * outputsocket = operation->getOutputSocket();
	if (outputsocket->isConnected()) {
		int index;
		WriteBufferOperation *writeOperation;
		writeOperation = new WriteBufferOperation();
		writeOperation->setbNodeTree(this->getContext().getbNodeTree());
		this->addOperation(writeOperation);
		for (index = 0 ; index < outputsocket->getNumberOfConnections();index ++) {
			SocketConnection * connection = outputsocket->getConnection(index);
			ReadBufferOperation *readoperation = new ReadBufferOperation();
			readoperation->setMemoryProxy(writeOperation->getMemoryProxy());
			connection->setFromSocket(readoperation->getOutputSocket());
			readoperation->getOutputSocket()->addConnection(connection);
			readoperation->readResolutionFromWriteBuffer();
			this->addOperation(readoperation);
		}
		ExecutionSystemHelper::addLink(this->getConnections(), outputsocket, writeOperation->getInputSocket(0));
		writeOperation->readResolutionFromInputSocket();
	}
}

void ExecutionSystem::convertToOperations()
{
	unsigned int index;
	// first determine data types of the nodes, this can be used by the node to convert to a different operation system
	this->determineActualSocketDataTypes((vector<NodeBase*>&)this->nodes);
	for (index = 0; index < this->nodes.size(); index++) {
		Node *node = (Node*)this->nodes[index];
		node->convertToOperations(this, &this->context);
	}

	// update the socket types of the operations. this will be used to add conversion operations in the system
	this->determineActualSocketDataTypes((vector<NodeBase*>&)this->operations);
	for (index = 0 ; index < this->connections.size(); index ++) {
		SocketConnection *connection = this->connections[index];
		if (connection->isValid()) {
			if (connection->getFromSocket()->getActualDataType() != connection->getToSocket()->getActualDataType()) {
				Converter::convertDataType(connection, this);
			}
		}
	}

	// determine all resolutions of the operations (Width/Height)
	for (index = 0 ; index < this->operations.size(); index ++) {
		NodeOperation *operation = this->operations[index];
		if (operation->isOutputOperation(context.isRendering()) && !operation->isPreviewOperation()) {
			unsigned int resolution[2] = {0,0};
			unsigned int preferredResolution[2] = {0,0};
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}
	for (index = 0 ; index < this->operations.size(); index ++) {
		NodeOperation *operation = this->operations[index];
		if (operation->isOutputOperation(context.isRendering()) && operation->isPreviewOperation()) {
			unsigned int resolution[2] = {0,0};
			unsigned int preferredResolution[2] = {0,0};
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}

	// add convert resolution operations when needed.
	for (index = 0 ; index < this->connections.size(); index ++) {
		SocketConnection *connection = this->connections[index];
		if (connection->isValid()) {
			if (connection->needsResolutionConversion()) {
				Converter::convertResolution(connection, this);
			}
		}
	}

}

void ExecutionSystem::groupOperations()
{
	vector<NodeOperation*> outputOperations;
	NodeOperation * operation;
	unsigned int index;
	// surround complex operations with ReadBufferOperation and WriteBufferOperation
	for (index = 0; index < this->operations.size(); index++) {
		operation = this->operations[index];
		if (operation->isComplex()) {
			this->addReadWriteBufferOperations(operation);
		}
	}
	ExecutionSystemHelper::findOutputNodeOperations(&outputOperations, this->getOperations(), this->context.isRendering());
	for (vector<NodeOperation*>::iterator iter = outputOperations.begin(); iter != outputOperations.end(); ++iter) {
		operation = *iter;
		ExecutionGroup *group = new ExecutionGroup();
		group->addOperation(this, operation);
		group->setOutputExecutionGroup(true);
		ExecutionSystemHelper::addExecutionGroup(this->getExecutionGroups(), group);
	}
}

void ExecutionSystem::addSocketConnection(SocketConnection *connection)
{
	this->connections.push_back(connection);
}


void ExecutionSystem::determineActualSocketDataTypes(vector<NodeBase*> &nodes)
{
	unsigned int index;
	/* first do all input nodes */
	for (index = 0; index < nodes.size(); index++) {
		NodeBase *node = nodes[index];
		if (node->isInputNode()) {
			node->determineActualSocketDataTypes();
		}
	}

	/* then all other nodes */
	for (index = 0; index < nodes.size(); index++) {
		NodeBase *node = nodes[index];
		if (!node->isInputNode()) {
			node->determineActualSocketDataTypes();
		}
	}
}

void ExecutionSystem::findOutputExecutionGroup(vector<ExecutionGroup*> *result, CompositorPriority priority) const
{
	unsigned int index;
	for (index = 0 ; index < this->groups.size() ; index ++) {
		ExecutionGroup *group = this->groups[index];
		if (group->isOutputExecutionGroup() && group->getRenderPriotrity() == priority) {
			result->push_back(group);
		}
	}
}

void ExecutionSystem::findOutputExecutionGroup(vector<ExecutionGroup*> *result) const
{
	unsigned int index;
	for (index = 0 ; index < this->groups.size() ; index ++) {
		ExecutionGroup *group = this->groups[index];
		if (group->isOutputExecutionGroup()) {
			result->push_back(group);
		}
	}
}
