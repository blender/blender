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

#include <list>
#include <sstream>
#include <string>

#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_threads.h"

#include "COM_Enums.h"
#include "COM_MemoryBuffer.h"
#include "COM_MemoryProxy.h"
#include "COM_MetaData.h"
#include "COM_Node.h"

#include "clew.h"

namespace blender::compositor {

class OpenCLDevice;
class ReadBufferOperation;
class WriteBufferOperation;

class NodeOperation;
typedef NodeOperation SocketReader;

/**
 * RESOLUTION_INPUT_ANY is a wildcard when any resolution of an input can be used.
 * This solves the issue that the FileInputNode in a group node cannot find the
 * correct resolution.
 */
static constexpr unsigned int RESOLUTION_INPUT_ANY = 999999;

/**
 * \brief Resize modes of inputsockets
 * How are the input and working resolutions matched
 * \ingroup Model
 */
enum class ResizeMode {
  /** \brief Center the input image to the center of the working area of the node, no resizing
   * occurs */
  Center = NS_CR_CENTER,
  /** \brief The bottom left of the input image is the bottom left of the working area of the node,
   * no resizing occurs */
  None = NS_CR_NONE,
  /** \brief Fit the width of the input image to the width of the working area of the node */
  FitWidth = NS_CR_FIT_WIDTH,
  /** \brief Fit the height of the input image to the height of the working area of the node */
  FitHeight = NS_CR_FIT_HEIGHT,
  /** \brief Fit the width or the height of the input image to the width or height of the working
   * area of the node, image will be larger than the working area */
  FitAny = NS_CR_FIT,
  /** \brief Fit the width and the height of the input image to the width and height of the working
   * area of the node, image will be equally larger than the working area */
  Stretch = NS_CR_STRETCH,
};

enum class PixelSampler {
  Nearest = 0,
  Bilinear = 1,
  Bicubic = 2,
};

class NodeOperationInput {
 private:
  NodeOperation *m_operation;

  /** Datatype of this socket. Is used for automatically data transformation.
   * \section data-conversion
   */
  DataType m_datatype;

  /** Resize mode of this socket */
  ResizeMode m_resizeMode;

  /** Connected output */
  NodeOperationOutput *m_link;

 public:
  NodeOperationInput(NodeOperation *op,
                     DataType datatype,
                     ResizeMode resizeMode = ResizeMode::Center);

  NodeOperation &getOperation() const
  {
    return *m_operation;
  }
  DataType getDataType() const
  {
    return m_datatype;
  }

  void setLink(NodeOperationOutput *link)
  {
    m_link = link;
  }
  NodeOperationOutput *getLink() const
  {
    return m_link;
  }
  bool isConnected() const
  {
    return m_link;
  }

  void setResizeMode(ResizeMode resizeMode)
  {
    this->m_resizeMode = resizeMode;
  }
  ResizeMode getResizeMode() const
  {
    return this->m_resizeMode;
  }

  SocketReader *getReader();

  void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};

class NodeOperationOutput {
 private:
  NodeOperation *m_operation;

  /** Datatype of this socket. Is used for automatically data transformation.
   * \section data-conversion
   */
  DataType m_datatype;

 public:
  NodeOperationOutput(NodeOperation *op, DataType datatype);

  NodeOperation &getOperation() const
  {
    return *m_operation;
  }
  DataType getDataType() const
  {
    return m_datatype;
  }

  /**
   * \brief determine the resolution of this data going through this socket
   * \param resolution: the result of this operation
   * \param preferredResolution: the preferable resolution as no resolution could be determined
   */
  void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};

struct NodeOperationFlags {
  /**
   * Is this an complex operation.
   *
   * The input and output buffers of Complex operations are stored in buffers. It allows
   * sequential and read/write.
   *
   * Complex operations are typically doing many reads to calculate the output of a single pixel.
   * Mostly Filter types (Blurs, Convolution, Defocus etc) need this to be set to true.
   */
  bool complex : 1;

  /**
   * Does this operation support OpenCL.
   */
  bool open_cl : 1;

  bool single_threaded : 1;

