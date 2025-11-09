/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>
#include <sstream>
#include <string>

#include "BLI_math_bits.h"

#include "gpu_shader_dependency_private.hh"
#include "mtl_backend.hh"
#include "mtl_shader_generate.hh"

using namespace blender;
using namespace blender::gpu;
using namespace blender::gpu::shader;

namespace blender::gpu {

struct Separator {};

/* Same as a string stream but insert a separator before each new argument after the first one. */
struct ArgumentStream : public std::stringstream {
  bool is_first_arg = true;

  std::stringstream &operator<<(Separator /*separator*/)
  {
    if (is_first_arg) {
      is_first_arg = false;
      *this << "\n  ";
    }
    else {
      *this << ",\n  ";
    }
    return *this;
  }
};

struct GeneratedStreams {
  std::stringstream wrapper_class_prefix;
  std::stringstream wrapper_class_members;
  ArgumentStream wrapper_constructor_parameters;
  ArgumentStream wrapper_constructor_assign;
  ArgumentStream entry_point_parameters;
  std::stringstream entry_point_start;
  ArgumentStream wrapper_instance_init;
};

using Sep = Separator;

static bool is_builtin_type(StringRefNull type)
{
  /* Add Types as needed. */
  static Set<StringRefNull> glsl_builtin_types = {
      {"float"},
      {"vec2"},
      {"vec3"},
      {"vec4"},
      {"float2"},
      {"float3"},
      {"float4"},
      {"int"},
      {"ivec2"},
      {"ivec3"},
      {"ivec4"},
      {"int2"},
      {"int3"},
      {"int4"},
      {"uint32_t"},
      {"uvec2"},
      {"uvec3"},
      {"uvec4"},
      {"uint"},
      {"uint2"},
      {"uint3"},
      {"uint4"},
      {"mat3"},
      {"mat4"},
      {"float3x3"},
      {"float4x4"},
      {"bool"},
      {"uchar"},
      {"uchar2"},
      {"uchar2"},
      {"uchar4"},
      {"vec3_1010102_Unorm"},
      {"vec3_1010102_Inorm"},
      {"packed_float2"},
      {"packed_float3"},
  };
  return glsl_builtin_types.contains(type);
}

static StringRefNull to_raw_type(ImageType type)
{
  bool supports_native_atomics = MTLBackend::get_capabilities().supports_texture_atomics;
  switch (type) {
    case ImageType::Float1D:
      return "texture1d";
    case ImageType::Float2D:
      return "texture2d";
    case ImageType::Float3D:
      return "texture3d";
    case ImageType::FloatCube:
      return "texturecube";
    case ImageType::Float1DArray:
      return "texture1d_array";
    case ImageType::Float2DArray:
      return "texture2d_array";
    case ImageType::FloatCubeArray:
      return "texturecube_array";
    case ImageType::FloatBuffer:
      return "texture_buffer";
    case ImageType::Depth2D:
      return "depth2d";
    case ImageType::Shadow2D:
      return "depth2d";
    case ImageType::Depth2DArray:
      return "depth2d_array";
    case ImageType::Shadow2DArray:
      return "depth2d_array";
    case ImageType::DepthCube:
      return "depthcube";
    case ImageType::ShadowCube:
      return "depthcube";
    case ImageType::DepthCubeArray:
      return "depthcube_array";
    case ImageType::ShadowCubeArray:
      return "depthcube_array";
    case ImageType::Int1D:
      return "texture1d";
    case ImageType::Int2D:
      return "texture2d";
    case ImageType::Int3D:
      return "texture3d";
    case ImageType::IntCube:
      return "texturecube";
    case ImageType::Int1DArray:
      return "texture1d_array";
    case ImageType::Int2DArray:
      return "texture2d_array";
    case ImageType::IntCubeArray:
      return "texturecube_array";
    case ImageType::IntBuffer:
      return "texture_buffer";
    case ImageType::Uint1D:
      return "texture1d";
    case ImageType::Uint2D:
      return "texture2d";
    case ImageType::Uint3D:
      return "texture3d";
    case ImageType::UintCube:
      return "texturecube";
    case ImageType::Uint1DArray:
      return "texture1d_array";
    case ImageType::Uint2DArray:
      return "texture2d_array";
    case ImageType::UintCubeArray:
      return "texturecube_array";
    case ImageType::UintBuffer:
      return "texture_buffer";
    case ImageType::AtomicInt2D:
    case ImageType::AtomicUint2D:
      return "texture2d";
    case ImageType::AtomicInt2DArray:
    case ImageType::AtomicUint2DArray:
      /* If texture atomics are natively supported, we use the native texture type, otherwise all
       * other formats are implemented via texture2d. */
      return supports_native_atomics ? "texture2d_array" : "texture2d";
    case ImageType::AtomicInt3D:
    case ImageType::AtomicUint3D:
      /* If texture atomics are natively supported, we use the native texture type, otherwise all
       * other formats are implemented via texture2d. */
      return supports_native_atomics ? "texture3d" : "texture2d";
    default: {
      /* Unrecognized type. */
      BLI_assert_unreachable();
      return "ERROR";
    }
  };
}

StringRefNull to_wrapper_type(ImageType type)
{
  bool supports_native_atomics = MTLBackend::get_capabilities().supports_texture_atomics;
  /* Add Types as needed. */
  switch (type) {
    case ImageType::Float1D:
    case ImageType::Int1D:
    case ImageType::Uint1D:
      return "sampler1D";
    case ImageType::Float1DArray:
    case ImageType::Int1DArray:
    case ImageType::Uint1DArray:
      return "sampler1DArray";
    case ImageType::Float2D:
    case ImageType::Int2D:
    case ImageType::Uint2D:
      return "sampler2D";
    case ImageType::Float2DArray:
    case ImageType::Int2DArray:
    case ImageType::Uint2DArray:
      return "sampler2DArray";
    case ImageType::Float3D:
    case ImageType::Int3D:
    case ImageType::Uint3D:
      return "sampler3D";
    case ImageType::FloatBuffer:
    case ImageType::IntBuffer:
    case ImageType::UintBuffer:
      return "samplerBuffer";
    case ImageType::FloatCube:
    case ImageType::IntCube:
    case ImageType::UintCube:
      return "samplerCube";
    case ImageType::FloatCubeArray:
    case ImageType::IntCubeArray:
    case ImageType::UintCubeArray:
      return "samplerCubeArray";
    case ImageType::Depth2D:
    case ImageType::Shadow2D:
      return "sampler2DDepth";
    case ImageType::Depth2DArray:
    case ImageType::Shadow2DArray:
      return "sampler2DArrayDepth";
    case ImageType::DepthCube:
    case ImageType::ShadowCube:
      return "depthCube";
    case ImageType::DepthCubeArray:
    case ImageType::ShadowCubeArray:
      return "depthCubeArray";
    /* If native texture atomics are unsupported, map types to fallback atomic structures which
     * contain a buffer pointer and metadata members for size and alignment. */
    case ImageType::AtomicInt2D:
    case ImageType::AtomicUint2D:
      return supports_native_atomics ? "sampler2D" : "sampler2DAtomic";
    case ImageType::AtomicInt3D:
    case ImageType::AtomicUint3D:
      return supports_native_atomics ? "sampler3D" : "sampler3DAtomic";
    case ImageType::AtomicInt2DArray:
    case ImageType::AtomicUint2DArray:
      return supports_native_atomics ? "sampler2DArray" : "sampler2DArrayAtomic";
    default: {
      /* Unrecognized type. */
      BLI_assert_unreachable();
      return "ERROR";
    }
  };
}

static StringRefNull to_component_type(ImageType type)
{
  /* Add Types as needed */
  switch (type) {
    /* Floating point return. */
    case ImageType::Float1D:
    case ImageType::Float2D:
    case ImageType::Float3D:
    case ImageType::FloatCube:
    case ImageType::Float1DArray:
    case ImageType::Float2DArray:
    case ImageType::FloatCubeArray:
    case ImageType::FloatBuffer:
    case ImageType::Depth2D:
    case ImageType::Shadow2D:
    case ImageType::Depth2DArray:
    case ImageType::Shadow2DArray:
    case ImageType::DepthCube:
    case ImageType::ShadowCube:
    case ImageType::DepthCubeArray:
    case ImageType::ShadowCubeArray: {
      return "float";
    }
    /* Integer return. */
    case ImageType::Int1D:
    case ImageType::Int2D:
    case ImageType::Int3D:
    case ImageType::IntCube:
    case ImageType::Int1DArray:
    case ImageType::Int2DArray:
    case ImageType::IntCubeArray:
    case ImageType::IntBuffer:
    case ImageType::AtomicInt2D:
    case ImageType::AtomicInt2DArray:
    case ImageType::AtomicInt3D: {
      return "int";
    }

    /* Unsigned Integer return. */
    case ImageType::Uint1D:
    case ImageType::Uint2D:
    case ImageType::Uint3D:
    case ImageType::UintCube:
    case ImageType::Uint1DArray:
    case ImageType::Uint2DArray:
    case ImageType::UintCubeArray:
    case ImageType::UintBuffer:
    case ImageType::AtomicUint2D:
    case ImageType::AtomicUint2DArray:
    case ImageType::AtomicUint3D: {
      return "uint32_t";
    }

    default: {
      /* Unrecognized type. */
      BLI_assert_unreachable();
      return "ERROR";
    }
  };
}

static const char *get_stage_class_name(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "mtl_Vert";
    case ShaderStage::FRAGMENT:
      return "mtl_Frag";
    case ShaderStage::COMPUTE:
      return "mtl_Comp";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}

static const char *get_stage_out_class_name(ShaderStage stage, const ShaderCreateInfo &info)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "mtl_VertOut";
    case ShaderStage::FRAGMENT:
      return (info.fragment_outputs_.is_empty() && info.depth_write_ == DepthWrite::UNCHANGED &&
              bool(info.builtins_ & BuiltinBits::STENCIL_REF) == false) ?
                 "void" :
                 "mtl_FragOut";
    case ShaderStage::COMPUTE:
      return "void";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}
