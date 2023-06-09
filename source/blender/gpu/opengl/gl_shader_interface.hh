/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
