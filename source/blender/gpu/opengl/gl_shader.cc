/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <iomanip>

#include "BKE_appdir.hh"
#include "BKE_global.hh"

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_time.h"
#include "BLI_vector.hh"

#include "BLI_system.h"
#include BLI_SYSTEM_PID_H

#include "GPU_capabilities.hh"
#include "GPU_debug.hh"
#include "GPU_platform.hh"
#include "gpu_capabilities_private.hh"
#include "gpu_shader_dependency_private.hh"

#include "gl_debug.hh"
#include "gl_vertex_buffer.hh"

#include "gl_compilation_subprocess.hh"
#include "gl_shader.hh"
#include "gl_shader_interface.hh"

#include <sstream>
#include <stdio.h>

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string>

#ifdef WIN32
#  define popen _popen
#  define pclose _pclose
#endif

using namespace blender;
using namespace blender::gpu;
using namespace blender::gpu::shader;

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

GLShader::GLShader(const char *name) : Shader(name)
{
#if 0 /* Would be nice to have, but for now the Deferred compilation \
       * does not have a GPUContext. */
  BLI_assert(GLContext::get() != nullptr);
#endif
}

GLShader::~GLShader()
{
#if 0 /* Would be nice to have, but for now the Deferred compilation \
       * does not have a GPUContext. */
  BLI_assert(GLContext::get() != nullptr);
#endif
}

