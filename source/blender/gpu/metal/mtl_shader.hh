/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "GPU_batch.h"
#include "GPU_capabilities.h"
#include "GPU_shader.h"
#include "GPU_vertex_format.h"

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <functional>
#include <unordered_map>

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

/* Metal COmpute Pipeline State instance. */
struct MTLComputePipelineStateInstance {
  /* Function instances with specialization.
   * Required for argument encoder construction. */
  id<MTLFunction> compute = nil;
  /* PSO handle. */
  id<MTLComputePipelineState> pso = nil;
  /* Base bind index for binding uniform buffers, offset based on other
   * bound buffers such as vertex buffers, as the count can vary. */
  int base_uniform_buffer_index = -1;
  /* Base bind index for binding storage buffers. */
  int base_storage_buffer_index = -1;

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
 **/
class MTLShader : public Shader {
  friend shader::ShaderCreateInfo;
  friend shader::StageInterfaceInfo;

 public:
  /* Cached SSBO vertex fetch attribute uniform locations. */
  int uni_ssbo_input_prim_type_loc = -1;
  int uni_ssbo_input_vert_count_loc = -1;
  int uni_ssbo_uses_indexed_rendering = -1;
  int uni_ssbo_uses_index_mode_u16 = -1;

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
  GPUVertBuf *transform_feedback_vertbuf_ = nullptr;

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
  MTLComputePipelineStateInstance compute_pso_instance_;

  /* True to enable multi-layered rendering support. */
  bool uses_mtl_array_index_ = false;

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

  /* Assign GLSL source. */
  void vertex_shader_from_glsl(MutableSpan<const char *> sources) override;
  void geometry_shader_from_glsl(MutableSpan<const char *> sources) override;
  void fragment_shader_from_glsl(MutableSpan<const char *> sources) override;
  void compute_shader_from_glsl(MutableSpan<const char *> sources) override;

  /* Compile and build - Return true if successful. */
  bool finalize(const shader::ShaderCreateInfo *info = nullptr) override;
  bool finalize_compute(const shader::ShaderCreateInfo *info);
  void warm_cache(int limit) override;

  /* Utility. */
  bool is_valid()
  {
    return valid_;
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
  bool transform_feedback_enable(GPUVertBuf *buf) override;
  void transform_feedback_disable() override;

  void bind() override;
  void unbind() override;

  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;
  bool get_push_constant_is_dirty();
  void push_constant_bindstate_mark_dirty(bool is_dirty);

  /* DEPRECATED: Kept only because of BGL API. (Returning -1 in METAL). */
  int program_handle_get() const override
  {
    return -1;
  }

  bool get_uses_ssbo_vertex_fetch()
  {
    return use_ssbo_vertex_fetch_mode_;
  }
  MTLPrimitiveType get_ssbo_vertex_fetch_output_prim_type()
  {
    return ssbo_vertex_fetch_output_prim_type_;
  }
  uint32_t get_ssbo_vertex_fetch_output_num_verts()
  {
    return ssbo_vertex_fetch_output_num_verts_;
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

  bool bake_compute_pipeline_state(MTLContext *ctx);
  const MTLComputePipelineStateInstance &get_compute_pipeline_state();

  /* Transform Feedback. */
  GPUVertBuf *get_transform_feedback_active_buffer();
  bool has_transform_feedback_varying(std::string str);

 private:
  /* Generate MSL shader from GLSL source. */
  bool generate_msl_from_glsl(const shader::ShaderCreateInfo *info);
  bool generate_msl_from_glsl_compute(const shader::ShaderCreateInfo *info);

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLShader");
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
inline bool mtl_vertex_format_resize(MTLVertexFormat mtl_format,
                                     uint32_t components,
                                     MTLVertexFormat *r_convertedFormat)
{
  MTLVertexFormat out_vert_format = MTLVertexFormatInvalid;
  switch (mtl_format) {
    /* Char. */
    case MTLVertexFormatChar:
    case MTLVertexFormatChar2:
    case MTLVertexFormatChar3:
    case MTLVertexFormatChar4:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatChar;
          break;
        case 2:
          out_vert_format = MTLVertexFormatChar2;
          break;
        case 3:
          out_vert_format = MTLVertexFormatChar3;
          break;
        case 4:
          out_vert_format = MTLVertexFormatChar4;
          break;
      }
      break;

    /* Normalized Char. */
    case MTLVertexFormatCharNormalized:
    case MTLVertexFormatChar2Normalized:
    case MTLVertexFormatChar3Normalized:
    case MTLVertexFormatChar4Normalized:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatCharNormalized;
          break;
        case 2:
          out_vert_format = MTLVertexFormatChar2Normalized;
          break;
        case 3:
          out_vert_format = MTLVertexFormatChar3Normalized;
          break;
        case 4:
          out_vert_format = MTLVertexFormatChar4Normalized;
          break;
      }
      break;

    /* Unsigned Char. */
    case MTLVertexFormatUChar:
    case MTLVertexFormatUChar2:
    case MTLVertexFormatUChar3:
    case MTLVertexFormatUChar4:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatUChar;
          break;
        case 2:
          out_vert_format = MTLVertexFormatUChar2;
          break;
        case 3:
          out_vert_format = MTLVertexFormatUChar3;
          break;
        case 4:
          out_vert_format = MTLVertexFormatUChar4;
          break;
      }
      break;

    /* Normalized Unsigned char */
    case MTLVertexFormatUCharNormalized:
    case MTLVertexFormatUChar2Normalized:
    case MTLVertexFormatUChar3Normalized:
    case MTLVertexFormatUChar4Normalized:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatUCharNormalized;
          break;
        case 2:
          out_vert_format = MTLVertexFormatUChar2Normalized;
          break;
        case 3:
          out_vert_format = MTLVertexFormatUChar3Normalized;
          break;
        case 4:
          out_vert_format = MTLVertexFormatUChar4Normalized;
          break;
      }
      break;