static const char *get_stage_in_class_name(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "mtl_VertIn";
    case ShaderStage::FRAGMENT:
      return "mtl_VertOut";
    case ShaderStage::COMPUTE:
      return "mtl_CompIn";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}

static const char *get_stage_out_instance_name(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "mtl_vert_out";
    case ShaderStage::FRAGMENT:
      return "mtl_frag_out";
    case ShaderStage::COMPUTE:
      return "";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}

static const char *get_stage_in_instance_name(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "mtl_vert_in";
    case ShaderStage::FRAGMENT:
      return "mtl_frag_in";
    case ShaderStage::COMPUTE:
      return "";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}

static const char *get_stage_instance_name(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "vert_inst";
    case ShaderStage::FRAGMENT:
      return "frag_inst";
    case ShaderStage::COMPUTE:
      return "comp_inst";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}

static const char *get_stage_type(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "vertex";
    case ShaderStage::FRAGMENT:
      return "fragment";
    case ShaderStage::COMPUTE:
      return "kernel";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}

static std::string to_string(shader::Type type, SpecializationConstant::Value value)
{
  switch (type) {
    case Type::uint_t:
      return std::to_string(value.u);
    case Type::int_t:
      return std::to_string(value.i);
    case Type::bool_t:
      return value.u ? "true" : "false";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static const char *to_string(const Interpolation &interp)
{
  switch (interp) {
    case Interpolation::SMOOTH:
      return " [[center_perspective]]";
    case Interpolation::FLAT:
      return " [[flat]]";
    case Interpolation::NO_PERSPECTIVE:
      return " [[center_no_perspective]]";
    default:
      BLI_assert_unreachable();
      return " error";
  }
}

#if 0
#  define LINE ""
#else
#  define LINE "\n#line " STRINGIFY(__LINE__) " \"" __FILE__ "\"\n"
#endif

std::string wrap_type(StringRefNull type_name, const ShaderStage stage)
{
  if (is_builtin_type(type_name)) {
    return type_name;
  }
  return std::string(get_stage_class_name(stage)) + "::" + type_name;
}

std::string ref_type(const ResourceString &str, const std::string &attribute = "")
{
  if (!str.is_array()) {
    return " &" + str + attribute;
  }
  return " (&" + str.str_no_array() + attribute + ")" + str.str_only_array();
}

std::string ptr_type(const ResourceString &str)
{
  if (!str.is_array()) {
    return " &" + str;
  }
  return " *" + str.str_no_array();
}

static void generate_uniforms(GeneratedStreams &generated,
                              Span<ShaderCreateInfo::PushConst> uniforms,
                              const ShaderStage stage)
{
  /* Only generate PushConstantBlock if we have uniforms. */
  if (uniforms.is_empty()) {
    return;
  }
  {
    /* Block definition. */
    auto &out = generated.wrapper_class_members;
    out << "  struct PushConstantBlock {\n";
    for (const ShaderCreateInfo::PushConst &uni : uniforms) {
      /* Subtile workaround to follow sane alignment rules.
       * Always use 4 bytes boolean like in GLSL. */
      Type type = (uni.type == Type::bool_t) ? Type::int_t : uni.type;
      out << "    " << type << " " << uni.name << uni.array_str() << ";\n";
    }
    out << "  };\n";

    /* References definition for global access. */
    for (const ShaderCreateInfo::PushConst &uni : uniforms) {
      Type type = (uni.type == Type::bool_t) ? Type::int_t : uni.type;
      out << "  const constant " << type << " (&" << uni.name << ")" << uni.array_str() << ";\n";
    }
  }
  {
    /* Constructor parameters. */
    auto &out = generated.wrapper_constructor_parameters;
    out << Sep() << "const constant PushConstantBlock &mtl_pc";
  }
  {
    /* Constructor assignments. */
    auto &out = generated.wrapper_constructor_assign;
    for (const ShaderCreateInfo::PushConst &uni : uniforms) {
      out << Sep() << uni.name << "(mtl_pc." << uni.name << ")";
    }
  }
  {
    /* Constructor arguments. */
    auto &out = generated.wrapper_instance_init;
    out << Sep() << "*mtl_pc";
  }
  {
    /* Entry point arguments. */
    auto &out = generated.entry_point_parameters;
    out << Sep() << "constant " << wrap_type("PushConstantBlock", stage) << " *mtl_pc";
    out << " [[buffer(" << MTL_PUSH_CONSTANT_BUFFER_SLOT << ")]]";
  }
}

static void generate_buffer(GeneratedStreams &generated,
                            const bool writeable,
                            StringRefNull type,
                            ResourceString name,
                            int slot,
                            const ShaderStage stage)
{
  const char *memory_scope = (writeable) ? "device " : "constant ";
  const char *const_qual = bool(stage & ShaderStage::VERTEX) ? "const " : "";

  {
    /* References definition for global access. */
    auto &out = generated.wrapper_class_members;
    out << "  " << memory_scope << type << ref_type(name) << ";\n";
  }
  {
    /* Constructor parameters. */
    auto &out = generated.wrapper_constructor_parameters;
    out << Sep() << const_qual << memory_scope << type << ref_type(name);
  }
  {
    /* Constructor assignments. */
    auto &out = generated.wrapper_constructor_assign;
    out << Sep() << name.str_no_array() << "(";
    /* Remove the const qualifier. Its only there to avoid a compiler warning. */
    /* The reason the warning exists is because the vertex shader might be executed more than once
     * per vertex, which could lead to weird situation when working with atomic counter for
     * instance. Given this is only used by the debug line shader (to decrement the primitives
     * lifetime) it is not a huge issue to silence the warning. In the future, it might be better
     * to add a flag on the create info to allow non-const resource in the vertex shader. */
    out << "const_cast<" << memory_scope << type << " (&)" << name.str_only_array() << ">(";
    out << name.str_no_array();
    out << ")";
    out << ")";
  }
  {
    /* Constructor arguments. */
    auto &out = generated.wrapper_instance_init;
    out << Sep() << name.str_no_array();
  }
  {
    /* Entry point arguments. */
    auto &out = generated.entry_point_parameters;
    out << Sep() << const_qual << memory_scope << wrap_type(type, stage);
    out << ref_type(name, " [[buffer(" + std::to_string(slot) + ")]]");
  }
}

StringRefNull to_access(const bool is_sampler, const shader::Qualifier qualifier)
{
  if (is_sampler) {
    return "access::sample";
  }

  switch (qualifier) {
    case shader::Qualifier::read:
      return "access::read";
    case shader::Qualifier::write:
      return "access::write";
    case shader::Qualifier::read_write:
      return "access::read_write";
    default:
      BLI_assert(false);
      return "";
  }
  return "";
}

static void generate_texture(GeneratedStreams &generated,
                             bool is_sampler,
                             shader::Qualifier qualifier,
                             const shader::ImageType type,
                             ResourceString name,
                             int slot,
                             const ShaderStage stage,
                             const bool use_sampler_argument_buffer = false)
{
  const bool supports_native_atomics = MTLBackend::get_capabilities().supports_texture_atomics;

  if (ELEM(type, ImageType::FloatBuffer, ImageType::IntBuffer, ImageType::UintBuffer)) {
    /* These cannot be declared with sample access. */
    is_sampler = false;
  }
  if (bool(stage & ShaderStage::VERTEX)) {
    /* Forcing sampling only access for vertex shaders.
     * Avoid this warning: "writable resources in non-void vertex function". */
    qualifier = shader::Qualifier::read;
  }
  /* Samplers use a different bind space and start at 0. */
  const std::string sampler_slot = std::to_string(slot - MTL_SAMPLER_SLOT_OFFSET);
  const std::string sampler_name = use_sampler_argument_buffer ?
                                       ("mtl_samplers.samplers[" + sampler_slot + "]") :
                                       (name + "_samp_");
  const std::string temp_args = to_component_type(type) + ", " + to_access(is_sampler, qualifier);
  const std::string type_str = to_raw_type(type) + "<" + temp_args + "> ";
  const std::string wrapper_str = "_" + to_wrapper_type(type) + "<" + temp_args + "> ";
  {
    /* References definition for global access. */
    auto &out = generated.wrapper_class_members;
    out << "  " << wrapper_str << name << ";\n";
  }
  {
    /* Constructor parameters. */
    auto &out = generated.wrapper_constructor_parameters;
    out << Sep() << wrapper_str << name;
  }
  {
    /* Constructor assignments. */
    auto &out = generated.wrapper_constructor_assign;
    out << Sep() << name << "(" << name << ")";
  }
  {
    /* Constructor arguments. */
    auto &out = generated.wrapper_instance_init;
    std::string atomic_args;
    if (!supports_native_atomics) {
      std::string prefix;
      std::string suffix;
      if (stage == ShaderStage::VERTEX) {
        /* Keep buffer declaration as const to avoid warning. */
        prefix = "const_cast<device " + to_component_type(type) + " *>(";
        suffix = ")";
      }

      /* Pass the additional buffer and the metadata. */
      if (ELEM(type,
               ImageType::AtomicUint2DArray,
               ImageType::AtomicUint3D,
               ImageType::AtomicInt2DArray,
               ImageType::AtomicInt3D))
      {
        /* Buffer-backed 2D Array and 3D texture types are not natively supported so texture size
         * is passed in as uniform metadata for 3D to 2D coordinate remapping. */
        atomic_args += ", {";
        atomic_args += prefix + name + "_buf_" + suffix + ", ";
        atomic_args += "ushort3(mtl_pc->" + name + "_metadata_.xyz), ";
        atomic_args += "ushort(mtl_pc->" + name + "_metadata_.w)";
        atomic_args += "}";
      }
      else if (ELEM(type, ImageType::AtomicUint2D, ImageType::AtomicInt2D)) {
        /* Only pass buffer and alignment. */
        atomic_args += ", {";
        atomic_args += prefix + name + "_buf_" + suffix + ", ";
        atomic_args += "ushort(mtl_pc->" + name + "_metadata_.w)";
        atomic_args += "}";
      }
    }
    if (is_sampler) {
      out << Sep() << wrapper_str << "{&" << name << ", &" << sampler_name << atomic_args << "}";
    }
    else {
      out << Sep() << wrapper_str << "{&" << name << ", nullptr " << atomic_args << "}";
    }
  }
  {
    /* Entry point arguments. */
    auto &out = generated.entry_point_parameters;
    out << Sep() << type_str << name << " [[texture(" << slot << ")]]";

    if (is_sampler && !use_sampler_argument_buffer) {
      out << Sep() << "sampler " << sampler_name << " [[sampler(" << sampler_slot << ")]]";
    }
  }
}

static void generate_resource(GeneratedStreams &generated,
                              const ShaderCreateInfo::Resource &res,
                              const ShaderStage stage,
                              const bool use_sampler_argument_buffer)
{
  switch (res.bind_type) {
    case ShaderCreateInfo::Resource::BindType::SAMPLER:
      generate_texture(generated,
                       true,
                       Qualifier::read,
                       res.sampler.type,
                       res.sampler.name,
                       MTL_SAMPLER_SLOT_OFFSET + res.slot,
                       stage,
                       use_sampler_argument_buffer);
      break;
    case ShaderCreateInfo::Resource::BindType::IMAGE:
      generate_texture(generated,
                       false,
                       res.image.qualifiers,
                       res.image.type,
                       res.image.name,
                       MTL_IMAGE_SLOT_OFFSET + res.slot,
                       stage);
      break;
    case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      generate_buffer(generated,
                      false,
                      res.uniformbuf.type_name,
                      res.uniformbuf.name,
                      MTL_UBO_SLOT_OFFSET + res.slot,
                      stage);
      break;
    case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      generate_buffer(generated,
                      bool(res.storagebuf.qualifiers & shader::Qualifier::write),
                      res.storagebuf.type_name,
                      res.storagebuf.name,
                      MTL_SSBO_SLOT_OFFSET + res.slot,
                      stage);
      break;
  }
}

static void generate_compilation_constant(GeneratedStreams &generated,
                                          const CompilationConstant &constant)
{
  /* Global scope definition before the wrapper class. */
  auto &out = generated.wrapper_class_prefix;
  out << "constant " << constant.type << " " << constant.name;
  out << " = " << to_string(constant.type, constant.value) << ";\n";
}

static void generate_specialization_constant(GeneratedStreams &generated,
                                             const SpecializationConstant &constant,
                                             int index)
{
  /* Global scope definition before the wrapper class. */
  auto &out = generated.wrapper_class_prefix;
  out << "constant " << constant.type << " " << constant.name;
  out << " [[function_constant(" << index << ")]];\n";
}

static void generate_shared_variable(GeneratedStreams &generated,
                                     const ShaderCreateInfo::SharedVariable &sv)
{
  {
    /* References definition for global access. */
    auto &out = generated.wrapper_class_members;
    out << "  threadgroup " << sv.type << ref_type(sv.name) << ";\n";
  }
  {
    /* Constructor parameters. */
    auto &out = generated.wrapper_constructor_parameters;
    out << Sep() << "threadgroup " << sv.type << ref_type(sv.name);
  }
  {
    /* Constructor assignments. */
    auto &out = generated.wrapper_constructor_assign;
    out << Sep() << sv.name.str_no_array() << "(" << sv.name.str_no_array() << ")";
  }
  {
    /* Entry point body start. */
    auto &out = generated.entry_point_start;
    out << "  threadgroup " << sv.type << " " << sv.name << ";\n";
  }
  {
    /* Constructor arguments. */
    auto &out = generated.wrapper_instance_init;
    out << Sep() << sv.name.str_no_array();
  }
}

static void generate_sampler_argument_buffer(GeneratedStreams &generated, int sampler_count)
{
  {
    /* Global scope definition before the wrapper class. */
    auto &out = generated.wrapper_class_prefix;
    out << "struct BindlessSamplers {\n";
    out << "  array<sampler, " << sampler_count << "> samplers [[id(0)]];\n";
    out << "};\n";
  }
  {
    /* Entry point arguments. */
    auto &out = generated.entry_point_parameters;
    out << Sep() << "constant BindlessSamplers &mtl_samplers";
    out << " [[buffer(" << MTL_SAMPLER_ARGUMENT_BUFFER_SLOT << ")]]";
  }
}

void generate_resources(GeneratedStreams &generated,
                        const ShaderStage stage,
                        const ShaderCreateInfo &info)
{
  const bool use_sampler_argument_buffer = bool(info.builtins_ &
                                                BuiltinBits::USE_SAMPLER_ARG_BUFFER);

  int specialization_constant_index = MTL_SPECIALIZATION_CONSTANT_OFFSET;
  for (const SpecializationConstant &sc : info.specialization_constants_) {
    generate_specialization_constant(generated, sc, specialization_constant_index++);
  }

  for (const CompilationConstant &cc : info.compilation_constants_) {
    generate_compilation_constant(generated, cc);
  }

  for (const ShaderCreateInfo::SharedVariable &sv : info.shared_variables_) {
    generate_shared_variable(generated, sv);
  }

  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    generate_resource(generated, res, stage, use_sampler_argument_buffer);
  }
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    generate_resource(generated, res, stage, use_sampler_argument_buffer);
  }
  for (const ShaderCreateInfo::Resource &res : info.geometry_resources_) {
    generate_resource(generated, res, stage, use_sampler_argument_buffer);
  }

  if (use_sampler_argument_buffer) {
    generate_sampler_argument_buffer(generated, info.max_sampler_slot() + 1);
  }

  generate_uniforms(generated, info.push_constants_, stage);
}

