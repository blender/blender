/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "BKE_appdir.hh"
#include "BKE_blender_version.h"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_time.h"

#include "CLG_log.h"

#include "vk_backend.hh"
#include "vk_graphics_pipeline.hh"
#include "vk_pipeline_pool.hh"

#ifdef WITH_BUILDINFO
extern "C" char build_hash[];
#endif
static CLG_LogRef LOG = {"gpu.vulkan"};

namespace blender::gpu {

void VKPipelinePool::init()
{
  VKDevice &device = VKBackend::get().device;
  VkPipelineCacheCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  vkCreatePipelineCache(device.vk_handle(), &create_info, nullptr, &vk_pipeline_cache_static_);
  debug::object_label(vk_pipeline_cache_static_, "VkPipelineCache.Static");
  vkCreatePipelineCache(device.vk_handle(), &create_info, nullptr, &vk_pipeline_cache_non_static_);
  debug::object_label(vk_pipeline_cache_non_static_, "VkPipelineCache.Dynamic");
}

/* -------------------------------------------------------------------- */
/** \name Compute pipelines
 * \{ */

VkPipeline VKPipelinePool::get_or_create_compute_pipeline(const VKComputeInfo &compute_info,
                                                          const bool is_static_shader,
                                                          VkPipeline vk_pipeline_base,
                                                          StringRefNull name)
{
  bool created = false;
  VkPipelineCache vk_pipeline_cache = is_static_shader ? vk_pipeline_cache_static_ :
                                                         vk_pipeline_cache_non_static_;
  return compute_.get_or_create(compute_info, vk_pipeline_cache, vk_pipeline_base, name, created);
}

template<>
VkPipeline VKPipelineMap<VKComputeInfo>::create(const VKComputeInfo &compute_info,
                                                VkPipelineCache vk_pipeline_cache,
                                                VkPipeline vk_pipeline_base,
                                                StringRefNull name)
{
  /* Building compute pipeline create info */
  const bool do_specialization_constants = !compute_info.specialization_constants.is_empty();
  VkComputePipelineCreateInfo vk_compute_pipeline_create_info = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      nullptr,
      0,
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       nullptr,
       0,
       VK_SHADER_STAGE_COMPUTE_BIT,
       compute_info.vk_shader_module,
       "main",
       nullptr},
      compute_info.vk_pipeline_layout,
      vk_pipeline_base,
      0};

  /* Specialization constants */
  VkSpecializationInfo vk_specialization_info;
  Array<VkSpecializationMapEntry> vk_specialization_map_entries(
      compute_info.specialization_constants.size());
  if (do_specialization_constants) {
    vk_compute_pipeline_create_info.stage.pSpecializationInfo = &vk_specialization_info;
    for (uint32_t index : IndexRange(compute_info.specialization_constants.size())) {
      vk_specialization_map_entries[index] = {
          index, uint32_t(index * sizeof(uint32_t)), sizeof(uint32_t)};
    }
    vk_specialization_info = {uint32_t(vk_specialization_map_entries.size()),
                              vk_specialization_map_entries.data(),
                              compute_info.specialization_constants.size() * sizeof(uint32_t),
                              compute_info.specialization_constants.data()};
  }

  /* Create pipeline. */
  VKBackend &backend = VKBackend::get();
  VKDevice &device = backend.device;

  double start_time = BLI_time_now_seconds();
  VkPipeline pipeline = VK_NULL_HANDLE;
  vkCreateComputePipelines(device.vk_handle(),
                           vk_pipeline_cache,
                           1,
                           &vk_compute_pipeline_create_info,
                           nullptr,
                           &pipeline);
  double end_time = BLI_time_now_seconds();
  debug::object_label(pipeline, name);
  CLOG_DEBUG(&LOG,
             "Compiled compute pipeline %s in %fms ",
             name.c_str(),
             (end_time - start_time) * 1000.0);

  return pipeline;
}

/* \} */

/* -------------------------------------------------------------------- */
/** \name Graphics pipelines
 * \{ */