    /* Short. */
    case MTLVertexFormatShort:
    case MTLVertexFormatShort2:
    case MTLVertexFormatShort3:
    case MTLVertexFormatShort4:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatShort;
          break;
        case 2:
          out_vert_format = MTLVertexFormatShort2;
          break;
        case 3:
          out_vert_format = MTLVertexFormatShort3;
          break;
        case 4:
          out_vert_format = MTLVertexFormatShort4;
          break;
      }
      break;

    /* Normalized Short. */
    case MTLVertexFormatShortNormalized:
    case MTLVertexFormatShort2Normalized:
    case MTLVertexFormatShort3Normalized:
    case MTLVertexFormatShort4Normalized:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatShortNormalized;
          break;
        case 2:
          out_vert_format = MTLVertexFormatShort2Normalized;
          break;
        case 3:
          out_vert_format = MTLVertexFormatShort3Normalized;
          break;
        case 4:
          out_vert_format = MTLVertexFormatShort4Normalized;
          break;
      }
      break;

    /* Unsigned Short. */
    case MTLVertexFormatUShort:
    case MTLVertexFormatUShort2:
    case MTLVertexFormatUShort3:
    case MTLVertexFormatUShort4:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatUShort;
          break;
        case 2:
          out_vert_format = MTLVertexFormatUShort2;
          break;
        case 3:
          out_vert_format = MTLVertexFormatUShort3;
          break;
        case 4:
          out_vert_format = MTLVertexFormatUShort4;
          break;
      }
      break;

    /* Normalized Unsigned Short. */
    case MTLVertexFormatUShortNormalized:
    case MTLVertexFormatUShort2Normalized:
    case MTLVertexFormatUShort3Normalized:
    case MTLVertexFormatUShort4Normalized:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatUShortNormalized;
          break;
        case 2:
          out_vert_format = MTLVertexFormatUShort2Normalized;
          break;
        case 3:
          out_vert_format = MTLVertexFormatUShort3Normalized;
          break;
        case 4:
          out_vert_format = MTLVertexFormatUShort4Normalized;
          break;
      }
      break;

    /* Integer. */
    case MTLVertexFormatInt:
    case MTLVertexFormatInt2:
    case MTLVertexFormatInt3:
    case MTLVertexFormatInt4:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatInt;
          break;
        case 2:
          out_vert_format = MTLVertexFormatInt2;
          break;
        case 3:
          out_vert_format = MTLVertexFormatInt3;
          break;
        case 4:
          out_vert_format = MTLVertexFormatInt4;
          break;
      }
      break;

    /* Unsigned Integer. */
    case MTLVertexFormatUInt:
    case MTLVertexFormatUInt2:
    case MTLVertexFormatUInt3:
    case MTLVertexFormatUInt4:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatUInt;
          break;
        case 2:
          out_vert_format = MTLVertexFormatUInt2;
          break;
        case 3:
          out_vert_format = MTLVertexFormatUInt3;
          break;
        case 4:
          out_vert_format = MTLVertexFormatUInt4;
          break;
      }
      break;

    /* Half. */
    case MTLVertexFormatHalf:
    case MTLVertexFormatHalf2:
    case MTLVertexFormatHalf3:
    case MTLVertexFormatHalf4:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatHalf;
          break;
        case 2:
          out_vert_format = MTLVertexFormatHalf2;
          break;
        case 3:
          out_vert_format = MTLVertexFormatHalf3;
          break;
        case 4:
          out_vert_format = MTLVertexFormatHalf4;
          break;
      }
      break;

    /* Float. */
    case MTLVertexFormatFloat:
    case MTLVertexFormatFloat2:
    case MTLVertexFormatFloat3:
    case MTLVertexFormatFloat4:
      switch (components) {
        case 1:
          out_vert_format = MTLVertexFormatFloat;
          break;
        case 2:
          out_vert_format = MTLVertexFormatFloat2;
          break;
        case 3:
          out_vert_format = MTLVertexFormatFloat3;
          break;
        case 4:
          out_vert_format = MTLVertexFormatFloat4;
          break;
      }
      break;

    /* Other formats */
    default:
      out_vert_format = mtl_format;
      break;
  }
  *r_convertedFormat = out_vert_format;
  return out_vert_format != MTLVertexFormatInvalid;
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
inline bool mtl_convert_vertex_format(MTLVertexFormat shader_attrib_format,
                                      GPUVertCompType component_type,
                                      uint32_t component_length,
                                      GPUVertFetchMode fetch_mode,
                                      MTLVertexFormat *r_convertedFormat)
{
  bool normalized = (fetch_mode == GPU_FETCH_INT_TO_FLOAT_UNIT);
  MTLVertexFormat out_vert_format = MTLVertexFormatInvalid;

  switch (component_type) {

    case GPU_COMP_I8:
      switch (fetch_mode) {
        case GPU_FETCH_INT:
          if (shader_attrib_format == MTLVertexFormatChar ||
              shader_attrib_format == MTLVertexFormatChar2 ||
              shader_attrib_format == MTLVertexFormatChar3 ||
              shader_attrib_format == MTLVertexFormatChar4)
          {

            /* No conversion Needed (as type matches) - Just a vector resize if needed. */
            bool can_convert = mtl_vertex_format_resize(
                shader_attrib_format, component_type, &out_vert_format);

            /* Ensure format resize successful. */
            BLI_assert(can_convert);
            UNUSED_VARS_NDEBUG(can_convert);
          }
          else if (shader_attrib_format == MTLVertexFormatInt4 && component_length == 4) {
            /* Allow type expansion - Shader expects MTLVertexFormatInt4, we can supply a type
             * with fewer bytes if component count is the same. Sign must also match original type
             *  -- which is not a problem in this case. */
            out_vert_format = MTLVertexFormatChar4;
          }
          else if (shader_attrib_format == MTLVertexFormatInt3 && component_length == 3) {
            /* Same as above case for matching length and signage (Len=3). */
            out_vert_format = MTLVertexFormatChar3;
          }
          else if (shader_attrib_format == MTLVertexFormatInt2 && component_length == 2) {
            /* Same as above case for matching length and signage (Len=2). */
            out_vert_format = MTLVertexFormatChar2;
          }
          else if (shader_attrib_format == MTLVertexFormatInt && component_length == 1) {
            /* Same as above case for matching length and signage (Len=1). */
            out_vert_format = MTLVertexFormatChar;
          }
          else if (shader_attrib_format == MTLVertexFormatInt && component_length == 4) {
            /* Special case here, format has been specified as GPU_COMP_U8 with 4 components, which
             * is equivalent to an Int -- so data will be compatible with the shader interface. */
            out_vert_format = MTLVertexFormatInt;
          }
          else {
            BLI_assert_msg(false,
                           "Source vertex data format is either Char, Char2, Char3, Char4 but "
                           "format in shader interface is NOT compatible.\n");
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;

        /* Source vertex data is integer type, but shader interface type is floating point.
         * If the input attribute is specified as normalized, we can convert. */
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          if (normalized) {
            switch (component_length) {
              case 1:
                out_vert_format = MTLVertexFormatCharNormalized;
                break;
              case 2:
                out_vert_format = MTLVertexFormatChar2Normalized;
                break;
              case 3:
                out_vert_format = MTLVertexFormatChar3Normalized;
                break;
              case 4:
                out_vert_format = MTLVertexFormatChar4Normalized;
                break;
              default:
                BLI_assert_msg(false, "invalid vertex format");
                out_vert_format = MTLVertexFormatInvalid;
            }
          }
          else {
            /* Cannot convert. */
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;
      }
      break;

    case GPU_COMP_U8:
      switch (fetch_mode) {
        /* Fetching INT: Check backing shader format matches source input. */
        case GPU_FETCH_INT:
          if (shader_attrib_format == MTLVertexFormatUChar ||
              shader_attrib_format == MTLVertexFormatUChar2 ||
              shader_attrib_format == MTLVertexFormatUChar3 ||
              shader_attrib_format == MTLVertexFormatUChar4)
          {

            /* No conversion Needed (as type matches) - Just a vector resize if needed. */
            bool can_convert = mtl_vertex_format_resize(
                shader_attrib_format, component_length, &out_vert_format);

            /* Ensure format resize successful. */
            BLI_assert(can_convert);
            UNUSED_VARS_NDEBUG(can_convert);
            /* TODO(Metal): Add other format conversions if needed. Currently no attributes hit
             * this path. */
          }
          else if (shader_attrib_format == MTLVertexFormatUInt4 && component_length == 4) {
            /* Allow type expansion - Shader expects MTLVertexFormatUInt4, we can supply a type
             * with fewer bytes if component count is the same. */
            out_vert_format = MTLVertexFormatUChar4;
          }
          else if (shader_attrib_format == MTLVertexFormatUInt3 && component_length == 3) {
            /* Same as above case for matching length and signage (Len=3). */
            out_vert_format = MTLVertexFormatUChar3;
          }
          else if (shader_attrib_format == MTLVertexFormatUInt2 && component_length == 2) {
            /* Same as above case for matching length and signage (Len=2). */
            out_vert_format = MTLVertexFormatUChar2;
          }
          else if (shader_attrib_format == MTLVertexFormatUInt && component_length == 1) {
            /* Same as above case for matching length and signage (Len=1). */
            out_vert_format = MTLVertexFormatUChar;
          }
          else if (shader_attrib_format == MTLVertexFormatInt && component_length == 4) {
            /* Special case here, format has been specified as GPU_COMP_U8 with 4 components, which
             * is equivalent to an Int-- so data will be compatible with shader interface. */
            out_vert_format = MTLVertexFormatInt;
          }
          else if (shader_attrib_format == MTLVertexFormatUInt && component_length == 4) {
            /* Special case here, format has been specified as GPU_COMP_U8 with 4 components, which
             * is equivalent to a UInt-- so data will be compatible with shader interface. */
            out_vert_format = MTLVertexFormatUInt;
          }
          else {
            BLI_assert_msg(false,
                           "Source vertex data format is either UChar, UChar2, UChar3, UChar4 but "
                           "format in shader interface is NOT compatible.\n");
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;

        /* Source vertex data is integral type, but shader interface type is floating point.
         * If the input attribute is specified as normalized, we can convert. */
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          if (normalized) {
            switch (component_length) {
              case 1:
                out_vert_format = MTLVertexFormatUCharNormalized;
                break;
              case 2:
                out_vert_format = MTLVertexFormatUChar2Normalized;
                break;
              case 3:
                out_vert_format = MTLVertexFormatUChar3Normalized;
                break;
              case 4:
                out_vert_format = MTLVertexFormatUChar4Normalized;
                break;
              default:
                BLI_assert_msg(false, "invalid vertex format");
                out_vert_format = MTLVertexFormatInvalid;
            }
          }
          else {
            /* Cannot convert. */
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;
      }
      break;

    case GPU_COMP_I16:
      switch (fetch_mode) {
        case GPU_FETCH_INT:
          if (shader_attrib_format == MTLVertexFormatShort ||
              shader_attrib_format == MTLVertexFormatShort2 ||
              shader_attrib_format == MTLVertexFormatShort3 ||
              shader_attrib_format == MTLVertexFormatShort4)
          {
            /* No conversion Needed (as type matches) - Just a vector resize if needed. */
            bool can_convert = mtl_vertex_format_resize(
                shader_attrib_format, component_length, &out_vert_format);

            /* Ensure conversion successful. */
            BLI_assert(can_convert);
            UNUSED_VARS_NDEBUG(can_convert);
          }
          else {
            BLI_assert_msg(false,
                           "Source vertex data format is either Short, Short2, Short3, Short4 but "
                           "format in shader interface is NOT compatible.\n");
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;

        /* Source vertex data is integral type, but shader interface type is floating point.
         * If the input attribute is specified as normalized, we can convert. */
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          if (normalized) {
            switch (component_length) {
              case 1:
                out_vert_format = MTLVertexFormatShortNormalized;
                break;
              case 2:
                out_vert_format = MTLVertexFormatShort2Normalized;
                break;
              case 3:
                out_vert_format = MTLVertexFormatShort3Normalized;
                break;
              case 4:
                out_vert_format = MTLVertexFormatShort4Normalized;
                break;
              default:
                BLI_assert_msg(false, "invalid vertex format");
                out_vert_format = MTLVertexFormatInvalid;
            }
          }
          else {
            /* Cannot convert. */
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;
      }
      break;

    case GPU_COMP_U16:
      switch (fetch_mode) {
        case GPU_FETCH_INT:
          if (shader_attrib_format == MTLVertexFormatUShort ||
              shader_attrib_format == MTLVertexFormatUShort2 ||
              shader_attrib_format == MTLVertexFormatUShort3 ||
              shader_attrib_format == MTLVertexFormatUShort4)
          {
            /* No conversion Needed (as type matches) - Just a vector resize if needed. */
            bool can_convert = mtl_vertex_format_resize(
                shader_attrib_format, component_length, &out_vert_format);

            /* Ensure format resize successful. */
            BLI_assert(can_convert);
            UNUSED_VARS_NDEBUG(can_convert);
          }
          else {
            BLI_assert_msg(false,
                           "Source vertex data format is either UShort, UShort2, UShort3, UShort4 "
                           "but format in shader interface is NOT compatible.\n");
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;

        /* Source vertex data is integral type, but shader interface type is floating point.
         * If the input attribute is specified as normalized, we can convert. */
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          if (normalized) {
            switch (component_length) {
              case 1:
                out_vert_format = MTLVertexFormatUShortNormalized;
                break;
              case 2:
                out_vert_format = MTLVertexFormatUShort2Normalized;
                break;
              case 3:
                out_vert_format = MTLVertexFormatUShort3Normalized;
                break;
              case 4:
                out_vert_format = MTLVertexFormatUShort4Normalized;
                break;
              default:
                BLI_assert_msg(false, "invalid vertex format");
                out_vert_format = MTLVertexFormatInvalid;
            }
          }
          else {
            /* Cannot convert. */
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;
      }
      break;

    case GPU_COMP_I32:
      switch (fetch_mode) {
        case GPU_FETCH_INT:
          if (shader_attrib_format == MTLVertexFormatInt ||
              shader_attrib_format == MTLVertexFormatInt2 ||
              shader_attrib_format == MTLVertexFormatInt3 ||
              shader_attrib_format == MTLVertexFormatInt4)
          {
            /* No conversion Needed (as type matches) - Just a vector resize if needed. */
            bool can_convert = mtl_vertex_format_resize(
                shader_attrib_format, component_length, &out_vert_format);

            /* Verify conversion successful. */
            BLI_assert(can_convert);
            UNUSED_VARS_NDEBUG(can_convert);
          }
          else {
            BLI_assert_msg(false,
                           "Source vertex data format is either Int, Int2, Int3, Int4 but format "
                           "in shader interface is NOT compatible.\n");
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          /* Unfortunately we cannot implicitly convert between Int and Float in METAL. */
          out_vert_format = MTLVertexFormatInvalid;
          break;
      }
      break;

    case GPU_COMP_U32:
      switch (fetch_mode) {
        case GPU_FETCH_INT:
          if (shader_attrib_format == MTLVertexFormatUInt ||
              shader_attrib_format == MTLVertexFormatUInt2 ||
              shader_attrib_format == MTLVertexFormatUInt3 ||
              shader_attrib_format == MTLVertexFormatUInt4)
          {
            /* No conversion Needed (as type matches) - Just a vector resize if needed. */
            bool can_convert = mtl_vertex_format_resize(
                shader_attrib_format, component_length, &out_vert_format);

            /* Verify conversion successful. */
            BLI_assert(can_convert);
            UNUSED_VARS_NDEBUG(can_convert);
          }
          else {
            BLI_assert_msg(false,
                           "Source vertex data format is either UInt, UInt2, UInt3, UInt4 but "
                           "format in shader interface is NOT compatible.\n");
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          /* Unfortunately we cannot convert between UInt and Float in METAL */
          out_vert_format = MTLVertexFormatInvalid;
          break;
      }
      break;

    case GPU_COMP_F32:
      switch (fetch_mode) {

        /* Source data is float. This will be compatible
         * if type specified in shader is also float. */
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT:
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          if (shader_attrib_format == MTLVertexFormatFloat ||
              shader_attrib_format == MTLVertexFormatFloat2 ||
              shader_attrib_format == MTLVertexFormatFloat3 ||
              shader_attrib_format == MTLVertexFormatFloat4)
          {
            /* No conversion Needed (as type matches) - Just a vector resize, if needed. */
            bool can_convert = mtl_vertex_format_resize(
                shader_attrib_format, component_length, &out_vert_format);

            /* Verify conversion successful. */
            BLI_assert(can_convert);
            UNUSED_VARS_NDEBUG(can_convert);
          }
          else {
            BLI_assert_msg(false,
                           "Source vertex data format is either Float, Float2, Float3, Float4 but "
                           "format in shader interface is NOT compatible.\n");
            out_vert_format = MTLVertexFormatInvalid;
          }
          break;

        case GPU_FETCH_INT:
          /* Unfortunately we cannot convert between Float and Int implicitly in METAL. */
          out_vert_format = MTLVertexFormatInvalid;
          break;
      }
      break;

    case GPU_COMP_I10:
      out_vert_format = MTLVertexFormatInt1010102Normalized;
      break;
    case GPU_COMP_MAX:
      BLI_assert_unreachable();
      break;
  }
  *r_convertedFormat = out_vert_format;
  return (out_vert_format != MTLVertexFormatInvalid);
}

inline uint comp_count_from_vert_format(MTLVertexFormat vert_format)
{
  switch (vert_format) {
    case MTLVertexFormatFloat:
    case MTLVertexFormatInt:
    case MTLVertexFormatUInt:
    case MTLVertexFormatShort:
    case MTLVertexFormatUChar:
    case MTLVertexFormatUCharNormalized:
      return 1;
    case MTLVertexFormatUChar2:
    case MTLVertexFormatUInt2:
    case MTLVertexFormatFloat2:
    case MTLVertexFormatInt2:
    case MTLVertexFormatUChar2Normalized:
      return 2;
    case MTLVertexFormatUChar3:
    case MTLVertexFormatUInt3:
    case MTLVertexFormatFloat3:
    case MTLVertexFormatInt3:
    case MTLVertexFormatShort3Normalized:
    case MTLVertexFormatUChar3Normalized:
      return 3;
    case MTLVertexFormatUChar4:
    case MTLVertexFormatFloat4:
    case MTLVertexFormatUInt4:
    case MTLVertexFormatInt4:
    case MTLVertexFormatUChar4Normalized:
    case MTLVertexFormatInt1010102Normalized:

    default:
      BLI_assert_msg(false, "Unrecognized attribute type. Add types to switch as needed.");
      return 0;
  }
}

inline GPUVertFetchMode fetchmode_from_vert_format(MTLVertexFormat vert_format)
{
  switch (vert_format) {
    case MTLVertexFormatFloat:
    case MTLVertexFormatFloat2:
    case MTLVertexFormatFloat3:
    case MTLVertexFormatFloat4:
      return GPU_FETCH_FLOAT;

    case MTLVertexFormatUChar:
    case MTLVertexFormatUChar2:
    case MTLVertexFormatUChar3:
    case MTLVertexFormatUChar4:
    case MTLVertexFormatChar:
    case MTLVertexFormatChar2:
    case MTLVertexFormatChar3:
    case MTLVertexFormatChar4:
    case MTLVertexFormatUShort:
    case MTLVertexFormatUShort2:
    case MTLVertexFormatUShort3:
    case MTLVertexFormatUShort4:
    case MTLVertexFormatShort:
    case MTLVertexFormatShort2:
    case MTLVertexFormatShort3:
    case MTLVertexFormatShort4:
    case MTLVertexFormatUInt:
    case MTLVertexFormatUInt2:
    case MTLVertexFormatUInt3:
    case MTLVertexFormatUInt4:
    case MTLVertexFormatInt:
    case MTLVertexFormatInt2:
    case MTLVertexFormatInt3:
    case MTLVertexFormatInt4:
      return GPU_FETCH_INT;

    case MTLVertexFormatUCharNormalized:
    case MTLVertexFormatUChar2Normalized:
    case MTLVertexFormatUChar3Normalized:
    case MTLVertexFormatUChar4Normalized:
    case MTLVertexFormatCharNormalized:
    case MTLVertexFormatChar2Normalized:
    case MTLVertexFormatChar3Normalized:
    case MTLVertexFormatChar4Normalized:
    case MTLVertexFormatUShortNormalized:
    case MTLVertexFormatUShort2Normalized:
    case MTLVertexFormatUShort3Normalized:
    case MTLVertexFormatUShort4Normalized:
    case MTLVertexFormatShortNormalized:
    case MTLVertexFormatShort2Normalized:
    case MTLVertexFormatShort3Normalized:
    case MTLVertexFormatShort4Normalized:
    case MTLVertexFormatInt1010102Normalized:
      return GPU_FETCH_INT_TO_FLOAT_UNIT;

    default:
      BLI_assert_msg(false, "Unrecognized attribute type. Add types to switch as needed.");
      return GPU_FETCH_FLOAT;
  }
}

inline GPUVertCompType comp_type_from_vert_format(MTLVertexFormat vert_format)
{
  switch (vert_format) {
    case MTLVertexFormatUChar:
    case MTLVertexFormatUChar2:
    case MTLVertexFormatUChar3:
    case MTLVertexFormatUChar4:
    case MTLVertexFormatUCharNormalized:
    case MTLVertexFormatUChar2Normalized:
    case MTLVertexFormatUChar3Normalized:
    case MTLVertexFormatUChar4Normalized:
      return GPU_COMP_U8;

    case MTLVertexFormatChar:
    case MTLVertexFormatChar2:
    case MTLVertexFormatChar3:
    case MTLVertexFormatChar4:
    case MTLVertexFormatCharNormalized:
    case MTLVertexFormatChar2Normalized:
    case MTLVertexFormatChar3Normalized:
    case MTLVertexFormatChar4Normalized:
      return GPU_COMP_I8;

    case MTLVertexFormatShort:
    case MTLVertexFormatShort2:
    case MTLVertexFormatShort3:
    case MTLVertexFormatShort4:
    case MTLVertexFormatShortNormalized:
    case MTLVertexFormatShort2Normalized:
    case MTLVertexFormatShort3Normalized:
    case MTLVertexFormatShort4Normalized:
      return GPU_COMP_I16;

    case MTLVertexFormatUShort:
    case MTLVertexFormatUShort2:
    case MTLVertexFormatUShort3:
    case MTLVertexFormatUShort4:
    case MTLVertexFormatUShortNormalized:
    case MTLVertexFormatUShort2Normalized:
    case MTLVertexFormatUShort3Normalized:
    case MTLVertexFormatUShort4Normalized:
      return GPU_COMP_U16;

    case MTLVertexFormatInt:
    case MTLVertexFormatInt2:
    case MTLVertexFormatInt3:
    case MTLVertexFormatInt4:
      return GPU_COMP_I32;

    case MTLVertexFormatUInt:
    case MTLVertexFormatUInt2:
    case MTLVertexFormatUInt3:
    case MTLVertexFormatUInt4:
      return GPU_COMP_U32;

    case MTLVertexFormatFloat:
    case MTLVertexFormatFloat2:
    case MTLVertexFormatFloat3:
    case MTLVertexFormatFloat4:
      return GPU_COMP_F32;

    case MTLVertexFormatInt1010102Normalized:
      return GPU_COMP_I10;

    default:
      BLI_assert_msg(false, "Unrecognized attribute type. Add types to switch as needed.");
      return GPU_COMP_F32;
  }
}

}  // namespace blender::gpu