static void generate_vertex_attributes(GeneratedStreams &generated, const ShaderCreateInfo &info)
{
  constexpr ShaderStage stage = ShaderStage::VERTEX;
  StringRefNull in_class_local = get_stage_in_class_name(stage);
  std::string in_class = get_stage_class_name(stage) + ("::" + in_class_local);
  StringRefNull in_inst = get_stage_in_instance_name(stage);

  if (info.vertex_inputs_.is_empty()) {
    return;
  }

  {
    /* References definition for global access. */
    auto &out = generated.wrapper_class_members;
    for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
      out << "  const " << attr.type << " " << attr.name << ";\n";
    }

    out << "  struct " << in_class_local << " {\n";
    for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
      out << "    " << attr.type << " " << attr.name << " [[attribute(" << attr.index << ")]];\n";
    }
    out << "  };\n\n";
  }
  {
    /* Constructor parameters. */
    auto &out = generated.wrapper_constructor_parameters;
    out << Sep() << "const thread " << in_class_local << " &mtl_vert_in";
  }
  {
    /* Constructor assignments. */
    auto &out = generated.wrapper_constructor_assign;
    for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
      out << Sep() << attr.name << "(mtl_vert_in." << attr.name << ")";
    }
  }
  {
    /* Entry point arguments. */
    auto &out = generated.entry_point_parameters;
    out << Sep() << in_class << " " << in_inst << " [[stage_in]]";
  }
  {
    /* Constructor arguments. */
    auto &out = generated.wrapper_instance_init;
    out << Sep() << in_inst;
  }
}