VkPipeline VKPipelinePool::get_or_create_graphics_pipeline(const VKGraphicsInfo &graphics_info,
                                                           const bool is_static_shader,
                                                           VkPipeline vk_pipeline_base,
                                                           StringRefNull name,
                                                           bool &r_created)
{
  BLI_assert_msg(
      graphics_info.shaders.state == graphics_info.fragment_out.state,
      "VKGraphicsInfo.shader.state and VKGraphicsInfo.fragment_out.state should be identical, "
      "otherwise an incorrect fragment output library will be linked.");
  VkPipelineCache vk_pipeline_cache = is_static_shader ? vk_pipeline_cache_static_ :
                                                         vk_pipeline_cache_non_static_;
  return graphics_.get_or_create(
      graphics_info, vk_pipeline_cache, vk_pipeline_base, name, r_created);
}

template<>
VkPipeline VKPipelineMap<VKGraphicsInfo>::create(const VKGraphicsInfo &graphics_info,
                                                 VkPipelineCache vk_pipeline_cache,
                                                 VkPipeline vk_pipeline_base,
                                                 StringRefNull name)
{
  VKDevice &device = VKBackend::get().device;
  const VKExtensions &extensions = device.extensions_get();
  VKGraphicsPipelineCreateInfoBuilder builder;
  builder.build_full(graphics_info, extensions, vk_pipeline_base);

  /* Build pipeline. */
  VkPipeline pipeline = VK_NULL_HANDLE;
  double start_time = BLI_time_now_seconds();
  vkCreateGraphicsPipelines(device.vk_handle(),
                            vk_pipeline_cache,
                            1,
                            &builder.vk_graphics_pipeline_create_info,
                            nullptr,
                            &pipeline);
  double end_time = BLI_time_now_seconds();
  debug::object_label(pipeline, name);
  CLOG_DEBUG(&LOG,
             "Compiled graphics pipeline %s in %fms ",
             name.c_str(),
             (end_time - start_time) * 1000.0);
  return pipeline;
}

/* \} */

void VKPipelinePool::discard(VKDiscardPool &discard_pool, VkPipelineLayout vk_pipeline_layout)
{
  graphics_.discard(discard_pool, vk_pipeline_layout);
  compute_.discard(discard_pool, vk_pipeline_layout);
}

