/*
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
 * Copyright 2011, Blender Foundation.
 */

#include <algorithm>
#include <math.h>
#include <sstream>
#include <stdlib.h>

#include "atomic_ops.h"

#include "COM_ExecutionGroup.h"
#include "COM_defines.h"
#include "COM_ExecutionSystem.h"
#include "COM_ReadBufferOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_WorkScheduler.h"
#include "COM_ViewerOperation.h"
#include "COM_ChunkOrder.h"
#include "COM_Debug.h"

#include "MEM_guardedalloc.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLT_translation.h"
#include "PIL_time.h"
#include "WM_api.h"
#include "WM_types.h"

ExecutionGroup::ExecutionGroup()
{
  this->m_isOutput = false;
  this->m_complex = false;
  this->m_chunkExecutionStates = NULL;
  this->m_bTree = NULL;
  this->m_height = 0;
  this->m_width = 0;
  this->m_cachedMaxReadBufferOffset = 0;
  this->m_numberOfXChunks = 0;
  this->m_numberOfYChunks = 0;
  this->m_numberOfChunks = 0;
  this->m_initialized = false;
  this->m_openCL = false;
  this->m_singleThreaded = false;
  this->m_chunksFinished = 0;
  BLI_rcti_init(&this->m_viewerBorder, 0, 0, 0, 0);
  this->m_executionStartTime = 0;
}

CompositorPriority ExecutionGroup::getRenderPriotrity()
{
  return this->getOutputOperation()->getRenderPriority();
}

bool ExecutionGroup::canContainOperation(NodeOperation *operation)
{
  if (!this->m_initialized) {
    return true;
  }

  if (operation->isReadBufferOperation()) {
    return true;
  }
  if (operation->isWriteBufferOperation()) {
    return false;
  }
  if (operation->isSetOperation()) {
    return true;
  }

  /* complex groups don't allow further ops (except read buffer and values, see above) */
  if (m_complex) {
    return false;
  }
  /* complex ops can't be added to other groups (except their own, which they initialize, see
   * above) */
  if (operation->isComplex()) {
    return false;
  }

  return true;
}

bool ExecutionGroup::addOperation(NodeOperation *operation)
{
  if (!canContainOperation(operation)) {
    return false;
  }

  if (!operation->isReadBufferOperation() && !operation->isWriteBufferOperation()) {
    m_complex = operation->isComplex();
    m_openCL = operation->isOpenCL();
    m_singleThreaded = operation->isSingleThreaded();
    m_initialized = true;
  }

  m_operations.push_back(operation);

  return true;
}

NodeOperation *ExecutionGroup::getOutputOperation() const
{
  return this
      ->m_operations[0];  // the first operation of the group is always the output operation.
}

void ExecutionGroup::initExecution()
{
  if (this->m_chunkExecutionStates != NULL) {
    MEM_freeN(this->m_chunkExecutionStates);
  }
  unsigned int index;
  determineNumberOfChunks();

  this->m_chunkExecutionStates = NULL;
  if (this->m_numberOfChunks != 0) {
    this->m_chunkExecutionStates = (ChunkExecutionState *)MEM_mallocN(
        sizeof(ChunkExecutionState) * this->m_numberOfChunks, __func__);
    for (index = 0; index < this->m_numberOfChunks; index++) {
      this->m_chunkExecutionStates[index] = COM_ES_NOT_SCHEDULED;
    }
  }

  unsigned int maxNumber = 0;

  for (index = 0; index < this->m_operations.size(); index++) {
    NodeOperation *operation = this->m_operations[index];
    if (operation->isReadBufferOperation()) {
      ReadBufferOperation *readOperation = (ReadBufferOperation *)operation;
      this->m_cachedReadOperations.push_back(readOperation);
      maxNumber = max(maxNumber, readOperation->getOffset());
    }
  }
  maxNumber++;
  this->m_cachedMaxReadBufferOffset = maxNumber;
}