static void generate_raster_builtin(GeneratedStreams &ss,
                                    std::stringstream &declaration,
                                    const StringRefNull type,
                                    const StringRefNull var,
                                    const StringRefNull attribute,
                                    const StringRefNull array = "",
                                    const bool is_const = false)
{
  StringRefNull const_qual = is_const ? "const " : "";
  StringRefNull mem_scope = "thread ";
  /* Declaration inside builtin class. */
  declaration << "    " << type << " " << var << " " << attribute << array << ";\n";
  /* Global scope access. */
  ss.wrapper_class_members << "  " << mem_scope << const_qual << type << " (&" << var << ")"
                           << array << ";\n";
  ss.wrapper_constructor_assign << Sep() << var << "(mtl_vert_out." << var << ")";
}

static std::string generate_raster_builtins(GeneratedStreams &ss,
                                            const ShaderCreateInfo &info,
                                            const ShaderStage stage)
{
  const bool is_frag = stage == ShaderStage::FRAGMENT;
  std::stringstream decl;

  /* If invariance is available, utilize this to consistently mitigate depth fighting artifacts
   * by ensuring that vertex position is consistently calculated between subsequent passes
   * with maximum precision. */
  /* TODO(fclem): Maybe worth enabling only for cases where it matters (only mesh rendering). */
  std::string pos_attr = "[[position]] [[invariant]]";
  if (stage == ShaderStage::VERTEX) {
    generate_raster_builtin(ss, decl, "float4", "gl_Position", pos_attr);
  }
  else if (bool(info.builtins_ & BuiltinBits::FRAG_COORD) && stage == ShaderStage::FRAGMENT) {
    generate_raster_builtin(ss, decl, "float4", "gl_FragCoord", pos_attr, "", true);
  }

  if (bool(info.builtins_ & BuiltinBits::LAYER)) {
    generate_raster_builtin(
        ss, decl, "uint", "gpu_Layer", "[[render_target_array_index]]", "", is_frag);
  }
  if (bool(info.builtins_ & BuiltinBits::VIEWPORT_INDEX)) {
    generate_raster_builtin(
        ss, decl, "uint", "gpu_ViewportIndex", "[[viewport_array_index]]", "", is_frag);
  }
  if (bool(info.builtins_ & BuiltinBits::POINT_SIZE) && stage == ShaderStage::VERTEX) {
    generate_raster_builtin(ss, decl, "float", "gl_PointSize", "[[point_size]]");
  }
  if (bool(info.builtins_ & BuiltinBits::CLIP_DISTANCES) && stage == ShaderStage::VERTEX) {
    generate_raster_builtin(ss, decl, "float", "gl_ClipDistance", "[[clip_distance]]", " [6]");
    /* We always create all planes and initialize them to 1 (passing). This way the shader doesn't
     * have to write to them for the ones it doesn't need. */
    StringRefNull vert_inout_inst = get_stage_out_instance_name(stage);
    for ([[maybe_unused]] const int i : IndexRange(6)) {
      ss.entry_point_start << "  " << vert_inout_inst << ".gl_ClipDistance[" << i << "] = 1.0f;\n";
    }
  }
  return decl.str();
}

