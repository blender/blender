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

#include "vk_common.hh"

namespace blender {
namespace gpu {

/**
 * Struct containing key information to identify a compute pipeline.
 */
struct VKComputeInfo {
  VkShaderModule vk_shader_module;
  VkPipelineLayout vk_pipeline_layout;
  Vector<shader::ShaderCreateInfo::SpecializationConstant::Value> specialization_constants;

  bool operator==(const VKComputeInfo &other) const
  {
    return vk_shader_module == other.vk_shader_module &&
           vk_pipeline_layout == other.vk_pipeline_layout &&
           specialization_constants == other.specialization_constants;
  };
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
  /* Partially initialized structures to reuse. */
  VkComputePipelineCreateInfo vk_compute_pipeline_create_info_;
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
};

}  // namespace gpu

}  // namespace blender
