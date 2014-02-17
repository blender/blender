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
#include "BLI_utildefines.h"
extern "C" {
#include "BKE_node.h"
}

#include "COM_Converter.h"
#include "COM_NodeOperation.h"
#include "COM_ExecutionGroup.h"
#include "COM_NodeBase.h"
#include "COM_WorkScheduler.h"
#include "COM_ReadBufferOperation.h"
#include "COM_GroupNode.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ExecutionSystemHelper.h"
#include "COM_Debug.h"

#include "BKE_global.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

ExecutionSystem::ExecutionSystem(RenderData *rd, Scene *scene, bNodeTree *editingtree, bool rendering, bool fastcalculation,
                                 const ColorManagedViewSettings *viewSettings, const ColorManagedDisplaySettings *displaySettings)
{
	this->m_context.setScene(scene);
	this->m_context.setbNodeTree(editingtree);
	this->m_context.setPreviewHash(editingtree->previews);
	this->m_context.setFastCalculation(fastcalculation);
	/* initialize the CompositorContext */
	if (rendering) {
		this->m_context.setQuality((CompositorQuality)editingtree->render_quality);
	}
	else {
		this->m_context.setQuality((CompositorQuality)editingtree->edit_quality);
	}
	this->m_context.setRendering(rendering);
	this->m_context.setHasActiveOpenCLDevices(WorkScheduler::hasGPUDevices() && (editingtree->flag & NTREE_COM_OPENCL));

	ExecutionSystemHelper::addbNodeTree(*this, 0, editingtree, NODE_INSTANCE_KEY_BASE);

	this->m_context.setRenderData(rd);
	this->m_context.setViewSettings(viewSettings);
	this->m_context.setDisplaySettings(displaySettings);

	this->convertToOperations();
	this->groupOperations(); /* group operations in ExecutionGroups */
	unsigned int index;
	unsigned int resolution[2];

	rctf *viewer_border = &editingtree->viewer_border;
	bool use_viewer_border = (editingtree->flag & NTREE_VIEWER_BORDER) &&
	                         viewer_border->xmin < viewer_border->xmax &&
	                         viewer_border->ymin < viewer_border->ymax;

	for (index = 0; index < this->m_groups.size(); index++) {
		resolution[0] = 0;
		resolution[1] = 0;
		ExecutionGroup *executionGroup = this->m_groups[index];
		executionGroup->determineResolution(resolution);

		if (rendering) {
			/* case when cropping to render border happens is handled in
			 * compositor output and render layer nodes
			 */
			if ((rd->mode & R_BORDER) && !(rd->mode & R_CROP)) {
				executionGroup->setRenderBorder(rd->border.xmin, rd->border.xmax,
				                                rd->border.ymin, rd->border.ymax);
			}
		}

		if (use_viewer_border) {
			executionGroup->setViewerBorder(viewer_border->xmin, viewer_border->xmax,
			                                viewer_border->ymin, viewer_border->ymax);
		}
	}

//	DebugInfo::graphviz(this);
}

ExecutionSystem::~ExecutionSystem()
{
	unsigned int index;
	for (index = 0; index < this->m_connections.size(); index++) {
		SocketConnection *connection = this->m_connections[index];
		delete connection;
	}
	this->m_connections.clear();
	for (index = 0; index < this->m_nodes.size(); index++) {
		Node *node = this->m_nodes[index];
		delete node;
	}
	this->m_nodes.clear();
	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		delete operation;
	}
	this->m_operations.clear();
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *group = this->m_groups[index];
		delete group;
	}
	this->m_groups.clear();
}