void GLShader::init(const shader::ShaderCreateInfo &info, bool is_batch_compilation)
{
  async_compilation_ = is_batch_compilation;

  /* Extract the constants names from info and store them locally. */
  for (const SpecializationConstant &constant : info.specialization_constants_) {
    specialization_constant_names_.append(constant.name.c_str());
  }

  /* NOTE: This is not threadsafe with regards to the specialization constants state access.
   * The shader creation must be externally synchronized. */
  main_program_ = program_cache_
                      .lookup_or_add_cb(constants->values,
                                        []() { return std::make_unique<GLProgram>(); })
                      .get();
  if (!main_program_->program_id) {
    main_program_->program_id = glCreateProgram();
    debug::object_label(GL_PROGRAM, main_program_->program_id, name);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Create Info
 * \{ */

static const char *to_string(const Interpolation &interp)
{
  switch (interp) {
    case Interpolation::SMOOTH:
      return "smooth";
    case Interpolation::FLAT:
      return "flat";
    case Interpolation::NO_PERSPECTIVE:
      return "noperspective";
    default:
      return "unknown";
  }
}

static const char *to_string(const Type &type)
{
  switch (type) {
    case Type::float_t:
      return "float";
    case Type::float2_t:
      return "vec2";
    case Type::float3_t:
      return "vec3";
    case Type::float4_t:
      return "vec4";
    case Type::float3x3_t:
      return "mat3";
    case Type::float4x4_t:
      return "mat4";
    case Type::uint_t:
      return "uint";
    case Type::uint2_t:
      return "uvec2";
    case Type::uint3_t:
      return "uvec3";
    case Type::uint4_t:
      return "uvec4";
    case Type::int_t:
      return "int";
    case Type::int2_t:
      return "ivec2";
    case Type::int3_t:
      return "ivec3";
    case Type::int4_t:
      return "ivec4";
    case Type::bool_t:
      return "bool";
    /* Alias special types. */
    case Type::uchar_t:
    case Type::ushort_t:
      return "uint";
    case Type::uchar2_t:
    case Type::ushort2_t:
      return "uvec2";
    case Type::uchar3_t:
    case Type::ushort3_t:
      return "uvec3";
    case Type::uchar4_t:
    case Type::ushort4_t:
      return "uvec4";
    case Type::char_t:
    case Type::short_t:
      return "int";
    case Type::char2_t:
    case Type::short2_t:
      return "ivec2";
    case Type::char3_t:
    case Type::short3_t:
      return "ivec3";
    case Type::char4_t:
    case Type::short4_t:
      return "ivec4";
    case Type::float3_10_10_10_2_t:
      return "vec3";
  }
  BLI_assert_unreachable();
  return "unknown";
}

static Type UNUSED_FUNCTION(to_component_type)(const Type &type)
{
  switch (type) {
    case Type::float_t:
    case Type::float2_t:
    case Type::float3_t:
    case Type::float4_t:
    case Type::float3x3_t:
    case Type::float4x4_t:
      return Type::float_t;
    case Type::uint_t:
    case Type::uint2_t:
    case Type::uint3_t:
    case Type::uint4_t:
      return Type::uint_t;
    case Type::int_t:
    case Type::int2_t:
    case Type::int3_t:
    case Type::int4_t:
    case Type::bool_t:
      return Type::int_t;
    /* Alias special types. */
    case Type::uchar_t:
    case Type::uchar2_t:
    case Type::uchar3_t:
    case Type::uchar4_t:
    case Type::ushort_t:
    case Type::ushort2_t:
    case Type::ushort3_t:
    case Type::ushort4_t:
      return Type::uint_t;
    case Type::char_t:
    case Type::char2_t:
    case Type::char3_t:
    case Type::char4_t:
    case Type::short_t:
    case Type::short2_t:
    case Type::short3_t:
    case Type::short4_t:
      return Type::int_t;
    case Type::float3_10_10_10_2_t:
      return Type::float_t;
  }
  BLI_assert_unreachable();
  return Type::float_t;
}

static const char *to_string(const TextureFormat &type)
{
  switch (type) {
    case TextureFormat::UINT_8_8_8_8:
      return "rgba8ui";
    case TextureFormat::SINT_8_8_8_8:
      return "rgba8i";
    case TextureFormat::UNORM_8_8_8_8:
      return "rgba8";
    case TextureFormat::UINT_32_32_32_32:
      return "rgba32ui";
    case TextureFormat::SINT_32_32_32_32:
      return "rgba32i";
    case TextureFormat::SFLOAT_32_32_32_32:
      return "rgba32f";
    case TextureFormat::UINT_16_16_16_16:
      return "rgba16ui";
    case TextureFormat::SINT_16_16_16_16:
      return "rgba16i";
    case TextureFormat::SFLOAT_16_16_16_16:
      return "rgba16f";
    case TextureFormat::UNORM_16_16_16_16:
      return "rgba16";
    case TextureFormat::UINT_8_8:
      return "rg8ui";
    case TextureFormat::SINT_8_8:
      return "rg8i";
    case TextureFormat::UNORM_8_8:
      return "rg8";
    case TextureFormat::UINT_32_32:
      return "rg32ui";
    case TextureFormat::SINT_32_32:
      return "rg32i";
    case TextureFormat::SFLOAT_32_32:
      return "rg32f";
    case TextureFormat::UINT_16_16:
      return "rg16ui";
    case TextureFormat::SINT_16_16:
      return "rg16i";
    case TextureFormat::SFLOAT_16_16:
      return "rg16f";
    case TextureFormat::UNORM_16_16:
      return "rg16";
    case TextureFormat::UINT_8:
      return "r8ui";
    case TextureFormat::SINT_8:
      return "r8i";
    case TextureFormat::UNORM_8:
      return "r8";
    case TextureFormat::UINT_32:
      return "r32ui";
    case TextureFormat::SINT_32:
      return "r32i";
    case TextureFormat::SFLOAT_32:
      return "r32f";
    case TextureFormat::UINT_16:
      return "r16ui";
    case TextureFormat::SINT_16:
      return "r16i";
    case TextureFormat::SFLOAT_16:
      return "r16f";
    case TextureFormat::UNORM_16:
      return "r16";
    case TextureFormat::UFLOAT_11_11_10:
      return "r11f_g11f_b10f";
    case TextureFormat::UNORM_10_10_10_2:
      return "rgb10_a2";
    default:
      return "unknown";
  }
}

static const char *to_string(const PrimitiveIn &layout)
{
  switch (layout) {
    case PrimitiveIn::POINTS:
      return "points";
    case PrimitiveIn::LINES:
      return "lines";
    case PrimitiveIn::LINES_ADJACENCY:
      return "lines_adjacency";
    case PrimitiveIn::TRIANGLES:
      return "triangles";
    case PrimitiveIn::TRIANGLES_ADJACENCY:
      return "triangles_adjacency";
    default:
      return "unknown";
  }
}

static const char *to_string(const PrimitiveOut &layout)
{
  switch (layout) {
    case PrimitiveOut::POINTS:
      return "points";
    case PrimitiveOut::LINE_STRIP:
      return "line_strip";
    case PrimitiveOut::TRIANGLE_STRIP:
      return "triangle_strip";
    default:
      return "unknown";
  }
}

static const char *to_string(const DepthWrite &value)
{
  switch (value) {
    case DepthWrite::ANY:
      return "depth_any";
    case DepthWrite::GREATER:
      return "depth_greater";
    case DepthWrite::LESS:
      return "depth_less";
    default:
      return "depth_unchanged";
  }
}

static void print_image_type(std::ostream &os,
                             const ImageType &type,
                             const ShaderCreateInfo::Resource::BindType bind_type)
{
  switch (type) {
    case ImageType::IntBuffer:
    case ImageType::Int1D:
    case ImageType::Int1DArray:
    case ImageType::Int2D:
    case ImageType::Int2DArray:
    case ImageType::Int3D:
    case ImageType::IntCube:
    case ImageType::IntCubeArray:
    case ImageType::AtomicInt2D:
    case ImageType::AtomicInt2DArray:
    case ImageType::AtomicInt3D:
      os << "i";
      break;
    case ImageType::UintBuffer:
    case ImageType::Uint1D:
    case ImageType::Uint1DArray:
    case ImageType::Uint2D:
    case ImageType::Uint2DArray:
    case ImageType::Uint3D:
    case ImageType::UintCube:
    case ImageType::UintCubeArray:
    case ImageType::AtomicUint2D:
    case ImageType::AtomicUint2DArray:
    case ImageType::AtomicUint3D:
      os << "u";
      break;
    default:
      break;
  }

  if (bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
    os << "image";
  }
  else {
    os << "sampler";
  }

  switch (type) {
    case ImageType::FloatBuffer:
    case ImageType::IntBuffer:
    case ImageType::UintBuffer:
      os << "Buffer";
      break;
    case ImageType::Float1D:
    case ImageType::Float1DArray:
    case ImageType::Int1D:
    case ImageType::Int1DArray:
    case ImageType::Uint1D:
    case ImageType::Uint1DArray:
      os << "1D";
      break;
    case ImageType::Float2D:
    case ImageType::Float2DArray:
    case ImageType::Int2D:
    case ImageType::Int2DArray:
    case ImageType::AtomicInt2D:
    case ImageType::AtomicInt2DArray:
    case ImageType::Uint2D:
    case ImageType::Uint2DArray:
    case ImageType::AtomicUint2D:
    case ImageType::AtomicUint2DArray:
    case ImageType::Shadow2D:
    case ImageType::Shadow2DArray:
    case ImageType::Depth2D:
    case ImageType::Depth2DArray:
      os << "2D";
      break;
    case ImageType::Float3D:
    case ImageType::Int3D:
    case ImageType::Uint3D:
    case ImageType::AtomicInt3D:
    case ImageType::AtomicUint3D:
      os << "3D";
      break;
    case ImageType::FloatCube:
    case ImageType::FloatCubeArray:
    case ImageType::IntCube:
    case ImageType::IntCubeArray:
    case ImageType::UintCube:
    case ImageType::UintCubeArray:
    case ImageType::ShadowCube:
    case ImageType::ShadowCubeArray:
    case ImageType::DepthCube:
    case ImageType::DepthCubeArray:
      os << "Cube";
      break;
    default:
      break;
  }

  switch (type) {
    case ImageType::Float1DArray:
    case ImageType::Float2DArray:
    case ImageType::FloatCubeArray:
    case ImageType::Int1DArray:
    case ImageType::Int2DArray:
    case ImageType::IntCubeArray:
    case ImageType::Uint1DArray:
    case ImageType::Uint2DArray:
    case ImageType::AtomicUint2DArray:
    case ImageType::UintCubeArray:
    case ImageType::Shadow2DArray:
    case ImageType::ShadowCubeArray:
    case ImageType::Depth2DArray:
    case ImageType::DepthCubeArray:
      os << "Array";
      break;
    default:
      break;
  }

  switch (type) {
    case ImageType::Shadow2D:
    case ImageType::Shadow2DArray:
    case ImageType::ShadowCube:
    case ImageType::ShadowCubeArray:
      os << "Shadow";
      break;
    default:
      break;
  }
  os << " ";
}

static std::ostream &print_qualifier(std::ostream &os, const Qualifier &qualifiers)
{
  if (!flag_is_set(qualifiers, Qualifier::no_restrict)) {
    os << "restrict ";
  }
  if (!flag_is_set(qualifiers, Qualifier::read)) {
    os << "writeonly ";
  }
  if (!flag_is_set(qualifiers, Qualifier::write)) {
    os << "readonly ";
  }
  return os;
}

static void print_resource(std::ostream &os,
                           const ShaderCreateInfo::Resource &res,
                           bool auto_resource_location)
{
  if (auto_resource_location && res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
    /* Skip explicit binding location for samplers when not needed, since drivers can usually
     * handle more sampler declarations this way (as long as they're not actually used by the
     * shader). See #105661. */
  }
  else if (GLContext::explicit_location_support) {
    os << "layout(binding = " << res.slot;
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
      os << ", " << to_string(res.image.format);
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
      os << ", std140";
    }
    else if (res.bind_type == ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER) {
      os << ", std430";
    }
    os << ") ";
  }
  else if (res.bind_type == ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER) {
    os << "layout(std140) ";
  }

  int64_t array_offset;
  StringRef name_no_array;

  switch (res.bind_type) {
    case ShaderCreateInfo::Resource::BindType::SAMPLER:
      os << "uniform ";
      print_image_type(os, res.sampler.type, res.bind_type);
      os << res.sampler.name << ";\n";
      break;
    case ShaderCreateInfo::Resource::BindType::IMAGE:
      os << "uniform ";
      print_qualifier(os, res.image.qualifiers);
      print_image_type(os, res.image.type, res.bind_type);
      os << res.image.name << ";\n";
      break;
    case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      array_offset = res.uniformbuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.uniformbuf.name :
                                             StringRef(res.uniformbuf.name.c_str(), array_offset);
      os << "uniform " << name_no_array << " { " << res.uniformbuf.type_name << " _"
         << res.uniformbuf.name << "; };\n";
      break;
    case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      array_offset = res.storagebuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.storagebuf.name :
                                             StringRef(res.storagebuf.name.c_str(), array_offset);
      print_qualifier(os, res.storagebuf.qualifiers);
      os << "buffer ";
      os << name_no_array << " { " << res.storagebuf.type_name << " _" << res.storagebuf.name
         << "; };\n";
      break;
  }
}

static void print_resource_alias(std::ostream &os, const ShaderCreateInfo::Resource &res)
{
  int64_t array_offset;
  StringRef name_no_array;

  switch (res.bind_type) {
    case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      array_offset = res.uniformbuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.uniformbuf.name :
                                             StringRef(res.uniformbuf.name.c_str(), array_offset);
      os << "#define " << name_no_array << " (_" << name_no_array << ")\n";
      break;
    case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      array_offset = res.storagebuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.storagebuf.name :
                                             StringRef(res.storagebuf.name.c_str(), array_offset);
      os << "#define " << name_no_array << " (_" << name_no_array << ")\n";
      break;
    default:
      break;
  }
}

static void print_interface(std::ostream &os,
                            const StringRefNull &prefix,
                            const StageInterfaceInfo &iface,
                            const StringRefNull &suffix = "")
{
  /* TODO(@fclem): Move that to interface check. */
  // if (iface.instance_name.is_empty()) {
  //   BLI_assert_msg(0, "Interfaces require an instance name for geometry shader.");
  //   std::cout << iface.name << ": Interfaces require an instance name for geometry shader.\n";
  //   continue;
  // }
  os << prefix << " " << iface.name << "{" << std::endl;
  for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
    os << "  " << to_string(inout.interp) << " " << to_string(inout.type) << " " << inout.name
       << ";\n";
  }
  os << "}";
  os << (iface.instance_name.is_empty() ? "" : "\n") << iface.instance_name << suffix << ";\n";
}

std::string GLShader::resources_declare(const ShaderCreateInfo &info) const
{
  std::stringstream ss;

  ss << "\n/* Compilation Constants (pass-through). */\n";
  for (const CompilationConstant &sc : info.compilation_constants_) {
    ss << "const ";
    switch (sc.type) {
      case Type::int_t:
        ss << "int " << sc.name << "=" << std::to_string(sc.value.i) << ";\n";
        break;
      case Type::uint_t:
        ss << "uint " << sc.name << "=" << std::to_string(sc.value.u) << "u;\n";
        break;
      case Type::bool_t:
        ss << "bool " << sc.name << "=" << (sc.value.u ? "true" : "false") << ";\n";
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  }
  ss << "\n/* Shared Variables. */\n";
  for (const ShaderCreateInfo::SharedVariable &sv : info.shared_variables_) {
    ss << "shared " << to_string(sv.type) << " " << sv.name << ";\n";
  }
  /* NOTE: We define macros in GLSL to trigger compilation error if the resource names
   * are reused for local variables. This is to match other backend behavior which needs accessors
   * macros. */
  ss << "\n/* Pass Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource(ss, res, info.auto_resource_location_);
  }
  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource_alias(ss, res);
  }
  ss << "\n/* Batch Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource(ss, res, info.auto_resource_location_);
  }
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource_alias(ss, res);
  }
  ss << "\n/* Geometry Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.geometry_resources_) {
    print_resource(ss, res, info.auto_resource_location_);
  }
  for (const ShaderCreateInfo::Resource &res : info.geometry_resources_) {
    print_resource_alias(ss, res);
  }
  ss << "\n/* Push Constants. */\n";
  int location = 0;
  for (const ShaderCreateInfo::PushConst &uniform : info.push_constants_) {
    /* See #131227: Work around legacy Intel bug when using layout locations. */
    if (!info.specialization_constants_.is_empty()) {
      ss << "layout(location = " << location << ") ";
      location += std::max(1, uniform.array_size);
    }
    ss << "uniform " << to_string(uniform.type) << " " << uniform.name;
    if (uniform.array_size > 0) {
      ss << "[" << uniform.array_size << "]";
    }
    ss << ";\n";
  }
