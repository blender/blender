/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include <sstream>

#include "GPU_capabilities.hh"

#include "vk_shader.hh"

#include "vk_backend.hh"
#include "vk_framebuffer.hh"
#include "vk_shader_interface.hh"
#include "vk_shader_log.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_attribute_object.hh"

#include "BLI_string_utils.hh"
#include "BLI_vector.hh"

#include "BKE_global.hh"

#include <fmt/format.h>

using namespace blender::gpu::shader;

namespace blender::gpu {

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
    default:
      return "unknown";
  }
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
    case ImageType::Uint2D:
    case ImageType::Uint2DArray:
    case ImageType::Shadow2D:
    case ImageType::Shadow2DArray:
    case ImageType::Depth2D:
    case ImageType::Depth2DArray:
    case ImageType::AtomicInt2D:
    case ImageType::AtomicInt2DArray:
    case ImageType::AtomicUint2D:
    case ImageType::AtomicUint2DArray:
      os << "2D";
      break;
    case ImageType::Float3D:
    case ImageType::Int3D:
    case ImageType::AtomicInt3D:
    case ImageType::Uint3D:
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
    case ImageType::UintCubeArray:
    case ImageType::Shadow2DArray:
    case ImageType::ShadowCubeArray:
    case ImageType::Depth2DArray:
    case ImageType::DepthCubeArray:
    case ImageType::AtomicUint2DArray:
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
                           const VKDescriptorSet::Location location,
                           const ShaderCreateInfo::Resource &res)
{
  os << "layout(binding = " << uint32_t(location);
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
                                             StringRef(res.uniformbuf.name.data(), array_offset);
      os << "uniform _" << name_no_array << " { " << res.uniformbuf.type_name << " "
         << res.uniformbuf.name << "; };\n";
      break;
    case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      array_offset = res.storagebuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.storagebuf.name :
                                             StringRef(res.storagebuf.name.data(), array_offset);
      print_qualifier(os, res.storagebuf.qualifiers);
      os << "buffer _";
      os << name_no_array << " { " << res.storagebuf.type_name << " " << res.storagebuf.name
         << "; };\n";
      break;
  }
}

static void print_resource(std::ostream &os,
                           const VKShaderInterface &shader_interface,
                           const ShaderCreateInfo::Resource &res)
{
  const VKDescriptorSet::Location location = shader_interface.descriptor_set_location(res);
  print_resource(os, location, res);
}

inline int get_location_count(const Type &type)
{
  if (type == shader::Type::float4x4_t) {
    return 4;
  }
  else if (type == shader::Type::float3x3_t) {
    return 3;
  }
  return 1;
}

static void print_interface_as_attributes(std::ostream &os,
                                          const std::string &prefix,
                                          const StageInterfaceInfo &iface,
                                          int &location)
{
  for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
    os << "layout(location=" << location << ") " << prefix << " " << to_string(inout.interp) << " "
       << to_string(inout.type) << " " << inout.name << ";\n";
    location += get_location_count(inout.type);
  }
}

static void print_interface_as_struct(std::ostream &os,
                                      const std::string &prefix,
                                      const StageInterfaceInfo &iface,
                                      int &location,
                                      const StringRefNull &suffix)
{
  std::string struct_name = prefix + iface.name;
  Interpolation qualifier = iface.inouts[0].interp;

  os << "struct " << struct_name << " {\n";
  for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
    os << "  " << to_string(inout.type) << " " << inout.name << ";\n";
  }
  os << "};\n";
  os << "layout(location=" << location << ") " << prefix << " " << to_string(qualifier) << " "
     << struct_name << " " << iface.instance_name << suffix << ";\n";

  for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
    location += get_location_count(inout.type);
  }
}

static void print_interface(std::ostream &os,
                            const std::string &prefix,
                            const StageInterfaceInfo &iface,
                            int &location,
                            const StringRefNull &suffix = "")
{
  if (iface.instance_name.is_empty()) {
    print_interface_as_attributes(os, prefix, iface, location);
  }
  else {
    print_interface_as_struct(os, prefix, iface, location, suffix);
  }
}