  /**
   * Does the operation needs a viewer border.
   * Basically, setting border need to happen for only operations
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
  bool use_render_border : 1;
  bool use_viewer_border : 1;

  /**
   * Is the resolution of the operation set.
   */
  bool is_resolution_set : 1;

  /**
   * Is this a set operation (value, color, vector).
   */
  bool is_set_operation : 1;
  bool is_write_buffer_operation : 1;
  bool is_read_buffer_operation : 1;
  bool is_proxy_operation : 1;
  bool is_viewer_operation : 1;
  bool is_preview_operation : 1;

  /**
   * When set additional data conversion operations are added to
   * convert the data. SocketProxyOperation don't always need to do data conversions.
   *
   * By default data conversions are enabled.
   */
  bool use_datatype_conversion : 1;

  NodeOperationFlags()
  {
    complex = false;
    single_threaded = false;
    open_cl = false;
    use_render_border = false;
    use_viewer_border = false;
    is_resolution_set = false;
    is_set_operation = false;
    is_read_buffer_operation = false;
    is_write_buffer_operation = false;
    is_proxy_operation = false;
    is_viewer_operation = false;
    is_preview_operation = false;
    use_datatype_conversion = true;
  }
};

/**
 * \brief NodeOperation contains calculation logic
 *
 * Subclasses needs to implement the execution method (defined in SocketReader) to implement logic.
 * \ingroup Model
 */
class NodeOperation {
 private:
  int m_id;
  std::string m_name;
  Vector<NodeOperationInput> m_inputs;
  Vector<NodeOperationOutput> m_outputs;

  /**
   * \brief the index of the input socket that will be used to determine the resolution
   */
  unsigned int m_resolutionInputSocketIndex;

  /**
   * \brief mutex reference for very special node initializations
   * \note only use when you really know what you are doing.
   * this mutex is used to share data among chunks in the same operation
   * \see TonemapOperation for an example of usage
   * \see NodeOperation.initMutex initializes this mutex
   * \see NodeOperation.deinitMutex deinitializes this mutex
   * \see NodeOperation.getMutex retrieve a pointer to this mutex.
   */
  ThreadMutex m_mutex;

  /**
   * \brief reference to the editing bNodeTree, used for break and update callback
   */
  const bNodeTree *m_btree;

 protected:
  /**
   * Width of the output of this operation.
   */
  unsigned int m_width;

  /**
   * Height of the output of this operation.
   */
  unsigned int m_height;

  /**
   * Flags how to evaluate this operation.
   */
  NodeOperationFlags flags;

 public:
  virtual ~NodeOperation()
  {
  }

  void set_name(const std::string name)
  {
    m_name = name;
  }

  const std::string get_name() const
  {
    return m_name;
  }

  void set_id(const int id)
  {
    m_id = id;
  }

  const int get_id() const
  {
    return m_id;
  }

  const NodeOperationFlags get_flags() const
  {
    return flags;
  }

  unsigned int getNumberOfInputSockets() const
  {
    return m_inputs.size();
  }
  unsigned int getNumberOfOutputSockets() const
  {
    return m_outputs.size();
  }
  NodeOperationOutput *getOutputSocket(unsigned int index = 0);
  NodeOperationInput *getInputSocket(unsigned int index);

  /**
   * \brief determine the resolution of this node
   * \note this method will not set the resolution, this is the responsibility of the caller
   * \param resolution: the result of this operation
   * \param preferredResolution: the preferable resolution as no resolution could be determined
   */
  virtual void determineResolution(unsigned int resolution[2],
                                   unsigned int preferredResolution[2]);

  /**
   * \brief isOutputOperation determines whether this operation is an output of the
   * ExecutionSystem during rendering or editing.
   *
   * Default behavior if not overridden, this operation will not be evaluated as being an output
   * of the ExecutionSystem.
   *
   * \see ExecutionSystem
   * \ingroup check
   * \param rendering: [true false]
   *  true: rendering
   *  false: editing
   *
   * \return bool the result of this method
   */
  virtual bool isOutputOperation(bool /*rendering*/) const
  {
    return false;
  }