static void generate_inout(std::stringstream &out,
                           StringRefNull iface_name,
                           const StageInterfaceInfo::InOut &inout)
{
  /* TODO(fclem): Move this to the GPU level and do not assert but simply fail compilation. */
  BLI_assert(inout.type != Type::float3x3_t && inout.type != Type::float4x4_t &&
             !inout.name.is_array());
  out << "    " << inout.type << " _" << iface_name << "_" << inout.name.str_no_array();
  out << to_string(inout.interp) << ";\n";
}

static void generate_vertex_out(GeneratedStreams &generated,
                                const ShaderCreateInfo &info,
                                const ShaderStage stage)
{
  std::string out_class_local = get_stage_out_class_name(ShaderStage::VERTEX, info);
  std::string out_class = get_stage_class_name(ShaderStage::VERTEX) + ("::" + out_class_local);

  StringRefNull const_qual = (stage == ShaderStage::FRAGMENT) ? "const " : "";
  StringRefNull mem_scope = "thread ";

  {
    auto &out = generated.wrapper_class_members;

    std::string builtins_decl = generate_raster_builtins(generated, info, stage);

    /* References definition for global access. */
    for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
      if (iface->instance_name.is_empty()) {
        for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
          out << "  " << const_qual << mem_scope << inout.type << " &" << inout.name << ";\n";
        }
      }
      else {
        out << "  struct " << iface->name << " {\n";
        for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
          /* Eventually, we only need one pointer per named interface. However, this require
           * MSL 3.0 which would mean artificially dropping support for older MacOS versions. */
          out << "  " << const_qual << mem_scope << inout.type << " &" << inout.name << ";\n";
        }
        out << "  } " << iface->instance_name << ";\n";
      }
    }

    /* Main Block Definition. */
    out << "  struct " << out_class_local << " {\n";
    out << builtins_decl;
    for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
      out << "    /* " << iface->name << " */\n";
      for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
        generate_inout(out, iface->instance_name, inout);
      }
    }
    out << "  };\n\n";
  }
  {
    /* Constructor parameters. */
    auto &out = generated.wrapper_constructor_parameters;
    out << Sep() << const_qual << mem_scope << out_class_local << " &mtl_vert_out";
  }
  {
    /* Constructor assignments. */
    auto &out = generated.wrapper_constructor_assign;
    for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
      if (iface->instance_name.is_empty()) {
        for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
          out << Sep() << inout.name << "(mtl_vert_out.__" << inout.name << ")";
        }
      }
      else {
        ArgumentStream args;
        for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
          args << Sep() << "  mtl_vert_out._" << iface->instance_name << "_" << inout.name;
        }
        out << Sep() << iface->instance_name << "({" << args.str() << "\n  })";
      }
    }
  }
  {
    /* Entry point arguments. */
    auto &out = generated.entry_point_parameters;
    if (stage == ShaderStage::FRAGMENT) {
      std::string in_class_local = get_stage_in_class_name(stage);
      std::string in_class = get_stage_class_name(stage) + ("::" + in_class_local);
      std::string in_inst = get_stage_in_instance_name(stage);

      out << Sep() << const_qual << in_class << " " << in_inst << " [[stage_in]]";
    }
  }
  {
    /* Constructor arguments. */
    auto &out = generated.wrapper_instance_init;
    if (stage == ShaderStage::FRAGMENT) {
      out << Sep() << get_stage_in_instance_name(ShaderStage::FRAGMENT);
    }
    else {
      out << Sep() << get_stage_out_instance_name(ShaderStage::VERTEX);
    }
  }
}

