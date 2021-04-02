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

#pragma once

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

#include "BLI_array.hh"
#include "BLI_rect.h"
#include "BLI_vector.hh"

#include "COM_CompositorContext.h"
#include "COM_Device.h"
#include "COM_MemoryProxy.h"
#include "COM_Node.h"
#include "COM_NodeOperation.h"
#include "COM_WorkPackage.h"
#include <vector>

namespace blender::compositor {

class ExecutionSystem;
class MemoryProxy;
class MemoryBuffer;
class ReadBufferOperation;
class Device;

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
  int m_id;

  /**
   * \brief list of operations in this ExecutionGroup
   */
  Vector<NodeOperation *> m_operations;

  ExecutionGroupFlags m_flags;

  /**
   * \brief Width of the output
   */
  unsigned int m_width;

  /**
   * \brief Height of the output
   */
  unsigned int m_height;

  /**
   * \brief size of a single chunk, being Width or of height
   * a chunk is always a square, except at the edges of the MemoryBuffer
   */
  unsigned int m_chunkSize;

  /**
   * \brief number of chunks in the x-axis
   */
  unsigned int m_x_chunks_len;

  /**
   * \brief number of chunks in the y-axis
   */
  unsigned int m_y_chunks_len;

  /**
   * \brief total number of chunks
   */
  unsigned int m_chunks_len;

  /**
   * \brief what is the maximum number field of all ReadBufferOperation in this ExecutionGroup.
   * \note this is used to construct the MemoryBuffers that will be passed during execution.
   */
  unsigned int m_max_read_buffer_offset;

  /**
   * \brief All read operations of this execution group.
   */
  Vector<ReadBufferOperation *> m_read_operations;

  /**
   * \brief reference to the original bNodeTree,
   * this field is only set for the 'top' execution group.
   * \note can only be used to call the callbacks for progress, status and break.
   */
  const bNodeTree *m_bTree;

  /**
   * \brief total number of chunks that have been calculated for this ExecutionGroup
   */
  unsigned int m_chunks_finished;

  /**
   * \brief m_work_packages holds all unit of work.
   */
  Vector<WorkPackage> m_work_packages;

  /**
   * \brief denotes boundary for border compositing
   * \note measured in pixel space
   */
  rcti m_viewerBorder;

  /**
   * \brief start time of execution
   */
  double m_executionStartTime;

  // methods
  /**
   * \brief check whether parameter operation can be added to the execution group
   * \param operation: the operation to be added
   */
  bool can_contain(NodeOperation &operation);

  /**
   * \brief Determine the rect (minx, maxx, miny, maxy) of a chunk at a position.
   */
  void determineChunkRect(rcti *r_rect,
                          const unsigned int xChunk,
                          const unsigned int yChunk) const;

  /**
   * \brief determine the number of chunks, based on the chunkSize, width and height.
   * \note The result are stored in the fields numberOfChunks, numberOfXChunks, numberOfYChunks
   */
  void init_number_of_chunks();

  /**
   * \brief try to schedule a specific chunk.
   * \note scheduling succeeds when all input requirements are met and the chunks hasn't been
   * scheduled yet.
   * \param graph:
   * \param xChunk:
   * \param yChunk:
   * \return [true:false]
   * true: package(s) are scheduled
   * false: scheduling is deferred (depending workpackages are scheduled)
   */
  bool scheduleChunkWhenPossible(ExecutionSystem *graph, const int chunk_x, const int chunk_y);

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
  bool scheduleAreaWhenPossible(ExecutionSystem *graph, rcti *area);

  /**
   * \brief add a chunk to the WorkScheduler.
   * \param chunknumber:
   */
  bool scheduleChunk(unsigned int chunkNumber);

