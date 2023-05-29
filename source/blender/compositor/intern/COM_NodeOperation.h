/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#pragma once

#include <functional>
#include <list>

#include "BLI_ghash.h"
#include "BLI_hash.hh"
#include "BLI_rect.h"
#include "BLI_span.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "COM_Enums.h"
#include "COM_MemoryBuffer.h"
#include "COM_MetaData.h"

#include "BKE_node_runtime.hh"

#include "clew.h"

#include "DNA_node_types.h"

namespace blender::compositor {

class OpenCLDevice;
class ReadBufferOperation;
class ExecutionSystem;
class NodeOperation;
class NodeOperationOutput;

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
  NodeOperation *operation_;

  /**
   * Datatype of this socket. Is used for automatically data transformation.
   * \section data-conversion
   */
  DataType datatype_;

  /** Resize mode of this socket */
  ResizeMode resize_mode_;

  /** Connected output */
  NodeOperationOutput *link_;

 public:
  NodeOperationInput(NodeOperation *op,
                     DataType datatype,
                     ResizeMode resize_mode = ResizeMode::Center);

  NodeOperation &get_operation() const
  {
    return *operation_;
  }
  DataType get_data_type() const
  {
    return datatype_;
  }

  void set_link(NodeOperationOutput *link)
  {
    link_ = link;
  }
  NodeOperationOutput *get_link() const
  {
    return link_;
  }
  bool is_connected() const
  {
    return link_;
  }

  void set_resize_mode(ResizeMode resize_mode)
  {
    resize_mode_ = resize_mode;
  }
  ResizeMode get_resize_mode() const
  {
    return resize_mode_;
  }

  SocketReader *get_reader();

  /**
   * \return Whether canvas area could be determined.
   */
  bool determine_canvas(const rcti &preferred_area, rcti &r_area);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:NodeOperation")
#endif
};

class NodeOperationOutput {
 private:
  NodeOperation *operation_;

  /**
   * Datatype of this socket. Is used for automatically data transformation.
   * \section data-conversion
   */
  DataType datatype_;

 public:
  NodeOperationOutput(NodeOperation *op, DataType datatype);

  NodeOperation &get_operation() const
  {
    return *operation_;
  }
  DataType get_data_type() const
  {
    return datatype_;
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
  int id_;
  std::string name_;
  Vector<NodeOperationInput> inputs_;
  Vector<NodeOperationOutput> outputs_;

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
   * \see NodeOperation.init_mutex initializes this mutex
   * \see NodeOperation.deinit_mutex deinitializes this mutex
   * \see NodeOperation.get_mutex retrieve a pointer to this mutex.
   */
  ThreadMutex mutex_;

  /**
   * \brief reference to the editing bNodeTree, used for break and update callback
   */
  const bNodeTree *btree_;

 protected:
  /**
   * Compositor execution model.
   */
  eExecutionModel execution_model_;

  rcti canvas_ = COM_AREA_NONE;

  /**
   * Flags how to evaluate this operation.
   */
  NodeOperationFlags flags_;

  ExecutionSystem *exec_system_;

 public:
  virtual ~NodeOperation() {}

  void set_name(const std::string name)
  {
    name_ = name;
  }

  const std::string get_name() const
  {
    return name_;
  }

  void set_id(const int id)
  {
    id_ = id;
  }

  const int get_id() const
  {
    return id_;
  }

  /** Get constant value when operation is constant, otherwise return default_value. */
  float get_constant_value_default(float default_value);
  /** Get constant elem when operation is constant, otherwise return default_elem. */
  const float *get_constant_elem_default(const float *default_elem);

  const NodeOperationFlags get_flags() const
  {
    return flags_;
  }

  /**
   * Generate a hash that identifies the operation result in the current execution.
   * Requires `hash_output_params` to be implemented, otherwise `std::nullopt` is returned.
   * If the operation parameters or its linked inputs change, the hash must be re-generated.
   */
  std::optional<NodeOperationHash> generate_hash();

  unsigned int get_number_of_input_sockets() const
  {
    return inputs_.size();
  }
  unsigned int get_number_of_output_sockets() const
  {
    return outputs_.size();
  }
  NodeOperationOutput *get_output_socket(unsigned int index = 0);
  NodeOperationInput *get_input_socket(unsigned int index);

  NodeOperation *get_input_operation(int index);

  virtual void determine_canvas(const rcti &preferred_area, rcti &r_area);

  /**
   * \brief is_output_operation determines whether this operation is an output of the
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
  virtual bool is_output_operation(bool /*rendering*/) const
  {
    return false;
  }

  void set_execution_model(const eExecutionModel model)
  {
    execution_model_ = model;
  }

