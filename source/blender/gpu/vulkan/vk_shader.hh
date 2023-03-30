/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_private.hh"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_pipeline.hh"

#include "BLI_string_ref.hh"

namespace blender::gpu {
class VKShaderInterface;

class VKShader : public Shader {
 private:
  VKContext *context_ = nullptr;
  VkShaderModule vertex_module_ = VK_NULL_HANDLE;
  VkShaderModule geometry_module_ = VK_NULL_HANDLE;
  VkShaderModule fragment_module_ = VK_NULL_HANDLE;
  VkShaderModule compute_module_ = VK_NULL_HANDLE;
  bool compilation_failed_ = false;
  VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VKPipeline compute_pipeline_;

 public:
  VKShader(const char *name);
  virtual ~VKShader();

  void vertex_shader_from_glsl(MutableSpan<const char *> sources) override;
  void geometry_shader_from_glsl(MutableSpan<const char *> sources) override;
  void fragment_shader_from_glsl(MutableSpan<const char *> sources) override;
  void compute_shader_from_glsl(MutableSpan<const char *> sources) override;
  bool finalize(const shader::ShaderCreateInfo *info = nullptr) override;
  void warm_cache(int limit) override;

  void transform_feedback_names_set(Span<const char *> name_list,
                                    eGPUShaderTFBType geom_type) override;
  bool transform_feedback_enable(GPUVertBuf *) override;
  void transform_feedback_disable() override;

  void bind() override;
  void unbind() override;

  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;

  std::string resources_declare(const shader::ShaderCreateInfo &info) const override;
  std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const override;
  std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const override;

  /* DEPRECATED: Kept only because of BGL API. */
  int program_handle_get() const override;

  VKPipeline &pipeline_get();
  VkPipelineLayout vk_pipeline_layout_get() const
  {
    return pipeline_layout_;
  }

  const VKShaderInterface &interface_get() const;

 private:
  Vector<uint32_t> compile_glsl_to_spirv(Span<const char *> sources, shaderc_shader_kind kind);
  void build_shader_module(Span<uint32_t> spirv_module, VkShaderModule *r_shader_module);
  void build_shader_module(MutableSpan<const char *> sources,
                           shaderc_shader_kind stage,
                           VkShaderModule *r_shader_module);
  bool finalize_descriptor_set_layouts(VkDevice vk_device,
                                       const VKShaderInterface &shader_interface,
                                       const shader::ShaderCreateInfo &info);
  bool finalize_pipeline_layout(VkDevice vk_device, const VKShaderInterface &shader_interface);
  bool finalize_graphics_pipeline(VkDevice vk_device);

  bool is_graphics_shader() const
  {
    return !is_compute_shader();
  }

  bool is_compute_shader() const
  {
    return compute_module_ != VK_NULL_HANDLE;
  }
};

}  // namespace blender::gpu
