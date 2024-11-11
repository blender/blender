/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_shader.hh"
#include "GPU_vertex_format.hh"

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <functional>
#include <unordered_map>

#include <deque>
#include <mutex>
#include <thread>

#include "mtl_framebuffer.hh"
#include "mtl_shader_interface.hh"
#include "mtl_shader_shared.h"
#include "mtl_state.hh"
#include "mtl_texture.hh"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_private.hh"

namespace blender::gpu {

class MTLShaderInterface;
class MTLContext;

/* Debug control. */
#define MTL_SHADER_DEBUG_EXPORT_SOURCE 0
#define MTL_SHADER_TRANSLATION_DEBUG_OUTPUT 0

/* Separate print used only during development and debugging. */
#if MTL_SHADER_TRANSLATION_DEBUG_OUTPUT
#  define shader_debug_printf printf
#else
#  define shader_debug_printf(...) /* Null print. */
#endif

/* Offset base specialization constant ID for function constants declared in CreateInfo. */
#define MTL_SHADER_SPECIALIZATION_CONSTANT_BASE_ID 30
/* Maximum threshold for specialized shader variant count.
 * This is a catch-all to prevent excessive PSO permutations from being created and also catch
 * parameters which should ideally not be used for specialization. */
#define MTL_SHADER_MAX_SPECIALIZED_PSOS 5

/* Desired reflection data for a buffer binding. */
struct MTLBufferArgumentData {
  uint32_t index;
  uint32_t size;
  uint32_t alignment;
  bool active;
};

/* Metal Render Pipeline State Instance. */
struct MTLRenderPipelineStateInstance {
  /* Function instances with specialization.
   * Required for argument encoder construction. */
  id<MTLFunction> vert;
  id<MTLFunction> frag;

  /* PSO handle. */
  id<MTLRenderPipelineState> pso;

  /** Derived information. */
  /* Unique index for PSO variant. */
  uint32_t shader_pso_index;
  /* Base bind index for binding uniform buffers, offset based on other
   * bound buffers such as vertex buffers, as the count can vary. */
  int base_uniform_buffer_index;
  /* Base bind index for binding storage buffers. */
  int base_storage_buffer_index;
  /* buffer bind slot used for null attributes (-1 if not needed). */
  int null_attribute_buffer_index;
  /* buffer bind used for transform feedback output buffer. */
  int transform_feedback_buffer_index;
  /* Topology class. */
  MTLPrimitiveTopologyClass prim_type;

  /** Reflection Data.
   * Currently used to verify whether uniform buffers of incorrect sizes being bound, due to left
   * over bindings being used for slots that did not need updating for a particular draw. Metal
   * Back-end over-generates bindings due to detecting their presence, though in many cases, the
   * bindings in the source are not all used for a given shader.
   * This information can also be used to eliminate redundant/unused bindings. */
  bool reflection_data_available;
  blender::Vector<MTLBufferArgumentData> buffer_bindings_reflection_data_vert;
  blender::Vector<MTLBufferArgumentData> buffer_bindings_reflection_data_frag;
};

/* Common compute pipeline state. */
struct MTLComputePipelineStateCommon {

  /* Thread-group information is common for all PSO variants. */
  int threadgroup_x_len = 1;
  int threadgroup_y_len = 1;
  int threadgroup_z_len = 1;

  inline void set_compute_workgroup_size(int workgroup_size_x,
                                         int workgroup_size_y,
                                         int workgroup_size_z)
  {
    this->threadgroup_x_len = workgroup_size_x;
    this->threadgroup_y_len = workgroup_size_y;
    this->threadgroup_z_len = workgroup_size_z;
  }
};

/* Metal Compute Pipeline State instance per PSO. */
struct MTLComputePipelineStateInstance {

  /** Derived information. */
  /* Unique index for PSO variant. */
  uint32_t shader_pso_index;
  /* Base bind index for binding uniform buffers, offset based on other
   * bound buffers such as vertex buffers, as the count can vary. */
  int base_uniform_buffer_index = -1;
  /* Base bind index for binding storage buffers. */
  int base_storage_buffer_index = -1;

