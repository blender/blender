/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "xxhash.h"

#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_utility_mixins.hh"

#include "gpu_state_private.hh"

#include "vk_common.hh"

namespace blender::gpu {
class VKDevice;
class VKDiscardPool;

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

  uint64_t hash() const
  {
    uint64_t hash = uint64_t(vk_shader_module);
    hash = hash * 33 ^ uint64_t(vk_pipeline_layout);
    hash = hash * 33 ^ specialization_constants.hash();
    return hash;
  }
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
      /* TODO: use an exact implementation and remove the hash compare. */
#if 0
      return vk_topology == other.vk_topology && attributes.hash() == other.attributes.hash() &&
             bindings.hash() == other.bindings.hash();
#endif
      return hash() == other.hash();
    }

    uint64_t hash() const
    {
      uint64_t hash = uint64_t(vk_topology);
      hash = hash * 33 ^
             XXH3_64bits(attributes.data(),
                         attributes.size() * sizeof(VkVertexInputAttributeDescription));
      hash = hash * 33 ^ XXH3_64bits(bindings.data(),
                                     bindings.size() * sizeof(VkVertexInputBindingDescription));
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
    std::optional<uint64_t> cached_hash;

    bool operator==(const FragmentShader &other) const
    {
      if (vk_fragment_module != other.vk_fragment_module ||
          viewports.size() != other.viewports.size() || scissors.size() != other.scissors.size() ||
          hash() != other.hash())
      {
        return false;
      }

      return true;
    }

    uint64_t hash() const
    {
      if (cached_hash.has_value()) {
        return *cached_hash;
      }
      return calc_hash();
    }

    void update_hash()
    {
      cached_hash = calc_hash();
    }

   private:
    uint64_t calc_hash() const
    {
      uint64_t hash = uint64_t(vk_fragment_module);
      hash = hash * 33 ^ uint64_t(viewports.size());
      hash = hash * 33 ^ uint64_t(scissors.size());

      return hash;
    }
  };
  struct FragmentOut {
    uint32_t color_attachment_size;

    /* Dynamic rendering */
    VkFormat depth_attachment_format;
    VkFormat stencil_attachment_format;
    Vector<VkFormat> color_attachment_formats;

    bool operator==(const FragmentOut &other) const
    {
#if 1
      return hash() == other.hash();
#else
      if (depth_attachment_format != other.depth_attachment_format ||
          stencil_attachment_format != other.stencil_attachment_format ||
          color_attachment_formats.size() != other.color_attachment_formats.size())
      {
        return false;
      }

      if (memcmp(color_attachment_formats.data(),
                 other.color_attachment_formats.data(),
                 color_attachment_formats.size() * sizeof(VkFormat)) == 0)
      {
        return false;
      }
      return true;
#endif
    }

    uint64_t hash() const
    {
      uint64_t hash = uint64_t(depth_attachment_format);
      hash = hash * 33 ^ uint64_t(stencil_attachment_format);
      hash = hash * 33 ^ XXH3_64bits(color_attachment_formats.data(),
                                     color_attachment_formats.size() * sizeof(VkFormat));
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
    hash = hash * 33 ^ specialization_constants.hash();
    hash = hash * 33 ^ state.data;
    hash = hash * 33 ^ mutable_state.data[0];
    hash = hash * 33 ^ mutable_state.data[1];
    hash = hash * 33 ^ mutable_state.data[2];
    return hash;
  }
};

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
 * TODO: GPUMaterials (or any other large shader) should be unloaded when the gpu::Shader is
 * destroyed. Exact details what the best approach is unclear as support for EEVEE is still
 * lacking.
 */
class VKPipelinePool : public NonCopyable {
  friend class VKDevice;

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
  VkPipelineRasterizationProvokingVertexStateCreateInfoEXT
      vk_pipeline_rasterization_provoking_vertex_state_info_;