/** \} */
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

static std::string combine_sources(Span<StringRefNull> sources)
{
  return fmt::to_string(fmt::join(sources, ""));
}

VKShader::VKShader(const char *name) : Shader(name)
{
  context_ = VKContext::get();
}

void VKShader::init(const shader::ShaderCreateInfo &info, bool /*is_batch_compilation*/)
{
  VKShaderInterface *vk_interface = new VKShaderInterface();
  vk_interface->init(info);
  interface = vk_interface;
  is_static_shader_ = info.do_static_compilation_;
  is_compute_shader_ = !info.compute_source_.is_empty() || !info.compute_source_generated.empty();
}

VKShader::~VKShader()
{
  VKDevice &device = VKBackend::get().device;
  VKDiscardPool &discard_pool = VKDiscardPool::discard_pool_get();

  if (vk_pipeline_layout != VK_NULL_HANDLE) {
    device.pipelines.discard(discard_pool, vk_pipeline_layout);
    discard_pool.discard_pipeline_layout(vk_pipeline_layout);
    vk_pipeline_layout = VK_NULL_HANDLE;
  }
  /* Unset not owning handles. */
  vk_descriptor_set_layout_ = VK_NULL_HANDLE;
}

void VKShader::build_shader_module(MutableSpan<StringRefNull> sources,
                                   shaderc_shader_kind stage,
                                   VKShaderModule &r_shader_module)
{
  r_shader_module.is_ready = false;
  const VKDevice &device = VKBackend::get().device;
  std::string source_patch;

  switch (stage) {
    case shaderc_vertex_shader:
      source_patch = device.glsl_vertex_patch_get();
      break;
    case shaderc_geometry_shader:
      source_patch = device.glsl_geometry_patch_get();
      break;
    case shaderc_fragment_shader:
      source_patch = device.glsl_fragment_patch_get();
      break;
    case shaderc_compute_shader:
      source_patch = device.glsl_compute_patch_get();
      break;
    default:
      BLI_assert_msg(0, "Only forced ShaderC shader kinds are supported.");
      break;
  }

  sources[SOURCES_INDEX_VERSION] = source_patch;
  r_shader_module.combined_sources = combine_sources(sources);
  VKShaderCompiler::compile_module(*this, stage, r_shader_module);
  r_shader_module.is_ready = true;
}

void VKShader::vertex_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                       MutableSpan<StringRefNull> sources)
{
  build_shader_module(sources, shaderc_vertex_shader, vertex_module);
}

void VKShader::geometry_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                         MutableSpan<StringRefNull> sources)
{
  build_shader_module(sources, shaderc_geometry_shader, geometry_module);
}

void VKShader::fragment_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                         MutableSpan<StringRefNull> sources)
{
  build_shader_module(sources, shaderc_fragment_shader, fragment_module);
}

void VKShader::compute_shader_from_glsl(const shader::ShaderCreateInfo & /*info*/,
                                        MutableSpan<StringRefNull> sources)
{
  build_shader_module(sources, shaderc_compute_shader, compute_module);
}

void VKShader::warm_cache(int /*limit*/)
{
  NOT_YET_IMPLEMENTED
}

bool VKShader::finalize(const shader::ShaderCreateInfo *info)
{
  /* Add-ons that still use old API will crash as the shader create info isn't available.
   * See #130555 */
  if (info == nullptr) {
    return false;
  }

  if (do_geometry_shader_injection(info)) {
    std::string source = workaround_geometry_shader_source_create(*info);
    Vector<StringRefNull> sources;
    sources.append("version");
    sources.append(source);
    geometry_shader_from_glsl(*info, sources);
  }

  const VKShaderInterface &vk_interface = interface_get();
  VKDevice &device = VKBackend::get().device;
  if (!finalize_descriptor_set_layouts(device, vk_interface)) {
    return false;
  }
  if (!finalize_pipeline_layout(device, vk_interface)) {
    return false;
  }

  push_constants = VKPushConstants(&vk_interface.push_constants_layout_get());
  return finalize_post();
}

