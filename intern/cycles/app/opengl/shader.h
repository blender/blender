/*
 * Copyright 2011-2022 OpenGL Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