  void setbNodeTree(const bNodeTree *tree)
  {
    this->m_btree = tree;
  }
  virtual void initExecution();

  /**
   * \brief when a chunk is executed by a CPUDevice, this method is called
   * \ingroup execution
   * \param rect: the rectangle of the chunk (location and size)
   * \param chunkNumber: the chunkNumber to be calculated
   * \param memoryBuffers: all input MemoryBuffer's needed
   */
  virtual void executeRegion(rcti * /*rect*/, unsigned int /*chunkNumber*/)
  {
  }

  /**
   * \brief when a chunk is executed by an OpenCLDevice, this method is called
   * \ingroup execution
   * \note this method is only implemented in WriteBufferOperation
   * \param context: the OpenCL context
   * \param program: the OpenCL program containing all compositor kernels
   * \param queue: the OpenCL command queue of the device the chunk is executed on
   * \param rect: the rectangle of the chunk (location and size)
   * \param chunkNumber: the chunkNumber to be calculated
   * \param memoryBuffers: all input MemoryBuffer's needed
   * \param outputBuffer: the outputbuffer to write to
   */
  virtual void executeOpenCLRegion(OpenCLDevice * /*device*/,
                                   rcti * /*rect*/,
                                   unsigned int /*chunkNumber*/,
                                   MemoryBuffer ** /*memoryBuffers*/,
                                   MemoryBuffer * /*outputBuffer*/)
  {
  }

  /**
   * \brief custom handle to add new tasks to the OpenCL command queue
   * in order to execute a chunk on an GPUDevice.
   * \ingroup execution
   * \param context: the OpenCL context
   * \param program: the OpenCL program containing all compositor kernels
   * \param queue: the OpenCL command queue of the device the chunk is executed on
   * \param outputMemoryBuffer: the allocated memory buffer in main CPU memory
   * \param clOutputBuffer: the allocated memory buffer in OpenCLDevice memory
   * \param inputMemoryBuffers: all input MemoryBuffer's needed
   * \param clMemToCleanUp: all created cl_mem references must be added to this list.
   * Framework will clean this after execution
   * \param clKernelsToCleanUp: all created cl_kernel references must be added to this list.
   * Framework will clean this after execution
   */
  virtual void executeOpenCL(OpenCLDevice * /*device*/,
                             MemoryBuffer * /*outputMemoryBuffer*/,
                             cl_mem /*clOutputBuffer*/,
                             MemoryBuffer ** /*inputMemoryBuffers*/,
                             std::list<cl_mem> * /*clMemToCleanUp*/,
                             std::list<cl_kernel> * /*clKernelsToCleanUp*/)
  {
  }
  virtual void deinitExecution();

  /**
   * \brief set the resolution
   * \param resolution: the resolution to set
   */
  void setResolution(unsigned int resolution[2])
  {
    if (!this->flags.is_resolution_set) {
      this->m_width = resolution[0];
      this->m_height = resolution[1];
      this->flags.is_resolution_set = true;
    }
  }

  /**
   * \brief is this operation the active viewer output
   * user can select an ViewerNode to be active
   * (the result of this node will be drawn on the backdrop).
   * \return [true:false]
   * \see BaseViewerOperation
   */
  virtual bool isActiveViewerOutput() const
  {
    return false;
  }

  virtual bool determineDependingAreaOfInterest(rcti *input,
                                                ReadBufferOperation *readOperation,
                                                rcti *output);

  /**
   * \brief set the index of the input socket that will determine the resolution of this
   * operation \param index: the index to set
   */
  void setResolutionInputSocketIndex(unsigned int index);

  /**
   * \brief get the render priority of this node.
   * \note only applicable for output operations like ViewerOperation
   * \return eCompositorPriority
   */
  virtual eCompositorPriority getRenderPriority() const
  {
    return eCompositorPriority::Low;
  }

  inline bool isBraked() const
  {
    return this->m_btree->test_break(this->m_btree->tbh);
  }

  inline void updateDraw()
  {
    if (this->m_btree->update_draw) {
      this->m_btree->update_draw(this->m_btree->udh);
    }
  }