  /* Function instances with specialization.
   * Required for argument encoder construction. */
  id<MTLFunction> compute = nil;
  /* PSO handle. */
  id<MTLComputePipelineState> pso = nil;
};

/* #MTLShaderBuilder source wrapper used during initial compilation. */
struct MTLShaderBuilder {
  NSString *msl_source_vert_ = @"";
  NSString *msl_source_frag_ = @"";
  NSString *msl_source_compute_ = @"";

  /* Generated GLSL source used during compilation. */
  std::string glsl_vertex_source_ = "";
  std::string glsl_fragment_source_ = "";
  std::string glsl_compute_source_ = "";

  /* Indicates whether source code has been provided via MSL directly. */
  bool source_from_msl_ = false;
};

/**
 * #MTLShader implements shader compilation, Pipeline State Object (PSO)
 * creation for rendering and uniform data binding.
 * Shaders can either be created from native MSL, or generated
 * from a GLSL source shader using #GPUShaderCreateInfo.
 *
 * Shader creation process:
 * - Create #MTLShader:
 *    - Convert GLSL to MSL source if required.
 * - set MSL source.
 * - set Vertex/Fragment function names.
 * - Create and populate #MTLShaderInterface.
 */
class MTLShader : public Shader {
  friend shader::ShaderCreateInfo;
  friend shader::StageInterfaceInfo;

 public:
  /* Cached SSBO vertex fetch attribute uniform locations. */
  int uni_ssbo_input_prim_type_loc = -1;
  int uni_ssbo_input_vert_count_loc = -1;
  int uni_ssbo_uses_indexed_rendering = -1;
  int uni_ssbo_uses_index_mode_u16 = -1;
  int uni_ssbo_index_base_loc = -1;

 private:
  /* Context Handle. */
  MTLContext *context_ = nullptr;

  /** Transform Feedback. */
  /* Transform feedback mode. */
  eGPUShaderTFBType transform_feedback_type_ = GPU_SHADER_TFB_NONE;
  /* Transform feedback outputs written to TFB buffer. */
  blender::Vector<std::string> tf_output_name_list_;
  /* Whether transform feedback is currently active. */
  bool transform_feedback_active_ = false;
  /* Vertex buffer to write transform feedback data into. */
  VertBuf *transform_feedback_vertbuf_ = nullptr;

  /** Shader source code. */
  MTLShaderBuilder *shd_builder_ = nullptr;
  NSString *vertex_function_name_ = @"";
  NSString *fragment_function_name_ = @"";
  NSString *compute_function_name_ = @"";

  /** Compiled shader resources. */
  id<MTLLibrary> shader_library_vert_ = nil;
  id<MTLLibrary> shader_library_frag_ = nil;
  id<MTLLibrary> shader_library_compute_ = nil;
  bool valid_ = false;

  /** Render pipeline state and PSO caching. */
  /* Metal API Descriptor used for creation of unique PSOs based on rendering state. */
  MTLRenderPipelineDescriptor *pso_descriptor_ = nil;
  /* Metal backend struct containing all high-level pipeline state parameters
   * which contribute to instantiation of a unique PSO. */
  MTLRenderPipelineStateDescriptor current_pipeline_state_;
  /* Cache of compiled PipelineStateObjects. */
  blender::Map<MTLRenderPipelineStateDescriptor, MTLRenderPipelineStateInstance *> pso_cache_;
  std::mutex pso_cache_lock_;

  /** Compute pipeline state and Compute PSO caching. */
  MTLComputePipelineStateCommon compute_pso_common_state_;
  blender::Map<MTLComputePipelineStateDescriptor, MTLComputePipelineStateInstance *>
      compute_pso_cache_;

  /* True to enable multi-layered rendering support. */
  bool uses_gpu_layer = false;

  /* True to enable multi-viewport rendering support. */
  bool uses_gpu_viewport_index = false;

  /** SSBO Vertex fetch pragma options. */
  /* Indicates whether to pass in VertexBuffer's as regular buffer bindings
   * and perform vertex assembly manually, rather than using Stage-in.
   * This is used to give a vertex shader full access to all of the
   * vertex data.
   * This is primarily used for optimization techniques and
   * alternative solutions for Geometry-shaders which are unsupported
   * by Metal. */
  bool use_ssbo_vertex_fetch_mode_ = false;
  /* Output primitive type when rendering sing ssbo_vertex_fetch. */
  MTLPrimitiveType ssbo_vertex_fetch_output_prim_type_;

