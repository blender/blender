/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_shader.hh"

namespace blender::gpu {
void VKShader::vertex_shader_from_glsl(MutableSpan<const char *> /*sources*/)
{
}

void VKShader::geometry_shader_from_glsl(MutableSpan<const char *> /*sources*/)
{
}

void VKShader::fragment_shader_from_glsl(MutableSpan<const char *> /*sources*/)
{
}

void VKShader::compute_shader_from_glsl(MutableSpan<const char *> /*sources*/)
{
}

bool VKShader::finalize(const shader::ShaderCreateInfo * /*info*/)
{
  return false;
}

void VKShader::transform_feedback_names_set(Span<const char *> /*name_list*/,
                                            eGPUShaderTFBType /*geom_type*/)
{
}

bool VKShader::transform_feedback_enable(GPUVertBuf *)
{
  return false;
}

void VKShader::transform_feedback_disable()
{
}

void VKShader::bind()
{
}

void VKShader::unbind()
{
}

void VKShader::uniform_float(int /*location*/,
                             int /*comp_len*/,
                             int /*array_size*/,
                             const float * /*data*/)
{
}
void VKShader::uniform_int(int /*location*/,
                           int /*comp_len*/,
                           int /*array_size*/,
                           const int * /*data*/)
{
}

std::string VKShader::resources_declare(const shader::ShaderCreateInfo & /*info*/) const
{
  return std::string();
}

std::string VKShader::vertex_interface_declare(const shader::ShaderCreateInfo & /*info*/) const
{
  return std::string();
}

std::string VKShader::fragment_interface_declare(const shader::ShaderCreateInfo & /*info*/) const
{
  return std::string();
}

std::string VKShader::geometry_interface_declare(const shader::ShaderCreateInfo & /*info*/) const
{
  return std::string();
}

std::string VKShader::geometry_layout_declare(const shader::ShaderCreateInfo & /*info*/) const
{
  return std::string();
}

std::string VKShader::compute_layout_declare(const shader::ShaderCreateInfo & /*info*/) const
{
  return std::string();
}

int VKShader::program_handle_get() const
{
  return -1;
}

}  // namespace blender::gpu