  Vector<VkDynamicState> vk_dynamic_states_;
  VkPipelineDynamicStateCreateInfo vk_pipeline_dynamic_state_create_info_;

  VkPipelineViewportStateCreateInfo vk_pipeline_viewport_state_create_info_;
  VkPipelineDepthStencilStateCreateInfo vk_pipeline_depth_stencil_state_create_info_;

  VkPipelineMultisampleStateCreateInfo vk_pipeline_multisample_state_create_info_;

  Vector<VkPipelineColorBlendAttachmentState> vk_pipeline_color_blend_attachment_states_;
  VkPipelineColorBlendStateCreateInfo vk_pipeline_color_blend_state_create_info_;
  VkPipelineColorBlendAttachmentState vk_pipeline_color_blend_attachment_state_template_;

  VkSpecializationInfo vk_specialization_info_;
  Vector<VkSpecializationMapEntry> vk_specialization_map_entries_;
  VkPushConstantRange vk_push_constant_range_;

  VkPipelineCache vk_pipeline_cache_static_;
  VkPipelineCache vk_pipeline_cache_non_static_;

  Mutex mutex_;

 public:
  VKPipelinePool();

  void init();

  /**
   * Get an existing or create a new compute pipeline based on the provided ComputeInfo.
   *
   * When vk_pipeline_base is a valid pipeline handle, the pipeline base will be used to speed up
   * pipeline creation process.
   */
  VkPipeline get_or_create_compute_pipeline(VKComputeInfo &compute_info,
                                            bool is_static_shader,
                                            VkPipeline vk_pipeline_base,
                                            StringRefNull name);

  /**
   * Get an existing or create a new compute pipeline based on the provided ComputeInfo.
   *
   * When vk_pipeline_base is a valid pipeline handle, the pipeline base will be used to speed up
   * pipeline creation process.
   */
  VkPipeline get_or_create_graphics_pipeline(VKGraphicsInfo &graphics_info,
                                             bool is_static_shader,
                                             VkPipeline vk_pipeline_base,
                                             StringRefNull name);

  /**
   * Discard all pipelines that uses the given pipeline_layout.
   */
  void discard(VKDiscardPool &discard_pool, VkPipelineLayout vk_pipeline_layout);

  /**
   * Destroy all created pipelines.
   *
   * Function is called just before the device is removed. This cannot be done in the destructor as
   * that would be called after the device is removed.
   */
  void free_data();

  /**
   * Read the static pipeline cache from cache file.
   *
   * Pipeline caches requires blender to be build with `WITH_BUILDINFO` enabled . Between commits
   * shader modules can change and shader module identifiers cannot be used. We use the build info
   * to check if the identifiers can be reused.
   *
   * Previous stored pipeline cache will not be read when G_DEBUG_GPU is enabled. In this case the
   * shader modules will be compiled with other settings and any cached pipeline will not be used
   * during this session.
   *
   * NOTE: When developing shaders we assume that `WITH_BUILDINFO` is turned off or `G_DEBUG_GPU`
   * flag is set.
   */
  void read_from_disk();

  /**
   * Store the static pipeline cache to disk.
   *
   * Pipeline caches requires blender to be build with `WITH_BUILDINFO` enabled . Between commits
   * shader modules can change and shader module identifiers cannot be used. We use the build info
   * to check if the identifiers can be reused.
   *
   * The cache will not be written when G_DEBUG_GPU is active. In this case the shader modules have
   * been generated with debug information and other compiler settings are used. This will clutter
   * the pipeline cache.
   *
   * NOTE: When developing shaders we assume that `WITH_BUILDINFO` is turned off or `G_DEBUG_GPU`
   * flag is set.
   */
  void write_to_disk();

 private:
  VkSpecializationInfo *specialization_info_update(
      Span<shader::SpecializationConstant::Value> specialization_constants);
  void specialization_info_reset();
};

}  // namespace blender::gpu