  /* Output vertices per original vertex shader instance.
   * This number will be multiplied by the number of input primitives
   * from the source draw call. */
  uint32_t ssbo_vertex_fetch_output_num_verts_ = 0;

  bool ssbo_vertex_attribute_bind_active_ = false;
  int ssbo_vertex_attribute_bind_mask_ = 0;
  bool ssbo_vbo_slot_used_[MTL_SSBO_VERTEX_FETCH_MAX_VBOS];

  struct ShaderSSBOAttributeBinding {
    int attribute_index = -1;
    int uniform_stride;
    int uniform_offset;
    int uniform_fetchmode;
    int uniform_vbo_id;
    int uniform_attr_type;
  };
  ShaderSSBOAttributeBinding cached_ssbo_attribute_bindings_[MTL_MAX_VERTEX_INPUT_ATTRIBUTES] = {};

  /* Metal Shader Uniform data store.
   * This blocks is used to store current shader push_constant
   * data before it is submitted to the GPU. This is currently
   * stored per shader instance, though depending on GPU module
   * functionality, this could potentially be a global data store.
   * This data is associated with the PushConstantBlock, which is
   * always at index zero in the UBO list. */
  void *push_constant_data_ = nullptr;
  bool push_constant_modified_ = false;

  /* Special definition for Max TotalThreadsPerThreadgroup tuning. */
  uint maxTotalThreadsPerThreadgroup_Tuning_ = 0;

  /* Set to true when batch compiling */
  bool async_compilation_ = false;

  bool finalize_shader(const shader::ShaderCreateInfo *info = nullptr);

 public:
  MTLShader(MTLContext *ctx, const char *name);
  MTLShader(MTLContext *ctx,
            MTLShaderInterface *interface,
            const char *name,
            NSString *input_vertex_source,
            NSString *input_fragment_source,
            NSString *vertex_function_name_,
            NSString *fragment_function_name_);
  ~MTLShader();

  void init(const shader::ShaderCreateInfo & /*info*/, bool is_batch_compilation) override;

  /* Assign GLSL source. */
  void vertex_shader_from_glsl(MutableSpan<StringRefNull> sources) override;
  void geometry_shader_from_glsl(MutableSpan<StringRefNull> sources) override;
  void fragment_shader_from_glsl(MutableSpan<StringRefNull> sources) override;
  void compute_shader_from_glsl(MutableSpan<StringRefNull> sources) override;

  /* Compile and build - Return true if successful. */
  bool finalize(const shader::ShaderCreateInfo *info = nullptr) override;
  bool finalize_compute(const shader::ShaderCreateInfo *info);
  void warm_cache(int limit) override;

  /* Utility. */
  bool is_valid()
  {
    return valid_;
  }
  bool has_compute_shader_lib()
  {
    return (shader_library_compute_ != nil);
  }
  bool has_parent_shader()
  {
    return (parent_shader_ != nil);
  }
  MTLRenderPipelineStateDescriptor &get_current_pipeline_state()
  {
    return current_pipeline_state_;
  }
  MTLShaderInterface *get_interface()
  {
    return static_cast<MTLShaderInterface *>(this->interface);
  }
  void *get_push_constant_data()
  {
    return push_constant_data_;
  }

  /* Shader source generators from create-info.
   * These aren't all used by Metal, as certain parts of source code generation
   * for shader entry-points and resource mapping occur during `finalize`. */
  std::string resources_declare(const shader::ShaderCreateInfo &info) const override;
  std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const override;
  std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const override;

  void transform_feedback_names_set(Span<const char *> name_list,
                                    const eGPUShaderTFBType geom_type) override;
  bool transform_feedback_enable(VertBuf *buf) override;
  void transform_feedback_disable() override;

  void bind() override;
  void unbind() override;

  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;
  bool get_push_constant_is_dirty();
  void push_constant_bindstate_mark_dirty(bool is_dirty);

  /* SSBO vertex fetch draw parameters. */
  bool get_uses_ssbo_vertex_fetch() const override
  {
    return use_ssbo_vertex_fetch_mode_;
  }
  int get_ssbo_vertex_fetch_output_num_verts() const override
  {
    return ssbo_vertex_fetch_output_num_verts_;
  }

