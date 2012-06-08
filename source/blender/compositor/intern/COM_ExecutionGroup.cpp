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

#include "COM_ExecutionGroup.h"
#include "COM_InputSocket.h"
#include "COM_SocketConnection.h"
#include "COM_defines.h"
#include "math.h"
#include "COM_ExecutionSystem.h"
#include <sstream>
#include "COM_ReadBufferOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WorkScheduler.h"
#include "COM_ViewerOperation.h"
#include <stdlib.h>
#include "BLI_math.h"
#include "PIL_time.h"
#include "COM_ChunkOrder.h"
#include <algorithm>
#include "BLI_math.h"
#include "COM_ExecutionSystemHelper.h"

ExecutionGroup::ExecutionGroup()
{
	this->isOutput = false;
	this->complex = false;
	this->chunkExecutionStates = NULL;
	this->bTree = NULL;
	this->height = 0;
	this->width = 0;
	this->cachedMaxReadBufferOffset = 0;
	this->numberOfXChunks = 0;
	this->numberOfYChunks = 0;
	this->numberOfChunks = 0;
	this->initialized = false;
	this->openCL = false;
	this->chunksFinished = 0;
}

CompositorPriority ExecutionGroup::getRenderPriotrity()
{
	return this->getOutputNodeOperation()->getRenderPriority();
}

bool ExecutionGroup::containsOperation(NodeOperation *operation)
{
	for (vector<NodeOperation*>::const_iterator iterator = this->operations.begin() ; iterator != this->operations.end() ; ++iterator) {
		NodeOperation *inListOperation = *iterator;
		if (inListOperation == operation) {
			return true;
		}
	}
	return false;
}

const bool ExecutionGroup::isComplex() const
{
	return this->complex;
}

bool ExecutionGroup::canContainOperation(NodeOperation *operation)
{
	if (!this->initialized) {return true;}
	if (operation->isReadBufferOperation()) {return true;}
	if (operation->isWriteBufferOperation()) {return false;}
	if (operation->isSetOperation()) {return true;}

	if (!this->isComplex()) {
		return (!operation->isComplex());
	}
	else {
		return false;
	}
}

void ExecutionGroup::addOperation(ExecutionSystem *system, NodeOperation *operation)
{
	if (containsOperation(operation)) return;
	if (canContainOperation(operation)) {
		if (!operation->isBufferOperation()) {
			this->complex = operation->isComplex();
			this->openCL = operation->isOpenCL();
			this->initialized = true;
		}
		this->operations.push_back(operation);
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation *readOperation = (ReadBufferOperation*)operation;
			WriteBufferOperation *writeOperation = readOperation->getMemoryProxy()->getWriteBufferOperation();
			this->addOperation(system, writeOperation);
		}
		else {
			unsigned int index;
			for (index = 0 ; index < operation->getNumberOfInputSockets(); index ++) {
				InputSocket * inputSocket = operation->getInputSocket(index);
				if (inputSocket->isConnected()) {
					NodeOperation *node = (NodeOperation*)inputSocket->getConnection()->getFromNode();
					this->addOperation(system, node);
				}
			}
		}
	}
	else {
		if (operation->isWriteBufferOperation()) {
			WriteBufferOperation * writeoperation = (WriteBufferOperation*)operation;
			if (writeoperation->getMemoryProxy()->getExecutor() == NULL) {
				ExecutionGroup *newGroup = new ExecutionGroup();
				writeoperation->getMemoryProxy()->setExecutor(newGroup);
				newGroup->addOperation(system, operation);
				ExecutionSystemHelper::addExecutionGroup(system->getExecutionGroups(), newGroup);
			}
		}
	}
}

NodeOperation *ExecutionGroup::getOutputNodeOperation() const
{
	return this->operations[0]; // the first operation of the group is always the output operation.
}