#if 0 /* #95278: This is not be enough to prevent some compilers think it is recursive. */
  for (const ShaderCreateInfo::PushConst &uniform : info.push_constants_) {
    /* #95278: Double macro to avoid some compilers think it is recursive. */
    ss << "#define " << uniform.name << "_ " << uniform.name << "\n";
    ss << "#define " << uniform.name << " (" << uniform.name << "_)\n";
  }
#endif
  ss << "\n";
  return ss.str();
}

std::string GLShader::constants_declare(
    const shader::SpecializationConstants &constants_state) const
{
  std::stringstream ss;

  ss << "/* Specialization Constants. */\n";
  for (int constant_index : IndexRange(constants_state.types.size())) {
    const StringRefNull name = specialization_constant_names_[constant_index];
    gpu::shader::Type constant_type = constants_state.types[constant_index];
    const SpecializationConstant::Value &value = constants_state.values[constant_index];

    switch (constant_type) {
      case Type::int_t:
        ss << "const int " << name << "=" << std::to_string(value.i) << ";\n";
        break;
      case Type::uint_t:
        ss << "const uint " << name << "=" << std::to_string(value.u) << "u;\n";
        break;
      case Type::bool_t:
        ss << "const bool " << name << "=" << (value.u ? "true" : "false") << ";\n";
        break;
      case Type::float_t:
        /* Use uint representation to allow exact same bit pattern even if NaN. */
        ss << "const float " << name << "= uintBitsToFloat(" << std::to_string(value.u) << "u);\n";
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  }
  return ss.str();
}

static std::string main_function_wrapper(std::string &pre_main, std::string &post_main)
{
  std::stringstream ss;
  /* Prototype for the original main. */
  ss << "\n";
  ss << "void main_function_();\n";
  /* Wrapper to the main function in order to inject code processing on globals. */
  ss << "void main() {\n";
  ss << pre_main;
  ss << "  main_function_();\n";
  ss << post_main;
  ss << "}\n";
  /* Rename the original main. */
  ss << "#define main main_function_\n";
  ss << "\n";
  return ss.str();
}

std::string GLShader::vertex_interface_declare(const ShaderCreateInfo &info) const
{
  std::stringstream ss;
  std::string post_main;

  ss << "\n/* Inputs. */\n";
  for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
    if (GLContext::explicit_location_support &&
        /* Fix issue with AMDGPU-PRO + workbench_prepass_mesh_vert.glsl being quantized. */
        GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) == false)
    {
      ss << "layout(location = " << attr.index << ") ";
    }
    ss << "in " << to_string(attr.type) << " " << attr.name << ";\n";
  }
  ss << "\n/* Interfaces. */\n";
  for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
    print_interface(ss, "out", *iface);
  }
  const bool has_geometry_stage = do_geometry_shader_injection(&info) ||
                                  !info.geometry_source_.is_empty();
  const bool do_layer_output = flag_is_set(info.builtins_, BuiltinBits::LAYER);
  const bool do_viewport_output = flag_is_set(info.builtins_, BuiltinBits::VIEWPORT_INDEX);
  if (has_geometry_stage) {
    if (do_layer_output) {
      ss << "out int gpu_Layer;\n";
    }
    if (do_viewport_output) {
      ss << "out int gpu_ViewportIndex;\n";
    }
  }
  else {
    if (do_layer_output) {
      ss << "#define gpu_Layer gl_Layer\n";
    }
    if (do_viewport_output) {
      ss << "#define gpu_ViewportIndex gl_ViewportIndex\n";
    }
  }
  if (flag_is_set(info.builtins_, BuiltinBits::CLIP_CONTROL)) {
    if (!has_geometry_stage) {
      /* Assume clip range is set to 0..1 and remap the range just like Vulkan and Metal.
       * If geometry stage is needed, do that remapping inside the geometry shader stage. */
      post_main += "gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n";
    }
  }
  if (flag_is_set(info.builtins_, BuiltinBits::BARYCENTRIC_COORD)) {
    if (!GLContext::native_barycentric_support) {
      /* Disabled or unsupported. */
    }
    else if (epoxy_has_gl_extension("GL_AMD_shader_explicit_vertex_parameter")) {
      /* Need this for stable barycentric. */
      ss << "flat out vec4 gpu_pos_flat;\n";
      ss << "out vec4 gpu_pos;\n";

      post_main += "  gpu_pos = gpu_pos_flat = gl_Position;\n";
    }
  }
  ss << "\n";

  if (post_main.empty() == false) {
    std::string pre_main;
    ss << main_function_wrapper(pre_main, post_main);
  }
  return ss.str();
}