  unsigned int getWidth() const
  {
    return m_width;
  }

  unsigned int getHeight() const
  {
    return m_height;
  }

  inline void readSampled(float result[4], float x, float y, PixelSampler sampler)
  {
    executePixelSampled(result, x, y, sampler);
  }

  inline void readFiltered(float result[4], float x, float y, float dx[2], float dy[2])
  {
    executePixelFiltered(result, x, y, dx, dy);
  }

  inline void read(float result[4], int x, int y, void *chunkData)
  {
    executePixel(result, x, y, chunkData);
  }

  virtual void *initializeTileData(rcti * /*rect*/)
  {
    return 0;
  }

  virtual void deinitializeTileData(rcti * /*rect*/, void * /*data*/)
  {
  }

  virtual MemoryBuffer *getInputMemoryBuffer(MemoryBuffer ** /*memoryBuffers*/)
  {
    return 0;
  }

  /**
   * Return the meta data associated with this branch.
   *
   * The return parameter holds an instance or is an nullptr. */
  virtual std::unique_ptr<MetaData> getMetaData()
  {
    return std::unique_ptr<MetaData>();
  }

 protected:
  NodeOperation();

  void addInputSocket(DataType datatype, ResizeMode resize_mode = ResizeMode::Center);
  void addOutputSocket(DataType datatype);

  void setWidth(unsigned int width)
  {
    this->m_width = width;
    this->flags.is_resolution_set = true;
  }
  void setHeight(unsigned int height)
  {
    this->m_height = height;
    this->flags.is_resolution_set = true;
  }
  SocketReader *getInputSocketReader(unsigned int inputSocketindex);
  NodeOperation *getInputOperation(unsigned int inputSocketindex);

  void deinitMutex();
  void initMutex();
  void lockMutex();
  void unlockMutex();

  /**
   * \brief set whether this operation is complex
   *
   * Complex operations are typically doing many reads to calculate the output of a single pixel.
   * Mostly Filter types (Blurs, Convolution, Defocus etc) need this to be set to true.
   */
  void setComplex(bool complex)
  {
    this->flags.complex = complex;
  }

  /**
   * \brief calculate a single pixel
   * \note this method is called for non-complex
   * \param result: is a float[4] array to store the result
   * \param x: the x-coordinate of the pixel to calculate in image space
   * \param y: the y-coordinate of the pixel to calculate in image space
   * \param inputBuffers: chunks that can be read by their ReadBufferOperation.
   */
  virtual void executePixelSampled(float /*output*/[4],
                                   float /*x*/,
                                   float /*y*/,
                                   PixelSampler /*sampler*/)
  {
  }

  /**
   * \brief calculate a single pixel
   * \note this method is called for complex
   * \param result: is a float[4] array to store the result
   * \param x: the x-coordinate of the pixel to calculate in image space
   * \param y: the y-coordinate of the pixel to calculate in image space
   * \param inputBuffers: chunks that can be read by their ReadBufferOperation.
   * \param chunkData: chunk specific data a during execution time.
   */
  virtual void executePixel(float output[4], int x, int y, void * /*chunkData*/)
  {
    executePixelSampled(output, x, y, PixelSampler::Nearest);
  }

  /**
   * \brief calculate a single pixel using an EWA filter
   * \note this method is called for complex
   * \param result: is a float[4] array to store the result
   * \param x: the x-coordinate of the pixel to calculate in image space
   * \param y: the y-coordinate of the pixel to calculate in image space
   * \param dx:
   * \param dy:
   * \param inputBuffers: chunks that can be read by their ReadBufferOperation.
   */
  virtual void executePixelFiltered(
      float /*output*/[4], float /*x*/, float /*y*/, float /*dx*/[2], float /*dy*/[2])
  {
  }

  /* allow the DebugInfo class to look at internals */
  friend class DebugInfo;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};

std::ostream &operator<<(std::ostream &os, const NodeOperationFlags &node_operation_flags);
std::ostream &operator<<(std::ostream &os, const NodeOperation &node_operation);

}  // namespace blender::compositor