  void set_bnodetree(const bNodeTree *tree)
  {
    btree_ = tree;
  }

  void set_execution_system(ExecutionSystem *system)
  {
    exec_system_ = system;
  }

  /**
   * Initializes operation data needed after operations are linked and resolutions determined. For
   * rendering heap memory data use init_execution().
   */
  virtual void init_data();

  virtual void init_execution();

  /**
   * \brief when a chunk is executed by a CPUDevice, this method is called
   * \ingroup execution
   * \param rect: the rectangle of the chunk (location and size)
   * \param chunk_number: the chunk_number to be calculated
   * \param memory_buffers: all input MemoryBuffer's needed
   */
  virtual void execute_region(rcti * /*rect*/, unsigned int /*chunk_number*/) {}

  /**
   * \brief when a chunk is executed by an OpenCLDevice, this method is called
   * \ingroup execution
   * \note this method is only implemented in WriteBufferOperation
   * \param context: the OpenCL context
   * \param program: the OpenCL program containing all compositor kernels
   * \param queue: the OpenCL command queue of the device the chunk is executed on
   * \param rect: the rectangle of the chunk (location and size)
   * \param chunk_number: the chunk_number to be calculated
   * \param memory_buffers: all input MemoryBuffer's needed
   * \param output_buffer: the outputbuffer to write to
   */
  virtual void execute_opencl_region(OpenCLDevice * /*device*/,
                                     rcti * /*rect*/,
                                     unsigned int /*chunk_number*/,
                                     MemoryBuffer ** /*memory_buffers*/,
                                     MemoryBuffer * /*output_buffer*/)
  {
  }

  /**
   * \brief custom handle to add new tasks to the OpenCL command queue
   * in order to execute a chunk on an GPUDevice.
   * \ingroup execution
   * \param context: the OpenCL context
   * \param program: the OpenCL program containing all compositor kernels
   * \param queue: the OpenCL command queue of the device the chunk is executed on
   * \param output_memory_buffer: the allocated memory buffer in main CPU memory
   * \param cl_output_buffer: the allocated memory buffer in OpenCLDevice memory
   * \param input_memory_buffers: all input MemoryBuffer's needed
   * \param cl_mem_to_clean_up: all created cl_mem references must be added to this list.
   * Framework will clean this after execution
   * \param cl_kernels_to_clean_up: all created cl_kernel references must be added to this list.
   * Framework will clean this after execution
   */
  virtual void execute_opencl(OpenCLDevice * /*device*/,
                              MemoryBuffer * /*output_memory_buffer*/,
                              cl_mem /*cl_output_buffer*/,
                              MemoryBuffer ** /*input_memory_buffers*/,
                              std::list<cl_mem> * /*cl_mem_to_clean_up*/,
                              std::list<cl_kernel> * /*cl_kernels_to_clean_up*/)
  {
  }
  virtual void deinit_execution();

  void set_canvas(const rcti &canvas_area);
  const rcti &get_canvas() const;
  /**
   * Mainly used for re-determining canvas of constant operations in cases where preferred canvas
   * depends on the constant element.
   */
  void unset_canvas();

  /**
   * \brief is this operation the active viewer output
   * user can select an ViewerNode to be active
   * (the result of this node will be drawn on the backdrop).
   * \return [true:false]
   * \see BaseViewerOperation
   */
  virtual bool is_active_viewer_output() const
  {
    return false;
  }

