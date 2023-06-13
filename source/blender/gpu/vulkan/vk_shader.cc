/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_shader.hh"

#include "vk_backend.hh"
#include "vk_memory.hh"
#include "vk_shader_interface.hh"
#include "vk_shader_log.hh"

#include "BLI_string_utils.h"
#include "BLI_vector.hh"

#include "BKE_global.h"

using namespace blender::gpu::shader;

extern "C" char datatoc_glsl_shader_defines_glsl[];

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
    case Type::FLOAT:
      return "float";
    case Type::VEC2:
      return "vec2";
    case Type::VEC3:
      return "vec3";
    case Type::VEC4:
      return "vec4";
    case Type::MAT3:
      return "mat3";
    case Type::MAT4:
      return "mat4";
    case Type::UINT:
      return "uint";
    case Type::UVEC2:
      return "uvec2";
    case Type::UVEC3:
      return "uvec3";
    case Type::UVEC4:
      return "uvec4";
    case Type::INT:
      return "int";
    case Type::IVEC2:
      return "ivec2";
    case Type::IVEC3:
      return "ivec3";
    case Type::IVEC4:
      return "ivec4";
    case Type::BOOL:
      return "bool";
    default:
      return "unknown";
  }
}

static const char *to_string(const eGPUTextureFormat &type)
{
  switch (type) {
    case GPU_RGBA8UI:
      return "rgba8ui";
    case GPU_RGBA8I:
      return "rgba8i";
    case GPU_RGBA8:
      return "rgba8";
    case GPU_RGBA32UI:
      return "rgba32ui";
    case GPU_RGBA32I:
      return "rgba32i";
    case GPU_RGBA32F:
      return "rgba32f";
    case GPU_RGBA16UI:
      return "rgba16ui";
    case GPU_RGBA16I:
      return "rgba16i";
    case GPU_RGBA16F:
      return "rgba16f";
    case GPU_RGBA16:
      return "rgba16";
    case GPU_RG8UI:
      return "rg8ui";
    case GPU_RG8I:
      return "rg8i";
    case GPU_RG8:
      return "rg8";
    case GPU_RG32UI:
      return "rg32ui";
    case GPU_RG32I:
      return "rg32i";
    case GPU_RG32F:
      return "rg32f";
    case GPU_RG16UI:
      return "rg16ui";
    case GPU_RG16I:
      return "rg16i";
    case GPU_RG16F:
      return "rg16f";
    case GPU_RG16:
      return "rg16";
    case GPU_R8UI:
      return "r8ui";
    case GPU_R8I:
      return "r8i";
    case GPU_R8:
      return "r8";
    case GPU_R32UI:
      return "r32ui";
    case GPU_R32I:
      return "r32i";
    case GPU_R32F:
      return "r32f";
    case GPU_R16UI:
      return "r16ui";
    case GPU_R16I:
      return "r16i";
    case GPU_R16F:
      return "r16f";
    case GPU_R16:
      return "r16";
    case GPU_R11F_G11F_B10F:
      return "r11f_g11f_b10f";
    case GPU_RGB10_A2:
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
    case ImageType::INT_BUFFER:
    case ImageType::INT_1D:
    case ImageType::INT_1D_ARRAY:
    case ImageType::INT_2D:
    case ImageType::INT_2D_ARRAY:
    case ImageType::INT_3D:
    case ImageType::INT_CUBE:
    case ImageType::INT_CUBE_ARRAY:
      os << "i";
      break;
    case ImageType::UINT_BUFFER:
    case ImageType::UINT_1D:
    case ImageType::UINT_1D_ARRAY:
    case ImageType::UINT_2D:
    case ImageType::UINT_2D_ARRAY:
    case ImageType::UINT_3D:
    case ImageType::UINT_CUBE:
    case ImageType::UINT_CUBE_ARRAY:
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
    case ImageType::FLOAT_BUFFER:
    case ImageType::INT_BUFFER:
    case ImageType::UINT_BUFFER:
      os << "Buffer";
      break;
    case ImageType::FLOAT_1D:
    case ImageType::FLOAT_1D_ARRAY:
    case ImageType::INT_1D:
    case ImageType::INT_1D_ARRAY:
    case ImageType::UINT_1D:
    case ImageType::UINT_1D_ARRAY:
      os << "1D";
      break;
    case ImageType::FLOAT_2D:
    case ImageType::FLOAT_2D_ARRAY:
    case ImageType::INT_2D:
    case ImageType::INT_2D_ARRAY:
    case ImageType::UINT_2D:
    case ImageType::UINT_2D_ARRAY:
    case ImageType::SHADOW_2D:
    case ImageType::SHADOW_2D_ARRAY:
    case ImageType::DEPTH_2D:
    case ImageType::DEPTH_2D_ARRAY:
      os << "2D";
      break;
    case ImageType::FLOAT_3D:
    case ImageType::INT_3D:
    case ImageType::UINT_3D:
      os << "3D";
      break;
    case ImageType::FLOAT_CUBE:
    case ImageType::FLOAT_CUBE_ARRAY:
    case ImageType::INT_CUBE:
    case ImageType::INT_CUBE_ARRAY:
    case ImageType::UINT_CUBE:
    case ImageType::UINT_CUBE_ARRAY:
    case ImageType::SHADOW_CUBE:
    case ImageType::SHADOW_CUBE_ARRAY:
    case ImageType::DEPTH_CUBE:
    case ImageType::DEPTH_CUBE_ARRAY:
      os << "Cube";
      break;
    default:
      break;
  }

  switch (type) {
    case ImageType::FLOAT_1D_ARRAY:
    case ImageType::FLOAT_2D_ARRAY:
    case ImageType::FLOAT_CUBE_ARRAY:
    case ImageType::INT_1D_ARRAY:
    case ImageType::INT_2D_ARRAY:
    case ImageType::INT_CUBE_ARRAY:
    case ImageType::UINT_1D_ARRAY:
    case ImageType::UINT_2D_ARRAY:
    case ImageType::UINT_CUBE_ARRAY:
    case ImageType::SHADOW_2D_ARRAY:
    case ImageType::SHADOW_CUBE_ARRAY:
    case ImageType::DEPTH_2D_ARRAY:
    case ImageType::DEPTH_CUBE_ARRAY:
      os << "Array";
      break;
    default:
      break;
  }

  switch (type) {
    case ImageType::SHADOW_2D:
    case ImageType::SHADOW_2D_ARRAY:
    case ImageType::SHADOW_CUBE:
    case ImageType::SHADOW_CUBE_ARRAY:
      os << "Shadow";
      break;
    default:
      break;
  }
  os << " ";
}