void ExecutionSystem::execute()
{
	DebugInfo::execute_started(this);
	
	unsigned int order = 0;
	for (vector<NodeOperation *>::iterator iter = this->m_operations.begin(); iter != this->m_operations.end(); ++iter) {
		NodeBase *node = *iter;
		NodeOperation *operation = (NodeOperation *) node;
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation *readOperation = (ReadBufferOperation *)operation;
			readOperation->setOffset(order);
			order++;
		}
	}
	unsigned int index;

	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		operation->setbNodeTree(this->m_context.getbNodeTree());
		operation->initExecution();
	}
	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation *readOperation = (ReadBufferOperation *)operation;
			readOperation->updateMemoryBuffer();
		}
	}
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *executionGroup = this->m_groups[index];
		executionGroup->setChunksize(this->m_context.getChunksize());
		executionGroup->initExecution();
	}

	WorkScheduler::start(this->m_context);

	executeGroups(COM_PRIORITY_HIGH);
	if (!this->getContext().isFastCalculation()) {
		executeGroups(COM_PRIORITY_MEDIUM);
		executeGroups(COM_PRIORITY_LOW);
	}

	WorkScheduler::finish();
	WorkScheduler::stop();

	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		operation->deinitExecution();
	}
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *executionGroup = this->m_groups[index];
		executionGroup->deinitExecution();
	}
}

void ExecutionSystem::executeGroups(CompositorPriority priority)
{
	unsigned int index;
	vector<ExecutionGroup *> executionGroups;
	this->findOutputExecutionGroup(&executionGroups, priority);

	for (index = 0; index < executionGroups.size(); index++) {
		ExecutionGroup *group = executionGroups[index];
		group->execute(this);
	}
}

void ExecutionSystem::addOperation(NodeOperation *operation)
{
	ExecutionSystemHelper::addOperation(this->m_operations, operation);
	DebugInfo::operation_added(operation);
}

