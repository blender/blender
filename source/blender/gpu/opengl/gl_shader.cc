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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.h"

#include "BLI_string.h"
#include "BLI_vector.hh"

#include "GPU_capabilities.h"
#include "GPU_platform.h"

#include "gl_backend.hh"
#include "gl_debug.hh"
#include "gl_vertex_buffer.hh"

#include "gl_shader.hh"
#include "gl_shader_interface.hh"

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
  shader_program_ = glCreateProgram();

  debug::object_label(GL_PROGRAM, shader_program_, name);
}

GLShader::~GLShader()
{
#if 0 /* Would be nice to have, but for now the Deferred compilation \
       * does not have a GPUContext. */
  BLI_assert(GLContext::get() != nullptr);
#endif
  /* Invalid handles are silently ignored. */
  glDeleteShader(vert_shader_);
  glDeleteShader(geom_shader_);
  glDeleteShader(frag_shader_);
  glDeleteShader(compute_shader_);
  glDeleteProgram(shader_program_);
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
      return "unkown";
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
      return "unkown";
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
  if ((qualifiers & Qualifier::RESTRICT) == Qualifier::RESTRICT) {
    os << "restrict ";
  }
  if ((qualifiers & Qualifier::READ_ONLY) == Qualifier::READ_ONLY) {
    os << "readonly ";
  }
  if ((qualifiers & Qualifier::WRITE_ONLY) == Qualifier::WRITE_ONLY) {
    os << "writeonly ";
  }
  return os;
}

static void print_resource(std::ostream &os, const ShaderCreateInfo::Resource &res)
{
  if (GLContext::explicit_location_support) {
    os << "layout(binding = " << res.slot;
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::IMAGE) {
      os << ", " << res.image.format;
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
  /* TODO(fclem) Move that to interface check. */
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

  /* NOTE: We define macros in GLSL to trigger compilation error if the resource names
   * are reused for local variables. This is to match other backend behavior which needs accessors
   * macros. */

  ss << "\n/* Pass Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource(ss, res);
  }
  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource_alias(ss, res);
  }
  ss << "\n/* Batch Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource(ss, res);
  }
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource_alias(ss, res);
  }
  ss << "\n/* Push Constants. */\n";
  for (const ShaderCreateInfo::PushConst &uniform : info.push_constants_) {
    ss << "uniform " << to_string(uniform.type) << " " << uniform.name;
    if (uniform.array_size > 0) {
      ss << "[" << uniform.array_size << "]";
    }
    ss << ";\n";
  }
#if 0 /* T95278: This is not be enough to prevent some compilers think it is recursive. */
  for (const ShaderCreateInfo::PushConst &uniform : info.push_constants_) {
    /* T95278: Double macro to avoid some compilers think it is recursive. */
    ss << "#define " << uniform.name << "_ " << uniform.name << "\n";
    ss << "#define " << uniform.name << " (" << uniform.name << "_)\n";
  }
#endif
  ss << "\n";
  return ss.str();
}

std::string GLShader::vertex_interface_declare(const ShaderCreateInfo &info) const
{
  std::stringstream ss;

  ss << "\n/* Inputs. */\n";
  for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
    if (GLContext::explicit_location_support &&
        /* Fix issue with amdgpu-pro + workbench_prepass_mesh_vert.glsl being quantized. */
        GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) == false) {
      ss << "layout(location = " << attr.index << ") ";
    }
    ss << "in " << to_string(attr.type) << " " << attr.name << ";\n";
  }
  ss << "\n/* Interfaces. */\n";
  for (const StageInterfaceInfo *iface : info.vertex_out_interfaces_) {
    print_interface(ss, "out", *iface);
  }
  ss << "\n";
  return ss.str();
}