  /* DEPRECATED: Kept only because of BGL API. (Returning -1 in METAL). */
  int program_handle_get() const override
  {
    return -1;
  }

  MTLPrimitiveType get_ssbo_vertex_fetch_output_prim_type()
  {
    return ssbo_vertex_fetch_output_prim_type_;
  }
  static int ssbo_vertex_type_to_attr_type(MTLVertexFormat attribute_type);
  void prepare_ssbo_vertex_fetch_metadata();

  /* SSBO Vertex Bindings Utility functions. */
  void ssbo_vertex_fetch_bind_attributes_begin();
  void ssbo_vertex_fetch_bind_attribute(const MTLSSBOAttribute &ssbo_attr);
  void ssbo_vertex_fetch_bind_attributes_end(id<MTLRenderCommandEncoder> active_encoder);

  /* Metal shader properties and source mapping. */
  void set_vertex_function_name(NSString *vetex_function_name);
  void set_fragment_function_name(NSString *fragment_function_name);
  void set_compute_function_name(NSString *compute_function_name);
  void shader_source_from_msl(NSString *input_vertex_source, NSString *input_fragment_source);
  void shader_compute_source_from_msl(NSString *input_compute_source);
  void set_interface(MTLShaderInterface *interface);

  MTLRenderPipelineStateInstance *bake_current_pipeline_state(MTLContext *ctx,
                                                              MTLPrimitiveTopologyClass prim_type);
  MTLRenderPipelineStateInstance *bake_pipeline_state(
      MTLContext *ctx,
      MTLPrimitiveTopologyClass prim_type,
      const MTLRenderPipelineStateDescriptor &pipeline_descriptor);

  MTLComputePipelineStateInstance *bake_compute_pipeline_state(
      MTLContext *ctx, MTLComputePipelineStateDescriptor &compute_pipeline_descriptor);

  const MTLComputePipelineStateCommon &get_compute_common_state()
  {
    return compute_pso_common_state_;
  }
  /* Transform Feedback. */
  VertBuf *get_transform_feedback_active_buffer();
  bool has_transform_feedback_varying(std::string str);

 private:
  /* Generate MSL shader from GLSL source. */
  bool generate_msl_from_glsl(const shader::ShaderCreateInfo *info);
  bool generate_msl_from_glsl_compute(const shader::ShaderCreateInfo *info);

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLShader");
};

class MTLParallelShaderCompiler {
 private:
  enum ParallelWorkType {
    PARALLELWORKTYPE_UNSPECIFIED,
    PARALLELWORKTYPE_COMPILE_SHADER,
    PARALLELWORKTYPE_BAKE_PSO,
  };

  struct ParallelWork {
    const shader::ShaderCreateInfo *info = nullptr;
    class MTLShaderCompiler *shader_compiler = nullptr;
    MTLShader *shader = nullptr;
    Vector<Shader::Constants::Value> specialization_values;

    ParallelWorkType work_type = PARALLELWORKTYPE_UNSPECIFIED;
    bool is_ready = false;
  };

  struct Batch {
    Vector<ParallelWork *> items;
    bool is_ready = false;
  };

  std::mutex batch_mutex;
  BatchHandle next_batch_handle = 1;
  Map<BatchHandle, Batch> batches;

  std::vector<std::thread> compile_threads;

  volatile bool terminate_compile_threads;
  std::condition_variable cond_var;
  std::mutex queue_mutex;
  std::deque<ParallelWork *> parallel_work_queue;

  void parallel_compilation_thread_func(GPUContext *blender_gpu_context);
  BatchHandle create_batch(size_t batch_size);
  void add_item_to_batch(ParallelWork *work_item, BatchHandle batch_handle);
  void add_parallel_item_to_queue(ParallelWork *add_parallel_item_to_queuework_item,
                                  BatchHandle batch_handle);

  std::atomic<int> ref_count = 1;

 public:
  MTLParallelShaderCompiler();
  ~MTLParallelShaderCompiler();

  void create_compile_threads();
  BatchHandle batch_compile(MTLShaderCompiler *shade_compiler,
                            Span<const shader::ShaderCreateInfo *> &infos);
  bool batch_is_ready(BatchHandle handle);
  Vector<Shader *> batch_finalize(BatchHandle &handle);

  SpecializationBatchHandle precompile_specializations(Span<ShaderSpecialization> specializations);
  bool specialization_batch_is_ready(SpecializationBatchHandle &handle);