bool VKShader::finalize_post()
{
  bool result = finalize_shader_module(vertex_module, "vertex") &&
                finalize_shader_module(geometry_module, "geometry") &&
                finalize_shader_module(fragment_module, "fragment") &&
                finalize_shader_module(compute_module, "compute");

  /* Ensure that pipeline of compute shaders are already build. This can improve performance as it
   * can triggers a back-end compilation step. In this step the Shader module SPIR-V is
   * compiled to a shader program that can be executed by the device. Depending on the driver this
   * can take some time as well. If this is done inside the main thread it will stall user
   * interactivity.
   */
  if (result && is_compute_shader_) {
    /* This is only done for the first shader compilation (not specialization).
     * Give the default constants. */
    ensure_and_get_compute_pipeline(*constants);
  }
  return result;
}

bool VKShader::finalize_shader_module(VKShaderModule &shader_module, const char *stage_name)
{
  VKLogParser parser;
  bool compilation_succeeded = ELEM(shader_module.compilation_result.GetCompilationStatus(),
                                    shaderc_compilation_status_null_result_object,
                                    shaderc_compilation_status_success);
  if (bool(shader_module.compilation_result.GetNumWarnings() +
           shader_module.compilation_result.GetNumErrors()))
  {
    print_log({shader_module.combined_sources},
              shader_module.compilation_result.GetErrorMessage().c_str(),
              stage_name,
              bool(shader_module.compilation_result.GetNumErrors()),
              &parser);
  }

  std::string full_name = std::string(name) + "_" + stage_name;
  shader_module.finalize(full_name.c_str());
  shader_module.combined_sources.clear();
  shader_module.sources_hash.clear();
  shader_module.compilation_result = {};
  shader_module.spirv_binary.clear();
  return compilation_succeeded;
}

bool VKShader::finalize_pipeline_layout(VKDevice &device,
                                        const VKShaderInterface &shader_interface)
{
  const uint32_t layout_count = vk_descriptor_set_layout_ == VK_NULL_HANDLE ? 0 : 1;
  VkPipelineLayoutCreateInfo pipeline_info = {};
  VkPushConstantRange push_constant_range = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_info.flags = 0;
  pipeline_info.setLayoutCount = layout_count;
  pipeline_info.pSetLayouts = &vk_descriptor_set_layout_;

  /* Setup push constants. */
  const VKPushConstants::Layout &push_constants_layout =
      shader_interface.push_constants_layout_get();
  if (push_constants_layout.storage_type_get() == VKPushConstants::StorageType::PUSH_CONSTANTS) {
    push_constant_range.offset = 0;
    push_constant_range.size = push_constants_layout.size_in_bytes();
    push_constant_range.stageFlags = is_compute_shader_ ? VK_SHADER_STAGE_COMPUTE_BIT :
                                                          VK_SHADER_STAGE_ALL_GRAPHICS;
    pipeline_info.pushConstantRangeCount = 1;
    pipeline_info.pPushConstantRanges = &push_constant_range;
  }

  if (vkCreatePipelineLayout(device.vk_handle(), &pipeline_info, nullptr, &vk_pipeline_layout) !=
      VK_SUCCESS)
  {
    return false;
  };

  debug::object_label(vk_pipeline_layout, name_get());

  return true;
}

bool VKShader::finalize_descriptor_set_layouts(VKDevice &vk_device,
                                               const VKShaderInterface &shader_interface)
{
  bool created;
  bool needed;

  vk_descriptor_set_layout_ = vk_device.descriptor_set_layouts_get().get_or_create(
      shader_interface.descriptor_set_layout_info_get(), created, needed);
  if (created) {
    debug::object_label(vk_descriptor_set_layout_, name_get());
  }
  if (!needed) {
    BLI_assert(vk_descriptor_set_layout_ == VK_NULL_HANDLE);
    return true;
  }
  return vk_descriptor_set_layout_ != VK_NULL_HANDLE;
}

void VKShader::bind(const shader::SpecializationConstants *constants_state)
{
  VKContext *ctx = VKContext::get();
  /* Copy constants state. */
  ctx->specialization_constants_set(constants_state);

  /* Intentionally empty. Binding of the pipeline are done just before drawing/dispatching.
   * See #VKPipeline.update_and_bind */
}