void ExecutionGroup::initExecution()
{
	if (this->chunkExecutionStates != NULL) {
		delete[] this->chunkExecutionStates;
	}
	unsigned int index;
	determineNumberOfChunks();

	this->chunkExecutionStates = NULL;
	if (this->numberOfChunks != 0) {
		this->chunkExecutionStates = new ChunkExecutionState[numberOfChunks];
		for (index = 0 ; index < numberOfChunks ; index ++) {
			this->chunkExecutionStates[index] = COM_ES_NOT_SCHEDULED;
		}
	}


	unsigned int maxNumber = 0;

	for (index = 0 ; index < this->operations.size(); index ++) {
		NodeOperation *operation = this->operations[index];
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation *readOperation = (ReadBufferOperation*)operation;
			this->cachedReadOperations.push_back(readOperation);
			maxNumber = max(maxNumber, readOperation->getOffset());
		}
	}
	maxNumber++;
	this->cachedMaxReadBufferOffset = maxNumber;

}

void ExecutionGroup::deinitExecution()
{
	if (this->chunkExecutionStates != NULL) {
		delete[] this->chunkExecutionStates;
		this->chunkExecutionStates = NULL;
	}
	this->numberOfChunks = 0;
	this->numberOfXChunks = 0;
	this->numberOfYChunks = 0;
	this->cachedReadOperations.clear();
	this->bTree = NULL;
}
void ExecutionGroup::determineResolution(unsigned int resolution[])
{
	NodeOperation *operation = this->getOutputNodeOperation();
	unsigned int preferredResolution[2];
	preferredResolution[0] = 0;
	preferredResolution[1] = 0;
	operation->determineResolution(resolution, preferredResolution);
	operation->setResolution(resolution);
	this->setResolution(resolution);
}

void ExecutionGroup::determineNumberOfChunks()
{
	const float chunkSizef = this->chunkSize;
	this->numberOfXChunks = ceil(this->width / chunkSizef);
	this->numberOfYChunks = ceil(this->height / chunkSizef);
	this->numberOfChunks = this->numberOfXChunks * this->numberOfYChunks;
}

/**
  * this method is called for the top execution groups. containing the compositor node or the preview node or the viewer node)
  */