  void increment_ref_count()
  {
    ref_count++;
  }
  void decrement_ref_count()
  {
    BLI_assert(ref_count > 0);
    ref_count--;
  }
  int get_ref_count()
  {
    return ref_count;
  }
};

class MTLShaderCompiler : public ShaderCompiler {
 private:
  MTLParallelShaderCompiler *parallel_shader_compiler;

 public:
  MTLShaderCompiler();
  virtual ~MTLShaderCompiler() override;

  virtual BatchHandle batch_compile(Span<const shader::ShaderCreateInfo *> &infos) override;
  virtual bool batch_is_ready(BatchHandle handle) override;
  virtual Vector<Shader *> batch_finalize(BatchHandle &handle) override;

  virtual SpecializationBatchHandle precompile_specializations(
      Span<ShaderSpecialization> specializations) override;
  virtual bool specialization_batch_is_ready(SpecializationBatchHandle &handle) override;

  void release_parallel_shader_compiler();
};

/* Vertex format conversion.
 * Determines whether it is possible to resize a vertex attribute type
 * during input assembly. A conversion is implied by the  difference
 * between the input vertex descriptor (from MTLBatch/MTLImmediate)
 * and the type specified in the shader source.
 *
 * e.g. vec3 to vec4 expansion, or vec4 to vec2 truncation.
 * NOTE: Vector expansion will replace empty elements with the values
 * (0,0,0,1).
 *
 * If implicit format resize is not possible, this function
 * returns false.
 *
 * Implicitly supported conversions in Metal are described here:
 * https://developer.apple.com/documentation/metal/mtlvertexattributedescriptor/1516081-format?language=objc
 */
inline MTLVertexFormat format_resize_comp(MTLVertexFormat mtl_format, uint32_t components)
{
#define RESIZE_TYPE(_type, _suffix) \
  case MTLVertexFormat##_type##_suffix: \
  case MTLVertexFormat##_type##2##_suffix: \
  case MTLVertexFormat##_type##3##_suffix: \
  case MTLVertexFormat##_type##4##_suffix: \
    switch (components) { \
      case 1: \
        return MTLVertexFormat##_type##_suffix; \
      case 2: \
        return MTLVertexFormat##_type##2##_suffix; \
      case 3: \
        return MTLVertexFormat##_type##3##_suffix; \
      case 4: \
        return MTLVertexFormat##_type##4##_suffix; \
    } \
    break;

  switch (mtl_format) {
    RESIZE_TYPE(Char, )
    RESIZE_TYPE(Char, Normalized)
    RESIZE_TYPE(UChar, )
    RESIZE_TYPE(UChar, Normalized)
    RESIZE_TYPE(Short, )
    RESIZE_TYPE(Short, Normalized)
    RESIZE_TYPE(UShort, )
    RESIZE_TYPE(UShort, Normalized)
    RESIZE_TYPE(Int, )
    RESIZE_TYPE(UInt, )
    RESIZE_TYPE(Half, )
    RESIZE_TYPE(Float, )
    default:
      /* Can only call this function on format that can be resized. */
      BLI_assert_unreachable();
      break;
  }

#undef RESIZE_TYPE
  return MTLVertexFormatInvalid;
}

inline MTLVertexFormat format_get_component_type(MTLVertexFormat mtl_format)
{
  return format_resize_comp(mtl_format, 1);
}

inline MTLVertexFormat to_mtl(GPUVertCompType component_type,
                              GPUVertFetchMode fetch_mode,
                              uint32_t component_len)
{
#define FORMAT_PER_COMP(_type, _suffix) \
  switch (component_len) { \
    case 1: \
      return MTLVertexFormat##_type##_suffix; \
    case 2: \
      return MTLVertexFormat##_type##2##_suffix; \
    case 3: \
      return MTLVertexFormat##_type##3##_suffix; \
    case 4: \
      return MTLVertexFormat##_type##4##_suffix; \
    default: \
      BLI_assert_msg(0, "Invalid attribute component count"); \
      break; \
  } \
  break;

#define FORMAT_PER_COMP_SMALL_INT(_type) \
  switch (fetch_mode) { \
    case GPU_FETCH_INT: \
      FORMAT_PER_COMP(_type, ) \
    case GPU_FETCH_INT_TO_FLOAT_UNIT: \
      FORMAT_PER_COMP(_type, Normalized) \
    case GPU_FETCH_FLOAT: \
      BLI_assert_msg(0, "Invalid fetch mode for integer attribute"); \
      break; \
    case GPU_FETCH_INT_TO_FLOAT: \
      /* Fallback to manual conversion */ \
      break; \
  } \
  break;

#define FORMAT_PER_COMP_INT(_type) \
  switch (fetch_mode) { \
    case GPU_FETCH_INT: \
      FORMAT_PER_COMP(_type, ) \
    case GPU_FETCH_FLOAT: \
      BLI_assert_msg(0, "Invalid fetch mode for integer attribute"); \
      break; \
    case GPU_FETCH_INT_TO_FLOAT_UNIT: \
    case GPU_FETCH_INT_TO_FLOAT: \
      /* Fallback to manual conversion */ \
      break; \
  } \
  break;

  switch (component_type) {
    case GPU_COMP_I8:
      FORMAT_PER_COMP_SMALL_INT(Char)
    case GPU_COMP_U8:
      FORMAT_PER_COMP_SMALL_INT(UChar)
    case GPU_COMP_I16:
      FORMAT_PER_COMP_SMALL_INT(Short)
    case GPU_COMP_U16:
      FORMAT_PER_COMP_SMALL_INT(UShort)
    case GPU_COMP_I32:
      FORMAT_PER_COMP_INT(Int)
    case GPU_COMP_U32:
      FORMAT_PER_COMP_INT(UInt)
    case GPU_COMP_F32:
      switch (fetch_mode) {
        case GPU_FETCH_FLOAT:
          FORMAT_PER_COMP(Float, )
          break;
        case GPU_FETCH_INT:
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
        case GPU_FETCH_INT_TO_FLOAT:
          BLI_assert_msg(0, "Invalid fetch mode for float attribute");
          break;
      }
    case GPU_COMP_I10:
      switch (fetch_mode) {
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          return MTLVertexFormatInt1010102Normalized;
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT:
        case GPU_FETCH_INT_TO_FLOAT:
          BLI_assert_msg(0, "Invalid fetch mode for compressed attribute");
          break;
      }
    case GPU_COMP_MAX:
      BLI_assert_unreachable();
      break;
  }
#undef FORMAT_PER_COMP
  /* Loading mode not natively supported. */
  return MTLVertexFormatInvalid;
}

inline int mtl_format_component_len(MTLVertexFormat format)
{
#define FORMAT_PER_TYPE(_comp, _value) \
  case MTLVertexFormatChar##_comp: \
  case MTLVertexFormatChar##_comp##Normalized: \
  case MTLVertexFormatUChar##_comp: \
  case MTLVertexFormatUChar##_comp##Normalized: \
  case MTLVertexFormatShort##_comp: \
  case MTLVertexFormatShort##_comp##Normalized: \
  case MTLVertexFormatUShort##_comp: \
  case MTLVertexFormatUShort##_comp##Normalized: \
  case MTLVertexFormatInt##_comp: \
  case MTLVertexFormatUInt##_comp: \
  case MTLVertexFormatHalf##_comp: \
  case MTLVertexFormatFloat##_comp: \
    return _value;

  switch (format) {
    FORMAT_PER_TYPE(, 1)
    FORMAT_PER_TYPE(2, 2)
    FORMAT_PER_TYPE(3, 3)
    FORMAT_PER_TYPE(4, 4)
    case MTLVertexFormatUInt1010102Normalized:
    case MTLVertexFormatInt1010102Normalized:
    case MTLVertexFormatUChar4Normalized_BGRA:
      return 4;
    case MTLVertexFormatFloatRG11B10:
      return 3;
    case MTLVertexFormatFloatRGB9E5:
      return 3;
    case MTLVertexFormatInvalid:
      return -1;
  }

#undef FORMAT_PER_TYPE
  return -1;
}

inline bool mtl_format_is_normalized(MTLVertexFormat format)
{
#define FORMAT_PER_TYPE(_comp) \
  case MTLVertexFormatChar##_comp##Normalized: \
  case MTLVertexFormatUChar##_comp##Normalized: \
  case MTLVertexFormatShort##_comp##Normalized: \
  case MTLVertexFormatUShort##_comp##Normalized: \
    return true;

  switch (format) {
    FORMAT_PER_TYPE()
    FORMAT_PER_TYPE(2)
    FORMAT_PER_TYPE(3)
    FORMAT_PER_TYPE(4)
    default:
      break;
  }

#undef FORMAT_PER_TYPE
  return false;
}

/**
 * Returns whether the METAL API can internally convert between the input type of data in the
 * incoming vertex buffer and the format used by the vertex attribute inside the shader.
 *
 * - Returns TRUE if the type can be converted internally, along with returning the appropriate
 *   type to be passed into the #MTLVertexAttributeDescriptorPSO.
 *
 * - Returns FALSE if the type cannot be converted internally e.g. casting Int4 to Float4.
 *
 * If implicit conversion is not possible, then we can fallback to performing manual attribute
 * conversion using the special attribute read function specializations in the shader.
 * These functions selectively convert between types based on the specified vertex
 * attribute `GPUVertFetchMode fetch_mode` e.g. `GPU_FETCH_INT`.
 */
inline MTLVertexFormat mtl_convert_vertex_format_ex(MTLVertexFormat shader_attr_format,
                                                    GPUVertCompType component_type,
                                                    uint32_t component_len,
                                                    GPUVertFetchMode fetch_mode)
{
  MTLVertexFormat vertex_attr_format = to_mtl(component_type, fetch_mode, component_len);

  if (vertex_attr_format == MTLVertexFormatInvalid) {
    /* No valid builtin conversion known or error. */
    return vertex_attr_format;
  }

  if (vertex_attr_format == shader_attr_format) {
    /* Everything matches. Nothing to do. */
    return vertex_attr_format;
  }

  if (vertex_attr_format == MTLVertexFormatInt1010102Normalized) {
    BLI_assert_msg(format_get_component_type(shader_attr_format) == MTLVertexFormatFloat,
                   "Vertex format is GPU_COMP_I10 but shader input is not float");
    return vertex_attr_format;
  }

  /* Attribute type mismatch. Check if casting is supported. */
  MTLVertexFormat shader_attr_comp_type = format_get_component_type(shader_attr_format);
  MTLVertexFormat vertex_attr_comp_type = format_get_component_type(vertex_attr_format);

  if (shader_attr_comp_type == vertex_attr_comp_type) {
    /* Conversion of vectors of different lengths is valid. */
    return vertex_attr_format;
  }

  if (shader_attr_comp_type != MTLVertexFormatFloat) {
    BLI_assert_msg(vertex_attr_comp_type != MTLVertexFormatFloat,
                   "Vertex format is GPU_COMP_F32 but shader input is not float");
  }
  /* Casting normalized MTLVertexFormat types are only valid to float or half. */
  if (shader_attr_comp_type == MTLVertexFormatFloat) {
    BLI_assert_msg(mtl_format_is_normalized(vertex_attr_comp_type),
                   "Vertex format is INT_TO_FLOAT_UNIT but shader input is not float");
  }
  /* The sign of an integer MTLVertexFormat can not be cast to a shader argument with an integer
   * type of a different sign. */
  if (shader_attr_comp_type == MTLVertexFormatInt) {
    BLI_assert_msg(ELEM(vertex_attr_comp_type, MTLVertexFormatChar, MTLVertexFormatShort),
                   "Vertex format is either I8 or I16 but shader input is not float");
  }
  if (shader_attr_comp_type == MTLVertexFormatUInt) {
    BLI_assert_msg(ELEM(vertex_attr_comp_type, MTLVertexFormatUChar, MTLVertexFormatUShort),
                   "Vertex format is either U8 or U16 but shader input is not float");
  }
  /* Valid automatic conversion. */
  return vertex_attr_format;
}

inline bool mtl_convert_vertex_format(MTLVertexFormat shader_attr_format,
                                      GPUVertCompType component_type,
                                      uint32_t component_len,
                                      GPUVertFetchMode fetch_mode,
                                      MTLVertexFormat *r_convertedFormat)
{
  *r_convertedFormat = mtl_convert_vertex_format_ex(
      shader_attr_format, component_type, component_len, fetch_mode);
  return (*r_convertedFormat != MTLVertexFormatInvalid);
}

}  // namespace blender::gpu