void VKShader::unbind() {}

void VKShader::uniform_float(int location, int comp_len, int array_size, const float *data)
{
  push_constants.push_constant_set(location, comp_len, array_size, data);
}

void VKShader::uniform_int(int location, int comp_len, int array_size, const int *data)
{
  push_constants.push_constant_set(location, comp_len, array_size, data);
}

std::string VKShader::resources_declare(const shader::ShaderCreateInfo &info) const
{
  const VKShaderInterface &vk_interface = interface_get();
  std::stringstream ss;

  ss << "\n/* Specialization Constants (pass-through). */\n";
  uint constant_id = 0;
  for (const SpecializationConstant &sc : info.specialization_constants_) {
    ss << "layout (constant_id=" << constant_id++ << ") const ";
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
      case Type::float_t:
        /* Use uint representation to allow exact same bit pattern even if NaN. uintBitsToFloat
         * isn't supported during global const initialization. */
        ss << "uint " << sc.name << "_uint=" << std::to_string(sc.value.u) << "u;\n";
        ss << "#define " << sc.name << " uintBitsToFloat(" << sc.name << "_uint)\n";
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  }

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

  ss << "\n/* Pass Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource(ss, vk_interface, res);
  }

  ss << "\n/* Batch Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource(ss, vk_interface, res);
  }

  ss << "\n/* Geometry Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.geometry_resources_) {
    print_resource(ss, vk_interface, res);
  }

  /* Push constants. */
  const VKPushConstants::Layout &push_constants_layout = vk_interface.push_constants_layout_get();
  const VKPushConstants::StorageType push_constants_storage =
      push_constants_layout.storage_type_get();
  if (push_constants_storage != VKPushConstants::StorageType::NONE) {
    ss << "\n/* Push Constants. */\n";
    if (push_constants_storage == VKPushConstants::StorageType::PUSH_CONSTANTS) {
      ss << "layout(push_constant, std430) uniform constants\n";
    }
    else if (push_constants_storage == VKPushConstants::StorageType::UNIFORM_BUFFER) {
      ss << "layout(binding = " << push_constants_layout.descriptor_set_location_get()
         << ", std140) uniform constants\n";
    }
    ss << "{\n";
    for (const ShaderCreateInfo::PushConst &uniform : info.push_constants_) {
      ss << "  " << to_string(uniform.type) << " pc_" << uniform.name;
      if (uniform.array_size > 0) {
        ss << "[" << uniform.array_size << "]";
      }
      ss << ";\n";
    }
    ss << "} PushConstants;\n";
    for (const ShaderCreateInfo::PushConst &uniform : info.push_constants_) {
      ss << "#define " << uniform.name << " (PushConstants.pc_" << uniform.name << ")\n";
    }
  }

  ss << "\n";
  return ss.str();
}

