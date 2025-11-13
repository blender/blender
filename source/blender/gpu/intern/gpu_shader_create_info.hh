/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Descriptor type used to define shader structure, resources and interfaces.
 *
 * Some rule of thumb:
 * - Do not include anything else than this file in each info file.
 */

#pragma once

#if !defined(GPU_SHADER)
#  include "BLI_enum_flags.hh"
#  include "BLI_hash.hh"
#  include "BLI_string_ref.hh"
#  include "BLI_utildefines_variadic.h"
#  include "BLI_vector.hh"
#  include "GPU_common_types.hh"
#  include "GPU_material.hh"
#  include "GPU_texture.hh"

#  include <iostream>
#endif

#if defined(GPU_SHADER)
#  include "gpu_shader_srd_cpp.hh"
#else
#  include "gpu_shader_srd_info.hh"
#endif

/* Force enable `printf` support in release build. */
#define GPU_FORCE_ENABLE_SHADER_PRINTF 0

#if !defined(NDEBUG) || GPU_FORCE_ENABLE_SHADER_PRINTF
#  define GPU_SHADER_PRINTF_ENABLE 1
#else
#  define GPU_SHADER_PRINTF_ENABLE 0
#endif
#define GPU_SHADER_PRINTF_SLOT 13
#define GPU_SHADER_PRINTF_MAX_CAPACITY (1024 * 4)

/* Used for primitive expansion. */
#define GPU_SSBO_INDEX_BUF_SLOT 7
/* Used for polylines. */
#define GPU_SSBO_POLYLINE_POS_BUF_SLOT 0
#define GPU_SSBO_POLYLINE_COL_BUF_SLOT 1

#if defined(GLSL_CPP_STUBS)
#  define GPU_SHADER_NAMED_INTERFACE_INFO(_interface, _inst_name) \
    namespace interface::_interface { \
    struct {
#  define GPU_SHADER_NAMED_INTERFACE_END(_inst_name) \
    } \
    _inst_name; \
    }

#  define GPU_SHADER_INTERFACE_INFO(_interface) namespace interface::_interface {
#  define GPU_SHADER_INTERFACE_END() }

