/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#ifdef WITH_HIP

#  include "device/device.h"
#  include "device/hip/kernel.h"
#  include "device/hip/queue.h"
#  include "device/hip/util.h"

#  include "util/map.h"

#  ifdef WITH_HIP_DYNLOAD
#    include "hipew.h"
#  endif

CCL_NAMESPACE_BEGIN

class DeviceQueue;

class HIPDevice : public GPUDevice {

  friend class HIPContextScope;

 public:
  hipDevice_t hipDevice;
  hipCtx_t hipContext;
  hipModule_t hipModule;
  int pitch_alignment;
  int hipDevId;
  int hipDevArchitecture;
  bool first_error;

  HIPDeviceKernels kernels;

  static bool have_precompiled_kernels();

  virtual BVHLayoutMask get_bvh_layout_mask(uint /*kernel_features*/) const override;

  void set_error(const string &error) override;

  HIPDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler);

  virtual ~HIPDevice();

  bool support_device(const uint /*kernel_features*/);

  bool check_peer_access(Device *peer_device) override;

  bool use_adaptive_compilation();

  virtual string compile_kernel_get_common_cflags(const uint kernel_features);

  virtual string compile_kernel(const uint kernel_features,
                                const char *name,
                                const char *base = "hip");

  virtual bool load_kernels(const uint kernel_features) override;
  void reserve_local_memory(const uint kernel_features);

  virtual void get_device_memory_info(size_t &total, size_t &free) override;
  virtual bool alloc_device(void *&device_pointer, size_t size) override;
  virtual void free_device(void *device_pointer) override;
  virtual bool alloc_host(void *&shared_pointer, size_t size) override;
  virtual void free_host(void *shared_pointer) override;
  virtual void transform_host_pointer(void *&device_pointer, void *&shared_pointer) override;
  virtual void copy_host_to_device(void *device_pointer, void *host_pointer, size_t size) override;

  void mem_alloc(device_memory &mem) override;

  void mem_copy_to(device_memory &mem) override;

  void mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem) override;

  void mem_zero(device_memory &mem) override;

  void mem_free(device_memory &mem) override;

  device_ptr mem_alloc_sub_ptr(device_memory &mem, size_t offset, size_t /*size*/) override;

  virtual void const_copy_to(const char *name, void *host, size_t size) override;

  void global_alloc(device_memory &mem);

  void global_free(device_memory &mem);

  void tex_alloc(device_texture &mem);

  void tex_free(device_texture &mem);

  /* Graphics resources interoperability. */
  virtual bool should_use_graphics_interop() override;

  virtual unique_ptr<DeviceQueue> gpu_queue_create() override;

  int get_num_multiprocessors();
  int get_max_num_threads_per_multiprocessor();

 protected:
  bool get_device_attribute(hipDeviceAttribute_t attribute, int *value);
  int get_device_default_attribute(hipDeviceAttribute_t attribute, int default_value);
};

CCL_NAMESPACE_END

#endif
