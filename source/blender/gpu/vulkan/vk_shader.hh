/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_string_ref.hh"

#include "gpu_shader_private.hh"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_push_constants.hh"
#include "vk_shader_module.hh"

#include "shaderc/shaderc.hpp"

namespace blender::gpu {
class VKShaderInterface;

class VKShader : public Shader {
 private:
  VKContext *context_ = nullptr;

  /**
   * Not owning handle to the descriptor layout.
   * The handle is owned by `VKDescriptorSetLayouts` of the device.
   */
  VkDescriptorSetLayout vk_descriptor_set_layout_ = VK_NULL_HANDLE;

  /**
   * Base VkPipeline handle. This handle is used as template when building a variation of
   * the shader. In case for compute shaders without specialization constants this handle is also
   * used as an early exit as in there would only be a single variation.
   */
  VkPipeline vk_pipeline_base_ = VK_NULL_HANDLE;

  bool is_compute_shader_ = false;
  bool is_static_shader_ = false;

 public:
  VKShaderModule vertex_module;
  VKShaderModule geometry_module;
  VKShaderModule fragment_module;
  VKShaderModule compute_module;

  VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
  VKPushConstants push_constants;

  VKShader(const char *name);
  virtual ~VKShader();

  void init(const shader::ShaderCreateInfo &info, bool is_batch_compilation) override;

  const shader::ShaderCreateInfo &patch_create_info(
      const shader::ShaderCreateInfo &original_info) override
  {
    return original_info;
  }

  void vertex_shader_from_glsl(const shader::ShaderCreateInfo &info,
                               MutableSpan<StringRefNull> sources) override;
  void geometry_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                 MutableSpan<StringRefNull> sources) override;
  void fragment_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                 MutableSpan<StringRefNull> sources) override;
  void compute_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                MutableSpan<StringRefNull> sources) override;
  bool finalize(const shader::ShaderCreateInfo *info = nullptr) override;
  bool finalize_post();

  void warm_cache(int limit) override;

  void bind(const shader::SpecializationConstants *constants_state) override;
  void unbind() override;

  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;

  std::string resources_declare(const shader::ShaderCreateInfo &info) const override;
  std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const override;
  std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const override;

  VkPipeline ensure_and_get_compute_pipeline(
      const shader::SpecializationConstants &constants_state);
  VkPipeline ensure_and_get_graphics_pipeline(GPUPrimType primitive,
                                              VKVertexAttributeObject &vao,
                                              VKStateManager &state_manager,
                                              VKFrameBuffer &framebuffer,
                                              shader::SpecializationConstants &constants_state);

  const VKShaderInterface &interface_get() const;

  /**
   * Some shaders don't have a descriptor set and should not bind any descriptor set to the
   * pipeline. This function can be used to determine if a descriptor set can be bound when this
   * shader or one of its pipelines are active.
   */
  bool has_descriptor_set() const
  {
    return vk_descriptor_set_layout_ != VK_NULL_HANDLE;
  }

  VkDescriptorSetLayout vk_descriptor_set_layout_get() const
  {
    return vk_descriptor_set_layout_;
  }

 private:
  void build_shader_module(MutableSpan<StringRefNull> sources,
                           shaderc_shader_kind stage,
                           VKShaderModule &r_shader_module);
  bool finalize_shader_module(VKShaderModule &shader_module, const char *stage_name);
  bool finalize_descriptor_set_layouts(VKDevice &vk_device,
                                       const VKShaderInterface &shader_interface);
  bool finalize_pipeline_layout(VKDevice &device, const VKShaderInterface &shader_interface);

  /**
   * \brief features available on newer implementation such as native barycentric coordinates
   * and layered rendering, necessitate a geometry shader to work on older hardware.
   */
  std::string workaround_geometry_shader_source_create(const shader::ShaderCreateInfo &info);
  bool do_geometry_shader_injection(const shader::ShaderCreateInfo *info) const;
};

static inline VKShader &unwrap(Shader &shader)
{
  return static_cast<VKShader &>(shader);
}

static inline VKShader *unwrap(Shader *shader)
{
  return static_cast<VKShader *>(shader);
}

}  // namespace blender::gpu