static void generate_fragment_builtin(GeneratedStreams &ss,
                                      std::stringstream &declaration,
                                      const StringRefNull type,
                                      const StringRefNull var,
                                      const StringRefNull native_type,
                                      const StringRefNull attribute,
                                      const bool is_const = false)
{
  StringRefNull const_qual = is_const ? "const " : "";
  StringRefNull mem_scope = "thread ";
  /* Declaration inside builtin class. */
  declaration << "    " << native_type << " " << var << " " << attribute << ";\n";
  /* Global scope access. */
  ss.wrapper_class_members << "  " << mem_scope << const_qual << type << " &" << var << ";\n";
  /* Normally done for uint to int cast, which is safe in this case. */
  std::string cast = "*reinterpret_cast<thread " + type + "*>";
  ss.wrapper_constructor_assign << Sep() << var << "(" << cast << "(&mtl_frag_out." << var << "))";
}

static std::string generate_fragment_builtins(GeneratedStreams &ss, const ShaderCreateInfo &info)
{
  std::stringstream decl;

  if (info.depth_write_ != DepthWrite::UNCHANGED) {
    auto attr = [](DepthWrite depth_write) {
      switch (depth_write) {
        case DepthWrite::ANY:
          return "[[depth(any)]]";
        case DepthWrite::GREATER:
          return "[[depth(greater)]]";
        case DepthWrite::LESS:
          return "[[depth(less)]]";
        case DepthWrite::UNCHANGED:
          return "";
      }
    };
    generate_fragment_builtin(ss, decl, "float", "gl_FragDepth", "float", attr(info.depth_write_));
  }

  if (bool(info.builtins_ & BuiltinBits::STENCIL_REF)) {
    generate_fragment_builtin(ss, decl, "int", "gl_FragStencilRefARB", "uint", "[[stencil]]");
  }

  return decl.str();
}

static void generate_subpass_inputs(GeneratedStreams &generated, const ShaderCreateInfo &info)
{
  constexpr ShaderStage stage = ShaderStage::FRAGMENT;
  StringRefNull in_class_local = "SubpassInputs";
  std::string in_class = wrap_type(in_class_local, stage);

  if (info.subpass_inputs_.is_empty()) {
    return;
  }

  StringRefNull mem_scope = "thread ";

  {
    auto &out = generated.wrapper_class_members;

    /* References definition for global access. */
    for (const ShaderCreateInfo::SubpassIn &input : info.subpass_inputs_) {
      out << "  " << mem_scope << input.type << " &" << input.name << ";\n";
    }

    /* Main Block Definition. */
    out << "  struct " << in_class_local << " {\n";
    for (const ShaderCreateInfo::SubpassIn &input : info.subpass_inputs_) {
      out << "    " << input.type << " " << input.name;
      out << " [[color(" << input.index << ")]]";
      if (input.raster_order_group >= 0) {
        out << " [[raster_order_group(" << input.raster_order_group << ")]]";
      }
      out << ";\n";
    }
    out << "  };\n";
  }
  {
    /* Constructor parameters. */
    auto &out = generated.wrapper_constructor_parameters;
    out << Sep() << mem_scope << in_class_local << " &mtl_subpass_in";
  }
  {
    /* Constructor assignments. */
    auto &out = generated.wrapper_constructor_assign;
    for (const ShaderCreateInfo::SubpassIn &input : info.subpass_inputs_) {
      out << Sep() << input.name << "(mtl_subpass_in." << input.name << ")";
    }
  }
  {
    /* Constructor arguments. */
    auto &out = generated.wrapper_instance_init;
    out << Sep() << "mtl_subpass_in";
  }
  {
    /* Entry point arguments. */
    auto &out = generated.entry_point_parameters;
    out << Sep() << in_class << " mtl_subpass_in";
  }
}

static void generate_fragment_out(GeneratedStreams &generated, const ShaderCreateInfo &info)
{
  constexpr ShaderStage stage = ShaderStage::FRAGMENT;
  StringRefNull out_class_local = get_stage_out_class_name(stage, info);
  std::string out_class = get_stage_class_name(stage) + ("::" + out_class_local);

  std::string builtins_decl = generate_fragment_builtins(generated, info);

  if (info.fragment_outputs_.is_empty() && builtins_decl.empty()) {
    return;
  }

  StringRefNull mem_scope = "thread ";

  {
    auto &out = generated.wrapper_class_members;

    /* References definition for global access. */
    for (const ShaderCreateInfo::FragOut &output : info.fragment_outputs_) {
      out << "  " << mem_scope << output.type << " &" << output.name << ";\n";
    }

    /* Main Block Definition. */
    out << "  struct " << out_class_local << " {\n";
    if (builtins_decl.empty() == false) {
      out << builtins_decl;
    }
    for (const ShaderCreateInfo::FragOut &output : info.fragment_outputs_) {
      out << "    " << output.type << " " << output.name;
      out << " [[color(" << output.index << ")]]";
      if (output.blend != DualBlend::NONE) {
        out << " [[index(" << ((output.blend == DualBlend::SRC_0) ? 0 : 1) << ")]]";
      }
      if (output.raster_order_group >= 0) {
        out << " [[raster_order_group(" << output.raster_order_group << ")]]";
      }
      out << ";\n";
    }
    out << "  };\n";
  }
  {
    /* Constructor parameters. */
    auto &out = generated.wrapper_constructor_parameters;
    out << Sep() << mem_scope << out_class_local << " &mtl_frag_out";
  }
  {
    /* Constructor assignments. */
    auto &out = generated.wrapper_constructor_assign;
    for (const ShaderCreateInfo::FragOut &output : info.fragment_outputs_) {
      out << Sep() << output.name << "(mtl_frag_out." << output.name << ")";
    }
  }
  {
    /* Constructor arguments. */
    auto &out = generated.wrapper_instance_init;
    out << Sep() << get_stage_out_instance_name(stage);
  }
}