static std::ostream &print_qualifier(std::ostream &os, const Qualifier &qualifiers)
{
  if (bool(qualifiers & Qualifier::NO_RESTRICT) == false) {
    os << "restrict ";
  }
  if (bool(qualifiers & Qualifier::READ) == false) {
    os << "writeonly ";
  }
  if (bool(qualifiers & Qualifier::WRITE) == false) {
    os << "readonly ";
  }
  return os;
}

static void print_resource(std::ostream &os,
                           const VKDescriptorSet::Location location,
                           const ShaderCreateInfo::Resource &res)
{
  os << "layout(binding = " << static_cast<uint32_t>(location);
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
                                             StringRef(res.uniformbuf.name.c_str(), array_offset);
      os << "uniform _" << name_no_array << " { " << res.uniformbuf.type_name << " "
         << res.uniformbuf.name << "; };\n";
      break;
    case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      array_offset = res.storagebuf.name.find_first_of("[");
      name_no_array = (array_offset == -1) ? res.storagebuf.name :
                                             StringRef(res.storagebuf.name.c_str(), array_offset);
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
  if (type == shader::Type::MAT4) {
    return 4;
  }
  else if (type == shader::Type::MAT3) {
    return 3;
  }
  return 1;
}

static void print_interface(std::ostream &os,
                            const std::string &prefix,
                            const StageInterfaceInfo &iface,
                            int &location,
                            const StringRefNull &suffix = "")
{
  if (iface.instance_name.is_empty()) {
    for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
      os << "layout(location=" << location << ") " << prefix << " " << to_string(inout.interp)
         << " " << to_string(inout.type) << " " << inout.name << ";\n";
      location += get_location_count(inout.type);
    }
  }
  else {
    std::string struct_name = prefix + iface.name;
    std::string iface_attribute;
    if (iface.instance_name.is_empty()) {
      iface_attribute = "iface_";
    }
    else {
      iface_attribute = iface.instance_name;
    }
    std::string flat = "";
    if (prefix == "in") {
      flat = "flat ";
    }
    const bool add_defines = iface.instance_name.is_empty();

    os << "struct " << struct_name << " {\n";
    for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
      os << "  " << to_string(inout.type) << " " << inout.name << ";\n";
    }
    os << "};\n";
    os << "layout(location=" << location << ") " << prefix << " " << flat << struct_name << " "
       << iface_attribute << suffix << ";\n";

    if (add_defines) {
      for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
        os << "#define " << inout.name << " (" << iface_attribute << "." << inout.name << ")\n";
      }
    }

    for (const StageInterfaceInfo::InOut &inout : iface.inouts) {
      location += get_location_count(inout.type);
    }
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