void ExecutionGroup::execute(ExecutionSystem *graph)
{
	CompositorContext& context = graph->getContext();
	const bNodeTree *bTree = context.getbNodeTree();
	if (this->width == 0 || this->height == 0) {return;} /// @note: break out... no pixels to calculate.
	if (bTree->test_break && bTree->test_break(bTree->tbh)) {return;} /// @note: early break out for blur and preview nodes
	if (this->numberOfChunks == 0) {return;} /// @note: early break out
	unsigned int chunkNumber;

	this->chunksFinished = 0;
	this->bTree = bTree;
	unsigned int index;
	unsigned int *chunkOrder = new unsigned int[this->numberOfChunks];

	for (chunkNumber = 0 ; chunkNumber<this->numberOfChunks ; chunkNumber++) {
		chunkOrder[chunkNumber] = chunkNumber;
	}
	NodeOperation *operation = this->getOutputNodeOperation();
	float centerX = 0.5;
	float centerY = 0.5;
	int chunkorder = COM_TO_CENTER_OUT;

	if (operation->isViewerOperation()) {
		ViewerBaseOperation *viewer = (ViewerBaseOperation*)operation;
		centerX = viewer->getCenterX();
		centerY = viewer->getCenterY();
		chunkorder = viewer->getChunkOrder();
	}

	switch (chunkorder) {
	case COM_TO_RANDOM:
		for (index = 0 ; index < 2 * numberOfChunks ; index ++) {
			int index1 = rand()%numberOfChunks;
			int index2 = rand()%numberOfChunks;
			int s = chunkOrder[index1];
			chunkOrder[index1] = chunkOrder[index2];
			chunkOrder[index2] = s;
		}
		break;
	case COM_TO_CENTER_OUT:
		{
			ChunkOrderHotspot **hotspots = new ChunkOrderHotspot*[1];
			hotspots[0] = new ChunkOrderHotspot(this->width*centerX, this->height*centerY, 0.0f);
			rcti rect;
			ChunkOrder *chunkOrders = new ChunkOrder[this->numberOfChunks];
			for (index = 0 ; index < this->numberOfChunks; index ++) {
				determineChunkRect(&rect, index);
				chunkOrders[index].setChunkNumber(index);
				chunkOrders[index].setX(rect.xmin);
				chunkOrders[index].setY(rect.ymin);
				chunkOrders[index].determineDistance(hotspots, 1);
			}

			sort(&chunkOrders[0], &chunkOrders[numberOfChunks-1]);
			for (index = 0 ; index < numberOfChunks; index ++) {
				chunkOrder[index] = chunkOrders[index].getChunkNumber();
			}

			delete hotspots[0];
			delete[] hotspots;
			delete[] chunkOrders;
		}
		break;
	case COM_TO_RULE_OF_THIRDS:
		{
			ChunkOrderHotspot **hotspots = new ChunkOrderHotspot*[9];
			unsigned int tx = this->width/6;
			unsigned int ty = this->height/6;
			unsigned int mx = this->width/2;
			unsigned int my = this->height/2;
			unsigned int bx = mx+2*tx;
			unsigned int by = my+2*ty;

			float addition = numberOfChunks/COM_RULE_OF_THIRDS_DIVIDER;
			hotspots[0] = new ChunkOrderHotspot(mx, my, addition*0);
			hotspots[1] = new ChunkOrderHotspot(tx, my, addition*1);
			hotspots[2] = new ChunkOrderHotspot(bx, my, addition*2);
			hotspots[3] = new ChunkOrderHotspot(bx, by, addition*3);
			hotspots[4] = new ChunkOrderHotspot(tx, ty, addition*4);
			hotspots[5] = new ChunkOrderHotspot(bx, ty, addition*5);
			hotspots[6] = new ChunkOrderHotspot(tx, by, addition*6);
			hotspots[7] = new ChunkOrderHotspot(mx, ty, addition*7);
			hotspots[8] = new ChunkOrderHotspot(mx, by, addition*8);
			rcti rect;
			ChunkOrder *chunkOrders = new ChunkOrder[this->numberOfChunks];
			for (index = 0 ; index < this->numberOfChunks; index ++) {
				determineChunkRect(&rect, index);
				chunkOrders[index].setChunkNumber(index);
				chunkOrders[index].setX(rect.xmin);
				chunkOrders[index].setY(rect.ymin);
				chunkOrders[index].determineDistance(hotspots, 9);
			}

			sort(&chunkOrders[0], &chunkOrders[numberOfChunks]);

			for (index = 0 ; index < numberOfChunks; index ++) {
				chunkOrder[index] = chunkOrders[index].getChunkNumber();
			}

			delete hotspots[0];
			delete hotspots[1];
			delete hotspots[2];
			delete hotspots[3];
			delete hotspots[4];
			delete hotspots[5];
			delete hotspots[6];
			delete hotspots[7];
			delete hotspots[8];
			delete[] hotspots;
			delete[] chunkOrders;
		}
		break;
	case COM_TO_TOP_DOWN:
	default:
		break;
	}

	bool breaked = false;
	bool finished = false;
	unsigned int startIndex = 0;
	const int maxNumberEvaluated = BLI_system_thread_count()*2;

	while (!finished && !breaked) {
				unsigned int index;
		bool startEvaluated = false;
		finished = true;
		int numberEvaluated = 0;

		for (index = startIndex ; index < numberOfChunks && numberEvaluated < maxNumberEvaluated; index ++) {
			int chunkNumber = chunkOrder[index];
			int yChunk = chunkNumber/this->numberOfXChunks;
			int xChunk = chunkNumber - (yChunk*this->numberOfXChunks);
			const ChunkExecutionState state = this->chunkExecutionStates[chunkNumber];
			if (state == COM_ES_NOT_SCHEDULED) {
				scheduleChunkWhenPossible(graph, xChunk, yChunk);
				finished=false;
				startEvaluated = true;
				numberEvaluated++;
			}
			else if (state == COM_ES_SCHEDULED) {
				finished=false;
				startEvaluated = true;
				numberEvaluated++;
			}
			else if (state == COM_ES_EXECUTED && !startEvaluated) {
				startIndex = index+1;
			}
		}
		PIL_sleep_ms(10);

		if (bTree->test_break && bTree->test_break(bTree->tbh)) {
			breaked = true;
		}
	}

	delete[] chunkOrder;
}