void VKPipelinePool::free_data()
{
  VKDevice &device = VKBackend::get().device;

  graphics_.free_data(device.vk_handle());
  compute_.free_data(device.vk_handle());

  vkDestroyPipelineCache(device.vk_handle(), vk_pipeline_cache_static_, nullptr);
  vkDestroyPipelineCache(device.vk_handle(), vk_pipeline_cache_non_static_, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Persistent cache
 * \{ */

#ifdef WITH_BUILDINFO
struct VKPipelineCachePrefixHeader {
  /* `BC` stands for "Blender Cache" + 2 bytes for file versioning. */
  uint32_t magic = 0xBC00;
  uint32_t blender_version = BLENDER_VERSION;
  uint32_t blender_version_patch = BLENDER_VERSION_PATCH;
  char commit_hash[8];
  uint32_t data_size;
  uint32_t vendor_id;
  uint32_t device_id;
  uint32_t driver_version;
  uint8_t pipeline_cache_uuid[VK_UUID_SIZE];

  VKPipelineCachePrefixHeader()
  {
    const VKDevice &device = VKBackend::get().device;
    data_size = 0;
    const VkPhysicalDeviceProperties &properties = device.physical_device_properties_get();
    vendor_id = properties.vendorID;
    device_id = properties.deviceID;
    driver_version = properties.driverVersion;
    memcpy(&pipeline_cache_uuid, &properties.pipelineCacheUUID, VK_UUID_SIZE);

    memset(commit_hash, 0, sizeof(commit_hash));
    STRNCPY(commit_hash, build_hash);
  }
};

static std::string pipeline_cache_filepath_get()
{
  static char tmp_dir_buffer[1024];
  BKE_appdir_folder_caches(tmp_dir_buffer, sizeof(tmp_dir_buffer));

  std::string cache_dir = std::string(tmp_dir_buffer) + "vk-pipeline-cache" + SEP_STR;
  BLI_dir_create_recursive(cache_dir.c_str());
  std::string cache_file = cache_dir + "static.bin";
  return cache_file;
}
#endif

void VKPipelinePool::read_from_disk()
{
#ifdef WITH_BUILDINFO
  /* Don't read the shader cache when GPU debugging is enabled. When enabled we use different
   * shaders and compilation settings. Previous generated pipelines will not be used. */
  if (bool(G.debug & G_DEBUG_GPU)) {
    return;
  }

  std::string cache_file = pipeline_cache_filepath_get();
  if (!BLI_exists(cache_file.c_str())) {
    return;
  }

  /* Prevent old cache files from being deleted if they're still being used. */
  BLI_file_touch(cache_file.c_str());
  /* Read cached binary. */
  fstream file(cache_file, std::ios::binary | std::ios::in | std::ios::ate);
  std::streamsize data_size = file.tellg();
  file.seekg(0, std::ios::beg);
  void *buffer = MEM_mallocN(data_size, __func__);
  file.read(reinterpret_cast<char *>(buffer), data_size);
  file.close();

  /* Validate the prefix header. */
  VKPipelineCachePrefixHeader prefix;
  VKPipelineCachePrefixHeader &read_prefix = *static_cast<VKPipelineCachePrefixHeader *>(buffer);
  prefix.data_size = read_prefix.data_size;
  if (memcmp(&read_prefix, &prefix, sizeof(VKPipelineCachePrefixHeader)) != 0) {
    /* Headers are different, most likely the cache will not work and potentially crash the driver.
     * [https://medium.com/@zeuxcg/creating-a-robust-pipeline-cache-with-vulkan-961d09416cda]
     */
    MEM_freeN(buffer);
    CLOG_INFO(&LOG,
              "Pipeline cache on disk [%s] is ignored as it was written by a different driver or "
              "Blender version. Cache will be overwritten when exiting.",
              cache_file.c_str());
    return;
  }

  CLOG_INFO(&LOG, "Initialize static pipeline cache from disk [%s].", cache_file.c_str());
  VKDevice &device = VKBackend::get().device;
  VkPipelineCacheCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  create_info.initialDataSize = read_prefix.data_size;
  create_info.pInitialData = static_cast<uint8_t *>(buffer) + sizeof(VKPipelineCachePrefixHeader);
  VkPipelineCache vk_pipeline_cache = VK_NULL_HANDLE;
  vkCreatePipelineCache(device.vk_handle(), &create_info, nullptr, &vk_pipeline_cache);
  MEM_freeN(buffer);

  vkMergePipelineCaches(device.vk_handle(), vk_pipeline_cache_static_, 1, &vk_pipeline_cache);
  vkDestroyPipelineCache(device.vk_handle(), vk_pipeline_cache, nullptr);
#endif
}

void VKPipelinePool::write_to_disk()
{
#ifdef WITH_BUILDINFO
  /* Don't write the pipeline cache when GPU debugging is enabled. When enabled we use different
   * shaders and compilation settings. Writing them to disk will clutter the pipeline cache. */
  if (bool(G.debug & G_DEBUG_GPU)) {
    return;
  }

  VKDevice &device = VKBackend::get().device;
  size_t data_size;
  vkGetPipelineCacheData(device.vk_handle(), vk_pipeline_cache_static_, &data_size, nullptr);
  void *buffer = MEM_mallocN(data_size, __func__);
  vkGetPipelineCacheData(device.vk_handle(), vk_pipeline_cache_static_, &data_size, buffer);

  std::string cache_file = pipeline_cache_filepath_get();
  CLOG_INFO(&LOG, "Writing static pipeline cache to disk [%s].", cache_file.c_str());

  fstream file(cache_file, std::ios::binary | std::ios::out);

  VKPipelineCachePrefixHeader header;
  header.data_size = data_size;
  file.write(reinterpret_cast<char *>(&header), sizeof(VKPipelineCachePrefixHeader));
  file.write(static_cast<char *>(buffer), data_size);

  MEM_freeN(buffer);
#endif
}

/** \} */

}  // namespace blender::gpu