static const std::string to_stage_name(shaderc_shader_kind stage)
{
  switch (stage) {
    case shaderc_vertex_shader:
      return std::string("vertex");
    case shaderc_geometry_shader:
      return std::string("geometry");
    case shaderc_fragment_shader:
      return std::string("fragment");
    case shaderc_compute_shader:
      return std::string("compute");

    default:
      BLI_assert_msg(false, "Do not know how to convert shaderc_shader_kind to stage name.");
      break;
  }
  return std::string("unknown stage");
}

static char *glsl_patch_get()
{
  static char patch[2048] = "\0";
  if (patch[0] != '\0') {
    return patch;
  }

  size_t slen = 0;
  /* Version need to go first. */
  STR_CONCAT(patch, slen, "#version 450\n");
  STR_CONCAT(patch, slen, "#define gl_VertexID gl_VertexIndex\n");
  STR_CONCAT(patch, slen, "#define gpu_BaseInstance (0)\n");
  STR_CONCAT(patch, slen, "#define gpu_InstanceIndex (gl_InstanceIndex)\n");
  STR_CONCAT(patch, slen, "#define GPU_ARB_texture_cube_map_array\n");

  STR_CONCAT(patch, slen, "#define gl_InstanceID gpu_InstanceIndex\n");

  STR_CONCAT(patch, slen, "#define DFDX_SIGN 1.0\n");
  STR_CONCAT(patch, slen, "#define DFDY_SIGN 1.0\n");

  /* GLSL Backend Lib. */
  STR_CONCAT(patch, slen, datatoc_glsl_shader_defines_glsl);

  BLI_assert(slen < sizeof(patch));
  return patch;
}

static std::string combine_sources(Span<const char *> sources)
{
  char *sources_combined = BLI_string_join_arrayN((const char **)sources.data(), sources.size());
  std::string result(sources_combined);
  MEM_freeN(sources_combined);
  return result;
}

Vector<uint32_t> VKShader::compile_glsl_to_spirv(Span<const char *> sources,
                                                 shaderc_shader_kind stage)
{
  std::string combined_sources = combine_sources(sources);
  VKBackend &backend = VKBackend::get();
  shaderc::Compiler &compiler = backend.get_shaderc_compiler();
  shaderc::CompileOptions options;
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  if (G.debug & G_DEBUG_GPU_RENDERDOC) {
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
    options.SetGenerateDebugInfo();
  }

  shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
      combined_sources, stage, name, options);
  if (module.GetNumErrors() != 0 || module.GetNumWarnings() != 0) {
    std::string log = module.GetErrorMessage();
    Vector<char> logcstr(log.c_str(), log.c_str() + log.size() + 1);

    VKLogParser parser;
    print_log(sources,
              logcstr.data(),
              to_stage_name(stage).c_str(),
              module.GetCompilationStatus() != shaderc_compilation_status_success,
              &parser);
  }

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    compilation_failed_ = true;
    return Vector<uint32_t>();
  }

  return Vector<uint32_t>(module.cbegin(), module.cend());
}

