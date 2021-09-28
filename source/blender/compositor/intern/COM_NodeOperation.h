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

#include "BLI_ghash.h"
#include "BLI_hash.hh"
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
class ExecutionSystem;

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
  /** No resizing or translation. */
  None = NS_CR_NONE,
  /**
   * Input image is translated so that its bottom left matches the bottom left of the working area
   * of the node, no resizing occurs.
   */
  Align = 100,
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

  bool determine_canvas(const rcti &preferred_area, rcti &r_area);

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

  void determine_canvas(const rcti &preferred_area, rcti &r_area);

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

  /**
   * TODO: Remove this flag and #SingleThreadedOperation if tiled implementation is removed.
   * Full-frame implementation doesn't need it.
   */
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
   * Is the canvas of the operation set.
   */
  bool is_canvas_set : 1;

  /**
   * Is this a set operation (value, color, vector).
   * TODO: To be replaced by is_constant_operation flag once tiled implementation is removed.
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

  /**
   * Has this operation fullframe implementation.
   */
  bool is_fullframe_operation : 1;

  /**
   * Whether operation is a primitive constant operation (Color/Vector/Value).
   */
  bool is_constant_operation : 1;

  /**
   * Whether operation have constant elements/pixels values when all its inputs are constant
   * operations.
   */
  bool can_be_constant : 1;

  NodeOperationFlags()
  {
    complex = false;
    single_threaded = false;
    open_cl = false;
    use_render_border = false;
    use_viewer_border = false;
    is_canvas_set = false;
    is_set_operation = false;
    is_read_buffer_operation = false;
    is_write_buffer_operation = false;
    is_proxy_operation = false;
    is_viewer_operation = false;
    is_preview_operation = false;
    use_datatype_conversion = true;
    is_fullframe_operation = false;
    is_constant_operation = false;
    can_be_constant = false;
  }
};

/** Hash that identifies an operation output result in the current execution. */
struct NodeOperationHash {
 private:
  NodeOperation *operation_;
  size_t type_hash_;
  size_t parents_hash_;
  size_t params_hash_;

  friend class NodeOperation;

 public:
  NodeOperation *get_operation() const
  {
    return operation_;
  }

  bool operator==(const NodeOperationHash &other) const
  {
    return type_hash_ == other.type_hash_ && parents_hash_ == other.parents_hash_ &&
           params_hash_ == other.params_hash_;
  }

  bool operator!=(const NodeOperationHash &other) const
  {
    return !(*this == other);
  }

  bool operator<(const NodeOperationHash &other) const
  {
    return type_hash_ < other.type_hash_ ||
           (type_hash_ == other.type_hash_ && parents_hash_ < other.parents_hash_) ||
           (type_hash_ == other.type_hash_ && parents_hash_ == other.parents_hash_ &&
            params_hash_ < other.params_hash_);
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

  size_t params_hash_;
  bool is_hash_output_params_implemented_;

  /**
   * \brief the index of the input socket that will be used to determine the canvas
   */
  unsigned int canvas_input_index_;

  std::function<void(rcti &canvas)> modify_determined_canvas_fn_;

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
   * Compositor execution model.
   */
  eExecutionModel execution_model_;

  rcti canvas_;

  /**
   * Flags how to evaluate this operation.
   */
  NodeOperationFlags flags;

  ExecutionSystem *exec_system_;

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

  float get_constant_value_default(float default_value);
  const float *get_constant_elem_default(const float *default_elem);

  const NodeOperationFlags get_flags() const
  {
    return flags;
  }

  std::optional<NodeOperationHash> generate_hash();

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

  NodeOperation *get_input_operation(int index)
  {
    /* TODO: Rename protected getInputOperation to get_input_operation and make it public replacing
     * this method. */
    return getInputOperation(index);
  }

  virtual void determine_canvas(const rcti &preferred_area, rcti &r_area);

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

  void set_execution_model(const eExecutionModel model)
  {
    execution_model_ = model;
  }

  void setbNodeTree(const bNodeTree *tree)
  {
    this->m_btree = tree;
  }

  void set_execution_system(ExecutionSystem *system)
  {
    exec_system_ = system;
  }

  /**
   * Initializes operation data needed after operations are linked and resolutions determined. For
   * rendering heap memory data use initExecution().
   */
  virtual void init_data();

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

  void set_canvas(const rcti &canvas_area);
  const rcti &get_canvas() const;
  void unset_canvas();

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
   * \brief set the index of the input socket that will determine the canvas of this
   * operation \param index: the index to set
   */
  void set_canvas_input_index(unsigned int index);

  /**
   * Set a custom function to modify determined canvas from main input just before setting it
   * as preferred for the other inputs.
   */
  void set_determined_canvas_modifier(std::function<void(rcti &canvas)> fn)
  {
    modify_determined_canvas_fn_ = fn;
  }

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
    return BLI_rcti_size_x(&get_canvas());
  }