std::string GLShader::fragment_interface_declare(const ShaderCreateInfo &info) const
{
  std::stringstream ss;
  std::string pre_main, post_main;

  ss << "\n/* Interfaces. */\n";
  const Span<StageInterfaceInfo *> in_interfaces = info.geometry_source_.is_empty() ?
                                                       info.vertex_out_interfaces_ :
                                                       info.geometry_out_interfaces_;
  for (const StageInterfaceInfo *iface : in_interfaces) {
    print_interface(ss, "in", *iface);
  }
  if (flag_is_set(info.builtins_, BuiltinBits::LAYER)) {
    ss << "#define gpu_Layer gl_Layer\n";
  }
  if (flag_is_set(info.builtins_, BuiltinBits::VIEWPORT_INDEX)) {
    ss << "#define gpu_ViewportIndex gl_ViewportIndex\n";
  }
  if (flag_is_set(info.builtins_, BuiltinBits::BARYCENTRIC_COORD)) {
    if (!GLContext::native_barycentric_support) {
      ss << "flat in vec4 gpu_pos[3];\n";
      ss << "smooth in vec3 gpu_BaryCoord;\n";
      ss << "noperspective in vec3 gpu_BaryCoordNoPersp;\n";
    }
    else if (epoxy_has_gl_extension("GL_AMD_shader_explicit_vertex_parameter")) {
      /* NOTE(fclem): This won't work with geometry shader. Hopefully, we don't need geometry
       * shader workaround if this extension/feature is detected. */
      ss << "\n/* Stable Barycentric Coordinates. */\n";
      ss << "flat in vec4 gpu_pos_flat;\n";
      ss << "__explicitInterpAMD in vec4 gpu_pos;\n";
      /* Globals. */
      ss << "vec3 gpu_BaryCoord;\n";
      ss << "vec3 gpu_BaryCoordNoPersp;\n";
      ss << "\n";
      ss << "vec2 stable_bary_(vec2 in_bary) {\n";
      ss << "  vec3 bary = vec3(in_bary, 1.0 - in_bary.x - in_bary.y);\n";
      ss << "  if (interpolateAtVertexAMD(gpu_pos, 0) == gpu_pos_flat) { return bary.zxy; }\n";
      ss << "  if (interpolateAtVertexAMD(gpu_pos, 2) == gpu_pos_flat) { return bary.yzx; }\n";
      ss << "  return bary.xyz;\n";
      ss << "}\n";
      ss << "\n";

      pre_main += "  gpu_BaryCoord = stable_bary_(gl_BaryCoordSmoothAMD);\n";
      pre_main += "  gpu_BaryCoordNoPersp = stable_bary_(gl_BaryCoordNoPerspAMD);\n";
    }
  }
  if (info.early_fragment_test_) {
    ss << "layout(early_fragment_tests) in;\n";
  }
  ss << "layout(" << to_string(info.depth_write_) << ") out float gl_FragDepth;\n";

  ss << "\n/* Sub-pass Inputs. */\n";
  for (const ShaderCreateInfo::SubpassIn &input : info.subpass_inputs_) {
    if (GLContext::framebuffer_fetch_support) {
      /* Declare as inout but do not write to it. */
      ss << "layout(location = " << std::to_string(input.index) << ") inout "
         << to_string(input.type) << " " << input.name << ";\n";
    }
    else {
      std::string image_name = "gpu_subpass_img_";
      image_name += std::to_string(input.index);

      /* Declare global for input. */
      ss << to_string(input.type) << " " << input.name << ";\n";

      /* IMPORTANT: We assume that the frame-buffer will be layered or not based on the layer
       * built-in flag. */
      bool is_layered_fb = flag_is_set(info.builtins_, BuiltinBits::LAYER);
      bool is_layered_input = ELEM(
          input.img_type, ImageType::Uint2DArray, ImageType::Int2DArray, ImageType::Float2DArray);

      /* Declare image. */
      using Resource = ShaderCreateInfo::Resource;
      /* NOTE(fclem): Using the attachment index as resource index might be problematic as it might
       * collide with other resources. */
      Resource res(Resource::BindType::SAMPLER, input.index);
      res.sampler.type = input.img_type;
      res.sampler.sampler = GPUSamplerState::default_sampler();
      res.sampler.name = image_name;
      print_resource(ss, res, false);

      char swizzle[] = "xyzw";
      swizzle[to_component_count(input.type)] = '\0';

      std::string texel_co = (is_layered_input) ?
                                 ((is_layered_fb)  ? "ivec3(gl_FragCoord.xy, gpu_Layer)" :
                                                     /* This should fetch the attached layer.
                                                      * But this is not simple to set. For now
                                                      * assume it is always the first layer. */
                                                     "ivec3(gl_FragCoord.xy, 0)") :
                                 "ivec2(gl_FragCoord.xy)";

      std::stringstream ss_pre;
      /* Populate the global before main using imageLoad. */
      ss_pre << "  " << input.name << " = texelFetch(" << image_name << ", " << texel_co << ", 0)."
             << swizzle << ";\n";

      pre_main += ss_pre.str();
    }
  }
  ss << "\n/* Outputs. */\n";
  for (const ShaderCreateInfo::FragOut &output : info.fragment_outputs_) {
    ss << "layout(location = " << output.index;
    switch (output.blend) {
      case DualBlend::SRC_0:
        ss << ", index = 0";
        break;
      case DualBlend::SRC_1:
        ss << ", index = 1";
        break;
      default:
        break;
    }
    ss << ") ";
    ss << "out " << to_string(output.type) << " " << output.name << ";\n";
  }
  ss << "\n";

  if (!pre_main.empty() || !post_main.empty()) {
    ss << main_function_wrapper(pre_main, post_main);
  }
  return ss.str();
}

std::string GLShader::geometry_layout_declare(const ShaderCreateInfo &info) const
{
  int max_verts = info.geometry_layout_.max_vertices;
  int invocations = info.geometry_layout_.invocations;

  std::stringstream ss;
  ss << "\n/* Geometry Layout. */\n";
  ss << "layout(" << to_string(info.geometry_layout_.primitive_in);
  if (invocations != -1) {
    ss << ", invocations = " << invocations;
  }
  ss << ") in;\n";

  ss << "layout(" << to_string(info.geometry_layout_.primitive_out)
     << ", max_vertices = " << max_verts << ") out;\n";
  ss << "\n";
  return ss.str();
}

static StageInterfaceInfo *find_interface_by_name(const Span<StageInterfaceInfo *> ifaces,
                                                  const StringRefNull &name)
{
  for (auto *iface : ifaces) {
    if (iface->instance_name == name) {
      return iface;
    }
  }
  return nullptr;
}

std::string GLShader::geometry_interface_declare(const ShaderCreateInfo &info) const
{
  std::stringstream ss;

  ss << "\n/* Interfaces. */\n";
  for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
    bool has_matching_output_iface = find_interface_by_name(info.geometry_out_interfaces_,
                                                            iface->instance_name) != nullptr;
    const char *suffix = (has_matching_output_iface) ? "_in[]" : "[]";
    print_interface(ss, "in", *iface, suffix);
  }
  ss << "\n";
  for (const StageInterfaceInfo *iface : info.geometry_out_interfaces_) {
    bool has_matching_input_iface = find_interface_by_name(info.vertex_out_interfaces_,
                                                           iface->instance_name) != nullptr;
    const char *suffix = (has_matching_input_iface) ? "_out" : "";
    print_interface(ss, "out", *iface, suffix);
  }
  ss << "\n";
  return ss.str();
}

