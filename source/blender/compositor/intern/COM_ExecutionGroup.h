/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

#include <iostream>

#include "BLI_array.hh"
#include "BLI_vector.hh"

#include "COM_Enums.h"
#include "COM_WorkPackage.h"

#include "DNA_node_types.h"
#include "DNA_vec_types.h"

namespace blender::compositor {

class ExecutionSystem;
class NodeOperation;
class MemoryProxy;
class MemoryBuffer;
class ReadBufferOperation;

struct ExecutionGroupFlags {
  bool initialized : 1;
  /**
   * Is this ExecutionGroup an output ExecutionGroup
   * An OutputExecution group are groups containing a
   * ViewerOperation, CompositeOperation, PreviewOperation.
   */
  bool is_output : 1;
  bool complex : 1;

  /**
   * Can this ExecutionGroup be scheduled on an OpenCLDevice.
   */
  bool open_cl : 1;

  /**
   * Schedule this execution group as a single chunk. This
   * chunk will be executed by a single thread.
   */
  bool single_threaded : 1;

  ExecutionGroupFlags()
  {
    initialized = false;
    is_output = false;
    complex = false;
    open_cl = false;
    single_threaded = false;
  }
};

std::ostream &operator<<(std::ostream &os, const ExecutionGroupFlags &flags);

/**
 * \brief Class ExecutionGroup is a group of Operations that are executed as one.
 * This grouping is used to combine Operations that can be executed as one whole when
 * multi-processing.
 * \ingroup Execution
 */
class ExecutionGroup {
 private:
  // fields
  /**
   * Id of the execution group. For debugging purposes.
   */
  int id_;

  /**
   * \brief list of operations in this ExecutionGroup
   */
  Vector<NodeOperation *> operations_;

  ExecutionGroupFlags flags_;

  /**
   * \brief Width of the output
   */
  unsigned int width_;

  /**
   * \brief Height of the output
   */
  unsigned int height_;

  /**
   * \brief size of a single chunk, being Width or of height
   * a chunk is always a square, except at the edges of the MemoryBuffer
   */
  unsigned int chunk_size_;

  /**
   * \brief number of chunks in the x-axis
   */
  unsigned int x_chunks_len_;

  /**
   * \brief number of chunks in the y-axis
   */
  unsigned int y_chunks_len_;

  /**
   * \brief total number of chunks
   */
  unsigned int chunks_len_;

  /**
   * \brief what is the maximum number field of all ReadBufferOperation in this ExecutionGroup.
   * \note this is used to construct the MemoryBuffers that will be passed during execution.
   */
  unsigned int max_read_buffer_offset_;

  /**
   * \brief All read operations of this execution group.
   */
  Vector<ReadBufferOperation *> read_operations_;

  /**
   * \brief reference to the original bNodeTree,
   * this field is only set for the 'top' execution group.
   * \note can only be used to call the callbacks for progress, status and break.
   */
  const bNodeTree *bTree_;

  /**
   * \brief total number of chunks that have been calculated for this ExecutionGroup
   */
  unsigned int chunks_finished_;

  /**
   * \brief work_packages_ holds all unit of work.
   */
  Vector<WorkPackage> work_packages_;

  /**
   * \brief denotes boundary for border compositing
   * \note measured in pixel space
   */
  rcti viewer_border_;

  /**
   * \brief start time of execution
   */
  double execution_start_time_;

  // methods
  /**
   * \brief check whether parameter operation can be added to the execution group
   * \param operation: the operation to be added
   */
  bool can_contain(NodeOperation &operation);

  /**
   * \brief Determine the rect (minx, maxx, miny, maxy) of a chunk at a position.
   */
  void determine_chunk_rect(rcti *r_rect, unsigned int x_chunk, unsigned int y_chunk) const;

  /**
   * \brief determine the number of chunks, based on the chunk_size, width and height.
   * \note The result are stored in the fields number_of_chunks, number_of_xchunks,
   * number_of_ychunks
   */
  void init_number_of_chunks();

  /**
   * \brief try to schedule a specific chunk.
   * \note scheduling succeeds when all input requirements are met and the chunks hasn't been
   * scheduled yet.
   * \param graph:
   * \param x_chunk:
   * \param y_chunk:
   * \return [true:false]
   * true: package(s) are scheduled
   * false: scheduling is deferred (depending workpackages are scheduled)
   */
  bool schedule_chunk_when_possible(ExecutionSystem *graph, int chunk_x, int chunk_y);

  /**
   * \brief try to schedule a specific area.
   * \note Check if a certain area is available, when not available this are will be checked.
   * \note This method is called from other ExecutionGroup's.
   * \param graph:
   * \param area:
   * \return [true:false]
   * true: package(s) are scheduled
   * false: scheduling is deferred (depending workpackages are scheduled)
   */
  bool schedule_area_when_possible(ExecutionSystem *graph, rcti *area);

