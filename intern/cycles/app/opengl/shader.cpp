/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "app/opengl/shader.h"

#include "util/log.h"
#include "util/string.h"

#include <epoxy/gl.h>

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * OpenGLShader.
 */

static const char *VERTEX_SHADER =
    "#version 330\n"
    "uniform vec2 fullscreen;\n"
    "in vec2 texCoord;\n"
    "in vec2 pos;\n"
    "out vec2 texCoord_interp;\n"
    "\n"
    "vec2 normalize_coordinates()\n"
    "{\n"
    "   return (vec2(2.0) * (pos / fullscreen)) - vec2(1.0);\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(normalize_coordinates(), 0.0, 1.0);\n"
    "   texCoord_interp = texCoord;\n"
    "}\n\0";

static const char *FRAGMENT_SHADER =
    "#version 330\n"
    "uniform sampler2D image_texture;\n"
    "in vec2 texCoord_interp;\n"
    "out vec4 fragColor;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   vec4 rgba = texture(image_texture, texCoord_interp);\n"
    /* Hard-coded Rec.709 gamma, should use OpenColorIO eventually. */
    "   fragColor = pow(rgba, vec4(0.45, 0.45, 0.45, 1.0));\n"
    "}\n\0";

static void shader_print_errors(const char *task, const char *log, const char *code)
{
  LOG(ERROR) << "Shader: " << task << " error:";
  LOG(ERROR) << "===== shader string ====";

  std::stringstream stream(code);
  string partial;

  int line = 1;
  while (getline(stream, partial, '\n')) {
    if (line < 10) {
      LOG(ERROR) << " " << line << " " << partial;
    }
    else {
      LOG(ERROR) << line << " " << partial;
    }
    line++;
  }
  LOG(ERROR) << log;
}

static int compile_shader_program(void)
{
  const struct Shader {
    const char *source;
    const GLenum type;
  } shaders[2] = {{VERTEX_SHADER, GL_VERTEX_SHADER}, {FRAGMENT_SHADER, GL_FRAGMENT_SHADER}};

  const GLuint program = glCreateProgram();

  for (int i = 0; i < 2; i++) {
    const GLuint shader = glCreateShader(shaders[i].type);

    string source_str = shaders[i].source;
    const char *c_str = source_str.c_str();

    glShaderSource(shader, 1, &c_str, NULL);
    glCompileShader(shader);

    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);

    if (!compile_status) {
      GLchar log[5000];
      GLsizei length = 0;
      glGetShaderInfoLog(shader, sizeof(log), &length, log);
      shader_print_errors("compile", log, c_str);
      return 0;
    }

    glAttachShader(program, shader);
  }

  /* Link output. */
  glBindFragDataLocation(program, 0, "fragColor");

  /* Link and error check. */
  glLinkProgram(program);

  GLint link_status;
  glGetProgramiv(program, GL_LINK_STATUS, &link_status);
  if (!link_status) {
    GLchar log[5000];
    GLsizei length = 0;
    glGetShaderInfoLog(program, sizeof(log), &length, log);
    shader_print_errors("linking", log, VERTEX_SHADER);
    shader_print_errors("linking", log, FRAGMENT_SHADER);
    return 0;
  }

  return program;
}

int OpenGLShader::get_position_attrib_location()
{
  if (position_attribute_location_ == -1) {
    const uint shader_program = get_shader_program();
    position_attribute_location_ = glGetAttribLocation(shader_program, position_attribute_name);
  }
  return position_attribute_location_;
}

int OpenGLShader::get_tex_coord_attrib_location()
{
  if (tex_coord_attribute_location_ == -1) {
    const uint shader_program = get_shader_program();
    tex_coord_attribute_location_ = glGetAttribLocation(shader_program, tex_coord_attribute_name);
  }
  return tex_coord_attribute_location_;
}

void OpenGLShader::bind(int width, int height)
{
  create_shader_if_needed();

  if (!shader_program_) {
    return;
  }

  glUseProgram(shader_program_);
  glUniform1i(image_texture_location_, 0);
  glUniform2f(fullscreen_location_, width, height);
}

void OpenGLShader::unbind() {}

uint OpenGLShader::get_shader_program()
{
  return shader_program_;
}

void OpenGLShader::create_shader_if_needed()
{
  if (shader_program_ || shader_compile_attempted_) {
    return;
  }

  shader_compile_attempted_ = true;

  shader_program_ = compile_shader_program();
  if (!shader_program_) {
    return;
  }

  glUseProgram(shader_program_);

  image_texture_location_ = glGetUniformLocation(shader_program_, "image_texture");
  if (image_texture_location_ < 0) {
    LOG(ERROR) << "Shader doesn't contain the 'image_texture' uniform.";
    destroy_shader();
    return;
  }

  fullscreen_location_ = glGetUniformLocation(shader_program_, "fullscreen");
  if (fullscreen_location_ < 0) {
    LOG(ERROR) << "Shader doesn't contain the 'fullscreen' uniform.";
    destroy_shader();
    return;
  }
}

void OpenGLShader::destroy_shader()
{
  glDeleteProgram(shader_program_);
  shader_program_ = 0;
}

CCL_NAMESPACE_END