void VKShader::build_shader_module(Span<uint32_t> spirv_module, VkShaderModule *r_shader_module)
{
  VK_ALLOCATION_CALLBACKS;

  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = spirv_module.size() * sizeof(uint32_t);
  create_info.pCode = spirv_module.data();

  const VKDevice &device = VKBackend::get().device_get();
  VkResult result = vkCreateShaderModule(
      device.device_get(), &create_info, vk_allocation_callbacks, r_shader_module);
  if (result != VK_SUCCESS) {
    compilation_failed_ = true;
    *r_shader_module = VK_NULL_HANDLE;
  }
}

VKShader::VKShader(const char *name) : Shader(name)
{
  context_ = VKContext::get();
}

VKShader::~VKShader()
{
  VK_ALLOCATION_CALLBACKS
  const VKDevice &device = VKBackend::get().device_get();
  if (vertex_module_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device.device_get(), vertex_module_, vk_allocation_callbacks);
    vertex_module_ = VK_NULL_HANDLE;
  }
  if (geometry_module_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device.device_get(), geometry_module_, vk_allocation_callbacks);
    geometry_module_ = VK_NULL_HANDLE;
  }
  if (fragment_module_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device.device_get(), fragment_module_, vk_allocation_callbacks);
    fragment_module_ = VK_NULL_HANDLE;
  }
  if (compute_module_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device.device_get(), compute_module_, vk_allocation_callbacks);
    compute_module_ = VK_NULL_HANDLE;
  }
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device.device_get(), pipeline_layout_, vk_allocation_callbacks);
    pipeline_layout_ = VK_NULL_HANDLE;
  }
  if (layout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device.device_get(), layout_, vk_allocation_callbacks);
    layout_ = VK_NULL_HANDLE;
  }
}

void VKShader::build_shader_module(MutableSpan<const char *> sources,
                                   shaderc_shader_kind stage,
                                   VkShaderModule *r_shader_module)
{
  BLI_assert_msg(ELEM(stage,
                      shaderc_vertex_shader,
                      shaderc_geometry_shader,
                      shaderc_fragment_shader,
                      shaderc_compute_shader),
                 "Only forced ShaderC shader kinds are supported.");
  sources[0] = glsl_patch_get();
  Vector<uint32_t> spirv_module = compile_glsl_to_spirv(sources, stage);
  build_shader_module(spirv_module, r_shader_module);
}

void VKShader::vertex_shader_from_glsl(MutableSpan<const char *> sources)
{
  build_shader_module(sources, shaderc_vertex_shader, &vertex_module_);
}

void VKShader::geometry_shader_from_glsl(MutableSpan<const char *> sources)
{
  build_shader_module(sources, shaderc_geometry_shader, &geometry_module_);
}

void VKShader::fragment_shader_from_glsl(MutableSpan<const char *> sources)
{
  build_shader_module(sources, shaderc_fragment_shader, &fragment_module_);
}

void VKShader::compute_shader_from_glsl(MutableSpan<const char *> sources)
{
  build_shader_module(sources, shaderc_compute_shader, &compute_module_);
}

void VKShader::warm_cache(int /*limit*/) {}

