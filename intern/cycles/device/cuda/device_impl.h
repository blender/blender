/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_CUDA

#  include "device/cuda/kernel.h"
#  include "device/cuda/queue.h"
#  include "device/cuda/util.h"
#  include "device/device.h"

#  ifdef WITH_CUDA_DYNLOAD
#    include "cuew.h"
#  else
#    include <cuda.h>
#    include <cudaGL.h>
#  endif

CCL_NAMESPACE_BEGIN

class DeviceQueue;

class CUDADevice : public GPUDevice {

  friend class CUDAContextScope;

 public:
  CUdevice cuDevice;
  CUcontext cuContext;
  CUmodule cuModule;
  int pitch_alignment;
  int cuDevId;
  int cuDevArchitecture;
  bool first_error;

  CUDADeviceKernels kernels;

  static bool have_precompiled_kernels();

  BVHLayoutMask get_bvh_layout_mask(uint /*kernel_features*/) const override;

  void set_error(const string &error) override;

  CUDADevice(const DeviceInfo &info, Stats &stats, Profiler &profiler, bool headless);

  ~CUDADevice() override;

  bool support_device(const uint /*kernel_features*/);

  bool check_peer_access(Device *peer_device) override;

  bool use_adaptive_compilation();

  string compile_kernel_get_common_cflags(const uint kernel_features);

  string compile_kernel(const string &cflags,
                        const char *name,
                        const char *base = "cuda",
                        bool force_ptx = false);

  bool load_kernels(const uint kernel_features) override;
  void reserve_local_memory(const uint kernel_features);

  /* All memory types. */
  void mem_alloc(device_memory &mem) override;
  void mem_copy_to(device_memory &mem) override;
  void mem_move_to_host(device_memory &mem) override;
  void mem_copy_from(
      device_memory &mem, const size_t y, size_t w, const size_t h, size_t elem) override;
  void mem_zero(device_memory &mem) override;
  void mem_free(device_memory &mem) override;

  device_ptr mem_alloc_sub_ptr(device_memory &mem, const size_t offset, size_t /*size*/) override;

  /* Global memory. */
  void global_alloc(device_memory &mem);
  void global_copy_to(device_memory &mem);
  void global_free(device_memory &mem);

  /* Texture memory. */
  void tex_alloc(device_texture &mem);
  void tex_copy_to(device_texture &mem);
  void tex_free(device_texture &mem);

  /* Device side memory. */
  void get_device_memory_info(size_t &total, size_t &free) override;
  bool alloc_device(void *&device_pointer, const size_t size) override;
  void free_device(void *device_pointer) override;

  /* Shared memory. */
  bool shared_alloc(void *&shared_pointer, const size_t size) override;
  void shared_free(void *shared_pointer) override;
  void *shared_to_device_pointer(const void *shared_pointer) override;

  /* Memory copy. */
  void copy_host_to_device(void *device_pointer, void *host_pointer, const size_t size) override;
  void const_copy_to(const char *name, void *host, const size_t size) override;

  bool should_use_graphics_interop(const GraphicsInteropDevice &interop_device,
                                   const bool log) override;

  unique_ptr<DeviceQueue> gpu_queue_create() override;

  int get_num_multiprocessors();
  int get_max_num_threads_per_multiprocessor();

 protected:
  bool get_device_attribute(CUdevice_attribute attribute, int *value);
  int get_device_default_attribute(CUdevice_attribute attribute, const int default_value);
};

CCL_NAMESPACE_END

#endif