void ExecutionGroup::deinitExecution()
{
  if (this->m_chunkExecutionStates != NULL) {
    MEM_freeN(this->m_chunkExecutionStates);
    this->m_chunkExecutionStates = NULL;
  }
  this->m_numberOfChunks = 0;
  this->m_numberOfXChunks = 0;
  this->m_numberOfYChunks = 0;
  this->m_cachedReadOperations.clear();
  this->m_bTree = NULL;
}
void ExecutionGroup::determineResolution(unsigned int resolution[2])
{
  NodeOperation *operation = this->getOutputOperation();
  resolution[0] = operation->getWidth();
  resolution[1] = operation->getHeight();
  this->setResolution(resolution);
  BLI_rcti_init(&this->m_viewerBorder, 0, this->m_width, 0, this->m_height);
}

void ExecutionGroup::determineNumberOfChunks()
{
  if (this->m_singleThreaded) {
    this->m_numberOfXChunks = 1;
    this->m_numberOfYChunks = 1;
    this->m_numberOfChunks = 1;
  }
  else {
    const float chunkSizef = this->m_chunkSize;
    const int border_width = BLI_rcti_size_x(&this->m_viewerBorder);
    const int border_height = BLI_rcti_size_y(&this->m_viewerBorder);
    this->m_numberOfXChunks = ceil(border_width / chunkSizef);
    this->m_numberOfYChunks = ceil(border_height / chunkSizef);
    this->m_numberOfChunks = this->m_numberOfXChunks * this->m_numberOfYChunks;
  }
}

/**
 * this method is called for the top execution groups. containing the compositor node or the
 * preview node or the viewer node)
 */
