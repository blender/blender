/* SPDX-FileCopyrightText: 2011-2022 OpenGL Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types.h"

CCL_NAMESPACE_BEGIN

class OpenGLShader {
 public:
  static constexpr const char *position_attribute_name = "pos";
  static constexpr const char *tex_coord_attribute_name = "texCoord";

  OpenGLShader() = default;
  virtual ~OpenGLShader() = default;

  /* Get attribute location for position and texture coordinate respectively.
   * NOTE: The shader needs to be bound to have access to those. */
  int get_position_attrib_location();
  int get_tex_coord_attrib_location();

  void bind(int width, int height);
  void unbind();

 protected:
  uint get_shader_program();

  void create_shader_if_needed();
  void destroy_shader();

  /* Cached values of various OpenGL resources. */
  int position_attribute_location_ = -1;
  int tex_coord_attribute_location_ = -1;

  uint shader_program_ = 0;
  int image_texture_location_ = -1;
  int fullscreen_location_ = -1;

  /* Shader compilation attempted. Which means, that if the shader program is 0 then compilation or
   * linking has failed. Do not attempt to re-compile the shader. */
  bool shader_compile_attempted_ = false;
};

CCL_NAMESPACE_END