std::string VKShader::vertex_interface_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;
  std::string post_main;

  ss << "\n/* Inputs. */\n";
  for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
    ss << "layout(location = " << attr.index << ") ";
    ss << "in " << to_string(attr.type) << " " << attr.name << ";\n";
  }
  ss << "\n/* Interfaces. */\n";
  int location = 0;
  for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
    print_interface(ss, "out", *iface, location);
  }

  const bool has_geometry_stage = do_geometry_shader_injection(&info) ||
                                  !info.geometry_source_.is_empty();
  const bool do_layer_output = flag_is_set(info.builtins_, BuiltinBits::LAYER);
  const bool do_viewport_output = flag_is_set(info.builtins_, BuiltinBits::VIEWPORT_INDEX);
  if (has_geometry_stage) {
    if (do_layer_output) {
      ss << "layout(location=" << (location++) << ") out int gpu_Layer;\n ";
    }
    if (do_viewport_output) {
      ss << "layout(location=" << (location++) << ") out int gpu_ViewportIndex;\n";
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
  ss << "\n";

  /* Retarget depth from -1..1 to 0..1. This will be done by geometry stage, when geometry shaders
   * are used. */
  const bool retarget_depth = !has_geometry_stage;
  if (retarget_depth) {
    post_main += "gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n";
  }

  if (post_main.empty() == false) {
    std::string pre_main;
    ss << main_function_wrapper(pre_main, post_main);
  }
  return ss.str();
}

static Type to_component_type(const Type &type)
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

std::string VKShader::fragment_interface_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;
  std::string pre_main;
  const VKExtensions &extensions = VKBackend::get().device.extensions_get();

  ss << "\n/* Interfaces. */\n";
  const Span<StageInterfaceInfo *> in_interfaces = info.geometry_source_.is_empty() ?
                                                       info.vertex_out_interfaces_ :
                                                       info.geometry_out_interfaces_;
  int location = 0;
  for (const StageInterfaceInfo *iface : in_interfaces) {
    print_interface(ss, "in", *iface, location);
  }
  if (flag_is_set(info.builtins_, BuiltinBits::LAYER)) {
    ss << "#define gpu_Layer gl_Layer\n";
  }
  if (flag_is_set(info.builtins_, BuiltinBits::VIEWPORT_INDEX)) {
    ss << "#define gpu_ViewportIndex gl_ViewportIndex\n";
  }

  if (!extensions.fragment_shader_barycentric &&
      flag_is_set(info.builtins_, BuiltinBits::BARYCENTRIC_COORD))
  {
    ss << "layout(location=" << (location++) << ") smooth in vec3 gpu_BaryCoord;\n";
    ss << "layout(location=" << (location++) << ") noperspective in vec3 gpu_BaryCoordNoPersp;\n";
  }

  if (info.early_fragment_test_) {
    ss << "layout(early_fragment_tests) in;\n";
  }
  const bool use_gl_frag_depth = info.depth_write_ != DepthWrite::UNCHANGED &&
                                 info.fragment_source_.find("gl_FragDepth") != std::string::npos;
  if (use_gl_frag_depth) {
    ss << "layout(" << to_string(info.depth_write_) << ") out float gl_FragDepth;\n";
  }

  ss << "\n/* Sub-pass Inputs. */\n";
  const VKShaderInterface &interface = interface_get();
  const bool use_local_read = extensions.dynamic_rendering_local_read;

  if (use_local_read) {
    uint32_t subpass_input_binding_index = 0;
    for (const ShaderCreateInfo::SubpassIn &input : info.subpass_inputs_) {
      std::string input_attachment_name = "gpu_input_attachment_";
      input_attachment_name += std::to_string(input.index);

      /* Declare global for input. */
      ss << to_string(input.type) << " " << input.name << ";\n";

      Type component_type = to_component_type(input.type);
      char typePrefix;
      switch (component_type) {
        case Type::int_t:
          typePrefix = 'i';
          break;
        case Type::uint_t:
          typePrefix = 'u';
          break;
        default:
          typePrefix = ' ';
          break;
      }
      ss << "layout(input_attachment_index = " << (input.index)
         << ", binding = " << (subpass_input_binding_index++) << ") uniform " << typePrefix
         << "subpassInput " << input_attachment_name << "; \n";

      std::stringstream ss_pre;
      static const std::string swizzle = "xyzw";
      /* Populate the global before main using subpassLoad. */
      ss_pre << "  " << input.name << " = " << input.type << "( subpassLoad("
             << input_attachment_name << ")." << swizzle.substr(0, to_component_count(input.type))
             << " ); \n";

      pre_main += ss_pre.str();
    }
  }
  else {
    for (const ShaderCreateInfo::SubpassIn &input : info.subpass_inputs_) {
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
      print_resource(ss, interface, res);

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
    const int location = output.index;
    ss << "layout(location = " << location;
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

  if (pre_main.empty() == false) {
    std::string post_main;
    ss << main_function_wrapper(pre_main, post_main);
  }
  return ss.str();
}

std::string VKShader::geometry_interface_declare(const shader::ShaderCreateInfo &info) const
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
                                                  const StringRefNull name)
{
  for (StageInterfaceInfo *iface : ifaces) {
    if (iface->instance_name == name) {
      return iface;
    }
  }
  return nullptr;
}

static void declare_emit_vertex(std::stringstream &ss)
{
  ss << "void gpu_EmitVertex() {\n";
  ss << "  gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n";
  ss << "  EmitVertex();\n";
  ss << "}\n";
}

std::string VKShader::geometry_layout_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;

  ss << "\n/* Interfaces. */\n";
  int location = 0;
  for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
    bool has_matching_output_iface = find_interface_by_name(info.geometry_out_interfaces_,
                                                            iface->instance_name) != nullptr;
    const char *suffix = (has_matching_output_iface) ? "_in[]" : "[]";
    print_interface(ss, "in", *iface, location, suffix);
  }
  ss << "\n";

  location = 0;
  for (const StageInterfaceInfo *iface : info.geometry_out_interfaces_) {
    bool has_matching_input_iface = find_interface_by_name(info.vertex_out_interfaces_,
                                                           iface->instance_name) != nullptr;
    const char *suffix = (has_matching_input_iface) ? "_out" : "";
    print_interface(ss, "out", *iface, location, suffix);
  }
  ss << "\n";

  declare_emit_vertex(ss);

  return ss.str();
}

