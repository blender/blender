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

#include "vk_resource_pool.hh"

namespace blender::gpu {
class VKDevice;

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
    uint32_t viewport_count;

    bool operator==(const FragmentShader &other) const
    {
      if (vk_fragment_module != other.vk_fragment_module || viewport_count != other.viewport_count)
      {
        return false;
      }

      return true;
    }

    uint64_t hash() const
    {
      uint64_t hash = uint64_t(vk_fragment_module);
      hash = hash * 33 ^ uint64_t(viewport_count);
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
 * Thread safe map to pipelines of a certain type (graphics or compute).
 */
template<typename PipelineInfo> class VKPipelineMap {
  Map<PipelineInfo, VkPipeline> pipelines_;
  Mutex mutex_;
  std::condition_variable_any new_pipeline_added_;

 public:
  /**
   * Get an existing or create a new pipeline based on the provided PipelineInfo.
   *
   * When vk_pipeline_base is a valid pipeline handle, the pipeline base will be used to speed up
   * pipeline creation process.
   *
   * \param compute_info:     Description of the pipeline to compile.
   * \param vk_pipeline_cache: Pipeline cache to use.
   * \param vk_pipeline_base: An already existing pipeline that can be used as a base when
   *                          compiling the pipeline.
   * \param name:             Name to give as a debug label when creating a pipeline.
   * \returns The handle of the compiled pipeline.
   */
  VkPipeline get_or_create(const PipelineInfo &pipeline_info,
                           VkPipelineCache vk_pipeline_cache,
                           VkPipeline vk_pipeline_base,
                           StringRefNull name)
  {
    bool do_wait_for_pipeline = false;
    bool do_compile_pipeline = false;
    {
      std::scoped_lock lock(mutex_);
      const VkPipeline *found_pipeline = pipelines_.lookup_ptr(pipeline_info);
      if (found_pipeline) {
        if (*found_pipeline == VK_NULL_HANDLE) {
          do_wait_for_pipeline = true;
        }
        else {
          /* Early exit: pipeline_info found and has a valid pipeline. */
          return *found_pipeline;
        }
      }
      else {
        pipelines_.add_new(pipeline_info, VK_NULL_HANDLE);
        do_compile_pipeline = true;
      }
    }
    if (do_wait_for_pipeline) {
      return wait_for_completion(pipeline_info);
    }
    if (do_compile_pipeline) {
      VkPipeline pipeline = create(pipeline_info, vk_pipeline_cache, vk_pipeline_base, name);
      /* Store result in the compute pipelines map. */
      {
        std::scoped_lock lock(mutex_);
        VkPipeline &pipeline_item = pipelines_.lookup(pipeline_info);
        pipeline_item = pipeline;
      }
      /* Notify other threads that a new pipeline is available. */
      {
        new_pipeline_added_.notify_all();
      }
      return pipeline;
    }
    BLI_assert_unreachable();
    return VK_NULL_HANDLE;
  }
  /**
   * Discard all pipelines associated with the given layout
   */
  void discard(VKDiscardPool &discard_pool, VkPipelineLayout vk_pipeline_layout)
  {
    std::scoped_lock lock(mutex_);
    pipelines_.remove_if([&](auto item) {
      if (item.key.vk_pipeline_layout == vk_pipeline_layout) {
        discard_pool.discard_pipeline(item.value);
        return true;
      }
      return false;
    });
  }
  /**
   * Free all data.
   *
   * \note Handle is passed to fix recursive inclusion of vk_device.hh
   */
  void free_data(VkDevice vk_device)
  {
    std::scoped_lock lock(mutex_);
    for (VkPipeline &vk_pipeline : pipelines_.values()) {
      vkDestroyPipeline(vk_device, vk_pipeline, nullptr);
    }
    pipelines_.clear();
  }
  /**
   * Number of pipelines stored inside the instance.
   */
  uint64_t size() const
  {
    return pipelines_.size();
  }

 private:
  /**
   * Create a new pipeline based on the provided PipelineInfo.
   *
   * When vk_pipeline_base is a valid pipeline handle, the pipeline base will be used to speed up
   * pipeline creation process.
   *
   * \param pipeline_info:     Description of the pipeline to compile.
   * \param vk_pipeline_cache: Pipeline cache to use.
   * \param vk_pipeline_base: An already existing pipeline that can be used as a base when
   *                          compiling the pipeline.
   * \param name:             Name to give as a debug label when creating a pipeline.
   * \returns The handle of the compiled pipeline.
   */
  VkPipeline create(const PipelineInfo &pipeline_info,
                    VkPipelineCache vk_pipeline_cache,
                    VkPipeline vk_pipeline_base,
                    StringRefNull name);
  /**
   * \brief wait for another thread to complete the same pipeline info.
   *
   * \param pipeline_info: Description of the pipeline to wait for.
   */
  VkPipeline wait_for_completion(const PipelineInfo &pipeline_info)
  {
    std::unique_lock<Mutex> lock(mutex_);
    const VkPipeline *pipeline = nullptr;
    new_pipeline_added_.wait(lock, [&]() {
      pipeline = pipelines_.lookup_ptr(pipeline_info);
      BLI_assert(pipeline != nullptr);
      return *pipeline != VK_NULL_HANDLE;
    });
    return *pipeline;
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
 */
class VKPipelinePool : public NonCopyable {
  friend class VKDevice;

 private:
  VkPipelineCache vk_pipeline_cache_static_;
  VkPipelineCache vk_pipeline_cache_non_static_;

  VKPipelineMap<VKComputeInfo> compute_;
  VKPipelineMap<VKGraphicsInfo> graphics_;

 public:
  void init();

  /**
   * Get an existing or create a new compute pipeline based on the provided ComputeInfo.
   *
   * When vk_pipeline_base is a valid pipeline handle, the pipeline base will be used to speed up
   * pipeline creation process.
   *
   * \param compute_info:     Description of the pipeline to compile.
   * \param is_static_shader: Pipelines from static shaders are cached between Blender sessions.
   *                          Pipelines from dynamic shaders are only cached for the duration of a
   *                          single Blender session.
   * \param vk_pipeline_base: An already existing pipeline that can be used as a base when
   *                          compiling the pipeline.
   * \param name:             Name to give as a debug label when creating a pipeline.
   * \returns The handle of the compiled pipeline.
   */
  VkPipeline get_or_create_compute_pipeline(const VKComputeInfo &compute_info,
                                            bool is_static_shader,
                                            VkPipeline vk_pipeline_base,
                                            StringRefNull name);

  /**
   * Get an existing or create a new compute pipeline based on the provided ComputeInfo.
   *
   * When vk_pipeline_base is a valid pipeline handle, the pipeline base will be used to speed up
   * pipeline creation process.
   *
   * \param graphics_info:    Description of the pipeline to compile.
   * \param is_static_shader: Pipelines from static shaders are cached between Blender sessions.
   *                          Pipelines from dynamic shaders are only cached for the duration of
   *                          a single Blender session.
   * \param vk_pipeline_base: An already existing pipeline that can be used as a base when
   *                          compiling the pipeline.
   * \param name:             Name to give as a debug label when creating a pipeline.
   * \returns The handle of the compiled pipeline.
   */
  VkPipeline get_or_create_graphics_pipeline(const VKGraphicsInfo &graphics_info,
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
   * The cache will not be written when G_DEBUG_GPU is active. In this case the shader modules will
   * been generated with debug information and other compiler settings are used. This will clutter
   * the pipeline cache.
   *
   * NOTE: When developing shaders we assume that `WITH_BUILDINFO` is turned off or `G_DEBUG_GPU`
   * flag is set.
   */
  void write_to_disk();
};

}  // namespace blender::gpu
