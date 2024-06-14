/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include <mutex>

#include "BLI_map.hh"
#include "BLI_utility_mixins.hh"

#include "gpu_state_private.hh"

#include "vk_common.hh"

namespace blender {
namespace gpu {

/**
 * Struct containing key information to identify a compute pipeline.
 */
struct VKComputeInfo {
  VkShaderModule vk_shader_module;
  VkPipelineLayout vk_pipeline_layout;
  Vector<shader::SpecializationConstant::Value> specialization_constants;

  bool operator==(const VKComputeInfo &other) const
  {
    return vk_shader_module == other.vk_shader_module &&
           vk_pipeline_layout == other.vk_pipeline_layout &&
           specialization_constants == other.specialization_constants;
  };
};

/**
 * Struct containing key information to identify graphics pipelines.
 *
 * The struct has already been separated into 4 stages to be compatible with
 * `VK_EXT_graphics_pipeline_library` being vertex_in, pre_rasterization, fragment_shader and
 * fragment_out.
 */
struct VKGraphicsInfo {
  struct VertexIn {
    VkPrimitiveTopology vk_topology;
    Vector<VkVertexInputAttributeDescription> attributes;
    Vector<VkVertexInputBindingDescription> bindings;

    bool operator==(const VertexIn &other) const
    {
      // TODO: use an exact implementation and remove the hash compare.
      /*
      return vk_topology == other.vk_topology && attributes.hash() == other.attributes.hash() &&
             bindings.hash() == other.bindings.hash();
      */
      return hash() == other.hash();
    }

    uint64_t hash() const
    {
      uint64_t hash = 0;
      hash = hash * 33 ^ uint64_t(vk_topology);
      for (const VkVertexInputAttributeDescription &attribute : attributes) {
        hash = hash * 33 ^ uint64_t(attribute.location);
        hash = hash * 33 ^ uint64_t(attribute.binding);
        hash = hash * 33 ^ uint64_t(attribute.format);
        hash = hash * 33 ^ uint64_t(attribute.offset);
      }
      for (const VkVertexInputBindingDescription &binding : bindings) {
        hash = hash * 33 ^ uint64_t(binding.binding);
        hash = hash * 33 ^ uint64_t(binding.inputRate);
        hash = hash * 33 ^ uint64_t(binding.stride);
      }
      return hash;
    }
  };
  struct PreRasterization {
    VkShaderModule vk_vertex_module;
    VkShaderModule vk_geometry_module;
    bool operator==(const PreRasterization &other) const
    {
      return vk_vertex_module == other.vk_vertex_module &&
             vk_geometry_module == other.vk_geometry_module;
    }
    uint64_t hash() const
    {
      uint64_t hash = 0;
      hash = hash * 33 ^ uint64_t(vk_vertex_module);
      hash = hash * 33 ^ uint64_t(vk_geometry_module);
      return hash;
    }
  };
  struct FragmentShader {
    VkShaderModule vk_fragment_module;
    Vector<VkViewport> viewports;
    Vector<VkRect2D> scissors;

    bool operator==(const FragmentShader &other) const
    {
      // TODO: Do not use hash.
      return vk_fragment_module == other.vk_fragment_module && hash() == other.hash();
    }
    uint64_t hash() const
    {
      uint64_t hash = 0;
      hash = hash * 33 ^ uint64_t(vk_fragment_module);
      for (const VkViewport &vk_viewport : viewports) {
        hash = hash * 33 ^ uint64_t(vk_viewport.x);
        hash = hash * 33 ^ uint64_t(vk_viewport.y);
        hash = hash * 33 ^ uint64_t(vk_viewport.width);
        hash = hash * 33 ^ uint64_t(vk_viewport.height);
        hash = hash * 33 ^ uint64_t(vk_viewport.minDepth);
        hash = hash * 33 ^ uint64_t(vk_viewport.maxDepth);
      }
      for (const VkRect2D &scissor : scissors) {
        hash = hash * 33 ^ uint64_t(scissor.offset.x);
        hash = hash * 33 ^ uint64_t(scissor.offset.y);
        hash = hash * 33 ^ uint64_t(scissor.extent.width);
        hash = hash * 33 ^ uint64_t(scissor.extent.height);
      }
      return hash;
    }
  };
  struct FragmentOut {
    VkFormat depth_attachment_format;
    VkFormat stencil_attachment_format;
    Vector<VkFormat> color_attachment_formats;

    bool operator==(const FragmentOut &other) const
    {
      return hash() == other.hash();
    }

    uint64_t hash() const
    {
      uint64_t hash = 0;
      hash = hash * 33 ^ uint64_t(depth_attachment_format);
      hash = hash * 33 ^ uint64_t(stencil_attachment_format);
      for (VkFormat color_attachment_format : color_attachment_formats) {
        hash = hash * 33 ^ uint64_t(color_attachment_format);
      }

      return hash;
    }
  };

  VertexIn vertex_in;
  PreRasterization pre_rasterization;
  FragmentShader fragment_shader;
  FragmentOut fragment_out;

  GPUState state;
  GPUStateMutable mutable_state;
  VkPipelineLayout vk_pipeline_layout;
  Vector<shader::SpecializationConstant::Value> specialization_constants;