std::string VKShader::compute_layout_declare(const shader::ShaderCreateInfo &info) const
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

/* -------------------------------------------------------------------- */
/** \name Passthrough geometry shader emulation
 *
 * \{ */

std::string VKShader::workaround_geometry_shader_source_create(
    const shader::ShaderCreateInfo &info)
{
  std::stringstream ss;
  const VKExtensions &extensions = VKBackend::get().device.extensions_get();

  const bool do_layer_output = flag_is_set(info.builtins_, BuiltinBits::LAYER);
  const bool do_viewport_output = flag_is_set(info.builtins_, BuiltinBits::VIEWPORT_INDEX);
  const bool do_barycentric_workaround = !extensions.fragment_shader_barycentric &&
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
  int location = 0;
  for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
    for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
      location += get_location_count(inout.type);
    }
  }

  int location_in = location;
  int location_out = location;
  if (do_layer_output) {
    ss << "layout(location=" << (location_in++) << ") in int gpu_Layer[];\n";
  }
  if (do_viewport_output) {
    ss << "layout(location=" << (location_in++) << ") in int gpu_ViewportIndex[];\n";
  }
  if (do_barycentric_workaround) {
    ss << "layout(location=" << (location_out++) << ") smooth out vec3 gpu_BaryCoord;\n";
    ss << "layout(location=" << (location_out++)
       << ") noperspective out vec3 gpu_BaryCoordNoPersp;\n";
  }
  ss << "\n";

  ss << "void main()\n";
  ss << "{\n";
  for (auto i : IndexRange(3)) {
    for (StageInterfaceInfo *iface : info_modified.vertex_out_interfaces_) {
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
    if (do_layer_output) {
      ss << "  gl_Layer = gpu_Layer[" << i << "];\n";
    }
    if (do_viewport_output) {
      ss << "  gl_ViewportIndex = gpu_ViewportIndex[" << i << "];\n";
    }
    ss << "  gpu_EmitVertex();\n";
  }
  ss << "}\n";
  return ss.str();
}

bool VKShader::do_geometry_shader_injection(const shader::ShaderCreateInfo *info) const
{
  const VKExtensions &extensions = VKBackend::get().device.extensions_get();
  BuiltinBits builtins = info->builtins_;
  if (!extensions.fragment_shader_barycentric &&
      flag_is_set(builtins, BuiltinBits::BARYCENTRIC_COORD))
  {
    return true;
  }
  if (!extensions.shader_output_layer && flag_is_set(builtins, BuiltinBits::LAYER)) {
    return true;
  }
  if (!extensions.shader_output_viewport_index &&
      flag_is_set(builtins, BuiltinBits::VIEWPORT_INDEX))
  {
    return true;
  }
  return false;
}

/** \} */