static void generate_vertex_interface(GeneratedStreams &generated, const ShaderCreateInfo &info)
{
  generate_vertex_attributes(generated, info);
  generate_vertex_out(generated, info, ShaderStage::VERTEX);
}

static void generate_fragment_interface(GeneratedStreams &generated, const ShaderCreateInfo &info)
{
  generate_subpass_inputs(generated, info);
  generate_vertex_out(generated, info, ShaderStage::FRAGMENT);
  generate_fragment_out(generated, info);
}

static void generate_stage_interfaces(GeneratedStreams &generated,
                                      const ShaderStage stage,
                                      const ShaderCreateInfo &info)
{
  if (stage == ShaderStage::VERTEX) {
    generate_vertex_interface(generated, info);
  }
  else if (stage == ShaderStage::FRAGMENT) {
    generate_fragment_interface(generated, info);
  }
}

static void generate_builtin(GeneratedStreams &ss,
                             const StringRefNull wrapper_type,
                             const StringRefNull wrapper_var,
                             const StringRefNull native_type,
                             const StringRefNull native_var)
{
  ss.wrapper_class_members << "  const " << wrapper_type << " " << wrapper_var << ";\n";
  ss.wrapper_constructor_parameters << Sep() << "const thread " << wrapper_type << " &"
                                    << wrapper_var;
  ss.wrapper_constructor_assign << Sep() << wrapper_var << "(" << wrapper_var << ")";
  ss.entry_point_parameters << Sep() << native_type << " " << wrapper_var << " " << native_var;
  ss.wrapper_instance_init << Sep() << wrapper_type << "(" << wrapper_var << ")";
}

static void generate_builtin(GeneratedStreams &generated,
                             const StringRefNull wrapper_type,
                             const StringRefNull wrapper_var,
                             const StringRefNull native_var)
{
  generate_builtin(generated, wrapper_type, wrapper_var, wrapper_type, native_var);
}

static void generate_instance_id(GeneratedStreams &ss)
{
  generate_builtin(ss, "int", "gpu_InstanceIndex", "uint", "[[instance_id]]");
  generate_builtin(ss, "int", "gpu_BaseInstance", "uint", "[[base_instance]]");
  /* MSL matches Vulkan semantic of gpu_InstanceIndex.
   * Thus we have to emulate gl_InstanceID support. */
  {
    auto &out = ss.wrapper_class_members;
    out << "  int gl_InstanceID;\n";
  }
  {
    auto &out = ss.wrapper_constructor_assign;
    out << Sep() << "gl_InstanceID(gpu_InstanceIndex - gpu_BaseInstance)";
  }
}

static void generate_builtins(GeneratedStreams &ss,
                              const ShaderStage stage,
                              const ShaderCreateInfo &info)
{
  if (stage == ShaderStage::VERTEX) {
    if (bool(info.builtins_ & BuiltinBits::VERTEX_ID)) {
      generate_builtin(ss, "int", "gl_VertexID", "uint", "[[vertex_id]]");
    }
    if (bool(info.builtins_ & BuiltinBits::INSTANCE_ID)) {
      generate_instance_id(ss);
    }
  }
  else if (stage == ShaderStage::FRAGMENT) {
    if (bool(info.builtins_ & BuiltinBits::FRONT_FACING)) {
      generate_builtin(ss, "bool", "gl_FrontFacing", "[[front_facing]]");
    }
    if (bool(info.builtins_ & BuiltinBits::PRIMITIVE_ID)) {
      generate_builtin(ss, "int", "gl_PrimitiveID", "uint", "[[primitive_id]]");
    }
    if (bool(info.builtins_ & BuiltinBits::POINT_COORD)) {
      generate_builtin(ss, "float2", "gl_PointCoord", "[[point_coord]]");
    }
    if (bool(info.builtins_ & BuiltinBits::BARYCENTRIC_COORD)) {
      generate_builtin(ss, "float3", "gpu_BaryCoord", "[[barycentric_coord]]");
    }
  }
  else if (stage == ShaderStage::COMPUTE) {
    /* Compute shader global variables. */
    if (bool(info.builtins_ & BuiltinBits::GLOBAL_INVOCATION_ID)) {
      generate_builtin(ss, "uint3", "gl_GlobalInvocationID", "[[thread_position_in_grid]]");
    }
    if (bool(info.builtins_ & BuiltinBits::WORK_GROUP_ID)) {
      generate_builtin(ss, "uint3", "gl_WorkGroupID", "[[threadgroup_position_in_grid]]");
    }
    if (bool(info.builtins_ & BuiltinBits::NUM_WORK_GROUP)) {
      generate_builtin(ss, "uint3", "gl_NumWorkGroups", "[[threadgroups_per_grid]]");
    }
    if (bool(info.builtins_ & BuiltinBits::LOCAL_INVOCATION_INDEX)) {
      generate_builtin(ss, "uint", "gl_LocalInvocationIndex", "[[thread_index_in_threadgroup]]");
    }
    if (bool(info.builtins_ & BuiltinBits::LOCAL_INVOCATION_ID)) {
      generate_builtin(ss, "uint3", "gl_LocalInvocationID", "[[thread_position_in_threadgroup]]");
    }
  }
  ss.wrapper_class_members << "\n";
}

