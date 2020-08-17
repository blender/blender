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

#include "GPU_extensions.h"
#include "GPU_platform.h"

#include "gl_shader.hh"

using namespace blender;
using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

GLShader::GLShader(const char *name) : Shader(name)
{
#if 0 /* Would be nice to have, but for now the Deferred compilation \
       * does not have a GPUContext. */
  BLI_assert(GPU_context_active_get() != NULL);
#endif
  shader_program_ = glCreateProgram();

#ifndef __APPLE__
  if ((G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    char sh_name[64];
    BLI_snprintf(sh_name, sizeof(sh_name), "ShaderProgram-%s", name);
    glObjectLabel(GL_PROGRAM, shader_program_, -1, sh_name);
  }
#endif
}

GLShader::~GLShader(void)
{
#if 0 /* Would be nice to have, but for now the Deferred compilation \
       * does not have a GPUContext. */
  BLI_assert(GPU_context_active_get() != NULL);
#endif
  /* Invalid handles are silently ignored. */
  glDeleteShader(vert_shader_);
  glDeleteShader(geom_shader_);
  glDeleteShader(frag_shader_);
  glDeleteProgram(shader_program_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader stage creation
 * \{ */

char *GLShader::glsl_patch_get(void)
{
  /** Used for shader patching. Init once. */
  static char patch[512] = "\0";
  if (patch[0] != '\0') {
    return patch;
  }

  size_t slen = 0;
  /* Version need to go first. */
  STR_CONCAT(patch, slen, "#version 330\n");

  /* Enable extensions for features that are not part of our base GLSL version
   * don't use an extension for something already available! */
  if (GLEW_ARB_texture_gather) {
    /* There is a bug on older Nvidia GPU where GL_ARB_texture_gather
     * is reported to be supported but yield a compile error (see T55802). */
    if (!GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY) || GLEW_VERSION_4_0) {
      STR_CONCAT(patch, slen, "#extension GL_ARB_texture_gather: enable\n");

      /* Some drivers don't agree on GLEW_ARB_texture_gather and the actual support in the
       * shader so double check the preprocessor define (see T56544). */
      if (!GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY) && !GLEW_VERSION_4_0) {
        STR_CONCAT(patch, slen, "#ifdef GL_ARB_texture_gather\n");
        STR_CONCAT(patch, slen, "#  define GPU_ARB_texture_gather\n");
        STR_CONCAT(patch, slen, "#endif\n");
      }
      else {
        STR_CONCAT(patch, slen, "#define GPU_ARB_texture_gather\n");
      }
    }
  }
  if (GLEW_ARB_shader_draw_parameters) {
    STR_CONCAT(patch, slen, "#extension GL_ARB_shader_draw_parameters : enable\n");
    STR_CONCAT(patch, slen, "#define GPU_ARB_shader_draw_parameters\n");
  }
  if (GPU_arb_texture_cube_map_array_is_supported()) {
    STR_CONCAT(patch, slen, "#extension GL_ARB_texture_cube_map_array : enable\n");
    STR_CONCAT(patch, slen, "#define GPU_ARB_texture_cube_map_array\n");
  }

  /* Derivative sign can change depending on implementation. */
  float derivatives[2];
  GPU_get_dfdy_factors(derivatives);
  STR_CONCATF(patch, slen, "#define DFDX_SIGN %1.1f\n", derivatives[0]);
  STR_CONCATF(patch, slen, "#define DFDY_SIGN %1.1f\n", derivatives[1]);

  BLI_assert(slen < sizeof(patch));
  return patch;
}

/* Create, compile and attach the shader stage to the shader program. */
GLuint GLShader::create_shader_stage(GLenum gl_stage, MutableSpan<const char *> sources)
{
  GLuint shader = glCreateShader(gl_stage);
  if (shader == 0) {
    fprintf(stderr, "GLShader: Error: Could not create shader object.");
    return 0;
  }

  /* Patch the shader code using the first source slot. */
  sources[0] = glsl_patch_get();

  glShaderSource(shader, sources.size(), sources.data(), NULL);
  glCompileShader(shader);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status || (G.debug & G_DEBUG_GPU)) {
    char log[5000] = "";
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    if (log[0] != '\0') {
      this->print_errors(sources, log);
    }
  }
  if (!status) {
    glDeleteShader(shader);
    return 0;
  }

#ifndef __APPLE__
  if ((G.debug & G_DEBUG_GPU) && (GLEW_VERSION_4_3 || GLEW_KHR_debug)) {
    char sh_name[64];
    switch (gl_stage) {
      case GL_VERTEX_SHADER:
        BLI_snprintf(sh_name, sizeof(sh_name), "VertShader-%s", name);
        break;
      case GL_GEOMETRY_SHADER:
        BLI_snprintf(sh_name, sizeof(sh_name), "GeomShader-%s", name);
        break;
      case GL_FRAGMENT_SHADER:
        BLI_snprintf(sh_name, sizeof(sh_name), "FragShader-%s", name);
        break;
    }
    glObjectLabel(GL_SHADER, shader, -1, sh_name);
  }
#endif

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

bool GLShader::finalize(void)
{
  glLinkProgram(shader_program_);

  GLint status;
  glGetProgramiv(shader_program_, GL_LINK_STATUS, &status);
  if (!status) {
    char log[5000];
    glGetProgramInfoLog(shader_program_, sizeof(log), NULL, log);
    fprintf(stderr, "\nLinking Error:\n\n%s", log);
    return false;
  }

  /* TODO(fclem) We need this to modify the image binding points using glUniform.
   * This could be avoided using glProgramUniform in GL 4.1. */
  glUseProgram(shader_program_);
  interface = GPU_shaderinterface_create(shader_program_);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

void GLShader::bind(void)
{
  BLI_assert(shader_program_ != 0);
  glUseProgram(shader_program_);
}

void GLShader::unbind(void)
{
#ifndef NDEBUG
  glUseProgram(0);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform feedback
 *
 * TODO(fclem) Should be replaced by compute shaders.
 * \{ */

/* Should be called before linking. */
void GLShader::transform_feedback_names_set(Span<const char *> name_list,
                                            const eGPUShaderTFBType geom_type)
{
  glTransformFeedbackVaryings(
      shader_program_, name_list.size(), name_list.data(), GL_INTERLEAVED_ATTRIBS);
  transform_feedback_type_ = geom_type;
}

bool GLShader::transform_feedback_enable(GPUVertBuf *buf)
{
  if (transform_feedback_type_ == GPU_SHADER_TFB_NONE) {
    return false;
  }

  BLI_assert(buf->vbo_id != 0);

  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf->vbo_id);

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

void GLShader::transform_feedback_disable(void)
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
    glGetActiveAttrib(shader_program_, i, sizeof(name), NULL, &size, &gl_type, name);

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

/** \} */
