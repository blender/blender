/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef WITH_CUDA

#  include "device/cuda/kernel.h"
#  include "device/cuda/queue.h"
#  include "device/cuda/util.h"
#  include "device/device.h"

#  include "util/map.h"

#  ifdef WITH_CUDA_DYNLOAD
#    include "cuew.h"
#  else
#    include <cuda.h>
#    include <cudaGL.h>
#  endif

CCL_NAMESPACE_BEGIN

class DeviceQueue;

class CUDADevice : public Device {

  friend class CUDAContextScope;

 public:
  CUdevice cuDevice;
  CUcontext cuContext;
  CUmodule cuModule;
  size_t device_texture_headroom;
  size_t device_working_headroom;
  bool move_texture_to_host;
  size_t map_host_used;
  size_t map_host_limit;
  int can_map_host;
  int pitch_alignment;
  int cuDevId;
  int cuDevArchitecture;
  bool first_error;

  struct CUDAMem {
    CUDAMem() : texobject(0), array(0), use_mapped_host(false)
    {
    }

    CUtexObject texobject;
    CUarray array;

    /* If true, a mapped host memory in shared_pointer is being used. */
    bool use_mapped_host;
  };
  typedef map<device_memory *, CUDAMem> CUDAMemMap;
  CUDAMemMap cuda_mem_map;
  thread_mutex cuda_mem_map_mutex;

  /* Bindless Textures */
  device_vector<TextureInfo> texture_info;
  bool need_texture_info;

  CUDADeviceKernels kernels;

  static bool have_precompiled_kernels();

  virtual bool show_samples() const override;

  virtual BVHLayoutMask get_bvh_layout_mask() const override;

  void set_error(const string &error) override;

  CUDADevice(const DeviceInfo &info, Stats &stats, Profiler &profiler);

  virtual ~CUDADevice();

  bool support_device(const uint /*kernel_features*/);

  bool check_peer_access(Device *peer_device) override;

  bool use_adaptive_compilation();

  virtual string compile_kernel_get_common_cflags(const uint kernel_features);

  string compile_kernel(const uint kernel_features,
                        const char *name,
                        const char *base = "cuda",
                        bool force_ptx = false);

  virtual bool load_kernels(const uint kernel_features) override;

  void reserve_local_memory(const uint kernel_features);

  void init_host_memory();

  void load_texture_info();

  void move_textures_to_host(size_t size, bool for_texture);

  CUDAMem *generic_alloc(device_memory &mem, size_t pitch_padding = 0);

  void generic_copy_to(device_memory &mem);

  void generic_free(device_memory &mem);

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

  virtual bool should_use_graphics_interop() override;

  virtual unique_ptr<DeviceQueue> gpu_queue_create() override;

  int get_num_multiprocessors();
  int get_max_num_threads_per_multiprocessor();

 protected:
  bool get_device_attribute(CUdevice_attribute attribute, int *value);
  int get_device_default_attribute(CUdevice_attribute attribute, int default_value);
};

CCL_NAMESPACE_END

#endif