  virtual bool determine_depending_area_of_interest(rcti *input,
                                                    ReadBufferOperation *read_operation,
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
  virtual eCompositorPriority get_render_priority() const
  {
    return eCompositorPriority::Low;
  }

  inline bool is_braked() const
  {
    return btree_->runtime->test_break(btree_->runtime->tbh);
  }

  inline void update_draw()
  {
    if (btree_->runtime->update_draw) {
      btree_->runtime->update_draw(btree_->runtime->udh);
    }
  }

  unsigned int get_width() const
  {
    return BLI_rcti_size_x(&get_canvas());
  }

  unsigned int get_height() const
  {
    return BLI_rcti_size_y(&get_canvas());
  }

  inline void read_sampled(float result[4], float x, float y, PixelSampler sampler)
  {
    execute_pixel_sampled(result, x, y, sampler);
  }

  inline void read_filtered(float result[4], float x, float y, float dx[2], float dy[2])
  {
    execute_pixel_filtered(result, x, y, dx, dy);
  }

  inline void read(float result[4], int x, int y, void *chunk_data)
  {
    execute_pixel(result, x, y, chunk_data);
  }

  virtual void *initialize_tile_data(rcti * /*rect*/)
  {
    return 0;
  }

  virtual void deinitialize_tile_data(rcti * /*rect*/, void * /*data*/) {}

  virtual MemoryBuffer *get_input_memory_buffer(MemoryBuffer ** /*memory_buffers*/)
  {
    return 0;
  }

  /**
   * Return the meta data associated with this branch.
   *
   * The return parameter holds an instance or is an nullptr. */
  virtual std::unique_ptr<MetaData> get_meta_data()
  {
    return std::unique_ptr<MetaData>();
  }

  /* -------------------------------------------------------------------- */
  /** \name Full Frame Methods
   * \{ */

  /**
   * Executes operation image manipulation algorithm rendering given areas.
   * \param output_buf: Buffer to write result to.
   * \param areas: Areas within this operation bounds to render.
   * \param inputs_bufs: Inputs operations buffers.
   */
  void render(MemoryBuffer *output_buf, Span<rcti> areas, Span<MemoryBuffer *> inputs_bufs);

  /**
   * Executes operation updating output memory buffer. Single-threaded calls.
   */
  virtual void update_memory_buffer(MemoryBuffer * /*output*/,
                                    const rcti & /*area*/,
                                    Span<MemoryBuffer *> /*inputs*/)
  {
  }

  /**
   * \brief Get input operation area being read by this operation on rendering given output area.
   *
   * Implementation don't need to ensure r_input_area is within input operation bounds.
   * The caller must clamp it.
   * TODO: See if it's possible to use parameter overloading (input_id for example).
   *
   * \param input_idx: Input operation index for which we want to calculate the area being read.
   * \param output_area: Area being rendered by this operation.
   * \param r_input_area: Returned input operation area that needs to be read in order to render
   * given output area.
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

  void add_input_socket(DataType datatype, ResizeMode resize_mode = ResizeMode::Center);
  void add_output_socket(DataType datatype);

  /* TODO(manzanilla): to be removed with tiled implementation. */
  void set_width(unsigned int width)
  {
    canvas_.xmax = canvas_.xmin + width;
    flags_.is_canvas_set = true;
  }
  void set_height(unsigned int height)
  {
    canvas_.ymax = canvas_.ymin + height;
    flags_.is_canvas_set = true;
  }

  SocketReader *get_input_socket_reader(unsigned int index);

  void deinit_mutex();
  void init_mutex();
  void lock_mutex();
  void unlock_mutex();

  /**
   * \brief set whether this operation is complex
   *
   * Complex operations are typically doing many reads to calculate the output of a single pixel.
   * Mostly Filter types (Blurs, Convolution, Defocus etc) need this to be set to true.
   */
  void set_complex(bool complex)
  {
    flags_.complex = complex;
  }

  /**
   * \brief calculate a single pixel
   * \note this method is called for non-complex
   * \param result: is a float[4] array to store the result
   * \param x: the x-coordinate of the pixel to calculate in image space
   * \param y: the y-coordinate of the pixel to calculate in image space
   * \param input_buffers: chunks that can be read by their ReadBufferOperation.
   */
  virtual void execute_pixel_sampled(float /*output*/[4],
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
   * \param input_buffers: chunks that can be read by their ReadBufferOperation.
   * \param chunk_data: chunk specific data a during execution time.
   */
  virtual void execute_pixel(float output[4], int x, int y, void * /*chunk_data*/)
  {
    execute_pixel_sampled(output, x, y, PixelSampler::Nearest);
  }

  /**
   * \brief calculate a single pixel using an EWA filter
   * \note this method is called for complex
   * \param result: is a float[4] array to store the result
   * \param x: the x-coordinate of the pixel to calculate in image space
   * \param y: the y-coordinate of the pixel to calculate in image space
   * \param dx:
   * \param dy:
   * \param input_buffers: chunks that can be read by their ReadBufferOperation.
   */
  virtual void execute_pixel_filtered(
      float /*output*/[4], float /*x*/, float /*y*/, float /*dx*/[2], float /*dy*/[2])
  {
  }

 private:
  /* -------------------------------------------------------------------- */
  /** \name Full Frame Methods
   * \{ */

  /**
   * Renders given areas using operations full frame implementation.
   */
  void render_full_frame(MemoryBuffer *output_buf,
                         Span<rcti> areas,
                         Span<MemoryBuffer *> inputs_bufs);

  /**
   * Renders given areas using operations tiled implementation.
   */
  void render_full_frame_fallback(MemoryBuffer *output_buf,
                                  Span<rcti> areas,
                                  Span<MemoryBuffer *> inputs);
  void render_tile(MemoryBuffer *output_buf, rcti *tile_rect);
  /**
   * \return Replaced inputs links.
   */
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