  /**
   * \brief determine the area of interest of a certain input area
   * \note This method only evaluates a single ReadBufferOperation
   * \param input: the input area
   * \param readOperation: The ReadBufferOperation where the area needs to be evaluated
   * \param output: the area needed of the ReadBufferOperation. Result
   */
  void determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
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
    return m_id;
  }

  const ExecutionGroupFlags get_flags() const
  {
    return m_flags;
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
  bool addOperation(NodeOperation *operation);

  /**
   * \brief set whether this ExecutionGroup is an output
   * \param isOutput:
   */
  void setOutputExecutionGroup(bool is_output)
  {
    this->m_flags.is_output = is_output;
  }

  /**
   * \brief determine the resolution of this ExecutionGroup
   * \param resolution:
   */
  void determineResolution(unsigned int resolution[2]);

  /**
   * \brief set the resolution of this executiongroup
   * \param resolution:
   */
  void setResolution(unsigned int resolution[2])
  {
    this->m_width = resolution[0];
    this->m_height = resolution[1];
  }

  /**
   * \brief get the width of this execution group
   */
  unsigned int getWidth() const
  {
    return m_width;
  }

  /**
   * \brief get the height of this execution group
   */
  unsigned int getHeight() const
  {
    return m_height;
  }

  /**
   * \brief get the output operation of this ExecutionGroup
   * \return NodeOperation *output operation
   */
  NodeOperation *getOutputOperation() const;

  /**
   * \brief compose multiple chunks into a single chunk
   * \return Memorybuffer *consolidated chunk
   */
  MemoryBuffer *constructConsolidatedMemoryBuffer(MemoryProxy &memoryProxy, rcti &rect);

  /**
   * \brief initExecution is called just before the execution of the whole graph will be done.
   * \note The implementation will calculate the chunkSize of this execution group.
   */
  void initExecution();

  /**
   * \brief get all inputbuffers needed to calculate an chunk
   * \note all inputbuffers must be executed
   * \param chunkNumber: the chunk to be calculated
   * \return (MemoryBuffer **) the inputbuffers
   */
  MemoryBuffer **getInputBuffersCPU();

  /**
   * \brief get all inputbuffers needed to calculate an chunk
   * \note all inputbuffers must be executed
   * \param chunkNumber: the chunk to be calculated
   * \return (MemoryBuffer **) the inputbuffers
   */
  MemoryBuffer **getInputBuffersOpenCL(int chunkNumber);

  /**
   * \brief allocate the outputbuffer of a chunk
   * \param chunkNumber: the number of the chunk in the ExecutionGroup
   * \param rect: the rect of that chunk
   * \see determineChunkRect
   */
  MemoryBuffer *allocateOutputBuffer(rcti &rect);

  /**
   * \brief after a chunk is executed the needed resources can be freed or unlocked.
   * \param chunknumber:
   * \param memorybuffers:
   */
  void finalizeChunkExecution(int chunkNumber, MemoryBuffer **memoryBuffers);

  /**
   * \brief deinitExecution is called just after execution the whole graph.
   * \note It will release all needed resources
   */
  void deinitExecution();

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
  void execute(ExecutionSystem *graph);

  /**
   * \brief Determine the rect (minx, maxx, miny, maxy) of a chunk.
   */
  void determineChunkRect(rcti *r_rect, const unsigned int chunkNumber) const;

  void setChunksize(int chunksize)
  {
    this->m_chunkSize = chunksize;
  }

  /**
   * \brief get the Render priority of this ExecutionGroup
   * \see ExecutionSystem.execute
   */
  eCompositorPriority getRenderPriority();

  /**
   * \brief set border for viewer operation
   * \note all the coordinates are assumed to be in normalized space
   */
  void setViewerBorder(float xmin, float xmax, float ymin, float ymax);

  void setRenderBorder(float xmin, float xmax, float ymin, float ymax);

  /* allow the DebugInfo class to look at internals */
  friend class DebugInfo;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:ExecutionGroup")
#endif
};

std::ostream &operator<<(std::ostream &os, const ExecutionGroup &execution_group);

}  // namespace blender::compositor
