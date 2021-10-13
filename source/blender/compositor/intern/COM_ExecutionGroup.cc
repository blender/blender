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

#include "COM_ExecutionGroup.h"
#include "COM_ChunkOrder.h"
#include "COM_Debug.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_WorkScheduler.h"
#include "COM_WriteBufferOperation.h"
#include "COM_defines.h"

#include "BLI_rand.hh"

#include "BLT_translation.h"

#include "PIL_time.h"

namespace blender::compositor {

std::ostream &operator<<(std::ostream &os, const ExecutionGroupFlags &flags)
{
  if (flags.initialized) {
    os << "init,";
  }
  if (flags.is_output) {
    os << "output,";
  }
  if (flags.complex) {
    os << "complex,";
  }
  if (flags.open_cl) {
    os << "open_cl,";
  }
  if (flags.single_threaded) {
    os << "single_threaded,";
  }
  return os;
}

ExecutionGroup::ExecutionGroup(int id)
{
  id_ = id;
  bTree_ = nullptr;
  height_ = 0;
  width_ = 0;
  max_read_buffer_offset_ = 0;
  x_chunks_len_ = 0;
  y_chunks_len_ = 0;
  chunks_len_ = 0;
  chunks_finished_ = 0;
  BLI_rcti_init(&viewerBorder_, 0, 0, 0, 0);
  executionStartTime_ = 0;
}

std::ostream &operator<<(std::ostream &os, const ExecutionGroup &execution_group)
{
  os << "ExecutionGroup(id=" << execution_group.get_id();
  os << ",flags={" << execution_group.get_flags() << "}";
  os << ",operation=" << *execution_group.getOutputOperation() << "";
  os << ")";
  return os;
}

eCompositorPriority ExecutionGroup::getRenderPriority()
{
  return this->getOutputOperation()->getRenderPriority();
}

bool ExecutionGroup::can_contain(NodeOperation &operation)
{
  if (!flags_.initialized) {
    return true;
  }

  if (operation.get_flags().is_read_buffer_operation) {
    return true;
  }
  if (operation.get_flags().is_write_buffer_operation) {
    return false;
  }
  if (operation.get_flags().is_set_operation) {
    return true;
  }

  /* complex groups don't allow further ops (except read buffer and values, see above) */
  if (flags_.complex) {
    return false;
  }
  /* complex ops can't be added to other groups (except their own, which they initialize, see
   * above) */
  if (operation.get_flags().complex) {
    return false;
  }

  return true;
}

bool ExecutionGroup::addOperation(NodeOperation *operation)
{
  if (!can_contain(*operation)) {
    return false;
  }

  if (!operation->get_flags().is_read_buffer_operation &&
      !operation->get_flags().is_write_buffer_operation) {
    flags_.complex = operation->get_flags().complex;
    flags_.open_cl = operation->get_flags().open_cl;
    flags_.single_threaded = operation->get_flags().single_threaded;
    flags_.initialized = true;
  }

  operations_.append(operation);

  return true;
}

NodeOperation *ExecutionGroup::getOutputOperation() const
{
  return this
      ->operations_[0]; /* the first operation of the group is always the output operation. */
}

void ExecutionGroup::init_work_packages()
{
  work_packages_.clear();
  if (chunks_len_ != 0) {
    work_packages_.resize(chunks_len_);
    for (unsigned int index = 0; index < chunks_len_; index++) {
      work_packages_[index].type = eWorkPackageType::Tile;
      work_packages_[index].state = eWorkPackageState::NotScheduled;
      work_packages_[index].execution_group = this;
      work_packages_[index].chunk_number = index;
      determineChunkRect(&work_packages_[index].rect, index);
    }
  }
}

void ExecutionGroup::init_read_buffer_operations()
{
  unsigned int max_offset = 0;
  for (NodeOperation *operation : operations_) {
    if (operation->get_flags().is_read_buffer_operation) {
      ReadBufferOperation *readOperation = static_cast<ReadBufferOperation *>(operation);
      read_operations_.append(readOperation);
      max_offset = MAX2(max_offset, readOperation->getOffset());
    }
  }
  max_offset++;
  max_read_buffer_offset_ = max_offset;
}

void ExecutionGroup::initExecution()
{
  init_number_of_chunks();
  init_work_packages();
  init_read_buffer_operations();
}

void ExecutionGroup::deinitExecution()
{
  work_packages_.clear();
  chunks_len_ = 0;
  x_chunks_len_ = 0;
  y_chunks_len_ = 0;
  read_operations_.clear();
  bTree_ = nullptr;
}

void ExecutionGroup::determineResolution(unsigned int resolution[2])
{
  NodeOperation *operation = this->getOutputOperation();
  resolution[0] = operation->getWidth();
  resolution[1] = operation->getHeight();
  this->setResolution(resolution);
  BLI_rcti_init(&viewerBorder_, 0, width_, 0, height_);
}

void ExecutionGroup::init_number_of_chunks()
{
  if (flags_.single_threaded) {
    x_chunks_len_ = 1;
    y_chunks_len_ = 1;
    chunks_len_ = 1;
  }
  else {
    const float chunkSizef = chunkSize_;
    const int border_width = BLI_rcti_size_x(&viewerBorder_);
    const int border_height = BLI_rcti_size_y(&viewerBorder_);
    x_chunks_len_ = ceil(border_width / chunkSizef);
    y_chunks_len_ = ceil(border_height / chunkSizef);
    chunks_len_ = x_chunks_len_ * y_chunks_len_;
  }
}

blender::Array<unsigned int> ExecutionGroup::get_execution_order() const
{
  blender::Array<unsigned int> chunk_order(chunks_len_);
  for (int chunk_index = 0; chunk_index < chunks_len_; chunk_index++) {
    chunk_order[chunk_index] = chunk_index;
  }

  NodeOperation *operation = this->getOutputOperation();
  float centerX = 0.5f;
  float centerY = 0.5f;
  ChunkOrdering order_type = ChunkOrdering::Default;

  if (operation->get_flags().is_viewer_operation) {
    ViewerOperation *viewer = (ViewerOperation *)operation;
    centerX = viewer->getCenterX();
    centerY = viewer->getCenterY();
    order_type = viewer->getChunkOrder();
  }

  const int border_width = BLI_rcti_size_x(&viewerBorder_);
  const int border_height = BLI_rcti_size_y(&viewerBorder_);
  int index;
  switch (order_type) {
    case ChunkOrdering::Random: {
      static blender::RandomNumberGenerator rng;
      blender::MutableSpan<unsigned int> span = chunk_order.as_mutable_span();
      /* Shuffle twice to make it more random. */
      rng.shuffle(span);
      rng.shuffle(span);
      break;
    }
    case ChunkOrdering::CenterOut: {
      ChunkOrderHotspot hotspot(border_width * centerX, border_height * centerY, 0.0f);
      blender::Array<ChunkOrder> chunk_orders(chunks_len_);
      for (index = 0; index < chunks_len_; index++) {
        const WorkPackage &work_package = work_packages_[index];
        chunk_orders[index].index = index;
        chunk_orders[index].x = work_package.rect.xmin - viewerBorder_.xmin;
        chunk_orders[index].y = work_package.rect.ymin - viewerBorder_.ymin;
        chunk_orders[index].update_distance(&hotspot, 1);
      }

      std::sort(&chunk_orders[0], &chunk_orders[chunks_len_ - 1]);
      for (index = 0; index < chunks_len_; index++) {
        chunk_order[index] = chunk_orders[index].index;
      }

      break;
    }
    case ChunkOrdering::RuleOfThirds: {
      unsigned int tx = border_width / 6;
      unsigned int ty = border_height / 6;
      unsigned int mx = border_width / 2;
      unsigned int my = border_height / 2;
      unsigned int bx = mx + 2 * tx;
      unsigned int by = my + 2 * ty;
      float addition = chunks_len_ / COM_RULE_OF_THIRDS_DIVIDER;

      ChunkOrderHotspot hotspots[9]{
          ChunkOrderHotspot(mx, my, addition * 0),
          ChunkOrderHotspot(tx, my, addition * 1),
          ChunkOrderHotspot(bx, my, addition * 2),
          ChunkOrderHotspot(bx, by, addition * 3),
          ChunkOrderHotspot(tx, ty, addition * 4),
          ChunkOrderHotspot(bx, ty, addition * 5),
          ChunkOrderHotspot(tx, by, addition * 6),
          ChunkOrderHotspot(mx, ty, addition * 7),
          ChunkOrderHotspot(mx, by, addition * 8),
      };

      blender::Array<ChunkOrder> chunk_orders(chunks_len_);
      for (index = 0; index < chunks_len_; index++) {
        const WorkPackage &work_package = work_packages_[index];
        chunk_orders[index].index = index;
        chunk_orders[index].x = work_package.rect.xmin - viewerBorder_.xmin;
        chunk_orders[index].y = work_package.rect.ymin - viewerBorder_.ymin;
        chunk_orders[index].update_distance(hotspots, 9);
      }

      std::sort(&chunk_orders[0], &chunk_orders[chunks_len_]);

      for (index = 0; index < chunks_len_; index++) {
        chunk_order[index] = chunk_orders[index].index;
      }

      break;
    }
    case ChunkOrdering::TopDown:
    default:
      break;
  }
  return chunk_order;
}

/**
 * this method is called for the top execution groups. containing the compositor node or the
 * preview node or the viewer node)
 */
void ExecutionGroup::execute(ExecutionSystem *graph)
{
  const CompositorContext &context = graph->getContext();
  const bNodeTree *bTree = context.getbNodeTree();
  if (width_ == 0 || height_ == 0) {
    return;
  } /** \note Break out... no pixels to calculate. */
  if (bTree->test_break && bTree->test_break(bTree->tbh)) {
    return;
  } /** \note Early break out for blur and preview nodes. */
  if (chunks_len_ == 0) {
    return;
  } /** \note Early break out. */
  unsigned int chunk_index;

  executionStartTime_ = PIL_check_seconds_timer();

  chunks_finished_ = 0;
  bTree_ = bTree;

  blender::Array<unsigned int> chunk_order = get_execution_order();

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

    for (int index = startIndex; index < chunks_len_ && numberEvaluated < maxNumberEvaluated;
         index++) {
      chunk_index = chunk_order[index];
      int yChunk = chunk_index / x_chunks_len_;
      int xChunk = chunk_index - (yChunk * x_chunks_len_);
      const WorkPackage &work_package = work_packages_[chunk_index];
      switch (work_package.state) {
        case eWorkPackageState::NotScheduled: {
          scheduleChunkWhenPossible(graph, xChunk, yChunk);
          finished = false;
          startEvaluated = true;
          numberEvaluated++;

          if (bTree->update_draw) {
            bTree->update_draw(bTree->udh);
          }
          break;
        }
        case eWorkPackageState::Scheduled: {
          finished = false;
          startEvaluated = true;
          numberEvaluated++;
          break;
        }
        case eWorkPackageState::Executed: {
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
}

MemoryBuffer **ExecutionGroup::getInputBuffersOpenCL(int chunkNumber)
{
  WorkPackage &work_package = work_packages_[chunkNumber];

  MemoryBuffer **memoryBuffers = (MemoryBuffer **)MEM_callocN(
      sizeof(MemoryBuffer *) * max_read_buffer_offset_, __func__);
  rcti output;
  for (ReadBufferOperation *readOperation : read_operations_) {
    MemoryProxy *memoryProxy = readOperation->getMemoryProxy();
    this->determineDependingAreaOfInterest(&work_package.rect, readOperation, &output);
    MemoryBuffer *memoryBuffer = memoryProxy->getExecutor()->constructConsolidatedMemoryBuffer(
        *memoryProxy, output);
    memoryBuffers[readOperation->getOffset()] = memoryBuffer;
  }
  return memoryBuffers;
}

MemoryBuffer *ExecutionGroup::constructConsolidatedMemoryBuffer(MemoryProxy &memoryProxy,
                                                                rcti &rect)
{
  MemoryBuffer *imageBuffer = memoryProxy.getBuffer();
  MemoryBuffer *result = new MemoryBuffer(&memoryProxy, rect, MemoryBufferState::Temporary);
  result->fill_from(*imageBuffer);
  return result;
}

void ExecutionGroup::finalizeChunkExecution(int chunkNumber, MemoryBuffer **memoryBuffers)
{
  WorkPackage &work_package = work_packages_[chunkNumber];
  if (work_package.state == eWorkPackageState::Scheduled) {
    work_package.state = eWorkPackageState::Executed;
  }

  atomic_add_and_fetch_u(&chunks_finished_, 1);
  if (memoryBuffers) {
    for (unsigned int index = 0; index < max_read_buffer_offset_; index++) {
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
  if (bTree_) {
    /* Status report is only performed for top level Execution Groups. */
    float progress = chunks_finished_;
    progress /= chunks_len_;
    bTree_->progress(bTree_->prh, progress);

    char buf[128];
    BLI_snprintf(
        buf, sizeof(buf), TIP_("Compositing | Tile %u-%u"), chunks_finished_, chunks_len_);
    bTree_->stats_draw(bTree_->sdh, buf);
  }
}

inline void ExecutionGroup::determineChunkRect(rcti *r_rect,
                                               const unsigned int xChunk,
                                               const unsigned int yChunk) const
{
  const int border_width = BLI_rcti_size_x(&viewerBorder_);
  const int border_height = BLI_rcti_size_y(&viewerBorder_);

  if (flags_.single_threaded) {
    BLI_rcti_init(r_rect, viewerBorder_.xmin, border_width, viewerBorder_.ymin, border_height);
  }
  else {
    const unsigned int minx = xChunk * chunkSize_ + viewerBorder_.xmin;
    const unsigned int miny = yChunk * chunkSize_ + viewerBorder_.ymin;
    const unsigned int width = MIN2((unsigned int)viewerBorder_.xmax, width_);
    const unsigned int height = MIN2((unsigned int)viewerBorder_.ymax, height_);
    BLI_rcti_init(r_rect,
                  MIN2(minx, width_),
                  MIN2(minx + chunkSize_, width),
                  MIN2(miny, height_),
                  MIN2(miny + chunkSize_, height));
  }
}

void ExecutionGroup::determineChunkRect(rcti *r_rect, const unsigned int chunkNumber) const
{
  const unsigned int yChunk = chunkNumber / x_chunks_len_;
  const unsigned int xChunk = chunkNumber - (yChunk * x_chunks_len_);
  determineChunkRect(r_rect, xChunk, yChunk);
}

MemoryBuffer *ExecutionGroup::allocateOutputBuffer(rcti &rect)
{
  /* We assume that this method is only called from complex execution groups. */
  NodeOperation *operation = this->getOutputOperation();
  if (operation->get_flags().is_write_buffer_operation) {
    WriteBufferOperation *writeOperation = (WriteBufferOperation *)operation;
    MemoryBuffer *buffer = new MemoryBuffer(
        writeOperation->getMemoryProxy(), rect, MemoryBufferState::Temporary);
    return buffer;
  }
  return nullptr;
}

bool ExecutionGroup::scheduleAreaWhenPossible(ExecutionSystem *graph, rcti *area)
{
  if (flags_.single_threaded) {
    return scheduleChunkWhenPossible(graph, 0, 0);
  }
  /* Find all chunks inside the rect
   * determine `minxchunk`, `minychunk`, `maxxchunk`, `maxychunk`
   * where x and y are chunk-numbers. */

  int indexx, indexy;
  int minx = max_ii(area->xmin - viewerBorder_.xmin, 0);
  int maxx = min_ii(area->xmax - viewerBorder_.xmin, viewerBorder_.xmax - viewerBorder_.xmin);
  int miny = max_ii(area->ymin - viewerBorder_.ymin, 0);
  int maxy = min_ii(area->ymax - viewerBorder_.ymin, viewerBorder_.ymax - viewerBorder_.ymin);
  int minxchunk = minx / (int)chunkSize_;
  int maxxchunk = (maxx + (int)chunkSize_ - 1) / (int)chunkSize_;
  int minychunk = miny / (int)chunkSize_;
  int maxychunk = (maxy + (int)chunkSize_ - 1) / (int)chunkSize_;
  minxchunk = max_ii(minxchunk, 0);
  minychunk = max_ii(minychunk, 0);
  maxxchunk = min_ii(maxxchunk, (int)x_chunks_len_);
  maxychunk = min_ii(maxychunk, (int)y_chunks_len_);

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
  WorkPackage &work_package = work_packages_[chunkNumber];
  if (work_package.state == eWorkPackageState::NotScheduled) {
    work_package.state = eWorkPackageState::Scheduled;
    WorkScheduler::schedule(&work_package);
    return true;
  }
  return false;
}

bool ExecutionGroup::scheduleChunkWhenPossible(ExecutionSystem *graph,
                                               const int chunk_x,
                                               const int chunk_y)
{
  if (chunk_x < 0 || chunk_x >= (int)x_chunks_len_) {
    return true;
  }
  if (chunk_y < 0 || chunk_y >= (int)y_chunks_len_) {
    return true;
  }

  /* Check if chunk is already executed or scheduled and not yet executed. */
  const int chunk_index = chunk_y * x_chunks_len_ + chunk_x;
  WorkPackage &work_package = work_packages_[chunk_index];
  if (work_package.state == eWorkPackageState::Executed) {
    return true;
  }
  if (work_package.state == eWorkPackageState::Scheduled) {
    return false;
  }

  bool can_be_executed = true;
  rcti area;

  for (ReadBufferOperation *read_operation : read_operations_) {
    BLI_rcti_init(&area, 0, 0, 0, 0);
    MemoryProxy *memory_proxy = read_operation->getMemoryProxy();
    determineDependingAreaOfInterest(&work_package.rect, read_operation, &area);
    ExecutionGroup *group = memory_proxy->getExecutor();

    if (!group->scheduleAreaWhenPossible(graph, &area)) {
      can_be_executed = false;
    }
  }

  if (can_be_executed) {
    scheduleChunk(chunk_index);
  }

  return false;
}

void ExecutionGroup::determineDependingAreaOfInterest(rcti *input,
                                                      ReadBufferOperation *readOperation,
                                                      rcti *output)
{
  this->getOutputOperation()->determineDependingAreaOfInterest(input, readOperation, output);
}

void ExecutionGroup::setViewerBorder(float xmin, float xmax, float ymin, float ymax)
{
  const NodeOperation &operation = *this->getOutputOperation();
  if (operation.get_flags().use_viewer_border) {
    BLI_rcti_init(&viewerBorder_, xmin * width_, xmax * width_, ymin * height_, ymax * height_);
  }
}

void ExecutionGroup::setRenderBorder(float xmin, float xmax, float ymin, float ymax)
{
  const NodeOperation &operation = *this->getOutputOperation();
  if (operation.isOutputOperation(true) && operation.get_flags().use_render_border) {
    BLI_rcti_init(&viewerBorder_, xmin * width_, xmax * width_, ymin * height_, ymax * height_);
  }
}

}  // namespace blender::compositor