std::string GLShader::compute_layout_declare(const ShaderCreateInfo &info) const
{
  std::stringstream ss;
  ss << "\n/* Compute Layout. */\n";
  ss << "layout(";
  ss << "  local_size_x = " << info.compute_layout_.local_size_x;
  ss << ", local_size_y = " << info.compute_layout_.local_size_y;
  ss << ", local_size_z = " << info.compute_layout_.local_size_z;
  ss << ") in;\n";
  ss << "\n";
  return ss.str();
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Passthrough geometry shader emulation
 *
 * \{ */

std::string GLShader::workaround_geometry_shader_source_create(
    const shader::ShaderCreateInfo &info)
{
  std::stringstream ss;

  const bool do_layer_output = flag_is_set(info.builtins_, BuiltinBits::LAYER);
  const bool do_viewport_output = flag_is_set(info.builtins_, BuiltinBits::VIEWPORT_INDEX);
  const bool do_barycentric_workaround = !GLContext::native_barycentric_support &&
                                         flag_is_set(info.builtins_,
                                                     BuiltinBits::BARYCENTRIC_COORD);

  shader::ShaderCreateInfo info_modified = info;
  info_modified.geometry_out_interfaces_ = info_modified.vertex_out_interfaces_;
  /**
   * NOTE(@fclem): Assuming we will render TRIANGLES. This will not work with other primitive
   * types. In this case, it might not trigger an error on some implementations.
   */
  info_modified.geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3);

  ss << geometry_layout_declare(info_modified);
  ss << geometry_interface_declare(info_modified);
  if (do_layer_output) {
    ss << "in int gpu_Layer[];\n";
  }
  if (do_viewport_output) {
    ss << "in int gpu_ViewportIndex[];\n";
  }

  if (do_barycentric_workaround) {
    ss << "flat out vec4 gpu_pos[3];\n";
    ss << "smooth out vec3 gpu_BaryCoord;\n";
    ss << "noperspective out vec3 gpu_BaryCoordNoPersp;\n";
  }
  ss << "\n";

  ss << "void main()\n";
  ss << "{\n";
  if (do_barycentric_workaround) {
    ss << "  gpu_pos[0] = gl_in[0].gl_Position;\n";
    ss << "  gpu_pos[1] = gl_in[1].gl_Position;\n";
    ss << "  gpu_pos[2] = gl_in[2].gl_Position;\n";
  }
  for (auto i : IndexRange(3)) {
    for (const StageInterfaceInfo *iface : info_modified.vertex_out_interfaces_) {
      for (auto &inout : iface->inouts) {
        ss << "  " << iface->instance_name << "_out." << inout.name;
        ss << " = " << iface->instance_name << "_in[" << i << "]." << inout.name << ";\n";
      }
    }
    if (do_barycentric_workaround) {
      ss << "  gpu_BaryCoordNoPersp = gpu_BaryCoord =";
      ss << " vec3(" << int(i == 0) << ", " << int(i == 1) << ", " << int(i == 2) << ");\n";
    }
    ss << "  gl_Position = gl_in[" << i << "].gl_Position;\n";
    if (flag_is_set(info.builtins_, BuiltinBits::CLIP_CONTROL)) {
      /* Assume clip range is set to 0..1 and remap the range just like Vulkan and Metal. */
      ss << "gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n";
    }
    if (do_layer_output) {
      ss << "  gl_Layer = gpu_Layer[" << i << "];\n";
    }
    if (do_viewport_output) {
      ss << "  gl_ViewportIndex = gpu_ViewportIndex[" << i << "];\n";
    }
    ss << "  EmitVertex();\n";
  }
  ss << "}\n";
  return ss.str();
}

bool GLShader::do_geometry_shader_injection(const shader::ShaderCreateInfo *info) const
{
  BuiltinBits builtins = info->builtins_;
  if (!GLContext::native_barycentric_support &&
      flag_is_set(builtins, BuiltinBits::BARYCENTRIC_COORD))
  {
    return true;
  }
  if (!GLContext::layered_rendering_support && flag_is_set(builtins, BuiltinBits::LAYER)) {
    return true;
  }
  if (!GLContext::layered_rendering_support && flag_is_set(builtins, BuiltinBits::VIEWPORT_INDEX))
  {
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader stage creation
 * \{ */

static StringRefNull glsl_patch_vertex_get()
{
  /** Used for shader patching. Init once. */
  static std::string patch = []() {
    std::stringstream ss;
    /* Version need to go first. */
    ss << "#version 430\n";

    /* Enable extensions for features that are not part of our base GLSL version
     * don't use an extension for something already available! */
    {
      /* Required extension. */
      ss << "#extension GL_ARB_shader_draw_parameters : enable\n";
      ss << "#define GPU_ARB_shader_draw_parameters\n";
      ss << "#define gpu_BaseInstance gl_BaseInstanceARB\n";
      ss << "#define GPU_ARB_clip_control\n";
    }
    if (GLContext::layered_rendering_support) {
      ss << "#extension GL_ARB_shader_viewport_layer_array: enable\n";
    }
    if (GLContext::native_barycentric_support) {
      ss << "#extension GL_AMD_shader_explicit_vertex_parameter: enable\n";
    }

    /* Vulkan GLSL compatibility. */
    ss << "#define gpu_InstanceIndex (gl_InstanceID + gpu_BaseInstance)\n";

    /* Array compatibility. */
    ss << "#define gpu_Array(_type) _type[]\n";

    /* Needs to have this defined upfront for configuring shader defines. */
    ss << "#define GPU_VERTEX_SHADER\n";

    shader::GeneratedSource extensions{"gpu_shader_glsl_extension.glsl", {}, ss.str()};
    shader::GeneratedSourceList sources{extensions};
    return fmt::to_string(fmt::join(
        gpu_shader_dependency_get_resolved_source("gpu_shader_compat_glsl.glsl", sources), ""));
  }();
  return patch;
}

static StringRefNull glsl_patch_geometry_get()
{
  /** Used for shader patching. Init once. */
  static std::string patch = []() {
    std::stringstream ss;
    /* Version need to go first. */
    ss << "#version 430\n";

    if (GLContext::layered_rendering_support) {
      ss << "#extension GL_ARB_shader_viewport_layer_array: enable\n";
    }
    if (GLContext::native_barycentric_support) {
      ss << "#extension GL_AMD_shader_explicit_vertex_parameter: enable\n";
    }
    ss << "#define GPU_ARB_clip_control\n";

    /* Array compatibility. */
    ss << "#define gpu_Array(_type) _type[]\n";

    /* Needs to have this defined upfront for configuring shader defines. */
    ss << "#define GPU_GEOMETRY_SHADER\n";

    shader::GeneratedSource extensions{"gpu_shader_glsl_extension.glsl", {}, ss.str()};
    shader::GeneratedSourceList sources{extensions};
    return fmt::to_string(fmt::join(
        gpu_shader_dependency_get_resolved_source("gpu_shader_compat_glsl.glsl", sources), ""));
  }();
  return patch;
}

static StringRefNull glsl_patch_fragment_get()
{
  /** Used for shader patching. Init once. */
  static std::string patch = []() {
    std::stringstream ss;
    /* Version need to go first. */
    ss << "#version 430\n";

    if (GLContext::layered_rendering_support) {
      ss << "#extension GL_ARB_shader_viewport_layer_array: enable\n";
    }
    if (GLContext::native_barycentric_support) {
      ss << "#extension GL_AMD_shader_explicit_vertex_parameter: enable\n";
    }
    if (GLContext::framebuffer_fetch_support) {
      ss << "#extension GL_EXT_shader_framebuffer_fetch: enable\n";
    }
    if (GPU_stencil_export_support()) {
      ss << "#extension GL_ARB_shader_stencil_export: enable\n";
      ss << "#define GPU_ARB_shader_stencil_export\n";
    }
    ss << "#define GPU_ARB_clip_control\n";

    /* Array compatibility. */
    ss << "#define gpu_Array(_type) _type[]\n";

    /* Needs to have this defined upfront for configuring shader defines. */
    ss << "#define GPU_FRAGMENT_SHADER\n";

    shader::GeneratedSource extensions{"gpu_shader_glsl_extension.glsl", {}, ss.str()};
    shader::GeneratedSourceList sources{extensions};
    return fmt::to_string(fmt::join(
        gpu_shader_dependency_get_resolved_source("gpu_shader_compat_glsl.glsl", sources), ""));
  }();
  return patch;
}

static StringRefNull glsl_patch_compute_get()
{
  /** Used for shader patching. Init once. */
  static std::string patch = []() {
    std::stringstream ss;
    /* Version need to go first. */
    ss << "#version 430\n";

    /* Array compatibility. */
    ss << "#define gpu_Array(_type) _type[]\n";

    /* Needs to have this defined upfront for configuring shader defines. */
    ss << "#define GPU_COMPUTE_SHADER\n";

    ss << "#define GPU_ARB_clip_control\n";

    shader::GeneratedSource extensions{"gpu_shader_glsl_extension.glsl", {}, ss.str()};
    shader::GeneratedSourceList sources{extensions};
    return fmt::to_string(fmt::join(
        gpu_shader_dependency_get_resolved_source("gpu_shader_compat_glsl.glsl", sources), ""));
  }();
  return patch;
}

StringRefNull GLShader::glsl_patch_get(GLenum gl_stage)
{
  if (gl_stage == GL_VERTEX_SHADER) {
    return glsl_patch_vertex_get();
  }
  if (gl_stage == GL_GEOMETRY_SHADER) {
    return glsl_patch_geometry_get();
  }
  if (gl_stage == GL_FRAGMENT_SHADER) {
    return glsl_patch_fragment_get();
  }
  if (gl_stage == GL_COMPUTE_SHADER) {
    return glsl_patch_compute_get();
  }
  BLI_assert_unreachable();
  return "";
}

static StringRefNull stage_name_get(GLenum gl_stage)
{
  switch (gl_stage) {
    case GL_VERTEX_SHADER:
      return "vertex";
    case GL_GEOMETRY_SHADER:
      return "geometry";
    case GL_FRAGMENT_SHADER:
      return "fragment";
    case GL_COMPUTE_SHADER:
      return "compute";
  }
  return "";
}

GLuint GLShader::create_shader_stage(GLenum gl_stage,
                                     MutableSpan<StringRefNull> sources,
                                     GLSources &gl_sources,
                                     const shader::SpecializationConstants &constants_state)
{
  /* Patch the shader sources to include specialization constants. */
  std::string constants_source;
  Vector<StringRefNull> recreated_sources;
  if (has_specialization_constants()) {
    constants_source = constants_declare(constants_state);
    if (sources.is_empty()) {
      recreated_sources = gl_sources.sources_get();
      sources = recreated_sources;
    }
  }

  /* Patch the shader code using the first source slot. */
  sources[SOURCES_INDEX_VERSION] = glsl_patch_get(gl_stage);
  sources[SOURCES_INDEX_SPECIALIZATION_CONSTANTS] = constants_source;

  if (async_compilation_) {
    gl_sources[SOURCES_INDEX_VERSION].source = std::string(sources[SOURCES_INDEX_VERSION]);
    gl_sources[SOURCES_INDEX_SPECIALIZATION_CONSTANTS].source = std::string(
        sources[SOURCES_INDEX_SPECIALIZATION_CONSTANTS]);
  }

  if (DEBUG_LOG_SHADER_SRC_ON_ERROR) {
    /* Store the generated source for printing in case the link fails. */
    StringRefNull source_type = stage_name_get(gl_stage);

    debug_source += "\n\n----------" + source_type + "----------\n\n";
    for (StringRefNull source : sources) {
      debug_source.append(source);
    }
  }

  if (async_compilation_) {
    /* Only build the sources. */
    return 0;
  }

  GLuint shader = glCreateShader(gl_stage);
  if (shader == 0) {
    fprintf(stderr, "GLShader: Error: Could not create shader object.\n");
    return 0;
  }

  std::string concat_source = fmt::to_string(fmt::join(sources, ""));

  std::string full_name = this->name_get() + "_" + stage_name_get(gl_stage);

  if (this->name_get() == G.gpu_debug_shader_source_name) {
    namespace fs = std::filesystem;
    fs::path shader_dir = fs::current_path() / "Shaders";
    fs::create_directories(shader_dir);
    fs::path file_path = shader_dir / (full_name + ".glsl");

    std::ofstream output_source_file(file_path);
    if (output_source_file) {
      output_source_file << concat_source;
      output_source_file.close();
    }
    else {
      std::cerr << "Shader Source Debug: Failed to open file: " << file_path << "\n";
    }
  }

  /* Patch line directives so that we can make error reporting consistent. */
  size_t start_pos = 0;
  while ((start_pos = concat_source.find("#line ", start_pos)) != std::string::npos) {
    concat_source[start_pos] = '/';
    concat_source[start_pos + 1] = '/';
  }

  const char *str_ptr = concat_source.c_str();
  glShaderSource(shader, 1, &str_ptr, nullptr);
  glCompileShader(shader);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status || (G.debug & G_DEBUG_GPU)) {
    char log[5000] = "";
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    if (log[0] != '\0') {
      GLLogParser parser;
      switch (gl_stage) {
        case GL_VERTEX_SHADER:
          this->print_log(sources, log, "VertShader", !status, &parser);
          break;
        case GL_GEOMETRY_SHADER:
          this->print_log(sources, log, "GeomShader", !status, &parser);
          break;
        case GL_FRAGMENT_SHADER:
          this->print_log(sources, log, "FragShader", !status, &parser);
          break;
        case GL_COMPUTE_SHADER:
          this->print_log(sources, log, "ComputeShader", !status, &parser);
          break;
      }
    }
  }
  if (!status) {
    glDeleteShader(shader);
    compilation_failed_ = true;
    return 0;
  }

  debug::object_label(gl_stage, shader, name);
  return shader;
}

void GLShader::update_program_and_sources(GLSources &stage_sources,
                                          MutableSpan<StringRefNull> sources)
{
  const bool store_sources = has_specialization_constants() || async_compilation_;
  if (store_sources && stage_sources.is_empty()) {
    stage_sources = sources;
  }
}

void GLShader::vertex_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                       MutableSpan<StringRefNull> sources)
{
  update_program_and_sources(vertex_sources_, sources);
  main_program_->vert_shader = create_shader_stage(
      GL_VERTEX_SHADER, sources, vertex_sources_, *constants);
}

void GLShader::geometry_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                         MutableSpan<StringRefNull> sources)
{
  update_program_and_sources(geometry_sources_, sources);
  main_program_->geom_shader = create_shader_stage(
      GL_GEOMETRY_SHADER, sources, geometry_sources_, *constants);
}

void GLShader::fragment_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                         MutableSpan<StringRefNull> sources)
{
  update_program_and_sources(fragment_sources_, sources);
  main_program_->frag_shader = create_shader_stage(
      GL_FRAGMENT_SHADER, sources, fragment_sources_, *constants);
}