void ExecutionSystem::addReadWriteBufferOperations(NodeOperation *operation)
{
	DebugInfo::operation_read_write_buffer(operation);
	
	// for every input add write and read operation if input is not a read operation
	// only add read operation to other links when they are attached to buffered operations.
	unsigned int index;
	for (index = 0; index < operation->getNumberOfInputSockets(); index++) {
		InputSocket *inputsocket = operation->getInputSocket(index);
		if (inputsocket->isConnected()) {
			SocketConnection *connection = inputsocket->getConnection();
			NodeOperation *otherEnd = (NodeOperation *)connection->getFromNode();
			if (!otherEnd->isReadBufferOperation()) {
				// check of other end already has write operation
				OutputSocket *fromsocket = connection->getFromSocket();
				WriteBufferOperation *writeoperation = fromsocket->findAttachedWriteBufferOperation();
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
	 * link the outputsocket to a write operation
	 * link the writeoperation to a read operation
	 * link the read operation to the next node.
	 */
	OutputSocket *outputsocket = operation->getOutputSocket();
	if (outputsocket->isConnected()) {
		WriteBufferOperation *writeOperation = NULL;
		/* try to find existing write buffer operation first */
		for (index = 0; index < outputsocket->getNumberOfConnections(); index++) {
			SocketConnection *connection = outputsocket->getConnection(index);
			NodeBase *otherEnd = connection->getToNode();
			if (otherEnd->isOperation()) {
				NodeOperation *otherEndOp = (NodeOperation *)otherEnd;
				if (otherEndOp->isWriteBufferOperation()) {
					writeOperation = (WriteBufferOperation *)otherEndOp;
					break;
				}
			}
		}
		/* if no write buffer operation exists yet, create a new one */
		if (!writeOperation) {
			writeOperation = new WriteBufferOperation();
			writeOperation->setbNodeTree(this->getContext().getbNodeTree());
			this->addOperation(writeOperation);
			ExecutionSystemHelper::addLink(this->getConnections(), outputsocket, writeOperation->getInputSocket(0));
		}
		writeOperation->readResolutionFromInputSocket();
		
		for (index = 0; index < outputsocket->getNumberOfConnections(); index++) {
			SocketConnection *connection = outputsocket->getConnection(index);
			/* skip existing connections to write buffer operation */
			if (connection->getToNode() == writeOperation)
				continue;
			
			ReadBufferOperation *readoperation = new ReadBufferOperation();
			readoperation->setMemoryProxy(writeOperation->getMemoryProxy());
			connection->setFromSocket(readoperation->getOutputSocket());
			readoperation->getOutputSocket()->addConnection(connection);
			readoperation->readResolutionFromWriteBuffer();
			this->addOperation(readoperation);
		}
	}
}

#ifndef NDEBUG
/* if this fails, there are still connection to/from this node,
 * which have not been properly relinked to operations!
 */
static void debug_check_node_connections(Node *node)
{
	/* note: connected inputs are not checked here,
	 * it would break quite a lot and such inputs are ignored later anyway
	 */
#if 0
	for (int i = 0; i < node->getNumberOfInputSockets(); ++i) {
		BLI_assert(!node->getInputSocket(i)->isConnected());
	}
#endif
	for (int i = 0; i < node->getNumberOfOutputSockets(); ++i) {
		BLI_assert(!node->getOutputSocket(i)->isConnected());
	}
}
#else
/* stub */
#define debug_check_node_connections(node)
#endif

void ExecutionSystem::convertToOperations()
{
	unsigned int index;

	for (index = 0; index < this->m_nodes.size(); index++) {
		Node *node = (Node *)this->m_nodes[index];
		DebugInfo::node_to_operations(node);
		node->convertToOperations(this, &this->m_context);

		debug_check_node_connections(node);
	}

	for (index = 0; index < this->m_connections.size(); index++) {
		SocketConnection *connection = this->m_connections[index];
		if (connection->isValid()) {
			if (connection->getFromSocket()->getDataType() != connection->getToSocket()->getDataType()) {
				Converter::convertDataType(connection, this);
			}
		}
	}

	// determine all resolutions of the operations (Width/Height)
	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		if (operation->isOutputOperation(this->m_context.isRendering()) && !operation->isPreviewOperation()) {
			unsigned int resolution[2] = {0, 0};
			unsigned int preferredResolution[2] = {0, 0};
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}
	for (index = 0; index < this->m_operations.size(); index++) {
		NodeOperation *operation = this->m_operations[index];
		if (operation->isOutputOperation(this->m_context.isRendering()) && operation->isPreviewOperation()) {
			unsigned int resolution[2] = {0, 0};
			unsigned int preferredResolution[2] = {0, 0};
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}

	// add convert resolution operations when needed.
	for (index = 0; index < this->m_connections.size(); index++) {
		SocketConnection *connection = this->m_connections[index];
		if (connection->isValid()) {
			if (connection->needsResolutionConversion()) {
				Converter::convertResolution(connection, this);
			}
		}
	}
}

void ExecutionSystem::groupOperations()
{
	vector<NodeOperation *> outputOperations;
	NodeOperation *operation;
	unsigned int index;
	// surround complex operations with ReadBufferOperation and WriteBufferOperation
	for (index = 0; index < this->m_operations.size(); index++) {
		operation = this->m_operations[index];
		if (operation->isComplex()) {
			this->addReadWriteBufferOperations(operation);
		}
	}
	ExecutionSystemHelper::findOutputNodeOperations(&outputOperations, this->getOperations(), this->m_context.isRendering());
	for (vector<NodeOperation *>::iterator iter = outputOperations.begin(); iter != outputOperations.end(); ++iter) {
		operation = *iter;
		ExecutionGroup *group = new ExecutionGroup();
		group->addOperation(this, operation);
		group->setOutputExecutionGroup(true);
		ExecutionSystemHelper::addExecutionGroup(this->getExecutionGroups(), group);
	}
}

void ExecutionSystem::addSocketConnection(SocketConnection *connection)
{
	this->m_connections.push_back(connection);
}

void ExecutionSystem::removeSocketConnection(SocketConnection *connection)
{
	for (vector<SocketConnection *>::iterator it = m_connections.begin(); it != m_connections.end(); ++it) {
		if (*it == connection) {
			this->m_connections.erase(it);
			return;
		}
	}
}


void ExecutionSystem::findOutputExecutionGroup(vector<ExecutionGroup *> *result, CompositorPriority priority) const
{
	unsigned int index;
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *group = this->m_groups[index];
		if (group->isOutputExecutionGroup() && group->getRenderPriotrity() == priority) {
			result->push_back(group);
		}
	}
}

void ExecutionSystem::findOutputExecutionGroup(vector<ExecutionGroup *> *result) const
{
	unsigned int index;
	for (index = 0; index < this->m_groups.size(); index++) {
		ExecutionGroup *group = this->m_groups[index];
		if (group->isOutputExecutionGroup()) {
			result->push_back(group);
		}
	}
}