  /**
   * \brief add a chunk to the WorkScheduler.
   * \param chunknumber:
   */
  bool schedule_chunk(unsigned int chunk_number);

  /**
   * \brief determine the area of interest of a certain input area
   * \note This method only evaluates a single ReadBufferOperation
   * \param input: the input area
   * \param read_operation: The ReadBufferOperation where the area needs to be evaluated
   * \param output: the area needed of the ReadBufferOperation. Result
   */
  void determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output);

  /**
   * Return the execution order of the user visible chunks.
   */
  blender::Array<unsigned int> get_execution_order() const;

  void init_read_buffer_operations();
  void init_work_packages();

 public:
  // constructors
  ExecutionGroup(int id);

  int get_id() const
  {
    return id_;
  }

  const ExecutionGroupFlags get_flags() const
  {
    return flags_;
  }

  // methods
  /**
   * \brief add an operation to this ExecutionGroup
   * \note this method will add input of the operations recursively
   * \note this method can create multiple ExecutionGroup's
   * \param system:
   * \param operation:
   * \return True if the operation was successfully added
   */
  bool add_operation(NodeOperation *operation);

  /**
   * \brief set whether this ExecutionGroup is an output
   * \param is_output:
   */
  void set_output_execution_group(bool is_output)
  {
    flags_.is_output = is_output;
  }

  /**
   * \brief determine the resolution of this ExecutionGroup
   * \param resolution:
   */
  void determine_resolution(unsigned int resolution[2]);

  /**
   * \brief set the resolution of this executiongroup
   * \param resolution:
   */
  void set_resolution(unsigned int resolution[2])
  {
    width_ = resolution[0];
    height_ = resolution[1];
  }

  /**
   * \brief get the width of this execution group
   */
  unsigned int get_width() const
  {
    return width_;
  }

  /**
   * \brief get the height of this execution group
   */
  unsigned int get_height() const
  {
    return height_;
  }

  /**
   * \brief get the output operation of this ExecutionGroup
   * \return NodeOperation *output operation
   */
  NodeOperation *get_output_operation() const;

  /**
   * \brief compose multiple chunks into a single chunk
   * \return `(Memorybuffer *)` consolidated chunk
   */
  MemoryBuffer *construct_consolidated_memory_buffer(MemoryProxy &memory_proxy, rcti &rect);

  /**
   * \brief init_execution is called just before the execution of the whole graph will be done.
   * \note The implementation will calculate the chunk_size of this execution group.
   */
  void init_execution();

  /**
   * \brief get all input-buffers needed to calculate an chunk
   * \note all input-buffers must be executed
   * \param chunk_number: the chunk to be calculated
   * \return `(MemoryBuffer **)` the input-buffers.
   */
  MemoryBuffer **get_input_buffers_opencl(int chunk_number);

  /**
   * \brief allocate the output-buffer of a chunk
   * \param chunk_number: the number of the chunk in the ExecutionGroup
   * \param rect: the rect of that chunk
   * \see determine_chunk_rect
   */
  MemoryBuffer *allocate_output_buffer(rcti &rect);

  /**
   * \brief after a chunk is executed the needed resources can be freed or unlocked.
   * \param chunknumber:
   * \param memorybuffers:
   */
  void finalize_chunk_execution(int chunk_number, MemoryBuffer **memory_buffers);

  /**
   * \brief deinit_execution is called just after execution the whole graph.
   * \note It will release all needed resources
   */
  void deinit_execution();

  /**
   * \brief schedule an ExecutionGroup
   * \note this method will return when all chunks have been calculated, or the execution has
   * breaked (by user)
   *
   * first the order of the chunks will be determined. This is determined by finding the
   * ViewerOperation and get the relevant information from it.
   *   - ChunkOrdering
   *   - CenterX
   *   - CenterY
   *
   * After determining the order of the chunks the chunks will be scheduled
   *
   * \see ViewerOperation
   * \param graph:
   */
  /**
   * This method is called for the top execution groups. containing the compositor node or the
   * preview node or the viewer node).
   */
  void execute(ExecutionSystem *graph);

  /**
   * \brief Determine the rect (minx, maxx, miny, maxy) of a chunk.
   */
  void determine_chunk_rect(rcti *r_rect, unsigned int chunk_number) const;

  void set_chunksize(int chunksize)
  {
    chunk_size_ = chunksize;
  }

  /**
   * \brief get the Render priority of this ExecutionGroup
   * \see ExecutionSystem.execute
   */
  eCompositorPriority get_render_priority();

  /**
   * \brief set border for viewer operation
   * \note all the coordinates are assumed to be in normalized space
   */
  void set_viewer_border(float xmin, float xmax, float ymin, float ymax);

  void set_render_border(float xmin, float xmax, float ymin, float ymax);

  /* allow the DebugInfo class to look at internals */
  friend class DebugInfo;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:ExecutionGroup")
#endif
};

std::ostream &operator<<(std::ostream &os, const ExecutionGroup &execution_group);

}  // namespace blender::compositor