void ExecutionGroup::execute(ExecutionSystem *graph)
{
  const CompositorContext &context = graph->getContext();
  const bNodeTree *bTree = context.getbNodeTree();
  if (this->m_width == 0 || this->m_height == 0) {
    return;
  }  /// \note: break out... no pixels to calculate.
  if (bTree->test_break && bTree->test_break(bTree->tbh)) {
    return;
  }  /// \note: early break out for blur and preview nodes
  if (this->m_numberOfChunks == 0) {
    return;
  }  /// \note: early break out
  unsigned int chunkNumber;

  this->m_executionStartTime = PIL_check_seconds_timer();

  this->m_chunksFinished = 0;
  this->m_bTree = bTree;
  unsigned int index;
  unsigned int *chunkOrder = (unsigned int *)MEM_mallocN(
      sizeof(unsigned int) * this->m_numberOfChunks, __func__);

  for (chunkNumber = 0; chunkNumber < this->m_numberOfChunks; chunkNumber++) {
    chunkOrder[chunkNumber] = chunkNumber;
  }
  NodeOperation *operation = this->getOutputOperation();
  float centerX = 0.5;
  float centerY = 0.5;
  OrderOfChunks chunkorder = COM_ORDER_OF_CHUNKS_DEFAULT;

  if (operation->isViewerOperation()) {
    ViewerOperation *viewer = (ViewerOperation *)operation;
    centerX = viewer->getCenterX();
    centerY = viewer->getCenterY();
    chunkorder = viewer->getChunkOrder();
  }

  const int border_width = BLI_rcti_size_x(&this->m_viewerBorder);
  const int border_height = BLI_rcti_size_y(&this->m_viewerBorder);

  switch (chunkorder) {
    case COM_TO_RANDOM:
      for (index = 0; index < 2 * this->m_numberOfChunks; index++) {
        int index1 = rand() % this->m_numberOfChunks;
        int index2 = rand() % this->m_numberOfChunks;
        int s = chunkOrder[index1];
        chunkOrder[index1] = chunkOrder[index2];
        chunkOrder[index2] = s;
      }
      break;
    case COM_TO_CENTER_OUT: {
      ChunkOrderHotspot *hotspots[1];
      hotspots[0] = new ChunkOrderHotspot(border_width * centerX, border_height * centerY, 0.0f);
      rcti rect;
      ChunkOrder *chunkOrders = (ChunkOrder *)MEM_mallocN(
          sizeof(ChunkOrder) * this->m_numberOfChunks, __func__);
      for (index = 0; index < this->m_numberOfChunks; index++) {
        determineChunkRect(&rect, index);
        chunkOrders[index].setChunkNumber(index);
        chunkOrders[index].setX(rect.xmin - this->m_viewerBorder.xmin);
        chunkOrders[index].setY(rect.ymin - this->m_viewerBorder.ymin);
        chunkOrders[index].determineDistance(hotspots, 1);
      }

      std::sort(&chunkOrders[0], &chunkOrders[this->m_numberOfChunks - 1]);
      for (index = 0; index < this->m_numberOfChunks; index++) {
        chunkOrder[index] = chunkOrders[index].getChunkNumber();
      }

      delete hotspots[0];
      MEM_freeN(chunkOrders);
      break;
    }
    case COM_TO_RULE_OF_THIRDS: {
      ChunkOrderHotspot *hotspots[9];
      unsigned int tx = border_width / 6;
      unsigned int ty = border_height / 6;
      unsigned int mx = border_width / 2;
      unsigned int my = border_height / 2;
      unsigned int bx = mx + 2 * tx;
      unsigned int by = my + 2 * ty;

      float addition = this->m_numberOfChunks / COM_RULE_OF_THIRDS_DIVIDER;
      hotspots[0] = new ChunkOrderHotspot(mx, my, addition * 0);
      hotspots[1] = new ChunkOrderHotspot(tx, my, addition * 1);
      hotspots[2] = new ChunkOrderHotspot(bx, my, addition * 2);
      hotspots[3] = new ChunkOrderHotspot(bx, by, addition * 3);
      hotspots[4] = new ChunkOrderHotspot(tx, ty, addition * 4);
      hotspots[5] = new ChunkOrderHotspot(bx, ty, addition * 5);
      hotspots[6] = new ChunkOrderHotspot(tx, by, addition * 6);
      hotspots[7] = new ChunkOrderHotspot(mx, ty, addition * 7);
      hotspots[8] = new ChunkOrderHotspot(mx, by, addition * 8);
      rcti rect;
      ChunkOrder *chunkOrders = (ChunkOrder *)MEM_mallocN(
          sizeof(ChunkOrder) * this->m_numberOfChunks, __func__);
      for (index = 0; index < this->m_numberOfChunks; index++) {
        determineChunkRect(&rect, index);
        chunkOrders[index].setChunkNumber(index);
        chunkOrders[index].setX(rect.xmin - this->m_viewerBorder.xmin);
        chunkOrders[index].setY(rect.ymin - this->m_viewerBorder.ymin);
        chunkOrders[index].determineDistance(hotspots, 9);
      }

      std::sort(&chunkOrders[0], &chunkOrders[this->m_numberOfChunks]);

      for (index = 0; index < this->m_numberOfChunks; index++) {
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
      MEM_freeN(chunkOrders);
      break;
    }
    case COM_TO_TOP_DOWN:
    default:
      break;
  }

  DebugInfo::execution_group_started(this);
  DebugInfo::graphviz(graph);

  bool breaked = false;
  bool finished = false;
  unsigned int startIndex = 0;
  const int maxNumberEvaluated = BLI_system_thread_count() * 2;

  while (!finished && !breaked) {
    bool startEvaluated = false;
    finished = true;
    int numberEvaluated = 0;

    for (index = startIndex;
         index < this->m_numberOfChunks && numberEvaluated < maxNumberEvaluated;
         index++) {
      chunkNumber = chunkOrder[index];
      int yChunk = chunkNumber / this->m_numberOfXChunks;
      int xChunk = chunkNumber - (yChunk * this->m_numberOfXChunks);
      const ChunkExecutionState state = this->m_chunkExecutionStates[chunkNumber];
      if (state == COM_ES_NOT_SCHEDULED) {
        scheduleChunkWhenPossible(graph, xChunk, yChunk);
        finished = false;
        startEvaluated = true;
        numberEvaluated++;

        if (bTree->update_draw) {
          bTree->update_draw(bTree->udh);
        }
      }
      else if (state == COM_ES_SCHEDULED) {
        finished = false;
        startEvaluated = true;
        numberEvaluated++;
      }
      else if (state == COM_ES_EXECUTED && !startEvaluated) {
        startIndex = index + 1;
      }
    }

    WorkScheduler::finish();

    if (bTree->test_break && bTree->test_break(bTree->tbh)) {
      breaked = true;
    }
  }
  DebugInfo::execution_group_finished(this);
  DebugInfo::graphviz(graph);

  MEM_freeN(chunkOrder);
}

MemoryBuffer **ExecutionGroup::getInputBuffersOpenCL(int chunkNumber)
{
  rcti rect;
  vector<MemoryProxy *> memoryproxies;
  unsigned int index;
  determineChunkRect(&rect, chunkNumber);

  this->determineDependingMemoryProxies(&memoryproxies);
  MemoryBuffer **memoryBuffers = (MemoryBuffer **)MEM_callocN(
      sizeof(MemoryBuffer *) * this->m_cachedMaxReadBufferOffset, __func__);
  rcti output;
  for (index = 0; index < this->m_cachedReadOperations.size(); index++) {
    ReadBufferOperation *readOperation =
        (ReadBufferOperation *)this->m_cachedReadOperations[index];
    MemoryProxy *memoryProxy = readOperation->getMemoryProxy();
    this->determineDependingAreaOfInterest(&rect, readOperation, &output);
    MemoryBuffer *memoryBuffer = memoryProxy->getExecutor()->constructConsolidatedMemoryBuffer(
        memoryProxy, &output);
    memoryBuffers[readOperation->getOffset()] = memoryBuffer;
  }
  return memoryBuffers;
}

MemoryBuffer *ExecutionGroup::constructConsolidatedMemoryBuffer(MemoryProxy *memoryProxy,
                                                                rcti *rect)
{
  MemoryBuffer *imageBuffer = memoryProxy->getBuffer();
  MemoryBuffer *result = new MemoryBuffer(memoryProxy, rect);
  result->copyContentFrom(imageBuffer);
  return result;
}

void ExecutionGroup::finalizeChunkExecution(int chunkNumber, MemoryBuffer **memoryBuffers)
{
  if (this->m_chunkExecutionStates[chunkNumber] == COM_ES_SCHEDULED) {
    this->m_chunkExecutionStates[chunkNumber] = COM_ES_EXECUTED;
  }

  atomic_add_and_fetch_u(&this->m_chunksFinished, 1);
  if (memoryBuffers) {
    for (unsigned int index = 0; index < this->m_cachedMaxReadBufferOffset; index++) {
      MemoryBuffer *buffer = memoryBuffers[index];
      if (buffer) {
        if (buffer->isTemporarily()) {
          memoryBuffers[index] = NULL;
          delete buffer;
        }
      }
    }
    MEM_freeN(memoryBuffers);
  }
  if (this->m_bTree) {
    // status report is only performed for top level Execution Groups.
    float progress = this->m_chunksFinished;
    progress /= this->m_numberOfChunks;
    this->m_bTree->progress(this->m_bTree->prh, progress);

    char buf[128];
    BLI_snprintf(buf,
                 sizeof(buf),
                 IFACE_("Compositing | Tile %u-%u"),
                 this->m_chunksFinished,
                 this->m_numberOfChunks);
    this->m_bTree->stats_draw(this->m_bTree->sdh, buf);
  }
}

inline void ExecutionGroup::determineChunkRect(rcti *rect,
                                               const unsigned int xChunk,
                                               const unsigned int yChunk) const
{
  const int border_width = BLI_rcti_size_x(&this->m_viewerBorder);
  const int border_height = BLI_rcti_size_y(&this->m_viewerBorder);

  if (this->m_singleThreaded) {
    BLI_rcti_init(
        rect, this->m_viewerBorder.xmin, border_width, this->m_viewerBorder.ymin, border_height);
  }
  else {
    const unsigned int minx = xChunk * this->m_chunkSize + this->m_viewerBorder.xmin;
    const unsigned int miny = yChunk * this->m_chunkSize + this->m_viewerBorder.ymin;
    const unsigned int width = min((unsigned int)this->m_viewerBorder.xmax, this->m_width);
    const unsigned int height = min((unsigned int)this->m_viewerBorder.ymax, this->m_height);
    BLI_rcti_init(rect,
                  min(minx, this->m_width),
                  min(minx + this->m_chunkSize, width),
                  min(miny, this->m_height),
                  min(miny + this->m_chunkSize, height));
  }
}

void ExecutionGroup::determineChunkRect(rcti *rect, const unsigned int chunkNumber) const
{
  const unsigned int yChunk = chunkNumber / this->m_numberOfXChunks;
  const unsigned int xChunk = chunkNumber - (yChunk * this->m_numberOfXChunks);
  determineChunkRect(rect, xChunk, yChunk);
}

MemoryBuffer *ExecutionGroup::allocateOutputBuffer(int /*chunkNumber*/, rcti *rect)
{
  // we assume that this method is only called from complex execution groups.
  NodeOperation *operation = this->getOutputOperation();
  if (operation->isWriteBufferOperation()) {
    WriteBufferOperation *writeOperation = (WriteBufferOperation *)operation;
    MemoryBuffer *buffer = new MemoryBuffer(writeOperation->getMemoryProxy(), rect);
    return buffer;
  }
  return NULL;
}

bool ExecutionGroup::scheduleAreaWhenPossible(ExecutionSystem *graph, rcti *area)
{
  if (this->m_singleThreaded) {
    return scheduleChunkWhenPossible(graph, 0, 0);
  }
  // find all chunks inside the rect
  // determine minxchunk, minychunk, maxxchunk, maxychunk where x and y are chunknumbers

  int indexx, indexy;
  int minx = max_ii(area->xmin - m_viewerBorder.xmin, 0);
  int maxx = min_ii(area->xmax - m_viewerBorder.xmin, m_viewerBorder.xmax - m_viewerBorder.xmin);
  int miny = max_ii(area->ymin - m_viewerBorder.ymin, 0);
  int maxy = min_ii(area->ymax - m_viewerBorder.ymin, m_viewerBorder.ymax - m_viewerBorder.ymin);
  int minxchunk = minx / (int)m_chunkSize;
  int maxxchunk = (maxx + (int)m_chunkSize - 1) / (int)m_chunkSize;
  int minychunk = miny / (int)m_chunkSize;
  int maxychunk = (maxy + (int)m_chunkSize - 1) / (int)m_chunkSize;
  minxchunk = max_ii(minxchunk, 0);
  minychunk = max_ii(minychunk, 0);
  maxxchunk = min_ii(maxxchunk, (int)m_numberOfXChunks);
  maxychunk = min_ii(maxychunk, (int)m_numberOfYChunks);

  bool result = true;
  for (indexx = minxchunk; indexx < maxxchunk; indexx++) {
    for (indexy = minychunk; indexy < maxychunk; indexy++) {
      if (!scheduleChunkWhenPossible(graph, indexx, indexy)) {
        result = false;
      }
    }
  }

  return result;
}

bool ExecutionGroup::scheduleChunk(unsigned int chunkNumber)
{
  if (this->m_chunkExecutionStates[chunkNumber] == COM_ES_NOT_SCHEDULED) {
    this->m_chunkExecutionStates[chunkNumber] = COM_ES_SCHEDULED;
    WorkScheduler::schedule(this, chunkNumber);
    return true;
  }
  return false;
}

bool ExecutionGroup::scheduleChunkWhenPossible(ExecutionSystem *graph, int xChunk, int yChunk)
{
  if (xChunk < 0 || xChunk >= (int)this->m_numberOfXChunks) {
    return true;
  }
  if (yChunk < 0 || yChunk >= (int)this->m_numberOfYChunks) {
    return true;
  }
  int chunkNumber = yChunk * this->m_numberOfXChunks + xChunk;
  // chunk is already executed
  if (this->m_chunkExecutionStates[chunkNumber] == COM_ES_EXECUTED) {
    return true;
  }

  // chunk is scheduled, but not executed
  if (this->m_chunkExecutionStates[chunkNumber] == COM_ES_SCHEDULED) {
    return false;
  }

  // chunk is nor executed nor scheduled.
  vector<MemoryProxy *> memoryProxies;
  this->determineDependingMemoryProxies(&memoryProxies);

  rcti rect;
  determineChunkRect(&rect, xChunk, yChunk);
  unsigned int index;
  bool canBeExecuted = true;
  rcti area;

  for (index = 0; index < this->m_cachedReadOperations.size(); index++) {
    ReadBufferOperation *readOperation =
        (ReadBufferOperation *)this->m_cachedReadOperations[index];
    BLI_rcti_init(&area, 0, 0, 0, 0);
    MemoryProxy *memoryProxy = memoryProxies[index];
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

void ExecutionGroup::determineDependingAreaOfInterest(rcti *input,
                                                      ReadBufferOperation *readOperation,
                                                      rcti *output)
{
  this->getOutputOperation()->determineDependingAreaOfInterest(input, readOperation, output);
}

void ExecutionGroup::determineDependingMemoryProxies(vector<MemoryProxy *> *memoryProxies)
{
  unsigned int index;
  for (index = 0; index < this->m_cachedReadOperations.size(); index++) {
    ReadBufferOperation *readOperation =
        (ReadBufferOperation *)this->m_cachedReadOperations[index];
    memoryProxies->push_back(readOperation->getMemoryProxy());
  }
}

bool ExecutionGroup::isOpenCL()
{
  return this->m_openCL;
}

void ExecutionGroup::setViewerBorder(float xmin, float xmax, float ymin, float ymax)
{
  NodeOperation *operation = this->getOutputOperation();

  if (operation->isViewerOperation() || operation->isPreviewOperation()) {
    BLI_rcti_init(&this->m_viewerBorder,
                  xmin * this->m_width,
                  xmax * this->m_width,
                  ymin * this->m_height,
                  ymax * this->m_height);
  }
}

void ExecutionGroup::setRenderBorder(float xmin, float xmax, float ymin, float ymax)
{
  NodeOperation *operation = this->getOutputOperation();

  if (operation->isOutputOperation(true)) {
    /* Basically, setting border need to happen for only operations
     * which operates in render resolution buffers (like compositor
     * output nodes).
     *
     * In this cases adding border will lead to mapping coordinates
     * from output buffer space to input buffer spaces when executing
     * operation.
     *
     * But nodes like viewer and file output just shall display or
     * safe the same exact buffer which goes to their input, no need
     * in any kind of coordinates mapping.
     */

    bool operationNeedsBorder = !(operation->isViewerOperation() ||
                                  operation->isPreviewOperation() ||
                                  operation->isFileOutputOperation());

    if (operationNeedsBorder) {
      BLI_rcti_init(&this->m_viewerBorder,
                    xmin * this->m_width,
                    xmax * this->m_width,
                    ymin * this->m_height,
                    ymax * this->m_height);
    }
  }
}