VkPipeline VKShader::ensure_and_get_compute_pipeline(
    const shader::SpecializationConstants &constants_state)
{
  BLI_assert(is_compute_shader_);
  BLI_assert(compute_module.vk_shader_module != VK_NULL_HANDLE);
  BLI_assert(vk_pipeline_layout != VK_NULL_HANDLE);

  /* Early exit when no specialization constants are used and the vk_pipeline_base_ is already
   * valid. This would handle most cases. */
  if (constants_state.values.is_empty() && vk_pipeline_base_ != VK_NULL_HANDLE) {
    return vk_pipeline_base_;
  }

  VKComputeInfo compute_info = {};
  compute_info.specialization_constants.extend(constants_state.values);
  compute_info.vk_shader_module = compute_module.vk_shader_module;
  compute_info.vk_pipeline_layout = vk_pipeline_layout;

  VKDevice &device = VKBackend::get().device;
  /* Store result in local variable to ensure thread safety. */
  VkPipeline vk_pipeline = device.pipelines.get_or_create_compute_pipeline(
      compute_info, is_static_shader_, vk_pipeline_base_, name_get());
  if (vk_pipeline_base_ == VK_NULL_HANDLE) {
    vk_pipeline_base_ = vk_pipeline;
  }
  return vk_pipeline;
}

VkPipeline VKShader::ensure_and_get_graphics_pipeline(GPUPrimType primitive,
                                                      VKVertexAttributeObject &vao,
                                                      VKStateManager &state_manager,
                                                      VKFrameBuffer &framebuffer,
                                                      SpecializationConstants &constants_state)
{
  BLI_assert(!is_compute_shader_);
  BLI_assert_msg(
      primitive != GPU_PRIM_POINTS || interface_get().is_point_shader(),
      "GPU_PRIM_POINTS is used with a shader that doesn't set point size before "
      "drawing fragments. Calling code should be adapted to use a shader that sets the "
      "gl_PointSize before entering the fragment stage. For example `GPU_SHADER_3D_POINT_*`.");

  /* TODO: Graphics info should be cached in VKContext and only the changes should be applied. */
  VKGraphicsInfo graphics_info = {};
  graphics_info.specialization_constants.extend(constants_state.values);
  graphics_info.vk_pipeline_layout = vk_pipeline_layout;

  graphics_info.vertex_in.vk_topology = to_vk_primitive_topology(primitive);
  graphics_info.vertex_in.attributes = vao.attributes;
  graphics_info.vertex_in.bindings = vao.bindings;

  graphics_info.pre_rasterization.vk_vertex_module = vertex_module.vk_shader_module;
  graphics_info.pre_rasterization.vk_geometry_module = geometry_module.vk_shader_module;

  graphics_info.fragment_shader.vk_fragment_module = fragment_module.vk_shader_module;
  graphics_info.state = state_manager.state;
  graphics_info.mutable_state = state_manager.mutable_state;
  graphics_info.fragment_shader.viewport_count = framebuffer.viewport_size();

  graphics_info.fragment_out.depth_attachment_format = framebuffer.depth_attachment_format_get();
  graphics_info.fragment_out.stencil_attachment_format =
      framebuffer.stencil_attachment_format_get();
  graphics_info.fragment_out.color_attachment_formats.extend(
      framebuffer.color_attachment_formats_get());
  graphics_info.fragment_out.color_attachment_size = framebuffer.color_attachment_size;

  VKDevice &device = VKBackend::get().device;
  /* Store result in local variable to ensure thread safety. */
  VkPipeline vk_pipeline = device.pipelines.get_or_create_graphics_pipeline(
      graphics_info, is_static_shader_, vk_pipeline_base_, name_get());
  if (vk_pipeline_base_ == VK_NULL_HANDLE) {
    vk_pipeline_base_ = vk_pipeline;
  }
  return vk_pipeline;
}

const VKShaderInterface &VKShader::interface_get() const
{
  BLI_assert_msg(interface != nullptr,
                 "Interface can be accessed after the VKShader has been initialized "
                 "`VKShader::init`");
  return *static_cast<const VKShaderInterface *>(interface);
}

}  // namespace blender::gpu
