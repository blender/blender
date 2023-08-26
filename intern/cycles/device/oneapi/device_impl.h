/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_ONEAPI

#  include <sycl/sycl.hpp>

#  include "device/device.h"
#  include "device/oneapi/device.h"
#  include "device/oneapi/queue.h"
#  include "kernel/device/oneapi/kernel.h"

#  include "util/map.h"

CCL_NAMESPACE_BEGIN

class DeviceQueue;

typedef void (*OneAPIDeviceIteratorCallback)(
    const char *id, const char *name, int num, bool hwrt_support, void *user_ptr);

class OneapiDevice : public Device {
 private:
  SyclQueue *device_queue_;
#  ifdef WITH_EMBREE_GPU
  RTCDevice embree_device;
  RTCScene embree_scene;
#  endif
  using ConstMemMap = map<string, device_vector<uchar> *>;
  ConstMemMap const_mem_map_;
  device_vector<TextureInfo> texture_info_;
  bool need_texture_info_;
  void *kg_memory_;
  void *kg_memory_device_;
  size_t kg_memory_size_ = (size_t)0;
  size_t max_memory_on_device_ = (size_t)0;
  std::string oneapi_error_string_;
  bool use_hardware_raytracing = false;
  unsigned int kernel_features = 0;
  int scene_max_shaders_ = 0;

 public:
  virtual BVHLayoutMask get_bvh_layout_mask(uint kernel_features) const override;

  OneapiDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler);

  virtual ~OneapiDevice();
#  ifdef WITH_EMBREE_GPU
  void build_bvh(BVH *bvh, Progress &progress, bool refit) override;
#  endif
  bool check_peer_access(Device *peer_device) override;

  bool load_kernels(const uint kernel_features) override;

  void load_texture_info();

  void generic_alloc(device_memory &mem);

  void generic_copy_to(device_memory &mem);

  void generic_free(device_memory &mem);

  string oneapi_error_message();

  int scene_max_shaders();

  void *kernel_globals_device_pointer();

  void mem_alloc(device_memory &mem) override;

  void mem_copy_to(device_memory &mem) override;

  void mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem) override;

  void mem_copy_from(device_memory &mem)
  {
    mem_copy_from(mem, 0, 0, 0, 0);
  }

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

  /* NOTE(@nsirgien): Create this methods to avoid some compilation problems on Windows with host
   * side compilation (MSVC). */
  void *usm_aligned_alloc_host(size_t memory_size, size_t alignment);
  void usm_free(void *usm_ptr);

  static std::vector<sycl::device> available_devices();
  static char *device_capabilities();
  static int parse_driver_build_version(const sycl::device &device);
  static void iterate_devices(OneAPIDeviceIteratorCallback cb, void *user_ptr);

  size_t get_memcapacity();
  int get_num_multiprocessors();
  int get_max_num_threads_per_multiprocessor();
  bool queue_synchronize(SyclQueue *queue);
  bool kernel_globals_size(size_t &kernel_global_size);
  void set_global_memory(SyclQueue *queue,
                         void *kernel_globals,
                         const char *memory_name,
                         void *memory_device_pointer);
  bool enqueue_kernel(KernelContext *kernel_context,
                      int kernel,
                      size_t global_size,
                      size_t local_size,
                      void **args);
  void get_adjusted_global_and_local_sizes(SyclQueue *queue,
                                           const DeviceKernel kernel,
                                           size_t &kernel_global_size,
                                           size_t &kernel_local_size);
  SyclQueue *sycl_queue();

 protected:
  bool can_use_hardware_raytracing_for_features(uint kernel_features) const;
  void check_usm(SyclQueue *queue, const void *usm_ptr, bool allow_host);
  bool create_queue(SyclQueue *&external_queue, int device_index, void *embree_device);
  void free_queue(SyclQueue *queue);
  void *usm_aligned_alloc_host(SyclQueue *queue, size_t memory_size, size_t alignment);
  void *usm_alloc_device(SyclQueue *queue, size_t memory_size);
  void usm_free(SyclQueue *queue, void *usm_ptr);
  bool usm_memcpy(SyclQueue *queue, void *dest, void *src, size_t num_bytes);
  bool usm_memset(SyclQueue *queue, void *usm_ptr, unsigned char value, size_t num_bytes);
};

CCL_NAMESPACE_END

#endif