bool VKShader::finalize(const shader::ShaderCreateInfo *info)
{
  if (compilation_failed_) {
    return false;
  }

  VKShaderInterface *vk_interface = new VKShaderInterface();
  vk_interface->init(*info);

  const VKDevice &device = VKBackend::get().device_get();
  if (!finalize_descriptor_set_layouts(device.device_get(), *vk_interface, *info)) {
    return false;
  }
  if (!finalize_pipeline_layout(device.device_get(), *vk_interface)) {
    return false;
  }

  /* TODO we might need to move the actual pipeline construction to a later stage as the graphics
   * pipeline requires more data before it can be constructed. */
  bool result;
  if (is_graphics_shader()) {
    BLI_assert((fragment_module_ != VK_NULL_HANDLE && info->tf_type_ == GPU_SHADER_TFB_NONE) ||
               (fragment_module_ == VK_NULL_HANDLE && info->tf_type_ != GPU_SHADER_TFB_NONE));
    BLI_assert(compute_module_ == VK_NULL_HANDLE);
    pipeline_ = VKPipeline::create_graphics_pipeline(layout_,
                                                     vk_interface->push_constants_layout_get());
    result = true;
  }
  else {
    BLI_assert(vertex_module_ == VK_NULL_HANDLE);
    BLI_assert(geometry_module_ == VK_NULL_HANDLE);
    BLI_assert(fragment_module_ == VK_NULL_HANDLE);
    BLI_assert(compute_module_ != VK_NULL_HANDLE);
    pipeline_ = VKPipeline::create_compute_pipeline(
        compute_module_, layout_, pipeline_layout_, vk_interface->push_constants_layout_get());
    result = pipeline_.is_valid();
  }

  if (result) {
    interface = vk_interface;
  }
  else {
    delete vk_interface;
  }
  return result;
}

bool VKShader::finalize_pipeline_layout(VkDevice vk_device,
                                        const VKShaderInterface &shader_interface)
{
  VK_ALLOCATION_CALLBACKS

  const uint32_t layout_count = layout_ == VK_NULL_HANDLE ? 0 : 1;
  VkPipelineLayoutCreateInfo pipeline_info = {};
  VkPushConstantRange push_constant_range = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_info.flags = 0;
  pipeline_info.setLayoutCount = layout_count;
  pipeline_info.pSetLayouts = &layout_;

  /* Setup push constants. */
  const VKPushConstants::Layout &push_constants_layout =
      shader_interface.push_constants_layout_get();
  if (push_constants_layout.storage_type_get() == VKPushConstants::StorageType::PUSH_CONSTANTS) {
    push_constant_range.offset = 0;
    push_constant_range.size = push_constants_layout.size_in_bytes();
    push_constant_range.stageFlags = VK_SHADER_STAGE_ALL;
    pipeline_info.pushConstantRangeCount = 1;
    pipeline_info.pPushConstantRanges = &push_constant_range;
  }

  if (vkCreatePipelineLayout(
          vk_device, &pipeline_info, vk_allocation_callbacks, &pipeline_layout_) != VK_SUCCESS)
  {
    return false;
  };

  return true;
}

static VkDescriptorType storage_descriptor_type(const shader::ImageType &image_type)
{
  switch (image_type) {
    case shader::ImageType::FLOAT_1D:
    case shader::ImageType::FLOAT_1D_ARRAY:
    case shader::ImageType::FLOAT_2D:
    case shader::ImageType::FLOAT_2D_ARRAY:
    case shader::ImageType::FLOAT_3D:
    case shader::ImageType::FLOAT_CUBE:
    case shader::ImageType::FLOAT_CUBE_ARRAY:
    case shader::ImageType::INT_1D:
    case shader::ImageType::INT_1D_ARRAY:
    case shader::ImageType::INT_2D:
    case shader::ImageType::INT_2D_ARRAY:
    case shader::ImageType::INT_3D:
    case shader::ImageType::INT_CUBE:
    case shader::ImageType::INT_CUBE_ARRAY:
    case shader::ImageType::UINT_1D:
    case shader::ImageType::UINT_1D_ARRAY:
    case shader::ImageType::UINT_2D:
    case shader::ImageType::UINT_2D_ARRAY:
    case shader::ImageType::UINT_3D:
    case shader::ImageType::UINT_CUBE:
    case shader::ImageType::UINT_CUBE_ARRAY:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    case shader::ImageType::FLOAT_BUFFER:
    case shader::ImageType::INT_BUFFER:
    case shader::ImageType::UINT_BUFFER:
      return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

    default:
      BLI_assert_msg(false, "ImageType not supported.");
  }

  return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
}