#  define GPU_SHADER_CREATE_INFO(_info) \
    namespace _info { \
    namespace gl_VertexShader { \
    } \
    namespace gl_FragmentShader { \
    } \
    namespace gl_ComputeShader { \
    }
#  define GPU_SHADER_CREATE_END() }

#  define SHADER_LIBRARY_CREATE_INFO(_info) using namespace _info;
#  define VERTEX_SHADER_CREATE_INFO(_info) \
    using namespace ::gl_VertexShader; \
    using namespace _info::gl_VertexShader; \
    using namespace _info;
#  define FRAGMENT_SHADER_CREATE_INFO(_info) \
    using namespace ::gl_FragmentShader; \
    using namespace _info::gl_FragmentShader; \
    using namespace _info;
#  define COMPUTE_SHADER_CREATE_INFO(_info) \
    using namespace ::gl_ComputeShader; \
    using namespace _info::gl_ComputeShader; \
    using namespace _info;

#elif !defined(GPU_SHADER_CREATE_INFO)
/* Helps intellisense / auto-completion inside info files. */
#  define GPU_SHADER_NAMED_INTERFACE_INFO(_interface, _inst_name) \
    static inline void autocomplete_helper_interface_##_interface() \
    { \
      StageInterfaceInfo _interface(#_interface, _inst_name); \
      _interface
#  define GPU_SHADER_INTERFACE_INFO(_interface) \
    static inline void autocomplete_helper_interface_##_interface() \
    { \
      StageInterfaceInfo _interface(#_interface); \
      _interface
#  define GPU_SHADER_CREATE_INFO(_info) \
    static inline void autocomplete_helper_info_##_info() \
    { \
      ShaderCreateInfo _info(#_info); \
      _info

#  define GPU_SHADER_NAMED_INTERFACE_END(_inst_name) \
    ; \
    }
#  define GPU_SHADER_INTERFACE_END() \
    ; \
    }
#  define GPU_SHADER_CREATE_END() \
    ; \
    }

#endif

#ifndef GLSL_CPP_STUBS
#  define SMOOTH(type, name) .smooth(Type::type##_t, #name)
#  define FLAT(type, name) .flat(Type::type##_t, #name)
#  define NO_PERSPECTIVE(type, name) .no_perspective(Type::type##_t, #name)

/* LOCAL_GROUP_SIZE(int size_x, int size_y = 1, int size_z = 1) */
#  define LOCAL_GROUP_SIZE(...) .local_group_size(__VA_ARGS__)

#  define VERTEX_IN(slot, type, name) .vertex_in(slot, Type::type##_t, #name)
#  define VERTEX_IN_SRD(srd) .shared_resource_descriptor(srd::populate)
#  define VERTEX_OUT(stage_interface) .vertex_out(stage_interface)
#  define VERTEX_OUT_SRD(srd) .vertex_out(srd)
/* TO REMOVE. */
#  define GEOMETRY_LAYOUT(...) .geometry_layout(__VA_ARGS__)
#  define GEOMETRY_OUT(stage_interface) .geometry_out(stage_interface)

#  define SUBPASS_IN(slot, type, img_type, name, rog) \
    .subpass_in(slot, Type::type##_t, ImageType::img_type, #name, rog)

#  define FRAGMENT_OUT(slot, type, name) .fragment_out(slot, Type::type##_t, #name)
#  define FRAGMENT_OUT_SRD(srd) .shared_resource_descriptor(srd::populate)
#  define FRAGMENT_OUT_DUAL(slot, type, name, blend) \
    .fragment_out(slot, Type::type##_t, #name, DualBlend::blend)
#  define FRAGMENT_OUT_ROG(slot, type, name, rog) \
    .fragment_out(slot, Type::type##_t, #name, DualBlend::NONE, rog)

#  define RESOURCE_SRD(srd) .shared_resource_descriptor(srd::populate)

#  define EARLY_FRAGMENT_TEST(enable) .early_fragment_test(enable)
#  define DEPTH_WRITE(value) .depth_write(value)

#  define SPECIALIZATION_CONSTANT(type, name, default_value) \
    .specialization_constant(Type::type##_t, #name, default_value)

#  define COMPILATION_CONSTANT(type, name, value) \
    .compilation_constant(Type::type##_t, #name, value)

#  define PUSH_CONSTANT(type, name) .push_constant(Type::type##_t, #name)
#  define PUSH_CONSTANT_ARRAY(type, name, array_size) \
    .push_constant(Type::type##_t, #name, array_size)

#  define UNIFORM_BUF(slot, type_name, name) .uniform_buf(slot, #type_name, #name)
#  define UNIFORM_BUF_FREQ(slot, type_name, name, freq) \
    .uniform_buf(slot, #type_name, #name, Frequency::freq)

#  define STORAGE_BUF(slot, qualifiers, type_name, name) \
    .storage_buf(slot, Qualifier::qualifiers, STRINGIFY(type_name), #name)
#  define STORAGE_BUF_FREQ(slot, qualifiers, type_name, name, freq) \
    .storage_buf(slot, Qualifier::qualifiers, STRINGIFY(type_name), #name, Frequency::freq)

#  define SAMPLER(slot, type, name) .sampler(slot, ImageType::type, #name)
#  define SAMPLER_FREQ(slot, type, name, freq) \
    .sampler(slot, ImageType::type, #name, Frequency::freq)

#  define IMAGE(slot, format, qualifiers, type, name) \
    .image(slot, \
           blender::gpu::TextureFormat::format, \
           Qualifier::qualifiers, \
           ImageReadWriteType::type, \
           #name)
#  define IMAGE_FREQ(slot, format, qualifiers, type, name, freq) \
    .image(slot, \
           blender::gpu::TextureFormat::format, \
           Qualifier::qualifiers, \
           ImageReadWriteType::type, \
           #name, \
           Frequency::freq)

#  define GROUP_SHARED(type, name) .shared_variable(Type::type##_t, #name)

#  define BUILTINS(builtin) .builtins(builtin)

#  define VERTEX_SOURCE(filename) .vertex_source(filename)
#  define FRAGMENT_SOURCE(filename) .fragment_source(filename)
#  define COMPUTE_SOURCE(filename) .compute_source(filename)

#  define VERTEX_FUNCTION(function) .vertex_function(function)
#  define FRAGMENT_FUNCTION(function) .fragment_function(function)
#  define COMPUTE_FUNCTION(function) .compute_function(function)

#  define DEFINE(name) .define(name)
#  define DEFINE_VALUE(name, value) .define(name, value)

#  define DO_STATIC_COMPILATION() .do_static_compilation(true)
#  define AUTO_RESOURCE_LOCATION() .auto_resource_location(true)

/* TO REMOVE. */
#  define METAL_BACKEND_ONLY() .metal_backend_only(true)

#  define ADDITIONAL_INFO(info_name) .additional_info(#info_name)
#  define TYPEDEF_SOURCE(filename) .typedef_source(filename)

#  define MTL_MAX_TOTAL_THREADS_PER_THREADGROUP(value) \
    .mtl_max_total_threads_per_threadgroup(value)

#else

#  define _read const
#  define _write
#  define _read_write

#  define SMOOTH(type, name) type name = {};
#  define FLAT(type, name) type name = {};
#  define NO_PERSPECTIVE(type, name) type name = {};

/* LOCAL_GROUP_SIZE(int size_x, int size_y = -1, int size_z = -1) */
#  define LOCAL_GROUP_SIZE(...)

#  define VERTEX_IN(slot, type, name) \
    namespace gl_VertexShader { \
    const type name = {}; \
    }
#  define VERTEX_IN_SRD(srd) \
    namespace gl_VertexShader { \
    using namespace srd; \
    }
#  define VERTEX_OUT(stage_interface) using namespace interface::stage_interface;
#  define VERTEX_OUT_SRD(srd) using namespace interface::srd;
/* TO REMOVE. */
#  define GEOMETRY_LAYOUT(...)
#  define GEOMETRY_OUT(stage_interface) using namespace interface::stage_interface;

#  define SUBPASS_IN(slot, type, img_type, name, rog) const type name = {};

#  define FRAGMENT_OUT(slot, type, name) \
    namespace gl_FragmentShader { \
    type name; \
    }
#  define FRAGMENT_OUT_DUAL(slot, type, name, blend) \
    namespace gl_FragmentShader { \
    type name; \
    }
#  define FRAGMENT_OUT_ROG(slot, type, name, rog) \
    namespace gl_FragmentShader { \
    type name; \
    }
#  define FRAGMENT_OUT_SRD(srd) \
    namespace gl_FragmentShader { \
    using namespace srd; \
    }

#  define RESOURCE_SRD(srd) using namespace srd;

#  define EARLY_FRAGMENT_TEST(enable)
#  define DEPTH_WRITE(value)

#  define SPECIALIZATION_CONSTANT(type, name, default_value) \
    constexpr type name = type(default_value);

#  define COMPILATION_CONSTANT(type, name, value) constexpr type name = type(value);

#  define PUSH_CONSTANT(type, name) extern const type name;
#  define PUSH_CONSTANT_ARRAY(type, name, array_size) extern const type name[array_size];

#  define UNIFORM_BUF(slot, type_name, name) extern const type_name name;
#  define UNIFORM_BUF_FREQ(slot, type_name, name, freq) extern const type_name name;

#  define STORAGE_BUF(slot, qualifiers, type_name, name) extern _##qualifiers type_name name;
#  define STORAGE_BUF_FREQ(slot, qualifiers, type_name, name, freq) \
    extern _##qualifiers type_name name;

#  define SAMPLER(slot, type, name) type name;
#  define SAMPLER_FREQ(slot, type, name, freq) type name;

#  define IMAGE(slot, format, qualifiers, type, name) _##qualifiers type name;
#  define IMAGE_FREQ(slot, format, qualifiers, type, name, freq) _##qualifiers type name;

#  define GROUP_SHARED(type, name) type name;

#  define BUILTINS(builtin)

#  define VERTEX_SOURCE(filename)
#  define FRAGMENT_SOURCE(filename)
#  define COMPUTE_SOURCE(filename)

#  define VERTEX_FUNCTION(filename)
#  define FRAGMENT_FUNCTION(filename)
#  define COMPUTE_FUNCTION(filename)

#  define DEFINE(name)
#  define DEFINE_VALUE(name, value)

#  define DO_STATIC_COMPILATION()
#  define AUTO_RESOURCE_LOCATION()

/* TO REMOVE. */
#  define METAL_BACKEND_ONLY()

#  define ADDITIONAL_INFO(info_name) \
    using namespace info_name; \
    using namespace info_name::gl_FragmentShader; \
    using namespace info_name::gl_VertexShader;

#  define TYPEDEF_SOURCE(filename)

#  define MTL_MAX_TOTAL_THREADS_PER_THREADGROUP(value)
#endif

#define _INFO_EXPAND2(a, b) ADDITIONAL_INFO(a) ADDITIONAL_INFO(b)
#define _INFO_EXPAND3(a, b, c) _INFO_EXPAND2(a, b) ADDITIONAL_INFO(c)
#define _INFO_EXPAND4(a, b, c, d) _INFO_EXPAND3(a, b, c) ADDITIONAL_INFO(d)
#define _INFO_EXPAND5(a, b, c, d, e) _INFO_EXPAND4(a, b, c, d) ADDITIONAL_INFO(e)
#define _INFO_EXPAND6(a, b, c, d, e, f) _INFO_EXPAND5(a, b, c, d, e) ADDITIONAL_INFO(f)

#define ADDITIONAL_INFO_EXPAND(...) VA_NARGS_CALL_OVERLOAD(_INFO_EXPAND, __VA_ARGS__)

#define CREATE_INFO_VARIANT(name, ...) \
  GPU_SHADER_CREATE_INFO(name) \
  DO_STATIC_COMPILATION() \
  ADDITIONAL_INFO_EXPAND(__VA_ARGS__) \
  GPU_SHADER_CREATE_END()

#if !defined(GLSL_CPP_STUBS)

namespace blender::gpu {
struct GPUSource;
}

namespace blender::gpu::shader {

/* All of these functions is a bit out of place */
static inline Type to_type(const GPUType type)
{
  switch (type) {
    case GPU_FLOAT:
      return Type::float_t;
    case GPU_VEC2:
      return Type::float2_t;
    case GPU_VEC3:
      return Type::float3_t;
    case GPU_VEC4:
      return Type::float4_t;
    case GPU_MAT3:
      return Type::float3x3_t;
    case GPU_MAT4:
      return Type::float4x4_t;
    default:
      BLI_assert_msg(0, "Error: Cannot convert GPUType to shader::Type.");
      return Type::float_t;
  }
}

static inline std::ostream &operator<<(std::ostream &stream, const Type type)
{
  switch (type) {
    case Type::float_t:
      return stream << "float";
    case Type::float2_t:
      return stream << "float2";
    case Type::float3_t:
      return stream << "float3";
    case Type::float4_t:
      return stream << "float4";
    case Type::float3x3_t:
      return stream << "float3x3";
    case Type::float4x4_t:
      return stream << "float4x4";
    case Type::float3_10_10_10_2_t:
      return stream << "vec3_1010102_Inorm";
    case Type::uchar_t:
      return stream << "uchar";
    case Type::uchar2_t:
      return stream << "uchar2";
    case Type::uchar3_t:
      return stream << "uchar3";
    case Type::uchar4_t:
      return stream << "uchar4";
    case Type::char_t:
      return stream << "char";
    case Type::char2_t:
      return stream << "char2";
    case Type::char3_t:
      return stream << "char3";
    case Type::char4_t:
      return stream << "char4";
    case Type::int_t:
      return stream << "int";
    case Type::int2_t:
      return stream << "int2";
    case Type::int3_t:
      return stream << "int3";
    case Type::int4_t:
      return stream << "int4";
    case Type::uint_t:
      return stream << "uint";
    case Type::uint2_t:
      return stream << "uint2";
    case Type::uint3_t:
      return stream << "uint3";
    case Type::uint4_t:
      return stream << "uint4";
    case Type::ushort_t:
      return stream << "ushort";
    case Type::ushort2_t:
      return stream << "ushort2";
    case Type::ushort3_t:
      return stream << "ushort3";
    case Type::ushort4_t:
      return stream << "ushort4";
    case Type::short_t:
      return stream << "short";
    case Type::short2_t:
      return stream << "short2";
    case Type::short3_t:
      return stream << "short3";
    case Type::short4_t:
      return stream << "short4";
    case Type::bool_t:
      return stream << "bool";
    default:
      BLI_assert(0);
      return stream;
  }
}

static inline std::ostream &operator<<(std::ostream &stream, const GPUType type)
{
  switch (type) {
    case GPU_CLOSURE:
      return stream << "Closure";
    default:
      return stream << to_type(type);
  }
}

enum class BuiltinBits {
  NONE = 0,
  /**
   * Allow getting barycentric coordinates inside the fragment shader.
   * \note Emulated on OpenGL.
   */
  BARYCENTRIC_COORD = (1 << 0),
  STENCIL_REF = (1 << 1),
  FRAG_COORD = (1 << 2),
  CLIP_DISTANCES = (1 << 3),
  FRONT_FACING = (1 << 4),
  GLOBAL_INVOCATION_ID = (1 << 5),
  INSTANCE_ID = (1 << 6),
  /**
   * Allow setting the target layer when the output is a layered frame-buffer.
   * \note Emulated through geometry shader on older hardware.
   */
  LAYER = (1 << 7),
  LOCAL_INVOCATION_ID = (1 << 8),
  LOCAL_INVOCATION_INDEX = (1 << 9),
  NUM_WORK_GROUP = (1 << 10),
  POINT_COORD = (1 << 11),
  POINT_SIZE = (1 << 12),
  PRIMITIVE_ID = (1 << 13),
  VERTEX_ID = (1 << 14),
  WORK_GROUP_ID = (1 << 15),
  WORK_GROUP_SIZE = (1 << 16),
  /**
   * Allow setting the target viewport when using multi viewport feature.
   * \note Emulated through geometry shader on older hardware.
   */
  VIEWPORT_INDEX = (1 << 17),

  /* Texture atomics requires usage options to alter compilation flag. */
  TEXTURE_ATOMIC = (1 << 18),

  /* Enable shader patching on GL to remap clip range to 0..1.
   * Will do nothing if ClipControl is unsupported. */
  CLIP_CONTROL = (1 << 19),

  /* On metal, tag the shader to use argument buffer to overcome the 16 sampler limit. */
  USE_SAMPLER_ARG_BUFFER = (1 << 20),

  /* Not a builtin but a flag we use to tag shaders that use the debug features. */
  USE_PRINTF = (1 << 28),
  USE_DEBUG_DRAW = (1 << 29),

  /* Shader source needs to be implemented at runtime. */
  RUNTIME_GENERATED = (1 << 30),
};
ENUM_OPERATORS(BuiltinBits);

/**
 * Follow convention described in:
 * https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_conservative_depth.txt
 */
enum class DepthWrite {
  /* UNCHANGED specified as default to indicate gl_FragDepth is not used. */
  UNCHANGED = 0,
  ANY,
  GREATER,
  LESS,
};

/* Samplers & images. */
enum class ImageType {
  undefined = 0,
#  define TYPES_EXPAND(s) \
    Float##s, Uint##s, Int##s, sampler##s = Float##s, usampler##s = Uint##s, isampler##s = Int##s
  /** Color samplers/image. */
  TYPES_EXPAND(1D),
  TYPES_EXPAND(1DArray),
  TYPES_EXPAND(2D),
  TYPES_EXPAND(2DArray),
  TYPES_EXPAND(3D),
  TYPES_EXPAND(Cube),
  TYPES_EXPAND(CubeArray),
  TYPES_EXPAND(Buffer),
#  undef TYPES_EXPAND

#  define TYPES_EXPAND(s) \
    Shadow##s, Depth##s, sampler##s##Shadow = Shadow##s, sampler##s##Depth = Depth##s
  /** Depth samplers (not supported as image). */
  TYPES_EXPAND(2D),
  TYPES_EXPAND(2DArray),
  TYPES_EXPAND(Cube),
  TYPES_EXPAND(CubeArray),
#  undef TYPES_EXPAND

#  define TYPES_EXPAND(s) \
    AtomicUint##s, AtomicInt##s, usampler##s##Atomic = AtomicUint##s, \
                                 isampler##s##Atomic = AtomicInt##s
  /** Atomic texture type wrappers.
   * For OpenGL, these map to the equivalent (U)INT_* types.
   * NOTE: Atomic variants MUST be used if the texture bound to this resource has usage flag:
   * `GPU_TEXTURE_USAGE_ATOMIC`, even if atomic texture operations are not used in the given
   * shader.
   * The shader source MUST also utilize the correct atomic sampler handle e.g.
   * `usampler2DAtomic` in conjunction with these types, for passing texture/image resources into
   * functions. */
  TYPES_EXPAND(2D),
  TYPES_EXPAND(2DArray),
  TYPES_EXPAND(3D),
#  undef TYPES_EXPAND
};

/* Samplers & images. */
enum class ImageReadWriteType {
  undefined = 0,
#  define TYPES_EXPAND(s) \
    Float##s = int(ImageType::Float##s), Uint##s = int(ImageType::Uint##s), \
    Int##s = int(ImageType::Int##s), image##s = Float##s, uimage##s = Uint##s, iimage##s = Int##s
  /** Color image. */
  TYPES_EXPAND(1D),
  TYPES_EXPAND(1DArray),
  TYPES_EXPAND(2D),
  TYPES_EXPAND(2DArray),
  TYPES_EXPAND(3D),
#  undef TYPES_EXPAND

#  define TYPES_EXPAND(s) \
    AtomicUint##s = int(ImageType::AtomicUint##s), AtomicInt##s = int(ImageType::AtomicInt##s), \
    uimage##s##Atomic = AtomicUint##s, iimage##s##Atomic = AtomicInt##s
  /** Atomic texture type wrappers.
   * For OpenGL, these map to the equivalent (U)INT_* types.
   * NOTE: Atomic variants MUST be used if the texture bound to this resource has usage flag:
   * `GPU_TEXTURE_USAGE_ATOMIC`, even if atomic texture operations are not used in the given
   * shader.
   * The shader source MUST also utilize the correct atomic sampler handle e.g.
   * `usampler2DAtomic` in conjunction with these types, for passing texture/image resources into
   * functions. */
  TYPES_EXPAND(2D),
  TYPES_EXPAND(2DArray),
  TYPES_EXPAND(3D),
#  undef TYPES_EXPAND
};

static inline bool is_atomic_type(ImageType type)
{
  switch (type) {
    case ImageType::AtomicUint2D:
    case ImageType::AtomicInt2D:
    case ImageType::AtomicUint2DArray:
    case ImageType::AtomicInt2DArray:
    case ImageType::AtomicUint3D:
    case ImageType::AtomicInt3D:
      return true;
    default:
      break;
  }
  return false;
}

/* Storage qualifiers. */
enum class Qualifier {
  /** Restrict flag is set by default. Unless specified otherwise. */
  no_restrict = (1 << 0),
  read = (1 << 1),
  write = (1 << 2),
  /** Shorthand version of combined flags. */
  read_write = read | write,
  QUALIFIER_MAX = (write << 1) - 1,
};
ENUM_OPERATORS(Qualifier);

/** Maps to different descriptor sets. */
enum class Frequency {
  BATCH = 0,
  PASS,
  /** Special frequency tag that will automatically source storage buffers from GPUBatch. */
  GEOMETRY,
};

/** Dual Source Blending Index. */
enum class DualBlend {
  NONE = 0,
  SRC_0,
  SRC_1,
};

/** Interpolation qualifiers. */
enum class Interpolation {
  SMOOTH = 0,
  FLAT,
  NO_PERSPECTIVE,
};

/** Input layout for geometry shader. */
enum class PrimitiveIn {
  POINTS = 0,
  LINES,
  LINES_ADJACENCY,
  TRIANGLES,
  TRIANGLES_ADJACENCY,
};

/** Output layout for geometry shader. */
enum class PrimitiveOut {
  POINTS = 0,
  LINE_STRIP,
  TRIANGLE_STRIP,
  LINES,
  TRIANGLES,
};

/* Same as StringRefNull but with a few extra member functions. */
struct ResourceString : public StringRefNull {
  constexpr ResourceString() : StringRefNull() {}
  constexpr ResourceString(const char *str, int64_t size) : StringRefNull(str, size) {}
  ResourceString(std::nullptr_t) = delete;
  constexpr ResourceString(const char *str) : StringRefNull(str) {}
  ResourceString(const std::string &str) : StringRefNull(str) {}
  ResourceString(const StringRefNull &str) : StringRefNull(str) {}

  int64_t array_offset() const
  {
    return this->find_first_of("[");
  }

  bool is_array() const
  {
    return array_offset() != -1;
  }

  StringRef str_no_array() const
  {
    int64_t offset = this->array_offset();
    if (offset == -1) {
      return *this;
    }
    return StringRef(this->c_str(), offset);
  }

  StringRef str_only_array() const
  {
    int64_t offset = this->array_offset();
    if (offset == -1) {
      return "";
    }
    return this->substr(this->array_offset());
  }
};

struct StageInterfaceInfo {
  struct InOut {
    Interpolation interp;
    Type type;
    ResourceString name;
  };

  StringRefNull name;
  /**
   * Name of the instance of the block (used to access).
   * Can be empty string (i.e: "") only if not using geometry shader.
   */
  StringRefNull instance_name;
  /** List of all members of the interface. */
  Vector<InOut> inouts;

  StageInterfaceInfo(const char *name_, const char *instance_name_ = "")
      : name(name_), instance_name(instance_name_) {};
  ~StageInterfaceInfo() = default;

  using Self = StageInterfaceInfo;

  Self &smooth(Type type, StringRefNull _name)
  {
    inouts.append({Interpolation::SMOOTH, type, _name});
    return *(Self *)this;
  }

  Self &flat(Type type, StringRefNull _name)
  {
    inouts.append({Interpolation::FLAT, type, _name});
    return *(Self *)this;
  }

  Self &no_perspective(Type type, StringRefNull _name)
  {
    inouts.append({Interpolation::NO_PERSPECTIVE, type, _name});
    return *(Self *)this;
  }
};

/** Sources from generated code. Map source name to content. */
struct GeneratedSource {
  /* Associated filename this source replaces. */
  StringRefNull filename;
  Vector<StringRefNull> dependencies;
  std::string content;
};

using GeneratedSourceList = Vector<shader::GeneratedSource, 0>;

/**
 * \brief Describe inputs & outputs, stage interfaces, resources and sources of a shader.
 *        If all data is correctly provided, this is all that is needed to create and compile
 *        a #blender::gpu::Shader.
 *
 * IMPORTANT: All strings are references only. Make sure all the strings used by a
 *            #ShaderCreateInfo are not freed until it is consumed or deleted.
 */
struct ShaderCreateInfo {
  /** Shader name for debugging. */
  StringRefNull name_;
  /** True if the shader is static and can be pre-compiled at compile time. */
  bool do_static_compilation_ = false;
  /** True if the shader is not part of gpu_shader_create_info_list. */
  bool is_generated_ = true;
  /** If true, all additionally linked create info will be merged into this one. */
  bool finalized_ = false;
  /** If true, all resources will have an automatic location assigned. */
  bool auto_resource_location_ = false;
  /** If true, force depth and stencil tests to always happen before fragment shader invocation. */
  bool early_fragment_test_ = false;
  /** Allow optimization when fragment shader writes to `gl_FragDepth`. */
  DepthWrite depth_write_ = DepthWrite::UNCHANGED;
  /** GPU Backend compatibility flag. Temporary requirement until Metal enablement is fully
   * complete. */
  bool metal_backend_only_ = false;
  /**
   * Maximum length of all the resource names including each null terminator.
   * Only for names used by #gpu::ShaderInterface.
   */
  size_t interface_names_size_ = 0;
  /** Manually set builtins. */
  BuiltinBits builtins_ = BuiltinBits::NONE;
  /** Manually set generated code. */
  std::string vertex_source_generated;
  std::string fragment_source_generated;
  std::string compute_source_generated;
  std::string geometry_source_generated;
  std::string typedef_source_generated;
  /** Manually set generated dependencies file names. */
  Vector<StringRefNull, 0> dependencies_generated;

  GeneratedSourceList generated_sources;

#  define TEST_EQUAL(a, b, _member) \
    if (!((a)._member == (b)._member)) { \
      return false; \
    }

#  define TEST_VECTOR_EQUAL(a, b, _vector) \
    TEST_EQUAL(a, b, _vector.size()); \
    for (auto i : _vector.index_range()) { \
      TEST_EQUAL(a, b, _vector[i]); \
    }

  struct VertIn {
    int index;
    Type type;
    ResourceString name;

    bool operator==(const VertIn &b) const
    {
      TEST_EQUAL(*this, b, index);
      TEST_EQUAL(*this, b, type);
      TEST_EQUAL(*this, b, name);
      return true;
    }
  };
  Vector<VertIn> vertex_inputs_;

  struct GeometryStageLayout {
    PrimitiveIn primitive_in;
    int invocations;
    PrimitiveOut primitive_out;
    /** Set to -1 by default to check if used. */
    int max_vertices = -1;

    bool operator==(const GeometryStageLayout &b) const
    {
      TEST_EQUAL(*this, b, primitive_in);
      TEST_EQUAL(*this, b, invocations);
      TEST_EQUAL(*this, b, primitive_out);
      TEST_EQUAL(*this, b, max_vertices);
      return true;
    }
  };
  GeometryStageLayout geometry_layout_;

  struct ComputeStageLayout {
    int local_size_x = -1;
    int local_size_y = -1;
    int local_size_z = -1;

    bool operator==(const ComputeStageLayout &b) const
    {
      TEST_EQUAL(*this, b, local_size_x);
      TEST_EQUAL(*this, b, local_size_y);
      TEST_EQUAL(*this, b, local_size_z);
      return true;
    }
  };
  ComputeStageLayout compute_layout_;

  struct FragOut {
    int index;
    Type type;
    DualBlend blend;
    StringRefNull name;
    /* NOTE: Currently only supported by Metal. */
    int raster_order_group;

    bool operator==(const FragOut &b) const
    {
      TEST_EQUAL(*this, b, index);
      TEST_EQUAL(*this, b, type);
      TEST_EQUAL(*this, b, blend);
      TEST_EQUAL(*this, b, name);
      TEST_EQUAL(*this, b, raster_order_group);
      return true;
    }
  };
  Vector<FragOut> fragment_outputs_;

  struct SubpassIn {
    int index;
    Type type;
    ImageType img_type;
    StringRefNull name;
    /* NOTE: Currently only supported by Metal. */
    int raster_order_group;

    bool operator==(const SubpassIn &b) const
    {
      TEST_EQUAL(*this, b, index);
      TEST_EQUAL(*this, b, type);
      TEST_EQUAL(*this, b, img_type);
      TEST_EQUAL(*this, b, name);
      TEST_EQUAL(*this, b, raster_order_group);
      return true;
    }
  };
  Vector<SubpassIn> subpass_inputs_;

  Vector<CompilationConstant, 0> compilation_constants_;
  Vector<SpecializationConstant> specialization_constants_;

  struct SharedVariable {
    Type type;
    ResourceString name;
  };

  Vector<SharedVariable, 0> shared_variables_;

  struct Sampler {
    ImageType type;
    GPUSamplerState sampler;
    StringRefNull name;
  };

  struct Image {
    TextureFormat format;
    ImageType type;
    Qualifier qualifiers;
    StringRefNull name;
  };

  struct UniformBuf {
    StringRefNull type_name;
    ResourceString name;
  };

  struct StorageBuf {
    Qualifier qualifiers;
    StringRefNull type_name;
    ResourceString name;
  };

  struct Resource {
    enum BindType {
      UNIFORM_BUFFER = 0,
      STORAGE_BUFFER,
      SAMPLER,
      IMAGE,
    };

    BindType bind_type;
    int slot;
    union {
      Sampler sampler;
      Image image;
      UniformBuf uniformbuf;
      StorageBuf storagebuf;
    };

    Resource(BindType type, int _slot) : bind_type(type), slot(_slot) {};

    bool operator==(const Resource &b) const
    {
      TEST_EQUAL(*this, b, bind_type);
      TEST_EQUAL(*this, b, slot);
      switch (bind_type) {
        case UNIFORM_BUFFER:
          TEST_EQUAL(*this, b, uniformbuf.type_name);
          TEST_EQUAL(*this, b, uniformbuf.name);
          break;
        case STORAGE_BUFFER:
          TEST_EQUAL(*this, b, storagebuf.qualifiers);
          TEST_EQUAL(*this, b, storagebuf.type_name);
          TEST_EQUAL(*this, b, storagebuf.name);
          break;
        case SAMPLER:
          TEST_EQUAL(*this, b, sampler.type);
          TEST_EQUAL(*this, b, sampler.sampler);
          TEST_EQUAL(*this, b, sampler.name);
          break;
        case IMAGE:
          TEST_EQUAL(*this, b, image.format);
          TEST_EQUAL(*this, b, image.type);
          TEST_EQUAL(*this, b, image.qualifiers);
          TEST_EQUAL(*this, b, image.name);
          break;
      }
      return true;
    }
  };
  /**
   * Resources are grouped by frequency of change.
   * Pass resources are meant to be valid for the whole pass.
   * Batch resources can be changed in a more granular manner (per object/material).
   * Geometry resources can be changed in a very granular manner (per draw-call).
   * Misuse will only produce suboptimal performance.
   */
  Vector<Resource> pass_resources_, batch_resources_, geometry_resources_;

  Vector<Resource> &resources_get_(Frequency freq)
  {
    switch (freq) {
      case Frequency::PASS:
        return pass_resources_;
      case Frequency::BATCH:
        return batch_resources_;
      case Frequency::GEOMETRY:
        return geometry_resources_;
    }
    BLI_assert_unreachable();
    return pass_resources_;
  }

  /* Return all resources regardless of their frequency. */
  Vector<Resource> resources_get_all_() const
  {
    Vector<Resource> all_resources;
    all_resources.extend(pass_resources_);
    all_resources.extend(batch_resources_);
    all_resources.extend(geometry_resources_);
    return all_resources;
  }

  Vector<StageInterfaceInfo *> vertex_out_interfaces_;
  Vector<StageInterfaceInfo *> geometry_out_interfaces_;

  struct PushConst {
    Type type;
    ResourceString name;
    int array_size;

    int array_size_safe() const
    {
      return (array_size > 0) ? array_size : 1;
    }

    std::string array_str() const
    {
      return array_size > 0 ? "[" + std::to_string(array_size) + "]" : "";
    }

    bool operator==(const PushConst &b) const
    {
      TEST_EQUAL(*this, b, type);
      TEST_EQUAL(*this, b, name);
      TEST_EQUAL(*this, b, array_size);
      return true;
    }
  };

  Vector<PushConst> push_constants_;

  /* Sources for resources type definitions. */
  Vector<StringRefNull> typedef_sources_;

  StringRefNull vertex_source_, geometry_source_, fragment_source_, compute_source_;
  StringRefNull vertex_entry_fn_ = "main", geometry_entry_fn_ = "main",
                fragment_entry_fn_ = "main", compute_entry_fn_ = "main";

  Vector<std::array<StringRefNull, 2>> defines_;
  /**
   * Name of other infos to recursively merge with this one.
   * No data slot must overlap otherwise we throw an error.
   */
  Vector<StringRefNull> additional_infos_;

  /* API-specific parameters. */
#  ifdef WITH_METAL_BACKEND
  ushort mtl_max_threads_per_threadgroup_ = 0;
#  endif

 public:
  ShaderCreateInfo(const char *name) : name_(name) {};
  ~ShaderCreateInfo() = default;

  using Self = ShaderCreateInfo;

  /* -------------------------------------------------------------------- */
  /** \name Shaders in/outs (fixed function pipeline config)
   * \{ */

  Self &vertex_in(int slot, Type type, StringRefNull name)
  {
    vertex_inputs_.append({slot, type, name});
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  Self &vertex_out(StageInterfaceInfo &interface)
  {
    vertex_out_interfaces_.append(&interface);
    return *(Self *)this;
  }

  Self &geometry_layout(PrimitiveIn prim_in,
                        PrimitiveOut prim_out,
                        int max_vertices,
                        int invocations = -1)
  {
    geometry_layout_.primitive_in = prim_in;
    geometry_layout_.primitive_out = prim_out;
    geometry_layout_.max_vertices = max_vertices;
    geometry_layout_.invocations = invocations;
    return *(Self *)this;
  }

  Self &local_group_size(int local_size_x, int local_size_y = 1, int local_size_z = 1)
  {
    compute_layout_.local_size_x = local_size_x;
    compute_layout_.local_size_y = local_size_y;
    compute_layout_.local_size_z = local_size_z;
    return *(Self *)this;
  }

  /**
   * Force fragment tests before fragment shader invocation.
   * IMPORTANT: This is incompatible with using the gl_FragDepth output.
   */
  Self &early_fragment_test(bool enable)
  {
    early_fragment_test_ = enable;
    return *(Self *)this;
  }

  /**
   * Only needed if geometry shader is enabled.
   * IMPORTANT: Input and output instance name will have respectively "_in" and "_out" suffix
   * appended in the geometry shader IF AND ONLY IF the vertex_out interface instance name matches
   * the geometry_out interface instance name.
   */
  Self &geometry_out(StageInterfaceInfo &interface)
  {
    geometry_out_interfaces_.append(&interface);
    return *(Self *)this;
  }

  Self &fragment_out(int slot,
                     Type type,
                     StringRefNull name,
                     DualBlend blend = DualBlend::NONE,
                     int raster_order_group = -1)
  {
    fragment_outputs_.append({slot, type, blend, name, raster_order_group});
    return *(Self *)this;
  }

  /**
   * Allows to fetch frame-buffer values from previous render sub-pass.
   *
   * On Apple Silicon, the additional `raster_order_group` is there to set the sub-pass
   * dependencies. Any sub-pass input need to have the same `raster_order_group` defined in the
   * shader writing them.
   *
   * IMPORTANT: Currently emulated on all backend except Metal. This is only for debugging purpose
   * as it is too slow to be viable.
   *
   * TODO(fclem): Vulkan can implement that using `subpassInput`. However sub-pass boundaries might
   * be difficult to inject implicitly and will require more high level changes.
   * TODO(fclem): OpenGL can emulate that using `GL_EXT_shader_framebuffer_fetch`.
   */
  Self &subpass_in(
      int slot, Type type, ImageType img_type, StringRefNull name, int raster_order_group = -1)
  {
    subpass_inputs_.append({slot, type, img_type, name, raster_order_group});
    return *(Self *)this;
  }

  Self &shared_resource_descriptor(void (*fn)(ShaderCreateInfo &))
  {
    fn(*this);
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Shader compilation constants
   *
   * Compilation constants are constants defined in the create info.
   * They cannot be changed after the shader is created.
   * It is a replacement to macros with added type safety.
   * \{ */

  Self &compilation_constant(Type type, StringRefNull name, double default_value)
  {
    CompilationConstant constant;
    constant.type = type;
    constant.name = name;
    switch (type) {
      case Type::int_t:
        constant.value.i = int(default_value);
        break;
      case Type::bool_t:
      case Type::uint_t:
        constant.value.u = uint(default_value);
        break;
      default:
        BLI_assert_msg(0, "Only scalar integer and bool types can be used as constants");
        break;
    }
    compilation_constants_.append(constant);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Shader specialization constants
   * \{ */

  /* Adds a specialization constant which is a dynamically modifiable value, which will be
   * statically compiled into a PSO configuration to provide optimal runtime performance,
   * with a reduced re-compilation cost vs Macro's with easier generation of unique permutations
   * based on run-time values.
   *
   * Tip: To evaluate use-cases of where specialization constants can provide a performance
   * gain, benchmark a given shader in its default case. Attempt to statically disable branches or
   * conditions which rely on uniform look-ups and measure if there is a marked improvement in
   * performance and/or reduction in memory bandwidth/register pressure.
   *
   * NOTE: Specialization constants will incur new compilation of PSOs and thus can incur an
   * unexpected cost. Specialization constants should be reserved for infrequently changing
   * parameters (e.g. user setting parameters such as toggling of features or quality level
   * presets), or those with a low set of possible runtime permutations.
   *
   * Specialization constants are assigned at runtime using:
   *  - `GPU_shader_constant_*(shader, name, value)`
   * or
   *  - `DrawPass::specialize_constant(shader, name, value)`
   *
   * All constants **MUST** be specified before binding a shader.
   */
  Self &specialization_constant(Type type, StringRefNull name, double default_value)
  {
    SpecializationConstant constant;
    constant.type = type;
    constant.name = name;
    switch (type) {
      case Type::int_t:
        constant.value.i = int(default_value);
        break;
      case Type::bool_t:
      case Type::uint_t:
        constant.value.u = uint(default_value);
        break;
      case Type::float_t:
        constant.value.f = float(default_value);
        break;
      default:
        BLI_assert_msg(0, "Only scalar types can be used as constants");
        break;
    }
    specialization_constants_.append(constant);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  /* TODO: Add API to specify unique specialization config permutations in CreateInfo, allowing
   * specialized compilation to be primed and handled in the background at start-up, rather than
   * waiting for a given permutation to occur dynamically. */

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Compute shader Shared variables
   * \{ */

  Self &shared_variable(Type type, StringRefNull name)
  {
    shared_variables_.append({type, name});
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Resources bindings points
   * \{ */

  Self &uniform_buf(int slot,
                    StringRefNull type_name,
                    StringRefNull name,
                    Frequency freq = Frequency::PASS)
  {
    Resource res(Resource::BindType::UNIFORM_BUFFER, slot);
    res.uniformbuf.name = name;
    res.uniformbuf.type_name = type_name;
    resources_get_(freq).append(res);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  Self &storage_buf(int slot,
                    Qualifier qualifiers,
                    StringRefNull type_name,
                    StringRefNull name,
                    Frequency freq = Frequency::PASS)
  {
    Resource res(Resource::BindType::STORAGE_BUFFER, slot);
    res.storagebuf.qualifiers = qualifiers;
    res.storagebuf.type_name = type_name;
    res.storagebuf.name = name;
    resources_get_(freq).append(res);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  Self &image(int slot,
              TextureFormat format,
              Qualifier qualifiers,
              ImageReadWriteType type,
              StringRefNull name,
              Frequency freq = Frequency::PASS)
  {
    Resource res(Resource::BindType::IMAGE, slot);
    res.image.format = format;
    res.image.qualifiers = qualifiers;
    res.image.type = ImageType(type);
    res.image.name = name;
    resources_get_(freq).append(res);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  Self &sampler(int slot,
                ImageType type,
                StringRefNull name,
                Frequency freq = Frequency::PASS,
                GPUSamplerState sampler = GPUSamplerState::internal_sampler())
  {
    Resource res(Resource::BindType::SAMPLER, slot);
    res.sampler.type = type;
    res.sampler.name = name;
    /* Produces ASAN errors for the moment. */
    // res.sampler.sampler = sampler;
    UNUSED_VARS(sampler);
    resources_get_(freq).append(res);
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Shader Source
   * \{ */

  Self &vertex_source(StringRefNull filename)
  {
    vertex_source_ = filename;
    return *(Self *)this;
  }

  Self &fragment_source(StringRefNull filename)
  {
    fragment_source_ = filename;
    return *(Self *)this;
  }

  Self &compute_source(StringRefNull filename)
  {
    compute_source_ = filename;
    return *(Self *)this;
  }

  Self &vertex_function(StringRefNull function_name)
  {
    vertex_entry_fn_ = function_name;
    return *(Self *)this;
  }

  Self &fragment_function(StringRefNull function_name)
  {
    fragment_entry_fn_ = function_name;
    return *(Self *)this;
  }

  Self &compute_function(StringRefNull function_name)
  {
    compute_entry_fn_ = function_name;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Push constants
   *
   * Data managed by blender::gpu::Shader. Can be set through uniform functions. Must be less than
   * 128bytes.
   * \{ */

  Self &push_constant(Type type, StringRefNull name, int array_size = 0)
  {
    /* We don't have support for UINT push constants yet, use INT instead. */
    BLI_assert(type != Type::uint_t);
    BLI_assert_msg(name.find("[") == -1,
                   "Array syntax is forbidden for push constants."
                   "Use the array_size parameter instead.");
    push_constants_.append({type, name, array_size});
    interface_names_size_ += name.size() + 1;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Defines
   * \{ */

  Self &define(StringRefNull name, StringRefNull value = "")
  {
    defines_.append({name, value});
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Defines
   * \{ */

  Self &do_static_compilation(bool value)
  {
    do_static_compilation_ = value;
    return *(Self *)this;
  }

  Self &builtins(BuiltinBits builtin)
  {
    builtins_ |= builtin;
    return *(Self *)this;
  }

  /* Defines how the fragment shader will write to gl_FragDepth. */
  Self &depth_write(DepthWrite value)
  {
    depth_write_ = value;
    return *(Self *)this;
  }

  Self &auto_resource_location(bool value)
  {
    auto_resource_location_ = value;
    return *(Self *)this;
  }

  Self &metal_backend_only(bool flag)
  {
    metal_backend_only_ = flag;
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Additional Create Info
   *
   * Used to share parts of the infos that are common to many shaders.
   * \{ */

  Self &additional_info(StringRefNull info_name)
  {
    additional_infos_.append(info_name);
    return *(Self *)this;
  }

  template<typename... Args> Self &additional_info(StringRefNull info_name, Args... args)
  {
    additional_info(info_name);
    additional_info(args...);
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Typedef Sources
   *
   * Some resource declarations might need some special structure defined.
   * Adding a file using typedef_source will include it before the resource
   * and interface definitions.
   * \{ */

  Self &typedef_source(StringRefNull filename)
  {
    typedef_sources_.append(filename);
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name API-Specific Parameters
   *
   * Optional parameters exposed by specific back-ends to enable additional features and
   * performance tuning.
   * NOTE: These functions can be exposed as a pass-through on unsupported configurations.
   * \{ */

  /**
   * \name mtl_max_total_threads_per_threadgroup
   * \a max_total_threads_per_threadgroup - Provides compiler hint for maximum threadgroup size up
   * front. Maximum value is 1024.
   */
  Self &mtl_max_total_threads_per_threadgroup(ushort max_total_threads_per_threadgroup)
  {
#  ifdef WITH_METAL_BACKEND
    mtl_max_threads_per_threadgroup_ = max_total_threads_per_threadgroup;
#  else
    UNUSED_VARS(max_total_threads_per_threadgroup);
#  endif
    return *(Self *)this;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Recursive evaluation.
   *
   * Flatten all dependency so that this descriptor contains all the data from the additional
   * descriptors. This avoids tedious traversal in shader source creation.
   * \{ */

  /* WARNING: Recursive evaluation is not thread safe.
   * Non-recursive evaluation expects their dependencies to be already finalized.
   * (All statically declared CreateInfos are automatically finalized at startup) */
  void finalize(const bool recursive = false);

  std::string resource_guard_defines() const;

  std::string check_error() const;
  bool is_vulkan_compatible() const;

  /** Error detection that some backend compilers do not complain about. */
  void validate_merge(const ShaderCreateInfo &other_info);
  void validate_vertex_attributes(const ShaderCreateInfo *other_info = nullptr);

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Operators.
   *
   * \{ */

  /* Comparison operator for GPUPass cache. We only compare if it will create the same shader
   * code. So we do not compare name and some other internal stuff. */
  bool operator==(const ShaderCreateInfo &b) const
  {
    TEST_EQUAL(*this, b, builtins_);
    TEST_EQUAL(*this, b, vertex_source_generated);
    TEST_EQUAL(*this, b, fragment_source_generated);
    TEST_EQUAL(*this, b, compute_source_generated);
    TEST_EQUAL(*this, b, typedef_source_generated);
    TEST_VECTOR_EQUAL(*this, b, vertex_inputs_);
    TEST_EQUAL(*this, b, geometry_layout_);
    TEST_EQUAL(*this, b, compute_layout_);
    TEST_VECTOR_EQUAL(*this, b, fragment_outputs_);
    TEST_VECTOR_EQUAL(*this, b, pass_resources_);
    TEST_VECTOR_EQUAL(*this, b, batch_resources_);
    TEST_VECTOR_EQUAL(*this, b, geometry_resources_);
    TEST_VECTOR_EQUAL(*this, b, vertex_out_interfaces_);
    TEST_VECTOR_EQUAL(*this, b, geometry_out_interfaces_);
    TEST_VECTOR_EQUAL(*this, b, push_constants_);
    TEST_VECTOR_EQUAL(*this, b, typedef_sources_);
    TEST_VECTOR_EQUAL(*this, b, subpass_inputs_);
    TEST_EQUAL(*this, b, vertex_source_);
    TEST_EQUAL(*this, b, geometry_source_);
    TEST_EQUAL(*this, b, fragment_source_);
    TEST_EQUAL(*this, b, compute_source_);
    TEST_VECTOR_EQUAL(*this, b, additional_infos_);
    TEST_VECTOR_EQUAL(*this, b, defines_);
    return true;
  }

  /** Debug print */
  friend std::ostream &operator<<(std::ostream &stream, const ShaderCreateInfo &info)
  {
    /* TODO(@fclem): Complete print. */

    auto print_resource = [&](const Resource &res) {
      switch (res.bind_type) {
        case Resource::BindType::UNIFORM_BUFFER:
          stream << "UNIFORM_BUFFER(" << res.slot << ", " << res.uniformbuf.name << ")"
                 << std::endl;
          break;
        case Resource::BindType::STORAGE_BUFFER:
          stream << "STORAGE_BUFFER(" << res.slot << ", " << res.storagebuf.name << ")"
                 << std::endl;
          break;
        case Resource::BindType::SAMPLER:
          stream << "SAMPLER(" << res.slot << ", " << res.sampler.name << ")" << std::endl;
          break;
        case Resource::BindType::IMAGE:
          stream << "IMAGE(" << res.slot << ", " << res.image.name << ")" << std::endl;
          break;
      }
    };

    /* TODO(@fclem): Order the resources. */
    for (const auto &res : info.batch_resources_) {
      print_resource(res);
    }
    for (const auto &res : info.pass_resources_) {
      print_resource(res);
    }
    for (const auto &res : info.geometry_resources_) {
      print_resource(res);
    }
    return stream;
  }

  bool has_resource_type(Resource::BindType bind_type) const
  {
    for (const auto &res : batch_resources_) {
      if (res.bind_type == bind_type) {
        return true;
      }
    }
    for (const auto &res : pass_resources_) {
      if (res.bind_type == bind_type) {
        return true;
      }
    }
    for (const auto &res : geometry_resources_) {
      if (res.bind_type == bind_type) {
        return true;
      }
    }
    return false;
  }

  bool has_resource_image() const
  {
    return has_resource_type(Resource::BindType::IMAGE);
  }

  int sampler_count() const
  {
    int count = 0;
    for (const ShaderCreateInfo::Resource &res : this->pass_resources_) {
      count += int(res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER);
    }
    for (const ShaderCreateInfo::Resource &res : this->batch_resources_) {
      count += int(res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER);
    }
    for (const ShaderCreateInfo::Resource &res : this->geometry_resources_) {
      count += int(res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER);
    }
    return count;
  }

  int max_sampler_slot() const
  {
    int slot = 0;
    for (const ShaderCreateInfo::Resource &res : this->pass_resources_) {
      if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
        slot = std::max(slot, res.slot);
      }
    }
    for (const ShaderCreateInfo::Resource &res : this->batch_resources_) {
      if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
        slot = std::max(slot, res.slot);
      }
    }
    for (const ShaderCreateInfo::Resource &res : this->geometry_resources_) {
      if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
        slot = std::max(slot, res.slot);
      }
    }
    return slot;
  }

  /** \} */

#  undef TEST_EQUAL
#  undef TEST_VECTOR_EQUAL
};

/* Storage for strings referenced but the patched create info. */
using ShaderCreateInfoStringCache = Vector<std::unique_ptr<std::string>, 0>;

}  // namespace blender::gpu::shader

namespace blender {
template<> struct DefaultHash<Vector<blender::gpu::shader::SpecializationConstant::Value>> {
  uint64_t operator()(const Vector<blender::gpu::shader::SpecializationConstant::Value> &key) const
  {
    uint64_t hash = 0;
    for (const blender::gpu::shader::SpecializationConstant::Value &value : key) {
      hash = hash * 33 ^ uint64_t(value.u);
    }
    return hash;
  }
};
}  // namespace blender

#endif
