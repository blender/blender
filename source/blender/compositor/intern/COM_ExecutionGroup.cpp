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
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "atomic_ops.h"

#include "COM_ChunkOrder.h"
#include "COM_Debug.h"
#include "COM_ExecutionGroup.h"
#include "COM_ExecutionSystem.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_WorkScheduler.h"
#include "COM_WriteBufferOperation.h"
#include "COM_defines.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLT_translation.h"
#include "MEM_guardedalloc.h"
#include "PIL_time.h"
#include "WM_api.h"
#include "WM_types.h"

ExecutionGroup::ExecutionGroup()
{
  this->m_is_output = false;
  this->m_complex = false;
  this->m_bTree = nullptr;
  this->m_height = 0;
  this->m_width = 0;
  this->m_max_read_buffer_offset = 0;
  this->m_x_chunks_len = 0;
  this->m_y_chunks_len = 0;
  this->m_chunks_len = 0;
  this->m_initialized = false;
  this->m_openCL = false;
  this->m_singleThreaded = false;
  this->m_chunks_finished = 0;
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

  m_operations.append(operation);

  return true;
}

NodeOperation *ExecutionGroup::getOutputOperation() const
{
  return this
      ->m_operations[0]; /* the first operation of the group is always the output operation. */
}

void ExecutionGroup::initExecution()
{
  m_chunk_execution_states.clear();
  determineNumberOfChunks();

  if (this->m_chunks_len != 0) {
    m_chunk_execution_states.resize(this->m_chunks_len);
    for (int index = 0; index < this->m_chunks_len; index++) {
      m_chunk_execution_states[index] = eChunkExecutionState::NOT_SCHEDULED;
    }
  }

  unsigned int max_offset = 0;

  for (NodeOperation *operation : m_operations) {
    if (operation->isReadBufferOperation()) {
      ReadBufferOperation *readOperation = static_cast<ReadBufferOperation *>(operation);
      this->m_read_operations.append(readOperation);
      max_offset = MAX2(max_offset, readOperation->getOffset());
    }
  }
  max_offset++;
  this->m_max_read_buffer_offset = max_offset;
}