std::string GLShader::fragment_interface_declare(const ShaderCreateInfo &info) const
{
  std::stringstream ss;
  ss << "\n/* Interfaces. */\n";
  const Vector<StageInterfaceInfo *> &in_interfaces = (info.geometry_source_.is_empty()) ?
                                                          info.vertex_out_interfaces_ :
                                                          info.geometry_out_interfaces_;
  for (const StageInterfaceInfo *iface : in_interfaces) {
    print_interface(ss, "in", *iface);
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
  return ss.str();
}

std::string GLShader::geometry_layout_declare(const ShaderCreateInfo &info) const
{
  int max_verts = info.geometry_layout_.max_vertices;
  int invocations = info.geometry_layout_.invocations;

  if (GLContext::geometry_shader_invocations == false && invocations != -1) {
    max_verts *= invocations;
    invocations = -1;
  }

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
    if (iface->name == name) {
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
  ss << "layout(local_size_x = " << info.compute_layout_.local_size_x;
  if (info.compute_layout_.local_size_y != -1) {
    ss << ", local_size_y = " << info.compute_layout_.local_size_y;
  }
  if (info.compute_layout_.local_size_z != -1) {
    ss << ", local_size_y = " << info.compute_layout_.local_size_z;
  }
  ss << ") in;\n";
  ss << "\n";
  return ss.str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader stage creation
 * \{ */

static char *glsl_patch_default_get()
{
  /** Used for shader patching. Init once. */
  static char patch[700] = "\0";
  if (patch[0] != '\0') {
    return patch;
  }

  size_t slen = 0;
  /* Version need to go first. */
  if (GLEW_VERSION_4_3) {
    STR_CONCAT(patch, slen, "#version 430\n");
  }
  else {
    STR_CONCAT(patch, slen, "#version 330\n");
  }

  /* Enable extensions for features that are not part of our base GLSL version
   * don't use an extension for something already available! */
  if (GLContext::texture_gather_support) {
    STR_CONCAT(patch, slen, "#extension GL_ARB_texture_gather: enable\n");
    /* Some drivers don't agree on GLEW_ARB_texture_gather and the actual support in the
     * shader so double check the preprocessor define (see T56544). */
    STR_CONCAT(patch, slen, "#ifdef GL_ARB_texture_gather\n");
    STR_CONCAT(patch, slen, "#  define GPU_ARB_texture_gather\n");
    STR_CONCAT(patch, slen, "#endif\n");
  }
  if (GLContext::shader_draw_parameters_support) {
    STR_CONCAT(patch, slen, "#extension GL_ARB_shader_draw_parameters : enable\n");
    STR_CONCAT(patch, slen, "#define GPU_ARB_shader_draw_parameters\n");
    STR_CONCAT(patch, slen, "#define gpu_BaseInstance gl_BaseInstanceARB\n");
  }
  if (GLContext::geometry_shader_invocations) {
    STR_CONCAT(patch, slen, "#extension GL_ARB_gpu_shader5 : enable\n");
    STR_CONCAT(patch, slen, "#define GPU_ARB_gpu_shader5\n");
  }
  if (GLContext::texture_cube_map_array_support) {
    STR_CONCAT(patch, slen, "#extension GL_ARB_texture_cube_map_array : enable\n");
    STR_CONCAT(patch, slen, "#define GPU_ARB_texture_cube_map_array\n");
  }
  if (GLEW_ARB_conservative_depth) {
    STR_CONCAT(patch, slen, "#extension GL_ARB_conservative_depth : enable\n");
  }
  if (GPU_shader_image_load_store_support()) {
    STR_CONCAT(patch, slen, "#extension GL_ARB_shader_image_load_store: enable\n");
    STR_CONCAT(patch, slen, "#extension GL_ARB_shading_language_420pack: enable\n");
  }

  /* Fallbacks. */
  if (!GLContext::shader_draw_parameters_support) {
    STR_CONCAT(patch, slen, "uniform int gpu_BaseInstance;\n");
  }

  /* Vulkan GLSL compat. */
  STR_CONCAT(patch, slen, "#define gpu_InstanceIndex (gl_InstanceID + gpu_BaseInstance)\n");

  /* Derivative sign can change depending on implementation. */
  STR_CONCATF(patch, slen, "#define DFDX_SIGN %1.1f\n", GLContext::derivative_signs[0]);
  STR_CONCATF(patch, slen, "#define DFDY_SIGN %1.1f\n", GLContext::derivative_signs[1]);

  BLI_assert(slen < sizeof(patch));
  return patch;
}

static char *glsl_patch_compute_get()
{
  /** Used for shader patching. Init once. */
  static char patch[512] = "\0";
  if (patch[0] != '\0') {
    return patch;
  }

  size_t slen = 0;
  /* Version need to go first. */
  STR_CONCAT(patch, slen, "#version 430\n");
  STR_CONCAT(patch, slen, "#extension GL_ARB_compute_shader :enable\n");
  BLI_assert(slen < sizeof(patch));
  return patch;
}

char *GLShader::glsl_patch_get(GLenum gl_stage)
{
  if (gl_stage == GL_COMPUTE_SHADER) {
    return glsl_patch_compute_get();
  }
  return glsl_patch_default_get();
}

GLuint GLShader::create_shader_stage(GLenum gl_stage, MutableSpan<const char *> sources)
{
  GLuint shader = glCreateShader(gl_stage);
  if (shader == 0) {
    fprintf(stderr, "GLShader: Error: Could not create shader object.");
    return 0;
  }

  /* Patch the shader code using the first source slot. */
  sources[0] = glsl_patch_get(gl_stage);

  glShaderSource(shader, sources.size(), sources.data(), nullptr);
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

  glAttachShader(shader_program_, shader);
  return shader;
}

void GLShader::vertex_shader_from_glsl(MutableSpan<const char *> sources)
{
  vert_shader_ = this->create_shader_stage(GL_VERTEX_SHADER, sources);
}

void GLShader::geometry_shader_from_glsl(MutableSpan<const char *> sources)
{
  geom_shader_ = this->create_shader_stage(GL_GEOMETRY_SHADER, sources);
}

void GLShader::fragment_shader_from_glsl(MutableSpan<const char *> sources)
{
  frag_shader_ = this->create_shader_stage(GL_FRAGMENT_SHADER, sources);
}

void GLShader::compute_shader_from_glsl(MutableSpan<const char *> sources)
{
  compute_shader_ = this->create_shader_stage(GL_COMPUTE_SHADER, sources);
}

bool GLShader::finalize(const shader::ShaderCreateInfo *info)
{
  if (compilation_failed_) {
    return false;
  }

  glLinkProgram(shader_program_);

  GLint status;
  glGetProgramiv(shader_program_, GL_LINK_STATUS, &status);
  if (!status) {
    char log[5000];
    glGetProgramInfoLog(shader_program_, sizeof(log), nullptr, log);
    Span<const char *> sources;
    GLLogParser parser;
    this->print_log(sources, log, "Linking", true, &parser);
    return false;
  }

  if (info != nullptr) {
    interface = new GLShaderInterface(shader_program_, *info);
  }
  else {
    interface = new GLShaderInterface(shader_program_);
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

void GLShader::bind()
{
  BLI_assert(shader_program_ != 0);
  glUseProgram(shader_program_);
}

void GLShader::unbind()
{
#ifndef NDEBUG
  glUseProgram(0);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform feedback
 *
 * TODO(fclem): Should be replaced by compute shaders.
 * \{ */

void GLShader::transform_feedback_names_set(Span<const char *> name_list,
                                            const eGPUShaderTFBType geom_type)
{
  glTransformFeedbackVaryings(
      shader_program_, name_list.size(), name_list.data(), GL_INTERLEAVED_ATTRIBS);
  transform_feedback_type_ = geom_type;
}

bool GLShader::transform_feedback_enable(GPUVertBuf *buf_)
{
  if (transform_feedback_type_ == GPU_SHADER_TFB_NONE) {
    return false;
  }

  GLVertBuf *buf = static_cast<GLVertBuf *>(unwrap(buf_));

  BLI_assert(buf->vbo_id_ != 0);

  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf->vbo_id_);

  switch (transform_feedback_type_) {
    case GPU_SHADER_TFB_POINTS:
      glBeginTransformFeedback(GL_POINTS);
      break;
    case GPU_SHADER_TFB_LINES:
      glBeginTransformFeedback(GL_LINES);
      break;
    case GPU_SHADER_TFB_TRIANGLES:
      glBeginTransformFeedback(GL_TRIANGLES);
      break;
    default:
      return false;
  }
  return true;
}

void GLShader::transform_feedback_disable()
{
  glEndTransformFeedback();
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
/** \name GPUVertFormat from Shader
 * \{ */

static uint calc_component_size(const GLenum gl_type)
{
  switch (gl_type) {
    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
    case GL_UNSIGNED_INT_VEC2:
      return 2;
    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
    case GL_UNSIGNED_INT_VEC3:
      return 3;
    case GL_FLOAT_VEC4:
    case GL_FLOAT_MAT2:
    case GL_INT_VEC4:
    case GL_UNSIGNED_INT_VEC4:
      return 4;
    case GL_FLOAT_MAT3:
      return 9;
    case GL_FLOAT_MAT4:
      return 16;
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2:
      return 6;
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2:
      return 8;
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3:
      return 12;
    default:
      return 1;
  }
}

static void get_fetch_mode_and_comp_type(int gl_type,
                                         GPUVertCompType *r_comp_type,
                                         GPUVertFetchMode *r_fetch_mode)
{
  switch (gl_type) {
    case GL_FLOAT:
    case GL_FLOAT_VEC2:
    case GL_FLOAT_VEC3:
    case GL_FLOAT_VEC4:
    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT3:
    case GL_FLOAT_MAT4:
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT3x2:
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x2:
    case GL_FLOAT_MAT4x3:
      *r_comp_type = GPU_COMP_F32;
      *r_fetch_mode = GPU_FETCH_FLOAT;
      break;
    case GL_INT:
    case GL_INT_VEC2:
    case GL_INT_VEC3:
    case GL_INT_VEC4:
      *r_comp_type = GPU_COMP_I32;
      *r_fetch_mode = GPU_FETCH_INT;
      break;
    case GL_UNSIGNED_INT:
    case GL_UNSIGNED_INT_VEC2:
    case GL_UNSIGNED_INT_VEC3:
    case GL_UNSIGNED_INT_VEC4:
      *r_comp_type = GPU_COMP_U32;
      *r_fetch_mode = GPU_FETCH_INT;
      break;
    default:
      BLI_assert(0);
  }
}

void GLShader::vertformat_from_shader(GPUVertFormat *format) const
{
  GPU_vertformat_clear(format);

  GLint attr_len;
  glGetProgramiv(shader_program_, GL_ACTIVE_ATTRIBUTES, &attr_len);

  for (int i = 0; i < attr_len; i++) {
    char name[256];
    GLenum gl_type;
    GLint size;
    glGetActiveAttrib(shader_program_, i, sizeof(name), nullptr, &size, &gl_type, name);

    /* Ignore OpenGL names like `gl_BaseInstanceARB`, `gl_InstanceID` and `gl_VertexID`. */
    if (glGetAttribLocation(shader_program_, name) == -1) {
      continue;
    }

    GPUVertCompType comp_type;
    GPUVertFetchMode fetch_mode;
    get_fetch_mode_and_comp_type(gl_type, &comp_type, &fetch_mode);

    int comp_len = calc_component_size(gl_type) * size;

    GPU_vertformat_attr_add(format, name, comp_type, comp_len, fetch_mode);
  }
}

int GLShader::program_handle_get() const
{
  return (int)this->shader_program_;
}

/** \} */