std::pair<std::string, std::string> generate_entry_point(const ShaderCreateInfo &info,
                                                         const ShaderStage stage,
                                                         const StringRefNull entry_point_name)
{
  StringRefNull stage_out_class_name = get_stage_out_class_name(stage, info);
  StringRefNull stage_out_inst_name = get_stage_out_instance_name(stage);
  StringRefNull stage_class_name = get_stage_class_name(stage);
  StringRefNull stage_inst_name = get_stage_instance_name(stage);
  StringRefNull stage_type_name = get_stage_type(stage);

  std::string stage_out_class = "void";
  if (stage_out_class_name != "void") {
    stage_out_class = stage_class_name + "::" + stage_out_class_name;
  }

  GeneratedStreams generated;
  generate_builtins(generated, stage, info);
  generate_stage_interfaces(generated, stage, info);
  generate_resources(generated, stage, info);

  std::stringstream prefix;
  prefix << LINE;
  prefix << generated.wrapper_class_prefix.str() << "\n\n";
  prefix << "struct " << stage_class_name << " {\n";

  /* User generated code goes here. */

  std::stringstream out;
  out << "\n";
  /* Undefine macros that can conflict with attributes. We still need to keep other user macros in
   * case they are used inside resources declaration. */
  out << "#undef color\n";
  out << "#undef user\n";

  out << generated.wrapper_class_members.str();
  out << "\n";
  out << "  " << stage_class_name << "(";
  if (stage == ShaderStage::COMPUTE) {
    out << "MSL_SHARED_VARS_ARGS\n"; /* TODO(fclem): Replace by interface. */
  }
  out << generated.wrapper_constructor_parameters.str() << "\n";
  out << "  ) " << (generated.wrapper_constructor_assign.is_first_arg ? "" : ":");
  if (stage == ShaderStage::COMPUTE) {
    out << "MSL_SHARED_VARS_ASSIGN\n"; /* TODO(fclem): Replace by interface. */
  }
  out << generated.wrapper_constructor_assign.str();
  out << " {}\n";
  out << "};\n\n";

  /* Entry point attribute. */
  if (info.early_fragment_test_ && stage == ShaderStage::FRAGMENT) {
    out << LINE << "[[early_fragment_tests]]";
  }

  /* Entry point signature. */
  out << LINE << stage_type_name << " " << stage_out_class << " " << entry_point_name;

  out << LINE;
  out << "(";
  out << generated.entry_point_parameters.str() << "\n";
  out << ")\n";

  /* Entry point body. */
  out << "{\n";
  {
    if (stage_out_class != "void") {
      out << LINE;
      out << "  " << stage_out_class << " " << stage_out_inst_name << ";";
    }

    out << LINE;
    out << generated.entry_point_start.str();

    if (stage == ShaderStage::COMPUTE) {
      out << "MSL_SHARED_VARS_DECLARE\n"; /* TODO(fclem): Replace by interface. */
    }

    out << LINE;
    out << "  " << stage_class_name << " " << stage_inst_name;
    out << "  {";
    if (stage == ShaderStage::COMPUTE) {
      out << "MSL_SHARED_VARS_PASS\n"; /* TODO(fclem): Replace by interface. */
    }
    out << generated.wrapper_instance_init.str() << "\n";
    out << "  };\n\n";

    out << LINE;
    out << "  " << stage_inst_name << ".main();\n";

    if (stage == ShaderStage::VERTEX) {
      /* For historical reasons vertex shader output is expected to be in OpenGL NDC coordinates:
       * Z in [-1..+1] and Y up. */
      std::string pos = stage_out_inst_name + ".gl_Position";
      /* Flip Y. */
      out << LINE << "  " << pos << ".y = -" << pos << ".y;\n";
      /* Remap Z from [-1..+1] to [0..1]. */
      out << LINE << "  " << pos << ".z = (" << pos << ".z + " << pos << ".w) / 2.0;\n";
    }

    if (stage_out_class != "void") {
      out << LINE << "  return " << stage_out_inst_name << ";\n";
    }
  }
  out << "}\n";

  return {prefix.str(), out.str()};
}

/* Return available buffer slots for vertex buffer bindings. */
uint32_t available_buffer_slots(const ShaderCreateInfo &info)
{
  uint32_t free_slots = ~((~0u) << 31u);

  auto occupy_slot = [&](const ShaderCreateInfo::Resource &res) {
    switch (res.bind_type) {
      case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
        free_slots &= ~(1u << (MTL_UBO_SLOT_OFFSET + res.slot));
        break;
      case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
        free_slots &= ~(1u << (MTL_SSBO_SLOT_OFFSET + res.slot));
        break;
      case ShaderCreateInfo::Resource::BindType::SAMPLER:
      case ShaderCreateInfo::Resource::BindType::IMAGE:
        break;
    };
  };

  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    occupy_slot(res);
  }
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    occupy_slot(res);
  }
  for (const ShaderCreateInfo::Resource &res : info.geometry_resources_) {
    occupy_slot(res);
  }

  if (info.push_constants_.is_empty() == false) {
    free_slots &= ~(1u << MTL_PUSH_CONSTANT_BUFFER_SLOT);
  }

  if (bool(info.builtins_ & BuiltinBits::USE_SAMPLER_ARG_BUFFER)) {
    free_slots &= ~(1u << MTL_SAMPLER_ARGUMENT_BUFFER_SLOT);
  }

  return free_slots;
}

void patch_create_info_atomic_workaround(std::unique_ptr<PatchedShaderCreateInfo> &patched_info,
                                         const shader::ShaderCreateInfo &original_info)
{
  uint32_t free_slots = 0;
  auto ensure_atomic_workaround = [&](ImageType type, StringRefNull name) {
    if (!ELEM(type,
              ImageType::AtomicUint2D,
              ImageType::AtomicUint2DArray,
              ImageType::AtomicUint3D,
              ImageType::AtomicInt2D,
              ImageType::AtomicInt2DArray,
              ImageType::AtomicInt3D))
    {
      return;
    }

    if (patched_info == nullptr) {
      patched_info = std::make_unique<PatchedShaderCreateInfo>(original_info);
      free_slots = available_buffer_slots(original_info);
    }
    int slot = bitscan_forward_clear_uint(&free_slots);
    patched_info->names.append(std::make_unique<std::string>(name + "_buf_[]"));
    patched_info->info.storage_buf(
        slot, Qualifier::read_write, to_component_type(type), *patched_info->names.last());
    patched_info->names.append(std::make_unique<std::string>(name + "_metadata_"));
    patched_info->info.push_constant(Type::uint4_t, *patched_info->names.last());
  };

  auto ensure_atomic_workaround_resource = [&](const ShaderCreateInfo::Resource &res) {
    switch (res.bind_type) {
      case ShaderCreateInfo::Resource::BindType::SAMPLER:
        ensure_atomic_workaround(res.sampler.type, res.sampler.name);
        break;
      case ShaderCreateInfo::Resource::BindType::IMAGE:
        ensure_atomic_workaround(res.image.type, res.image.name);
        break;
      case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
        break;
    }
  };

  for (const ShaderCreateInfo::Resource &res : original_info.pass_resources_) {
    ensure_atomic_workaround_resource(res);
  }
  for (const ShaderCreateInfo::Resource &res : original_info.batch_resources_) {
    ensure_atomic_workaround_resource(res);
  }
  for (const ShaderCreateInfo::Resource &res : original_info.geometry_resources_) {
    ensure_atomic_workaround_resource(res);
  }
}

}  // namespace blender::gpu