MemoryBuffer** ExecutionGroup::getInputBuffersCPU()
{
	vector<MemoryProxy*> memoryproxies;
	unsigned int index;

	this->determineDependingMemoryProxies(&memoryproxies);
	MemoryBuffer **memoryBuffers = new MemoryBuffer*[this->cachedMaxReadBufferOffset];
	for (index = 0 ; index < this->cachedMaxReadBufferOffset ; index ++) {
		memoryBuffers[index] = NULL;
	}
	for (index = 0 ; index < this->cachedReadOperations.size(); index ++) {
		ReadBufferOperation *readOperation = (ReadBufferOperation*)this->cachedReadOperations[index];
		memoryBuffers[readOperation->getOffset()] = readOperation->getMemoryProxy()->getBuffer();
	}
	return memoryBuffers;
}

MemoryBuffer** ExecutionGroup::getInputBuffersOpenCL(int chunkNumber)
{
	rcti rect;
	vector<MemoryProxy*> memoryproxies;
	unsigned int index;
	determineChunkRect(&rect, chunkNumber);

	this->determineDependingMemoryProxies(&memoryproxies);
	MemoryBuffer **memoryBuffers = new MemoryBuffer*[this->cachedMaxReadBufferOffset];
	for (index = 0 ; index < this->cachedMaxReadBufferOffset ; index ++) {
		memoryBuffers[index] = NULL;
	}
	rcti output;
	for (index = 0 ; index < this->cachedReadOperations.size(); index ++) {
		ReadBufferOperation *readOperation = (ReadBufferOperation*)this->cachedReadOperations[index];
		MemoryProxy * memoryProxy = readOperation->getMemoryProxy();
		this->determineDependingAreaOfInterest(&rect, readOperation, &output);
		MemoryBuffer *memoryBuffer = memoryProxy->getExecutor()->constructConsolidatedMemoryBuffer(memoryProxy, &output);
		memoryBuffers[readOperation->getOffset()] = memoryBuffer;
	}
	return memoryBuffers;
}

MemoryBuffer *ExecutionGroup::constructConsolidatedMemoryBuffer(MemoryProxy *memoryProxy, rcti *rect)
{
	MemoryBuffer* imageBuffer = memoryProxy->getBuffer();
	MemoryBuffer* result = new MemoryBuffer(memoryProxy, rect);
	result->copyContentFrom(imageBuffer);
	return result;
}

void ExecutionGroup::finalizeChunkExecution(int chunkNumber, MemoryBuffer** memoryBuffers)
{
	if (this->chunkExecutionStates[chunkNumber] == COM_ES_SCHEDULED)
		this->chunkExecutionStates[chunkNumber] = COM_ES_EXECUTED;
	
	this->chunksFinished++;
	if (memoryBuffers) {
		for (unsigned int index = 0 ; index < this->cachedMaxReadBufferOffset; index ++) {
			MemoryBuffer * buffer = memoryBuffers[index];
			if (buffer) {
				if (buffer->isTemporarily()) {
					memoryBuffers[index] = NULL;
					delete buffer;
				}
			}
		}
		delete[] memoryBuffers;
	}
	if (bTree) {
		// status report is only performed for top level Execution Groups.
		float progress = chunksFinished;
		progress/=numberOfChunks;
		bTree->progress(bTree->prh, progress);
	}
}

inline void ExecutionGroup::determineChunkRect(rcti *rect, const unsigned int xChunk, const unsigned int yChunk ) const
{
	const unsigned int minx = xChunk * chunkSize;
	const unsigned int miny = yChunk * chunkSize;
	BLI_init_rcti(rect, minx, min(minx + this->chunkSize, this->width), miny, min(miny + this->chunkSize, this->height));
}

void ExecutionGroup::determineChunkRect(rcti *rect, const unsigned int chunkNumber) const
{
	const unsigned int yChunk = chunkNumber / numberOfXChunks;
	const unsigned int xChunk = chunkNumber - (yChunk * numberOfXChunks);
	determineChunkRect(rect, xChunk, yChunk);
}

