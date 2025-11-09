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
#include "mtl_shader_shared.hh"
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

/* Maximum threshold for specialized shader variant count.
 * This is a catch-all to prevent excessive PSO permutations from being created and also catch
 * parameters which should ideally not be used for specialization. */
#define MTL_SHADER_MAX_SPECIALIZED_PSOS 5

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
  /* buffer bind slot used for null attributes (-1 if not needed). */
  int null_attribute_buffer_index;
  /* Topology class. */
  MTLPrimitiveTopologyClass prim_type;

  /**
   * Reflection Data.
   * This information can also be used to eliminate redundant/unused bindings.
   * Does only contains SSBO, UBO, Argument and Push Constant buffers. VBO bindings are masked out.
   */
  uint32_t used_buf_vert_mask = 0;
  uint32_t used_buf_frag_mask = 0;
  /* Same thing for images. */
  uint16_t used_ima_vert_mask = 0;
  uint16_t used_ima_frag_mask = 0;
  /* Same thing for samplers. */
  uint64_t used_tex_vert_mask = 0;
  uint64_t used_tex_frag_mask = 0;

  void parse_reflection_data(::MTLRenderPipelineReflection *reflection_data);
};

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

struct MTLComputePipelineStateInstance {
  /** Derived information. */
  /* Unique index for PSO variant. */
  uint32_t shader_pso_index;

  /* Function instances with specialization.
   * Required for argument encoder construction. */
  id<MTLFunction> compute = nil;
  /* PSO handle. */
  id<MTLComputePipelineState> pso = nil;
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

 private:
  /* Context Handle. */
  MTLContext *context_ = nullptr;

  /* Can be nullptr if no uniform is present inside the shader. */
  MTLPushConstantBuf *push_constant_buf_ = nullptr;

  /** Compiled shader resources. */
  id<MTLLibrary> shader_library_vert_ = nil;
  id<MTLLibrary> shader_library_frag_ = nil;
  id<MTLLibrary> shader_library_comp_ = nil;
  bool valid_ = false;

  /** Render pipeline state and PSO caching. */
  /* Metal API Descriptor used for creation of unique PSOs based on rendering state. */
  ::MTLRenderPipelineDescriptor *pso_descriptor_ = nil;
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

  /* Set to true when batch compiling */
  bool async_compilation_ = false;

 public:
  MTLShader(MTLContext *ctx, const char *name);
  ~MTLShader();

  void init(const shader::ShaderCreateInfo & /*info*/, bool is_batch_compilation) override;

  /* Patch create infos for any additional resources that could be needed. */
  const shader::ShaderCreateInfo &patch_create_info(
      const shader::ShaderCreateInfo &original_info) override;

  /* Assign GLSL source. */
  void vertex_shader_from_glsl(const shader::ShaderCreateInfo &info,
                               MutableSpan<StringRefNull> sources) override;
  void geometry_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                 MutableSpan<StringRefNull> sources) override;
  void fragment_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                 MutableSpan<StringRefNull> sources) override;
  void compute_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                MutableSpan<StringRefNull> sources) override;

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
    return (shader_library_comp_ != nil);
  }
  bool has_parent_shader()
  {
    return (parent_shader_ != nil);
  }
  MTLRenderPipelineStateDescriptor &get_current_pipeline_state()
  {
    return current_pipeline_state_;
  }

  MTLShaderInterface &get_interface()
  {
    return *static_cast<MTLShaderInterface *>(this->interface);
  }

  /* Might return nullptr if no push constants are present in the interface. */
  MTLPushConstantBuf *get_push_constant_buf()
  {
    return push_constant_buf_;
  }

  /* Shader source generators from create-info.
   * These aren't all used by Metal, as certain parts of source code generation
   * for shader entry-points and resource mapping occur during `finalize`. */
  std::string resources_declare(const shader::ShaderCreateInfo & /*info*/) const override
  {
    return "";
  }
  std::string vertex_interface_declare(const shader::ShaderCreateInfo & /*info*/) const override
  {
    return "";
  }
  std::string fragment_interface_declare(const shader::ShaderCreateInfo & /*info*/) const override
  {
    return "";
  }
  std::string geometry_interface_declare(const shader::ShaderCreateInfo & /*info*/) const override
  {
    return "";
  }
  std::string geometry_layout_declare(const shader::ShaderCreateInfo & /*info*/) const override
  {
    return "";
  }
  std::string compute_layout_declare(const shader::ShaderCreateInfo & /*info*/) const override
  {
    return "";
  }

  void bind(const shader::SpecializationConstants *constants_state) override;
  void unbind() override;

  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;

  MTLRenderPipelineStateInstance *bake_current_pipeline_state(MTLContext *ctx,
                                                              MTLPrimitiveTopologyClass prim_type);
  /* Bakes and caches a PSO for graphic. */
  MTLRenderPipelineStateInstance *bake_graphic_pipeline_state(
      MTLContext *ctx,
      MTLPrimitiveTopologyClass prim_type,
      const MTLRenderPipelineStateDescriptor &pipeline_descriptor);

  /* Bakes and caches a PSO for compute. */
  MTLComputePipelineStateInstance *bake_compute_pipeline_state(
      MTLContext *ctx, MTLComputePipelineStateDescriptor &compute_pipeline_descriptor);

  const MTLComputePipelineStateCommon &get_compute_common_state()
  {
    return compute_pso_common_state_;
  }

 private:
  /** Create, compile and attach the shader stage to the shader program. */
  id<MTLLibrary> create_shader_library(const shader::ShaderCreateInfo &info,
                                       ShaderStage stage,
                                       MutableSpan<StringRefNull> sources);

  std::string entry_point_name_get(const ShaderStage stage);

  MEM_CXX_CLASS_ALLOC_FUNCS("MTLShader");
};

class MTLShaderCompiler : public ShaderCompiler {
 public:
  MTLShaderCompiler();

  Shader *compile_shader(const shader::ShaderCreateInfo &info) override;
  void specialize_shader(ShaderSpecialization &specialization) override;
};

/* Vertex format conversion.
 * Determines whether it is possible to resize a vertex attribute type
 * during input assembly. A conversion is implied by the difference
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
  } \
  break;

#define FORMAT_PER_COMP_INT(_type) \
  switch (fetch_mode) { \
    case GPU_FETCH_INT: \
      FORMAT_PER_COMP(_type, ) \
    case GPU_FETCH_INT_TO_FLOAT_UNIT: \
    case GPU_FETCH_FLOAT: \
      BLI_assert_msg(0, "Invalid fetch mode for integer attribute"); \
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
          BLI_assert_msg(0, "Invalid fetch mode for float attribute");
          break;
      }
    case GPU_COMP_I10:
      switch (fetch_mode) {
        case GPU_FETCH_INT_TO_FLOAT_UNIT:
          return MTLVertexFormatInt1010102Normalized;
        case GPU_FETCH_FLOAT:
        case GPU_FETCH_INT:
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
#if defined(MAC_OS_VERSION_14_0)
    case MTLVertexFormatFloatRG11B10:
      return 3;
    case MTLVertexFormatFloatRGB9E5:
      return 3;
#endif
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

}  // namespace blender::gpu
