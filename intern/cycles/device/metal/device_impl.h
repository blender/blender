/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_METAL

#  include "bvh/bvh.h"
#  include "device/device.h"
#  include "device/metal/bvh.h"
#  include "device/metal/device.h"
#  include "device/metal/kernel.h"
#  include "device/metal/queue.h"
#  include "device/metal/util.h"

#  include <Metal/Metal.h>

CCL_NAMESPACE_BEGIN

class DeviceQueue;

class MetalDevice : public Device {
 public:
  id<MTLDevice> mtlDevice = nil;
  id<MTLLibrary> mtlLibrary[PSO_NUM] = {nil};
  id<MTLArgumentEncoder> mtlBufferKernelParamsEncoder =
      nil; /* encoder used for fetching device pointers from MTLBuffers */
  id<MTLCommandQueue> mtlGeneralCommandQueue = nil;
  id<MTLArgumentEncoder> mtlAncillaryArgEncoder =
      nil; /* encoder used for fetching device pointers from MTLBuffers */
  string source[PSO_NUM];
  string kernels_md5[PSO_NUM];
  string global_defines_md5[PSO_NUM];

  bool capture_enabled = false;

  KernelParamsMetal launch_params = {0};

  /* MetalRT members ----------------------------------*/
  BVHMetal *bvhMetalRT = nullptr;
  bool motion_blur = false;
  id<MTLArgumentEncoder> mtlASArgEncoder =
      nil; /* encoder used for fetching device pointers from MTLAccelerationStructure */
  /*---------------------------------------------------*/

  MetalGPUVendor device_vendor;

  uint kernel_features;
  MTLResourceOptions default_storage_mode;
  int max_threads_per_threadgroup;

  int mtlDevId = 0;
  bool has_error = false;

  struct MetalMem {
    device_memory *mem = nullptr;
    int pointer_index = -1;
    id<MTLBuffer> mtlBuffer = nil;
    id<MTLTexture> mtlTexture = nil;
    uint64_t offset = 0;
    uint64_t size = 0;
    void *hostPtr = nullptr;
    bool use_UMA = false; /* If true, UMA memory in shared_pointer is being used. */
  };
  typedef map<device_memory *, unique_ptr<MetalMem>> MetalMemMap;
  MetalMemMap metal_mem_map;
  std::vector<id<MTLResource>> delayed_free_list;
  std::recursive_mutex metal_mem_map_mutex;

  /* Bindless Textures */
  bool is_texture(const TextureInfo &tex);
  device_vector<TextureInfo> texture_info;
  bool need_texture_info;
  id<MTLArgumentEncoder> mtlTextureArgEncoder = nil;
  id<MTLArgumentEncoder> mtlBufferArgEncoder = nil;
  id<MTLBuffer> buffer_bindings_1d = nil;
  id<MTLBuffer> texture_bindings_2d = nil;
  id<MTLBuffer> texture_bindings_3d = nil;
  std::vector<id<MTLTexture>> texture_slot_map;

  /* BLAS encoding & lookup */
  id<MTLArgumentEncoder> mtlBlasArgEncoder = nil;
  id<MTLBuffer> blas_buffer = nil;

  bool use_metalrt = false;
  MetalPipelineType kernel_specialization_level = PSO_GENERIC;

  int device_id = 0;

  static thread_mutex existing_devices_mutex;
  static std::map<int, MetalDevice *> active_device_ids;

  static bool is_device_cancelled(int device_id);

  static MetalDevice *get_device_by_ID(int device_idID,
                                       thread_scoped_lock &existing_devices_mutex_lock);

  virtual bool is_ready(string &status) const override;

  virtual void cancel() override;

  virtual BVHLayoutMask get_bvh_layout_mask(uint /*kernel_features*/) const override;

  void set_error(const string &error) override;

  MetalDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler);

  virtual ~MetalDevice();

  bool support_device(const uint /*kernel_features*/);

  bool check_peer_access(Device *peer_device) override;

  bool use_adaptive_compilation();

  bool use_local_atomic_sort() const;

  string preprocess_source(MetalPipelineType pso_type,
                           const uint kernel_features,
                           string *source = nullptr);

  bool make_source_and_check_if_compile_needed(MetalPipelineType pso_type);

  void make_source(MetalPipelineType pso_type, const uint kernel_features);

  virtual bool load_kernels(const uint kernel_features) override;

  void load_texture_info();

  void erase_allocation(device_memory &mem);

  virtual bool should_use_graphics_interop() override;

  virtual unique_ptr<DeviceQueue> gpu_queue_create() override;

  virtual void build_bvh(BVH *bvh, Progress &progress, bool refit) override;

  virtual void optimize_for_scene(Scene *scene) override;

  static void compile_and_load(int device_id, MetalPipelineType pso_type);

  /* ------------------------------------------------------------------ */
  /* low-level memory management */

  bool max_working_set_exceeded(size_t safety_margin = 8 * 1024 * 1024) const;

  MetalMem *generic_alloc(device_memory &mem);

  void generic_copy_to(device_memory &mem);

  void generic_free(device_memory &mem);

  void mem_alloc(device_memory &mem) override;

  void mem_copy_to(device_memory &mem) override;

  void mem_copy_from(device_memory &mem)
  {
    mem_copy_from(mem, -1, -1, -1, -1);
  }
  void mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem) override;

  void mem_zero(device_memory &mem) override;

  void mem_free(device_memory &mem) override;

  device_ptr mem_alloc_sub_ptr(device_memory &mem, size_t offset, size_t /*size*/) override;

  virtual void const_copy_to(const char *name, void *host, size_t size) override;

  void global_alloc(device_memory &mem);

  void global_free(device_memory &mem);

  void tex_alloc(device_texture &mem);

  void tex_alloc_as_buffer(device_texture &mem);

  void tex_free(device_texture &mem);

  void flush_delayed_free_list();
};

CCL_NAMESPACE_END

#endif