  bool operator==(const VKGraphicsInfo &other) const
  {
    return vertex_in == other.vertex_in && pre_rasterization == other.pre_rasterization &&
           fragment_shader == other.fragment_shader && fragment_out == other.fragment_out &&
           vk_pipeline_layout == other.vk_pipeline_layout &&
           specialization_constants == other.specialization_constants && state == other.state &&
           mutable_state == other.mutable_state;
  };
  uint64_t hash() const
  {
    uint64_t hash = 0;
    hash = hash * 33 ^ vertex_in.hash();
    hash = hash * 33 ^ pre_rasterization.hash();
    hash = hash * 33 ^ fragment_shader.hash();
    hash = hash * 33 ^ fragment_out.hash();
    hash = hash * 33 ^ uint64_t(vk_pipeline_layout);
    hash = hash * 33 ^ get_default_hash(specialization_constants);
    hash = hash * 33 ^ state.data;
    hash = hash * 33 ^ mutable_state.data[0];
    hash = hash * 33 ^ mutable_state.data[1];
    hash = hash * 33 ^ mutable_state.data[2];
    return hash;
  }
};

}  // namespace gpu

template<> struct DefaultHash<gpu::VKComputeInfo> {
  uint64_t operator()(const gpu::VKComputeInfo &key) const
  {
    uint64_t hash = uint64_t(key.vk_shader_module);
    hash = hash * 33 ^ uint64_t(key.vk_pipeline_layout);
    hash = hash * 33 ^ get_default_hash(key.specialization_constants);
    return hash;
  }
};

namespace gpu {
class VKDevice;

/**
 * Pipelines are lazy initialized and same pipelines should share their handle.
 *
 * A requirement of our render graph implementation is that changes of the pipeline can be detected
 * based on the VkPipeline handle. We only want to rebind the pipeline handle when the handle
 * actually changes. This improves performance (especially on NVIDIA) devices where pipeline binds
 * are known to be costly.
 *
 * Especially for graphics pipelines many parameters are needed to compile a graphics pipeline.
 * Some of the information would be boiler plating; or at least from Blender point of view. To
 * improve lookup performance we use a slimmed down version of the pipeline create info structs.
 * The idea is that we can limit the required data because we control which data we actually use,
 * removing the boiler plating and improve hashing performance better than the VkPipelineCache can
 * give us.
 *
 * TODO: Extensions like `VK_EXT_graphics_pipeline_library` should fit in this class and ease the
 * development for graphics pipelines. Geometry in and frame-buffer out could be cached separately
 * to reduce pipeline creation times. Most likely we will add support when we work on graphic
 * pipelines. Recent drivers all support this extension, but the full coverage is still <20%. A
 * fallback should made available for older drivers is required.
 *
 * TODO: Creation of shader modules needs to be revisited.
 * VK_EXT_graphics_pipeline_library deprecates the use of shader modules and use the `spriv` bin
 * directly. In this extension the pipeline and shader module are the same. The current approach
 * should also be revisited as the latest drivers all implement pipeline libraries, but there are
 * some platforms where the driver isn't been updated and doesn't implement this extension. In
 * that case shader modules should still be used.
 *
 * TODO: GPUMaterials (or any other large shader) should be unloaded when the GPUShader is
 * destroyed. Exact details what the best approach is unclear as support for EEVEE is still
 * lacking.
 */
class VKPipelinePool : public NonCopyable {
 public:
 private:
  Map<VKComputeInfo, VkPipeline> compute_pipelines_;
  Map<VKGraphicsInfo, VkPipeline> graphic_pipelines_;
  /* Partially initialized structures to reuse. */
  VkComputePipelineCreateInfo vk_compute_pipeline_create_info_;

  VkGraphicsPipelineCreateInfo vk_graphics_pipeline_create_info_;
  VkPipelineRenderingCreateInfo vk_pipeline_rendering_create_info_;
  VkPipelineShaderStageCreateInfo vk_pipeline_shader_stage_create_info_[3];
  VkPipelineInputAssemblyStateCreateInfo vk_pipeline_input_assembly_state_create_info_;
  VkPipelineVertexInputStateCreateInfo vk_pipeline_vertex_input_state_create_info_;

  VkPipelineRasterizationStateCreateInfo vk_pipeline_rasterization_state_create_info_;
  VkPipelineViewportStateCreateInfo vk_pipeline_viewport_state_create_info_;
  VkPipelineDepthStencilStateCreateInfo vk_pipeline_depth_stencil_state_create_info_;

  VkPipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info_;

  Vector<VkPipelineColorBlendAttachmentState> vk_pipeline_color_blend_attachment_states_;
  VkPipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info_;
  VkPipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_state_template_;

  VkSpecializationInfo vk_specialization_info_;
  Vector<VkSpecializationMapEntry> vk_specialization_map_entries_;
  VkPushConstantRange vk_push_constant_range_;

  std::mutex mutex_;

 public:
  VKPipelinePool();
  /**
   * Get an existing or create a new compute pipeline based on the provided ComputeInfo.
   *
   * When vk_pipeline_base is a valid pipeline handle, the pipeline base will be used to speed up
   * pipeline creation process.
   */
  VkPipeline get_or_create_compute_pipeline(VKComputeInfo &compute_info,
                                            VkPipeline vk_pipeline_base = VK_NULL_HANDLE);

  /**
   * Get an existing or create a new compute pipeline based on the provided ComputeInfo.
   *
   * When vk_pipeline_base is a valid pipeline handle, the pipeline base will be used to speed up
   * pipeline creation process.
   */
  VkPipeline get_or_create_graphics_pipeline(VKGraphicsInfo &graphics_info,
                                             VkPipeline vk_pipeline_base = VK_NULL_HANDLE);

  /**
   * Remove all shader pipelines that uses the given shader_module.
   */
  void remove(Span<VkShaderModule> vk_shader_modules);

  /**
   * Destroy all created pipelines.
   *
   * Function is called just before the device is removed. This cannot be done in the destructor as
   * that would be called after the device is removed.
   */
  void free_data();

 private:
  VkSpecializationInfo *specialization_info_update(
      Span<shader::SpecializationConstant::Value> specialization_constants);
  void specialization_info_reset();
};

}  // namespace gpu

}  // namespace blender