void ExecutionGroup::deinitExecution()
{
  m_chunk_execution_states.clear();
  this->m_chunks_len = 0;
  this->m_x_chunks_len = 0;
  this->m_y_chunks_len = 0;
  this->m_read_operations.clear();
  this->m_bTree = nullptr;
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
    this->m_x_chunks_len = 1;
    this->m_y_chunks_len = 1;
    this->m_chunks_len = 1;
  }
  else {
    const float chunkSizef = this->m_chunkSize;
    const int border_width = BLI_rcti_size_x(&this->m_viewerBorder);
    const int border_height = BLI_rcti_size_y(&this->m_viewerBorder);
    this->m_x_chunks_len = ceil(border_width / chunkSizef);
    this->m_y_chunks_len = ceil(border_height / chunkSizef);
    this->m_chunks_len = this->m_x_chunks_len * this->m_y_chunks_len;
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
  } /** \note Break out... no pixels to calculate. */
  if (bTree->test_break && bTree->test_break(bTree->tbh)) {
    return;
  } /** \note Early break out for blur and preview nodes. */
  if (this->m_chunks_len == 0) {
    return;
  } /** \note Early break out. */
  unsigned int chunkNumber;

  this->m_executionStartTime = PIL_check_seconds_timer();

  this->m_chunks_finished = 0;
  this->m_bTree = bTree;
  unsigned int index;
  unsigned int *chunkOrder = (unsigned int *)MEM_mallocN(sizeof(unsigned int) * this->m_chunks_len,
                                                         __func__);

  for (chunkNumber = 0; chunkNumber < this->m_chunks_len; chunkNumber++) {
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
      for (index = 0; index < 2 * this->m_chunks_len; index++) {
        int index1 = rand() % this->m_chunks_len;
        int index2 = rand() % this->m_chunks_len;
        int s = chunkOrder[index1];
        chunkOrder[index1] = chunkOrder[index2];
        chunkOrder[index2] = s;
      }
      break;
    case COM_TO_CENTER_OUT: {
      ChunkOrderHotspot *hotspots[1];
      hotspots[0] = new ChunkOrderHotspot(border_width * centerX, border_height * centerY, 0.0f);
      rcti rect;
      ChunkOrder *chunkOrders = (ChunkOrder *)MEM_mallocN(sizeof(ChunkOrder) * this->m_chunks_len,
                                                          __func__);
      for (index = 0; index < this->m_chunks_len; index++) {
        determineChunkRect(&rect, index);
        chunkOrders[index].number = index;
        chunkOrders[index].x = rect.xmin - this->m_viewerBorder.xmin;
        chunkOrders[index].y = rect.ymin - this->m_viewerBorder.ymin;
        chunkOrders[index].update_distance(hotspots, 1);
      }

      std::sort(&chunkOrders[0], &chunkOrders[this->m_chunks_len - 1]);
      for (index = 0; index < this->m_chunks_len; index++) {
        chunkOrder[index] = chunkOrders[index].number;
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

      float addition = this->m_chunks_len / COM_RULE_OF_THIRDS_DIVIDER;
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
      ChunkOrder *chunkOrders = (ChunkOrder *)MEM_mallocN(sizeof(ChunkOrder) * this->m_chunks_len,
                                                          __func__);
      for (index = 0; index < this->m_chunks_len; index++) {
        determineChunkRect(&rect, index);
        chunkOrders[index].number = index;
        chunkOrders[index].x = rect.xmin - this->m_viewerBorder.xmin;
        chunkOrders[index].y = rect.ymin - this->m_viewerBorder.ymin;
        chunkOrders[index].update_distance(hotspots, 9);
      }

      std::sort(&chunkOrders[0], &chunkOrders[this->m_chunks_len]);

      for (index = 0; index < this->m_chunks_len; index++) {
        chunkOrder[index] = chunkOrders[index].number;
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

    for (index = startIndex; index < this->m_chunks_len && numberEvaluated < maxNumberEvaluated;
         index++) {
      chunkNumber = chunkOrder[index];
      int yChunk = chunkNumber / this->m_x_chunks_len;
      int xChunk = chunkNumber - (yChunk * this->m_x_chunks_len);
      switch (m_chunk_execution_states[chunkNumber]) {
        case eChunkExecutionState::NOT_SCHEDULED: {
          scheduleChunkWhenPossible(graph, xChunk, yChunk);
          finished = false;
          startEvaluated = true;
          numberEvaluated++;

          if (bTree->update_draw) {
            bTree->update_draw(bTree->udh);
          }
          break;
        }
        case eChunkExecutionState::SCHEDULED: {
          finished = false;
          startEvaluated = true;
          numberEvaluated++;
          break;
        }
        case eChunkExecutionState::EXECUTED: {
          if (!startEvaluated) {
            startIndex = index + 1;
          }
        }
      };
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
  std::vector<MemoryProxy *> memoryproxies;
  determineChunkRect(&rect, chunkNumber);

  this->determineDependingMemoryProxies(&memoryproxies);
  MemoryBuffer **memoryBuffers = (MemoryBuffer **)MEM_callocN(
      sizeof(MemoryBuffer *) * this->m_max_read_buffer_offset, __func__);
  rcti output;
  for (ReadBufferOperation *readOperation : m_read_operations) {
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
  if (this->m_chunk_execution_states[chunkNumber] == eChunkExecutionState::SCHEDULED) {
    this->m_chunk_execution_states[chunkNumber] = eChunkExecutionState::EXECUTED;
  }

  atomic_add_and_fetch_u(&this->m_chunks_finished, 1);
  if (memoryBuffers) {
    for (unsigned int index = 0; index < this->m_max_read_buffer_offset; index++) {
      MemoryBuffer *buffer = memoryBuffers[index];
      if (buffer) {
        if (buffer->isTemporarily()) {
          memoryBuffers[index] = nullptr;
          delete buffer;
        }
      }
    }
    MEM_freeN(memoryBuffers);
  }
  if (this->m_bTree) {
    // status report is only performed for top level Execution Groups.
    float progress = this->m_chunks_finished;
    progress /= this->m_chunks_len;
    this->m_bTree->progress(this->m_bTree->prh, progress);

    char buf[128];
    BLI_snprintf(buf,
                 sizeof(buf),
                 TIP_("Compositing | Tile %u-%u"),
                 this->m_chunks_finished,
                 this->m_chunks_len);
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
    const unsigned int width = MIN2((unsigned int)this->m_viewerBorder.xmax, this->m_width);
    const unsigned int height = MIN2((unsigned int)this->m_viewerBorder.ymax, this->m_height);
    BLI_rcti_init(rect,
                  MIN2(minx, this->m_width),
                  MIN2(minx + this->m_chunkSize, width),
                  MIN2(miny, this->m_height),
                  MIN2(miny + this->m_chunkSize, height));
  }
}

void ExecutionGroup::determineChunkRect(rcti *rect, const unsigned int chunkNumber) const
{
  const unsigned int yChunk = chunkNumber / this->m_x_chunks_len;
  const unsigned int xChunk = chunkNumber - (yChunk * this->m_x_chunks_len);
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
  return nullptr;
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
  maxxchunk = min_ii(maxxchunk, (int)m_x_chunks_len);
  maxychunk = min_ii(maxychunk, (int)m_y_chunks_len);

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
  if (this->m_chunk_execution_states[chunkNumber] == eChunkExecutionState::NOT_SCHEDULED) {
    this->m_chunk_execution_states[chunkNumber] = eChunkExecutionState::SCHEDULED;
    WorkScheduler::schedule(this, chunkNumber);
    return true;
  }
  return false;
}

bool ExecutionGroup::scheduleChunkWhenPossible(ExecutionSystem *graph, int xChunk, int yChunk)
{
  if (xChunk < 0 || xChunk >= (int)this->m_x_chunks_len) {
    return true;
  }
  if (yChunk < 0 || yChunk >= (int)this->m_y_chunks_len) {
    return true;
  }
  int chunkNumber = yChunk * this->m_x_chunks_len + xChunk;
  // chunk is already executed
  if (this->m_chunk_execution_states[chunkNumber] == eChunkExecutionState::EXECUTED) {
    return true;
  }

  // chunk is scheduled, but not executed
  if (this->m_chunk_execution_states[chunkNumber] == eChunkExecutionState::SCHEDULED) {
    return false;
  }

  // chunk is nor executed nor scheduled.
  std::vector<MemoryProxy *> memoryProxies;
  this->determineDependingMemoryProxies(&memoryProxies);

  rcti rect;
  determineChunkRect(&rect, xChunk, yChunk);
  unsigned int index;
  bool canBeExecuted = true;
  rcti area;

  for (index = 0; index < m_read_operations.size(); index++) {
    ReadBufferOperation *readOperation = m_read_operations[index];
    BLI_rcti_init(&area, 0, 0, 0, 0);
    MemoryProxy *memoryProxy = memoryProxies[index];
    determineDependingAreaOfInterest(&rect, readOperation, &area);
    ExecutionGroup *group = memoryProxy->getExecutor();

    if (group != nullptr) {
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

void ExecutionGroup::determineDependingMemoryProxies(std::vector<MemoryProxy *> *memoryProxies)
{
  for (ReadBufferOperation *readOperation : m_read_operations) {
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
