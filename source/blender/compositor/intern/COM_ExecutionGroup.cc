/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ExecutionGroup.h"
#include "COM_ChunkOrder.h"
#include "COM_Debug.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ViewerOperation.h"
#include "COM_WorkScheduler.h"
#include "COM_WriteBufferOperation.h"
#include "COM_defines.h"

#include "BLI_rand.hh"
#include "BLI_string.h"
#include "BLI_time.h"

#include "BLT_translation.h"

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
  BLI_rcti_init(&viewer_border_, 0, 0, 0, 0);
  execution_start_time_ = 0;
}

std::ostream &operator<<(std::ostream &os, const ExecutionGroup &execution_group)
{
  os << "ExecutionGroup(id=" << execution_group.get_id();
  os << ",flags={" << execution_group.get_flags() << "}";
  os << ",operation=" << *execution_group.get_output_operation() << "";
  os << ")";
  return os;
}

eCompositorPriority ExecutionGroup::get_render_priority()
{
  return this->get_output_operation()->get_render_priority();
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

bool ExecutionGroup::add_operation(NodeOperation *operation)
{
  if (!can_contain(*operation)) {
    return false;
  }

  if (!operation->get_flags().is_read_buffer_operation &&
      !operation->get_flags().is_write_buffer_operation)
  {
    flags_.complex = operation->get_flags().complex;
    flags_.open_cl = operation->get_flags().open_cl;
    flags_.single_threaded = operation->get_flags().single_threaded;
    flags_.initialized = true;
  }

  operations_.append(operation);

  return true;
}

NodeOperation *ExecutionGroup::get_output_operation() const
{
  /* The first operation of the group is always the output operation. */
  return this->operations_[0];
}

void ExecutionGroup::init_work_packages()
{
  work_packages_.clear();
  if (chunks_len_ != 0) {
    work_packages_.resize(chunks_len_);
    for (uint index = 0; index < chunks_len_; index++) {
      work_packages_[index].type = eWorkPackageType::Tile;
      work_packages_[index].state = eWorkPackageState::NotScheduled;
      work_packages_[index].execution_group = this;
      work_packages_[index].chunk_number = index;
      determine_chunk_rect(&work_packages_[index].rect, index);
    }
  }
}

void ExecutionGroup::init_read_buffer_operations()
{
  uint max_offset = 0;
  for (NodeOperation *operation : operations_) {
    if (operation->get_flags().is_read_buffer_operation) {
      ReadBufferOperation *read_operation = static_cast<ReadBufferOperation *>(operation);
      read_operations_.append(read_operation);
      max_offset = std::max(max_offset, read_operation->get_offset());
    }
  }
  max_offset++;
  max_read_buffer_offset_ = max_offset;
}

void ExecutionGroup::init_execution()
{
  init_number_of_chunks();
  init_work_packages();
  init_read_buffer_operations();
}

void ExecutionGroup::deinit_execution()
{
  work_packages_.clear();
  chunks_len_ = 0;
  x_chunks_len_ = 0;
  y_chunks_len_ = 0;
  read_operations_.clear();
  bTree_ = nullptr;
}

void ExecutionGroup::determine_resolution(uint resolution[2])
{
  NodeOperation *operation = this->get_output_operation();
  resolution[0] = operation->get_width();
  resolution[1] = operation->get_height();
  this->set_resolution(resolution);
  BLI_rcti_init(&viewer_border_, 0, width_, 0, height_);
}

void ExecutionGroup::init_number_of_chunks()
{
  if (flags_.single_threaded) {
    x_chunks_len_ = 1;
    y_chunks_len_ = 1;
    chunks_len_ = 1;
  }
  else {
    const float chunk_sizef = chunk_size_;
    const int border_width = BLI_rcti_size_x(&viewer_border_);
    const int border_height = BLI_rcti_size_y(&viewer_border_);
    x_chunks_len_ = ceil(border_width / chunk_sizef);
    y_chunks_len_ = ceil(border_height / chunk_sizef);
    chunks_len_ = x_chunks_len_ * y_chunks_len_;
  }
}

blender::Array<uint> ExecutionGroup::get_execution_order() const
{
  blender::Array<uint> chunk_order(chunks_len_);
  for (int chunk_index = 0; chunk_index < chunks_len_; chunk_index++) {
    chunk_order[chunk_index] = chunk_index;
  }

  NodeOperation *operation = this->get_output_operation();
  float centerX = 0.5f;
  float centerY = 0.5f;
  ChunkOrdering order_type = ChunkOrdering::Default;

  if (operation->get_flags().is_viewer_operation) {
    ViewerOperation *viewer = (ViewerOperation *)operation;
    centerX = viewer->getCenterX();
    centerY = viewer->getCenterY();
    order_type = viewer->get_chunk_order();
  }

  const int border_width = BLI_rcti_size_x(&viewer_border_);
  const int border_height = BLI_rcti_size_y(&viewer_border_);
  int index;
  switch (order_type) {
    case ChunkOrdering::Random: {
      static blender::RandomNumberGenerator rng;
      blender::MutableSpan<uint> span = chunk_order.as_mutable_span();
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
        chunk_orders[index].x = work_package.rect.xmin - viewer_border_.xmin;
        chunk_orders[index].y = work_package.rect.ymin - viewer_border_.ymin;
        chunk_orders[index].update_distance(&hotspot, 1);
      }

      std::sort(&chunk_orders[0], &chunk_orders[chunks_len_ - 1]);
      for (index = 0; index < chunks_len_; index++) {
        chunk_order[index] = chunk_orders[index].index;
      }

      break;
    }
    case ChunkOrdering::RuleOfThirds: {
      uint tx = border_width / 6;
      uint ty = border_height / 6;
      uint mx = border_width / 2;
      uint my = border_height / 2;
      uint bx = mx + 2 * tx;
      uint by = my + 2 * ty;
      float addition = chunks_len_ / COM_RULE_OF_THIRDS_DIVIDER;

      ChunkOrderHotspot hotspots[9] = {
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
        chunk_orders[index].x = work_package.rect.xmin - viewer_border_.xmin;
        chunk_orders[index].y = work_package.rect.ymin - viewer_border_.ymin;
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

void ExecutionGroup::execute(ExecutionSystem *graph)
{
  const CompositorContext &context = graph->get_context();
  const bNodeTree *bTree = context.get_bnodetree();
  if (width_ == 0 || height_ == 0) {
    return;
  } /** \note Break out... no pixels to calculate. */
  if (bTree->runtime->test_break && bTree->runtime->test_break(bTree->runtime->tbh)) {
    return;
  } /** \note Early break out for blur and preview nodes. */
  if (chunks_len_ == 0) {
    return;
  } /** \note Early break out. */
  uint chunk_index;

  execution_start_time_ = BLI_check_seconds_timer();

  chunks_finished_ = 0;
  bTree_ = bTree;

  blender::Array<uint> chunk_order = get_execution_order();

  DebugInfo::execution_group_started(this);
  DebugInfo::graphviz(graph);

  bool breaked = false;
  bool finished = false;
  uint start_index = 0;
  const int max_number_evaluated = BLI_system_thread_count() * 2;

  while (!finished && !breaked) {
    bool start_evaluated = false;
    finished = true;
    int number_evaluated = 0;

    for (int index = start_index; index < chunks_len_ && number_evaluated < max_number_evaluated;
         index++)
    {
      chunk_index = chunk_order[index];
      int y_chunk = chunk_index / x_chunks_len_;
      int x_chunk = chunk_index - (y_chunk * x_chunks_len_);
      const WorkPackage &work_package = work_packages_[chunk_index];
      switch (work_package.state) {
        case eWorkPackageState::NotScheduled: {
          schedule_chunk_when_possible(graph, x_chunk, y_chunk);
          finished = false;
          start_evaluated = true;
          number_evaluated++;

          if (bTree->runtime->update_draw) {
            bTree->runtime->update_draw(bTree->runtime->udh);
          }
          break;
        }
        case eWorkPackageState::Scheduled: {
          finished = false;
          start_evaluated = true;
          number_evaluated++;
          break;
        }
        case eWorkPackageState::Executed: {
          if (!start_evaluated) {
            start_index = index + 1;
          }
        }
      };
    }

    WorkScheduler::finish();

    if (bTree->runtime->test_break && bTree->runtime->test_break(bTree->runtime->tbh)) {
      breaked = true;
    }
  }
  DebugInfo::execution_group_finished(this);
  DebugInfo::graphviz(graph);
}

MemoryBuffer **ExecutionGroup::get_input_buffers_opencl(int chunk_number)
{
  WorkPackage &work_package = work_packages_[chunk_number];

  MemoryBuffer **memory_buffers = (MemoryBuffer **)MEM_callocN(
      sizeof(MemoryBuffer *) * max_read_buffer_offset_, __func__);
  rcti output;
  for (ReadBufferOperation *read_operation : read_operations_) {
    MemoryProxy *memory_proxy = read_operation->get_memory_proxy();
    this->determine_depending_area_of_interest(&work_package.rect, read_operation, &output);
    MemoryBuffer *memory_buffer =
        memory_proxy->get_executor()->construct_consolidated_memory_buffer(*memory_proxy, output);
    memory_buffers[read_operation->get_offset()] = memory_buffer;
  }
  return memory_buffers;
}

MemoryBuffer *ExecutionGroup::construct_consolidated_memory_buffer(MemoryProxy &memory_proxy,
                                                                   rcti &rect)
{
  MemoryBuffer *image_buffer = memory_proxy.get_buffer();
  MemoryBuffer *result = new MemoryBuffer(&memory_proxy, rect, MemoryBufferState::Temporary);
  result->fill_from(*image_buffer);
  return result;
}

void ExecutionGroup::finalize_chunk_execution(int chunk_number, MemoryBuffer **memory_buffers)
{
  WorkPackage &work_package = work_packages_[chunk_number];
  if (work_package.state == eWorkPackageState::Scheduled) {
    work_package.state = eWorkPackageState::Executed;
  }

  atomic_add_and_fetch_u(&chunks_finished_, 1);
  if (memory_buffers) {
    for (uint index = 0; index < max_read_buffer_offset_; index++) {
      MemoryBuffer *buffer = memory_buffers[index];
      if (buffer) {
        if (buffer->is_temporarily()) {
          memory_buffers[index] = nullptr;
          delete buffer;
        }
      }
    }
    MEM_freeN(memory_buffers);
  }
  if (bTree_) {
    /* Status report is only performed for top level Execution Groups. */
    float progress = chunks_finished_;
    progress /= chunks_len_;
    bTree_->runtime->progress(bTree_->runtime->prh, progress);

    char buf[128];
    SNPRINTF(buf, RPT_("Compositing | Tile %u-%u"), chunks_finished_, chunks_len_);
    bTree_->runtime->stats_draw(bTree_->runtime->sdh, buf);
  }
}

inline void ExecutionGroup::determine_chunk_rect(rcti *r_rect,
                                                 const uint x_chunk,
                                                 const uint y_chunk) const
{
  const int border_width = BLI_rcti_size_x(&viewer_border_);
  const int border_height = BLI_rcti_size_y(&viewer_border_);

  if (flags_.single_threaded) {
    BLI_rcti_init(r_rect, viewer_border_.xmin, border_width, viewer_border_.ymin, border_height);
  }
  else {
    const uint minx = x_chunk * chunk_size_ + viewer_border_.xmin;
    const uint miny = y_chunk * chunk_size_ + viewer_border_.ymin;
    const uint width = std::min(uint(viewer_border_.xmax), width_);
    const uint height = std::min(uint(viewer_border_.ymax), height_);
    BLI_rcti_init(r_rect,
                  std::min(minx, width_),
                  std::min(minx + chunk_size_, width),
                  std::min(miny, height_),
                  std::min(miny + chunk_size_, height));
  }
}

void ExecutionGroup::determine_chunk_rect(rcti *r_rect, const uint chunk_number) const
{
  const uint y_chunk = chunk_number / x_chunks_len_;
  const uint x_chunk = chunk_number - (y_chunk * x_chunks_len_);
  determine_chunk_rect(r_rect, x_chunk, y_chunk);
}

MemoryBuffer *ExecutionGroup::allocate_output_buffer(rcti &rect)
{
  /* We assume that this method is only called from complex execution groups. */
  NodeOperation *operation = this->get_output_operation();
  if (operation->get_flags().is_write_buffer_operation) {
    WriteBufferOperation *write_operation = (WriteBufferOperation *)operation;
    MemoryBuffer *buffer = new MemoryBuffer(
        write_operation->get_memory_proxy(), rect, MemoryBufferState::Temporary);
    return buffer;
  }
  return nullptr;
}

bool ExecutionGroup::schedule_area_when_possible(ExecutionSystem *graph, rcti *area)
{
  if (flags_.single_threaded) {
    return schedule_chunk_when_possible(graph, 0, 0);
  }
  /* Find all chunks inside the rect
   * determine `minxchunk`, `minychunk`, `maxxchunk`, `maxychunk`
   * where x and y are chunk-numbers. */

  int indexx, indexy;
  int minx = max_ii(area->xmin - viewer_border_.xmin, 0);
  int maxx = min_ii(area->xmax - viewer_border_.xmin, viewer_border_.xmax - viewer_border_.xmin);
  int miny = max_ii(area->ymin - viewer_border_.ymin, 0);
  int maxy = min_ii(area->ymax - viewer_border_.ymin, viewer_border_.ymax - viewer_border_.ymin);
  int minxchunk = minx / int(chunk_size_);
  int maxxchunk = (maxx + int(chunk_size_) - 1) / int(chunk_size_);
  int minychunk = miny / int(chunk_size_);
  int maxychunk = (maxy + int(chunk_size_) - 1) / int(chunk_size_);
  minxchunk = max_ii(minxchunk, 0);
  minychunk = max_ii(minychunk, 0);
  maxxchunk = min_ii(maxxchunk, int(x_chunks_len_));
  maxychunk = min_ii(maxychunk, int(y_chunks_len_));

  bool result = true;
  for (indexx = minxchunk; indexx < maxxchunk; indexx++) {
    for (indexy = minychunk; indexy < maxychunk; indexy++) {
      if (!schedule_chunk_when_possible(graph, indexx, indexy)) {
        result = false;
      }
    }
  }

  return result;
}

bool ExecutionGroup::schedule_chunk(uint chunk_number)
{
  WorkPackage &work_package = work_packages_[chunk_number];
  if (work_package.state == eWorkPackageState::NotScheduled) {
    work_package.state = eWorkPackageState::Scheduled;
    WorkScheduler::schedule(&work_package);
    return true;
  }
  return false;
}

bool ExecutionGroup::schedule_chunk_when_possible(ExecutionSystem *graph,
                                                  const int chunk_x,
                                                  const int chunk_y)
{
  if (chunk_x < 0 || chunk_x >= int(x_chunks_len_)) {
    return true;
  }
  if (chunk_y < 0 || chunk_y >= int(y_chunks_len_)) {
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
    MemoryProxy *memory_proxy = read_operation->get_memory_proxy();
    determine_depending_area_of_interest(&work_package.rect, read_operation, &area);
    ExecutionGroup *group = memory_proxy->get_executor();

    if (!group->schedule_area_when_possible(graph, &area)) {
      can_be_executed = false;
    }
  }

  if (can_be_executed) {
    schedule_chunk(chunk_index);
  }

  return false;
}

void ExecutionGroup::determine_depending_area_of_interest(rcti *input,
                                                          ReadBufferOperation *read_operation,
                                                          rcti *output)
{
  this->get_output_operation()->determine_depending_area_of_interest(
      input, read_operation, output);
}

void ExecutionGroup::set_viewer_border(float xmin, float xmax, float ymin, float ymax)
{
  const NodeOperation &operation = *this->get_output_operation();
  if (operation.get_flags().use_viewer_border) {
    BLI_rcti_init(&viewer_border_, xmin * width_, xmax * width_, ymin * height_, ymax * height_);
  }
}

void ExecutionGroup::set_render_border(float xmin, float xmax, float ymin, float ymax)
{
  const NodeOperation &operation = *this->get_output_operation();
  if (operation.is_output_operation(true) && operation.get_flags().use_render_border) {
    BLI_rcti_init(&viewer_border_, xmin * width_, xmax * width_, ymin * height_, ymax * height_);
  }
}

}  // namespace blender::compositor