MemoryBuffer *ExecutionGroup::allocateOutputBuffer(int chunkNumber, rcti *rect)
{
	// we asume that this method is only called from complex execution groups.
	NodeOperation * operation = this->getOutputNodeOperation();
	if (operation->isWriteBufferOperation()) {
		WriteBufferOperation *writeOperation = (WriteBufferOperation*)operation;
		MemoryBuffer *buffer = new MemoryBuffer(writeOperation->getMemoryProxy(), rect);
		return buffer;
	}
	return NULL;
}


bool ExecutionGroup::scheduleAreaWhenPossible(ExecutionSystem * graph, rcti *area)
{
	// find all chunks inside the rect
	// determine minxchunk, minychunk, maxxchunk, maxychunk where x and y are chunknumbers

	float chunkSizef = this->chunkSize;

	int indexx, indexy;
	const int minxchunk = floor(area->xmin/chunkSizef);
	const int maxxchunk = ceil((area->xmax-1)/chunkSizef);
	const int minychunk = floor(area->ymin/chunkSizef);
	const int maxychunk = ceil((area->ymax-1)/chunkSizef);

	bool result = true;
	for (indexx = max(minxchunk, 0); indexx<maxxchunk ; indexx++) {
		for (indexy = max(minychunk, 0); indexy<maxychunk ; indexy++) {
			if (!scheduleChunkWhenPossible(graph, indexx, indexy)) {
				result = false;
			}
		}
	}

	return result;
}

bool ExecutionGroup::scheduleChunk(unsigned int chunkNumber)
{
	if (this->chunkExecutionStates[chunkNumber] == COM_ES_NOT_SCHEDULED) {
		this->chunkExecutionStates[chunkNumber] = COM_ES_SCHEDULED;
		WorkScheduler::schedule(this, chunkNumber);
		return true;
	}
	return false;
}

bool ExecutionGroup::scheduleChunkWhenPossible(ExecutionSystem * graph, int xChunk, int yChunk)
{
	if (xChunk < 0 || xChunk >= (int)this->numberOfXChunks) {
		return true;
	}
	if (yChunk < 0 || yChunk >= (int)this->numberOfYChunks) {
		return true;
	}
	int chunkNumber = yChunk*this->numberOfXChunks + xChunk;
	// chunk is already executed
	if (this->chunkExecutionStates[chunkNumber] == COM_ES_EXECUTED) {
		return true;
	}

	// chunk is scheduled, but not executed
	if (this->chunkExecutionStates[chunkNumber] == COM_ES_SCHEDULED) {
		return false;
	}

	// chunk is nor executed nor scheduled.
	vector<MemoryProxy*> memoryProxies;
	this->determineDependingMemoryProxies(&memoryProxies);

	rcti rect;
	determineChunkRect(&rect, xChunk, yChunk);
	unsigned int index;
	bool canBeExecuted = true;
	rcti area;

	for (index = 0 ; index < cachedReadOperations.size() ; index ++) {
		ReadBufferOperation * readOperation = (ReadBufferOperation*)cachedReadOperations[index];
		BLI_init_rcti(&area, 0, 0, 0, 0);
		MemoryProxy * memoryProxy = memoryProxies[index];
		determineDependingAreaOfInterest(&rect, readOperation, &area);
		ExecutionGroup *group = memoryProxy->getExecutor();

		if (group != NULL) {
			if (!group->scheduleAreaWhenPossible(graph, &area)) {
				canBeExecuted = false;
			}
		}
		else {
			throw "ERROR";
		}
	}

	if (canBeExecuted) {
		scheduleChunk(chunkNumber);
	}

	return false;
}

void ExecutionGroup::determineDependingAreaOfInterest(rcti * input, ReadBufferOperation *readOperation, rcti *output)
{
	this->getOutputNodeOperation()->determineDependingAreaOfInterest(input, readOperation, output);
}

void ExecutionGroup::determineDependingMemoryProxies(vector<MemoryProxy*> *memoryProxies)
{
	unsigned int index;
	for (index = 0 ; index < this->cachedReadOperations.size() ; index ++) {
		ReadBufferOperation * readOperation = (ReadBufferOperation*) this->cachedReadOperations[index];
		memoryProxies->push_back(readOperation->getMemoryProxy());
	}
}

bool ExecutionGroup::isOpenCL()
{
	return this->openCL;
}