static VkDescriptorType sampler_descriptor_type(const shader::ImageType &image_type)
{
  switch (image_type) {
    case shader::ImageType::FLOAT_1D:
    case shader::ImageType::FLOAT_1D_ARRAY:
    case shader::ImageType::FLOAT_2D:
    case shader::ImageType::FLOAT_2D_ARRAY:
    case shader::ImageType::FLOAT_3D:
    case shader::ImageType::FLOAT_CUBE:
    case shader::ImageType::FLOAT_CUBE_ARRAY:
    case shader::ImageType::INT_1D:
    case shader::ImageType::INT_1D_ARRAY:
    case shader::ImageType::INT_2D:
    case shader::ImageType::INT_2D_ARRAY:
    case shader::ImageType::INT_3D:
    case shader::ImageType::INT_CUBE:
    case shader::ImageType::INT_CUBE_ARRAY:
    case shader::ImageType::UINT_1D:
    case shader::ImageType::UINT_1D_ARRAY:
    case shader::ImageType::UINT_2D:
    case shader::ImageType::UINT_2D_ARRAY:
    case shader::ImageType::UINT_3D:
    case shader::ImageType::UINT_CUBE:
    case shader::ImageType::UINT_CUBE_ARRAY:
    case shader::ImageType::SHADOW_2D:
    case shader::ImageType::SHADOW_2D_ARRAY:
    case shader::ImageType::SHADOW_CUBE:
    case shader::ImageType::SHADOW_CUBE_ARRAY:
    case shader::ImageType::DEPTH_2D:
    case shader::ImageType::DEPTH_2D_ARRAY:
    case shader::ImageType::DEPTH_CUBE:
    case shader::ImageType::DEPTH_CUBE_ARRAY:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    case shader::ImageType::FLOAT_BUFFER:
    case shader::ImageType::INT_BUFFER:
    case shader::ImageType::UINT_BUFFER:
      return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
  }

  return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
}

