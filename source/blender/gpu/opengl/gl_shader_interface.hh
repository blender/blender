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
 *
 * GPU shader interface (C --> GLSL)
 *
 * Structure detailing needed vertex inputs and resources for a specific shader.
 * A shader interface can be shared between two similar shaders.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_vector.hh"

#include "glew-mx.h"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_interface.hh"

namespace blender::gpu {

class GLVaoCache;

/**
 * Implementation of Shader interface using OpenGL.
 */
class GLShaderInterface : public ShaderInterface {
 private:
  /** Reference to VaoCaches using this interface */
  Vector<GLVaoCache *> refs_;

 public:
  GLShaderInterface(GLuint program, const shader::ShaderCreateInfo &info);
  GLShaderInterface(GLuint program);
  ~GLShaderInterface();

  void ref_add(GLVaoCache *ref);
  void ref_remove(GLVaoCache *ref);

  MEM_CXX_CLASS_ALLOC_FUNCS("GLShaderInterface");
};

}  // namespace blender::gpu