void GLShader::compute_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                        MutableSpan<StringRefNull> sources)
{
  update_program_and_sources(compute_sources_, sources);
  main_program_->compute_shader = create_shader_stage(
      GL_COMPUTE_SHADER, sources, compute_sources_, *constants);
}

bool GLShader::finalize(const shader::ShaderCreateInfo *info)
{
  if (compilation_failed_) {
    return false;
  }

  if (info && do_geometry_shader_injection(info)) {
    std::string source = workaround_geometry_shader_source_create(*info);
    Vector<StringRefNull> sources;
    sources.append("version");
    sources.append("/* Specialization Constants. */\n");
    sources.append(source);
    geometry_shader_from_glsl(*info, sources);
  }

  if (async_compilation_) {
    return true;
  }

  main_program_->program_link(name);
  return post_finalize(info);
}

bool GLShader::post_finalize(const shader::ShaderCreateInfo *info)
{
  GLuint program_id = main_program_->program_id;
  GLint status;
  glGetProgramiv(program_id, GL_LINK_STATUS, &status);
  if (!status) {
    char log[5000];
    glGetProgramInfoLog(program_id, sizeof(log), nullptr, log);
    GLLogParser parser;
    print_log({debug_source}, log, "Linking", true, &parser);
    return false;
  }

  /* Reset for specialization constants variations. */
  async_compilation_ = false;

  if (info != nullptr) {
    interface = new GLShaderInterface(main_program_->program_id, *info);
  }
  else {
    interface = new GLShaderInterface(main_program_->program_id);
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

void GLShader::bind(const shader::SpecializationConstants *constants_state)
{
  GLProgram &program = program_get(constants_state);
  glUseProgram(program.program_id);
}

void GLShader::unbind()
{
#ifndef NDEBUG
  glUseProgram(0);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms setters
 * \{ */

void GLShader::uniform_float(int location, int comp_len, int array_size, const float *data)
{
  switch (comp_len) {
    case 1:
      glUniform1fv(location, array_size, data);
      break;
    case 2:
      glUniform2fv(location, array_size, data);
      break;
    case 3:
      glUniform3fv(location, array_size, data);
      break;
    case 4:
      glUniform4fv(location, array_size, data);
      break;
    case 9:
      glUniformMatrix3fv(location, array_size, 0, data);
      break;
    case 16:
      glUniformMatrix4fv(location, array_size, 0, data);
      break;
    default:
      BLI_assert(0);
      break;
  }
}

void GLShader::uniform_int(int location, int comp_len, int array_size, const int *data)
{
  switch (comp_len) {
    case 1:
      glUniform1iv(location, array_size, data);
      break;
    case 2:
      glUniform2iv(location, array_size, data);
      break;
    case 3:
      glUniform3iv(location, array_size, data);
      break;
    case 4:
      glUniform4iv(location, array_size, data);
      break;
    default:
      BLI_assert(0);
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sources
 * \{ */
GLSource::GLSource(StringRefNull other)
{
  if (!gpu_shader_dependency_get_filename_from_source_string(other).is_empty()) {
    source = "";
    source_ref = other;
  }
  else {
    source = other;
    source_ref = std::nullopt;
  }
}

GLSources &GLSources::operator=(Span<StringRefNull> other)
{
  clear();
  reserve(other.size());

  for (StringRefNull other_source : other) {
    /* Don't store empty string as compilers can optimize these away and result in pointing to a
     * string that isn't c-str compliant anymore. */
    if (other_source.is_empty()) {
      continue;
    }
    append(GLSource(other_source));
  }

  return *this;
}

Vector<StringRefNull> GLSources::sources_get() const
{
  Vector<StringRefNull> result;
  result.reserve(size());

  for (const GLSource &source : *this) {
    if (source.source_ref) {
      result.append(*source.source_ref);
    }
    else {
      result.append(source.source);
    }
  }
  return result;
}

std::string GLSources::to_string() const
{
  std::string result;
  for (const GLSource &source : *this) {
    if (source.source_ref) {
      result.append(*source.source_ref);
    }
    else {
      result.append(source.source);
    }
  }
  return result;
}

size_t GLSourcesBaked::size()
{
  size_t result = 0;
  result += comp.empty() ? 0 : comp.size() + sizeof('\0');
  result += vert.empty() ? 0 : vert.size() + sizeof('\0');
  result += geom.empty() ? 0 : geom.size() + sizeof('\0');
  result += frag.empty() ? 0 : frag.size() + sizeof('\0');
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Specialization Constants
 * \{ */

GLShader::GLProgram::~GLProgram()
{
  /* This can run from any thread even without a GLContext bound. */
  /* Invalid handles are silently ignored. */
  GLContext::shader_free(vert_shader);
  GLContext::shader_free(geom_shader);
  GLContext::shader_free(frag_shader);
  GLContext::shader_free(compute_shader);
  GLContext::program_free(program_id);
}

void GLShader::GLProgram::program_link(StringRefNull shader_name)
{
  if (this->program_id == 0) {
    this->program_id = glCreateProgram();
    debug::object_label(GL_PROGRAM, this->program_id, shader_name.c_str());
  }

  GLuint program_id = this->program_id;

  if (this->vert_shader) {
    glAttachShader(program_id, this->vert_shader);
  }
  if (this->geom_shader) {
    glAttachShader(program_id, this->geom_shader);
  }
  if (this->frag_shader) {
    glAttachShader(program_id, this->frag_shader);
  }
  if (this->compute_shader) {
    glAttachShader(program_id, this->compute_shader);
  }
  glLinkProgram(program_id);
}

GLShader::GLProgram &GLShader::program_get(const shader::SpecializationConstants *constants_state)
{
  BLI_assert(constants_state == nullptr || this->has_specialization_constants() == true);

  if (constants_state == nullptr) {
    /* Early exit for shaders that doesn't use specialization constants. */
    BLI_assert(main_program_);
    return *main_program_;
  }

  program_cache_mutex_.lock();

  GLProgram &program = *program_cache_.lookup_or_add_cb(
      constants_state->values, []() { return std::make_unique<GLProgram>(); });

  program_cache_mutex_.unlock();

  /* Avoid two threads trying to specialize the same shader at the same time. */
  std::scoped_lock lock(program.compilation_mutex);

  if (program.program_id != 0) {
    /* Specialization is already compiled. */
    return program;
  }

  if (!vertex_sources_.is_empty()) {
    program.vert_shader = create_shader_stage(
        GL_VERTEX_SHADER, {}, vertex_sources_, *constants_state);
  }
  if (!geometry_sources_.is_empty()) {
    program.geom_shader = create_shader_stage(
        GL_GEOMETRY_SHADER, {}, geometry_sources_, *constants_state);
  }
  if (!fragment_sources_.is_empty()) {
    program.frag_shader = create_shader_stage(
        GL_FRAGMENT_SHADER, {}, fragment_sources_, *constants_state);
  }
  if (!compute_sources_.is_empty()) {
    program.compute_shader = create_shader_stage(
        GL_COMPUTE_SHADER, {}, compute_sources_, *constants_state);
  }

  if (async_compilation_) {
    program.program_id = glCreateProgram();
    debug::object_label(GL_PROGRAM, program.program_id, name);
    return program;
  }

  GPU_debug_group_begin(GPU_DEBUG_SHADER_SPECIALIZATION_GROUP);
  GPU_debug_group_begin(this->name);

  program.program_link(name);

  /* Ensure the specialization compiled correctly.
   * Specialization compilation should never fail, but adding this check seems to bypass an
   * internal Nvidia driver issue (See #142046). */
  GLint status;
  glGetProgramiv(program.program_id, GL_LINK_STATUS, &status);
  BLI_assert(status);

  GPU_debug_group_end();
  GPU_debug_group_end();

  return program;
}

GLSourcesBaked GLShader::get_sources()
{
  GLSourcesBaked result;
  result.comp = compute_sources_.to_string();
  result.vert = vertex_sources_.to_string();
  result.geom = geometry_sources_.to_string();
  result.frag = fragment_sources_.to_string();
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GLShaderCompiler
 * \{ */

void GLShaderCompiler::specialize_shader(ShaderSpecialization &specialization)
{
  dynamic_cast<GLShader *>(specialization.shader)->program_get(&specialization.constants);
}

/** \} */

#if BLI_SUBPROCESS_SUPPORT

/* -------------------------------------------------------------------- */
/** \name Compiler workers
 * \{ */

GLCompilerWorker::GLCompilerWorker()
{
  static size_t pipe_id = 0;
  pipe_id++;

  std::string name = "BLENDER_SHADER_COMPILER_" + std::to_string(getpid()) + "_" +
                     std::to_string(pipe_id);

  shared_mem_ = std::make_unique<SharedMemory>(
      name, compilation_subprocess_shared_memory_size, true);
  start_semaphore_ = std::make_unique<SharedSemaphore>(name + "_START", false);
  end_semaphore_ = std::make_unique<SharedSemaphore>(name + "_END", false);
  close_semaphore_ = std::make_unique<SharedSemaphore>(name + "_CLOSE", false);

  subprocess_.create({"--compilation-subprocess", name.c_str()});
}

GLCompilerWorker::~GLCompilerWorker()
{
  close_semaphore_->increment();
  /* Flag start so the subprocess can reach the close semaphore. */
  start_semaphore_->increment();
}

void GLCompilerWorker::compile(const GLSourcesBaked &sources)
{
  BLI_assert(state_ == AVAILABLE);

  ShaderSourceHeader *shared_src = reinterpret_cast<ShaderSourceHeader *>(shared_mem_->get_data());
  char *next_src = shared_src->sources;

  auto add_src = [&](const std::string &src) {
    if (!src.empty()) {
      const size_t src_size = src.size() + 1;
      memcpy(next_src, src.c_str(), src_size);
      next_src += src_size;
    }
  };

  add_src(sources.comp);
  add_src(sources.vert);
  add_src(sources.geom);
  add_src(sources.frag);

  BLI_assert(size_t(next_src) <= size_t(shared_src) + compilation_subprocess_shared_memory_size);

  if (!sources.comp.empty()) {
    BLI_assert(sources.vert.empty() && sources.geom.empty() && sources.frag.empty());
    shared_src->type = ShaderSourceHeader::Type::COMPUTE;
  }
  else {
    BLI_assert(sources.comp.empty() && !sources.vert.empty() && !sources.frag.empty());
    shared_src->type = sources.geom.empty() ?
                           ShaderSourceHeader::Type::GRAPHICS :
                           ShaderSourceHeader::Type::GRAPHICS_WITH_GEOMETRY_STAGE;
  }

  start_semaphore_->increment();

  state_ = COMPILATION_REQUESTED;
  compilation_start = BLI_time_now_seconds();
}

bool GLCompilerWorker::block_until_ready()
{
  BLI_assert(ELEM(state_, COMPILATION_REQUESTED, COMPILATION_READY));
  if (state_ == COMPILATION_READY) {
    return true;
  }

  auto delete_cached_binary = [&]() {
    /* If the subprocess crashed when loading the binary,
     * its name should be stored in shared memory.
     * Delete it to prevent more crashes in the future. */
    char str_start[] = "SOURCE_HASH:";
    char *shared_mem = reinterpret_cast<char *>(shared_mem_->get_data());
    if (BLI_str_startswith(shared_mem, str_start)) {
      std::string path = GL_shader_cache_dir_get() + SEP_STR +
                         std::string(shared_mem + sizeof(str_start) - 1);
      if (BLI_exists(path.c_str())) {
        BLI_delete(path.c_str(), false, false);
      }
    }
  };

  while (!end_semaphore_->try_decrement(1000)) {
    if (is_lost()) {
      delete_cached_binary();
      return false;
    }
  }

  state_ = COMPILATION_READY;
  return true;
}

bool GLCompilerWorker::is_lost()
{
  /* Use a timeout for hanged processes. */
  float max_timeout_seconds = 30.0f;
  return !subprocess_.is_running() ||
         (state_ == COMPILATION_REQUESTED &&
          (BLI_time_now_seconds() - compilation_start) > max_timeout_seconds);
}

bool GLCompilerWorker::load_program_binary(GLint program)
{
  if (!block_until_ready()) {
    return false;
  }

  ShaderBinaryHeader *binary = (ShaderBinaryHeader *)shared_mem_->get_data();

  state_ = COMPILATION_FINISHED;

  if (binary->size > 0) {
    GPU_debug_group_begin("Load Binary");
    glProgramBinary(program, binary->format, binary->data, binary->size);
    GPU_debug_group_end();
    return true;
  }

  return false;
}

void GLCompilerWorker::release()
{
  state_ = AVAILABLE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GLSubprocessShaderCompiler
 * \{ */

GLSubprocessShaderCompiler::~GLSubprocessShaderCompiler()
{
  /* Must be called before we destruct the GLCompilerWorkers. */
  destruct_compilation_worker();

  for (GLCompilerWorker *worker : workers_) {
    delete worker;
  }
}

GLCompilerWorker *GLSubprocessShaderCompiler::get_compiler_worker()
{
  auto new_worker = [&]() {
    GLCompilerWorker *result = new GLCompilerWorker();
    std::lock_guard lock(workers_mutex_);
    workers_.append(result);
    return result;
  };

  static thread_local GLCompilerWorker *worker = new_worker();

  if (worker->is_lost()) {
    std::cerr << "ERROR: Compilation subprocess lost\n";
    {
      std::lock_guard lock(workers_mutex_);
      workers_.remove_first_occurrence_and_reorder(worker);
    }
    delete worker;
    worker = new_worker();
  }

  return worker;
}

Shader *GLSubprocessShaderCompiler::compile_shader(const shader::ShaderCreateInfo &info)
{
  const_cast<ShaderCreateInfo *>(&info)->finalize();
  GLShader *shader = static_cast<GLShader *>(compile(info, true));
  GLSourcesBaked sources = shader->get_sources();

  size_t required_size = sources.size();
  bool do_async_compilation = required_size <= sizeof(ShaderSourceHeader::sources);
  if (!do_async_compilation) {
    /* TODO: Can't reuse? */
    delete shader;
    return compile(info, false);
  }

  GLCompilerWorker *worker = get_compiler_worker();
  worker->compile(sources);

  GPU_debug_group_begin("Subprocess Compilation");

  /* This path is always called for the default shader compilation. Not for specialization.
   * Use the default constant template. */
  const shader::SpecializationConstants &constants = GPU_shader_get_default_constant_state(shader);

  if (!worker->load_program_binary(shader->program_cache_.lookup(constants.values)->program_id) ||
      !shader->post_finalize(&info))
  {
    /* Compilation failed, try to compile it locally. */
    delete shader;
    shader = nullptr;
  }

  GPU_debug_group_end();

  worker->release();

  if (!shader) {
    return compile(info, false);
  }

  return shader;
}

void GLSubprocessShaderCompiler::specialize_shader(ShaderSpecialization &specialization)
{
  static std::mutex mutex;

  GLShader *shader = static_cast<GLShader *>(specialization.shader);

  auto program_get = [&]() -> GLShader::GLProgram * {
    if (shader->program_cache_.contains(specialization.constants.values)) {
      return shader->program_cache_.lookup(specialization.constants.values).get();
    }
    return nullptr;
  };

  auto program_release = [&]() {
    /* Compilation failed, local compilation will be tried later on shader bind. */
    GLShader::GLProgram *program = program_get();
    glDeleteProgram(program->program_id);
    program->program_id = 0;
  };

  GLSourcesBaked sources;
  {
    std::lock_guard lock(mutex);

    if (program_get()) {
      /* Already compiled. */
      return;
    }

    /** WORKAROUND: Set async_compilation to true, so only the sources are generated. */
    shader->async_compilation_ = true;
    shader->program_get(&specialization.constants);
    shader->async_compilation_ = false;
    sources = shader->get_sources();

    size_t required_size = sources.size();
    bool do_async_compilation = required_size <= sizeof(ShaderSourceHeader::sources);
    if (!do_async_compilation) {
      program_release();
      return;
    }
  }

  GPU_debug_group_begin("Subprocess Specialization");

  GLCompilerWorker *worker = get_compiler_worker();
  worker->compile(sources);
  worker->block_until_ready();

  std::lock_guard lock(mutex);

  if (!worker->load_program_binary(program_get()->program_id)) {
    program_release();
  }

  GPU_debug_group_end();

  worker->release();
}

/** \} */

#endif