static VkDescriptorType descriptor_type(const shader::ShaderCreateInfo::Resource &resource)
{
  switch (resource.bind_type) {
    case shader::ShaderCreateInfo::Resource::BindType::IMAGE:
      return storage_descriptor_type(resource.image.type);
    case shader::ShaderCreateInfo::Resource::BindType::SAMPLER:
      return sampler_descriptor_type(resource.sampler.type);
    case shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }
  BLI_assert_unreachable();
  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

static VkDescriptorSetLayoutBinding create_descriptor_set_layout_binding(
    const VKDescriptorSet::Location location, const shader::ShaderCreateInfo::Resource &resource)
{
  VkDescriptorSetLayoutBinding binding = {};
  binding.binding = location;
  binding.descriptorType = descriptor_type(resource);
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_ALL;
  binding.pImmutableSamplers = nullptr;

  return binding;
}

static VkDescriptorSetLayoutBinding create_descriptor_set_layout_binding(
    const VKPushConstants::Layout &push_constants_layout)
{
  BLI_assert(push_constants_layout.storage_type_get() ==
             VKPushConstants::StorageType::UNIFORM_BUFFER);
  VkDescriptorSetLayoutBinding binding = {};
  binding.binding = push_constants_layout.descriptor_set_location_get();
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_ALL;
  binding.pImmutableSamplers = nullptr;

  return binding;
}

static void add_descriptor_set_layout_bindings(
    const VKShaderInterface &interface,
    const Vector<shader::ShaderCreateInfo::Resource> &resources,
    Vector<VkDescriptorSetLayoutBinding> &r_bindings)
{
  for (const shader::ShaderCreateInfo::Resource &resource : resources) {
    const VKDescriptorSet::Location location = interface.descriptor_set_location(resource);
    r_bindings.append(create_descriptor_set_layout_binding(location, resource));
  }

  /* Add push constants to the descriptor when push constants are stored in an uniform buffer. */
  const VKPushConstants::Layout &push_constants_layout = interface.push_constants_layout_get();
  if (push_constants_layout.storage_type_get() == VKPushConstants::StorageType::UNIFORM_BUFFER) {
    r_bindings.append(create_descriptor_set_layout_binding(push_constants_layout));
  }
}

static VkDescriptorSetLayoutCreateInfo create_descriptor_set_layout(
    const VKShaderInterface &interface,
    const Vector<shader::ShaderCreateInfo::Resource> &resources,
    Vector<VkDescriptorSetLayoutBinding> &r_bindings)
{
  add_descriptor_set_layout_bindings(interface, resources, r_bindings);
  VkDescriptorSetLayoutCreateInfo set_info = {};
  set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  set_info.flags = 0;
  set_info.pNext = nullptr;
  set_info.bindingCount = r_bindings.size();
  set_info.pBindings = r_bindings.data();
  return set_info;
}

static bool descriptor_sets_needed(const VKShaderInterface &shader_interface,
                                   const shader::ShaderCreateInfo &info)
{
  return !info.pass_resources_.is_empty() || !info.batch_resources_.is_empty() ||
         shader_interface.push_constants_layout_get().storage_type_get() ==
             VKPushConstants::StorageType::UNIFORM_BUFFER;
}

bool VKShader::finalize_descriptor_set_layouts(VkDevice vk_device,
                                               const VKShaderInterface &shader_interface,
                                               const shader::ShaderCreateInfo &info)
{
  if (!descriptor_sets_needed(shader_interface, info)) {
    return true;
  }

  VK_ALLOCATION_CALLBACKS

  /* Currently we create a single descriptor set. The goal would be to create one descriptor set
   * for #Frequency::PASS/BATCH. This isn't possible as areas expect that the binding location is
   * static and predictable (EEVEE-NEXT) or the binding location can be mapped to a single number
   * (Python). */
  Vector<ShaderCreateInfo::Resource> all_resources;
  all_resources.extend(info.pass_resources_);
  all_resources.extend(info.batch_resources_);

  Vector<VkDescriptorSetLayoutBinding> bindings;
  VkDescriptorSetLayoutCreateInfo layout_info = create_descriptor_set_layout(
      shader_interface, all_resources, bindings);
  if (vkCreateDescriptorSetLayout(vk_device, &layout_info, vk_allocation_callbacks, &layout_) !=
      VK_SUCCESS)
  {
    return false;
  };
  debug::object_label(layout_, name_get());

  return true;
}

void VKShader::transform_feedback_names_set(Span<const char *> /*name_list*/,
                                            eGPUShaderTFBType /*geom_type*/)
{
}

bool VKShader::transform_feedback_enable(GPUVertBuf *)
{
  return false;
}

void VKShader::transform_feedback_disable() {}

void VKShader::update_graphics_pipeline(VKContext &context,
                                        const GPUPrimType prim_type,
                                        const VKVertexAttributeObject &vertex_attribute_object)
{
  BLI_assert(is_graphics_shader());
  pipeline_get().finalize(context,
                          vertex_module_,
                          geometry_module_,
                          fragment_module_,
                          pipeline_layout_,
                          prim_type,
                          vertex_attribute_object);
}

void VKShader::bind()
{
  /* Intentionally empty. Binding of the pipeline are done just before drawing/dispatching.
   * See #VKPipeline.update_and_bind */
}

void VKShader::unbind() {}

void VKShader::uniform_float(int location, int comp_len, int array_size, const float *data)
{
  VKPushConstants &push_constants = pipeline_get().push_constants_get();
  push_constants.push_constant_set(location, comp_len, array_size, data);
}

void VKShader::uniform_int(int location, int comp_len, int array_size, const int *data)
{
  VKPushConstants &push_constants = pipeline_get().push_constants_get();
  push_constants.push_constant_set(location, comp_len, array_size, data);
}

std::string VKShader::resources_declare(const shader::ShaderCreateInfo &info) const
{
  VKShaderInterface interface;
  interface.init(info);
  std::stringstream ss;

  ss << "\n/* Pass Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource(ss, interface, res);
  }

  ss << "\n/* Batch Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource(ss, interface, res);
  }

  /* Push constants. */
  const VKPushConstants::Layout &push_constants_layout = interface.push_constants_layout_get();
  const VKPushConstants::StorageType push_constants_storage =
      push_constants_layout.storage_type_get();
  if (push_constants_storage != VKPushConstants::StorageType::NONE) {
    ss << "\n/* Push Constants. */\n";
    if (push_constants_storage == VKPushConstants::StorageType::PUSH_CONSTANTS) {
      ss << "layout(push_constant) uniform constants\n";
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
  if (bool(info.builtins_ & BuiltinBits::BARYCENTRIC_COORD)) {
    /* Need this for stable barycentric. */
    ss << "flat out vec4 gpu_pos_flat;\n";
    ss << "out vec4 gpu_pos;\n";

    post_main += "  gpu_pos = gpu_pos_flat = gl_Position;\n";
  }
  ss << "\n";

  if (post_main.empty() == false) {
    std::string pre_main;
    ss << main_function_wrapper(pre_main, post_main);
  }
  return ss.str();
}

std::string VKShader::fragment_interface_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;
  std::string pre_main;

  ss << "\n/* Interfaces. */\n";
  const Vector<StageInterfaceInfo *> &in_interfaces = info.geometry_source_.is_empty() ?
                                                          info.vertex_out_interfaces_ :
                                                          info.geometry_out_interfaces_;
  int location = 0;
  for (const StageInterfaceInfo *iface : in_interfaces) {
    print_interface(ss, "in", *iface, location);
  }
  if (bool(info.builtins_ & BuiltinBits::BARYCENTRIC_COORD)) {
    std::cout << "native" << std::endl;
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
    ss << "vec4 gpu_position_at_vertex(int v) {\n";
    ss << "  if (interpolateAtVertexAMD(gpu_pos, 0) == gpu_pos_flat) { v = (v + 2) % 3; }\n";
    ss << "  if (interpolateAtVertexAMD(gpu_pos, 2) == gpu_pos_flat) { v = (v + 1) % 3; }\n";
    ss << "  return interpolateAtVertexAMD(gpu_pos, v);\n";
    ss << "}\n";

    pre_main += "  gpu_BaryCoord = stable_bary_(gl_BaryCoordSmoothAMD);\n";
    pre_main += "  gpu_BaryCoordNoPersp = stable_bary_(gl_BaryCoordNoPerspAMD);\n";
  }
  if (info.early_fragment_test_) {
    ss << "layout(early_fragment_tests) in;\n";
  }
  ss << "layout(" << to_string(info.depth_write_) << ") out float gl_FragDepth;\n";
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

static StageInterfaceInfo *find_interface_by_name(const Vector<StageInterfaceInfo *> &ifaces,
                                                  const StringRefNull &name)
{
  for (auto *iface : ifaces) {
    if (iface->instance_name == name) {
      return iface;
    }
  }
  return nullptr;
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

  return ss.str();
}

std::string VKShader::compute_layout_declare(const shader::ShaderCreateInfo &info) const
{
  std::stringstream ss;
  ss << "\n/* Compute Layout. */\n";
  ss << "layout(local_size_x = " << info.compute_layout_.local_size_x;
  if (info.compute_layout_.local_size_y != -1) {
    ss << ", local_size_y = " << info.compute_layout_.local_size_y;
  }
  if (info.compute_layout_.local_size_z != -1) {
    ss << ", local_size_z = " << info.compute_layout_.local_size_z;
  }
  ss << ") in;\n";
  ss << "\n";
  return ss.str();
}

int VKShader::program_handle_get() const
{
  return -1;
}

VKPipeline &VKShader::pipeline_get()
{
  return pipeline_;
}

const VKShaderInterface &VKShader::interface_get() const
{
  BLI_assert_msg(interface != nullptr,
                 "Unable to access the shader interface when finalizing the shader, use the "
                 "instance created in the finalize method.");
  return *static_cast<const VKShaderInterface *>(interface);
}

}  // namespace blender::gpu