  unsigned int getHeight() const
  {
    return BLI_rcti_size_y(&get_canvas());
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

  /* -------------------------------------------------------------------- */
  /** \name Full Frame Methods
   * \{ */

  void render(MemoryBuffer *output_buf, Span<rcti> areas, Span<MemoryBuffer *> inputs_bufs);

  /**
   * Executes operation updating output memory buffer. Single-threaded calls.
   */
  virtual void update_memory_buffer(MemoryBuffer *UNUSED(output),
                                    const rcti &UNUSED(area),
                                    Span<MemoryBuffer *> UNUSED(inputs))
  {
  }

  /**
   * Get input operation area being read by this operation on rendering given output area.
   */
  virtual void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area);
  void get_area_of_interest(NodeOperation *input_op, const rcti &output_area, rcti &r_input_area);

  /** \} */

 protected:
  NodeOperation();

  /* Overridden by subclasses to allow merging equal operations on compiling. Implementations must
   * hash any subclass parameter that affects the output result using `hash_params` methods. */
  virtual void hash_output_params()
  {
    is_hash_output_params_implemented_ = false;
  }

  static void combine_hashes(size_t &combined, size_t other)
  {
    combined = BLI_ghashutil_combine_hash(combined, other);
  }

  template<typename T> void hash_param(T param)
  {
    combine_hashes(params_hash_, get_default_hash(param));
  }

  template<typename T1, typename T2> void hash_params(T1 param1, T2 param2)
  {
    combine_hashes(params_hash_, get_default_hash_2(param1, param2));
  }

  template<typename T1, typename T2, typename T3> void hash_params(T1 param1, T2 param2, T3 param3)
  {
    combine_hashes(params_hash_, get_default_hash_3(param1, param2, param3));
  }

  void addInputSocket(DataType datatype, ResizeMode resize_mode = ResizeMode::Center);
  void addOutputSocket(DataType datatype);

  /* TODO(manzanilla): to be removed with tiled implementation. */
  void setWidth(unsigned int width)
  {
    canvas_.xmax = canvas_.xmin + width;
    this->flags.is_canvas_set = true;
  }
  void setHeight(unsigned int height)
  {
    canvas_.ymax = canvas_.ymin + height;
    this->flags.is_canvas_set = true;
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

 private:
  /* -------------------------------------------------------------------- */
  /** \name Full Frame Methods
   * \{ */

  void render_full_frame(MemoryBuffer *output_buf,
                         Span<rcti> areas,
                         Span<MemoryBuffer *> inputs_bufs);

  void render_full_frame_fallback(MemoryBuffer *output_buf,
                                  Span<rcti> areas,
                                  Span<MemoryBuffer *> inputs);
  void render_tile(MemoryBuffer *output_buf, rcti *tile_rect);
  Vector<NodeOperationOutput *> replace_inputs_with_buffers(Span<MemoryBuffer *> inputs_bufs);
  void remove_buffers_and_restore_original_inputs(
      Span<NodeOperationOutput *> original_inputs_links);

  /** \} */

  /* allow the DebugInfo class to look at internals */
  friend class DebugInfo;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};

std::ostream &operator<<(std::ostream &os, const NodeOperationFlags &node_operation_flags);
std::ostream &operator<<(std::ostream &os, const NodeOperation &node_operation);

}  // namespace blender::compositor
