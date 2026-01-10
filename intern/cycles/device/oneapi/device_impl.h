/* SPDX-FileCopyrightText: 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_ONEAPI
#  include "device/device.h"
#  include "device/oneapi/device.h"
#  include "device/oneapi/queue.h"
#  include "kernel/device/oneapi/kernel.h"

#  include "util/map.h"
#  include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class DeviceQueue;

using OneAPIDeviceIteratorCallback =
    void (*)(const char *, const char *, const int, bool, bool, bool, void *);

class OneapiDevice : public GPUDevice {
 private:
  SyclQueue *device_queue_ = nullptr;
#  ifdef WITH_EMBREE_GPU
  RTCDevice embree_device = nullptr;
#    if RTC_VERSION >= 40400
  RTCTraversable embree_traversable = nullptr;
#    else
  RTCScene embree_traversable = nullptr;
#    endif
#    if RTC_VERSION >= 40302
  thread_mutex scene_data_mutex;
  vector<RTCScene> all_embree_scenes;
#    endif
#  endif
  using ConstMemMap = map<string, unique_ptr<device_vector<uchar>>>;
  ConstMemMap const_mem_map_;
  void *kg_memory_ = nullptr;
  void *kg_memory_device_ = nullptr;
  size_t kg_memory_size_ = 0;
  size_t max_memory_on_device_ = 0;
  std::string oneapi_error_string_;
  bool use_hardware_raytracing = false;
  unsigned int kernel_features = 0;
  int scene_max_shaders_ = 0;
  /* Currently, there are some functional errors in the different software layers of the DPC++/L0
   * support regarding several Intel's dGPU executions. As a result, to provide proper
   * functionality to Blender users, we need to detect such configurations and enable some
   * workarounds for them. These workarounds don't make sense to enable by default due to a
   * performance impact - which is not as important for the discussed configuration, as without
   * workarounds, the configuration with several dGPUs would simply not be functional, making the
   * performance topic irrelevant anyway. For an example of such issues, see Blender issue #138384.
   */
  bool is_several_intel_dgpu_devices_detected = false;

  size_t get_free_mem() const;

 public:
  BVHLayoutMask get_bvh_layout_mask(const uint requested_features) const override;

  OneapiDevice(const DeviceInfo &info, Stats &stats, Profiler &profiler, bool headless);

  ~OneapiDevice() override;
#  ifdef WITH_EMBREE_GPU
  void build_bvh(BVH *bvh, Progress &progress, bool refit) override;
#  endif
  bool check_peer_access(Device *peer_device) override;

  bool load_kernels(const uint requested_features) override;

  void reserve_private_memory(const uint kernel_features);

  string oneapi_error_message();

  int scene_max_shaders();

  void *kernel_globals_device_pointer();

  /* All memory types. */
  void mem_alloc(device_memory &mem) override;
  void mem_copy_to(device_memory &mem) override;
  void mem_move_to_host(device_memory &mem) override;
  void mem_copy_from(
      device_memory &mem, const size_t y, size_t w, const size_t h, size_t elem) override;
  void mem_copy_from(device_memory &mem)
  {
    mem_copy_from(mem, 0, 0, 0, 0);
  }
  void mem_zero(device_memory &mem) override;
  void mem_free(device_memory &mem) override;

  device_ptr mem_alloc_sub_ptr(device_memory &mem, const size_t offset, size_t /*size*/) override;

  /* Global memory. */
  void global_alloc(device_memory &mem);
  void global_copy_to(device_memory &mem);
  void global_free(device_memory &mem);

  /* Image memory. */
  void image_alloc(device_image &mem);
  void image_copy_to(device_image &mem);
  void image_free(device_image &mem);

  /* Host side memory, override for more efficient copies. */
  void *host_alloc(const MemoryType type, const size_t size) override;
  void host_free(const MemoryType type, void *host_pointer, const size_t size) override;

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

  /* Graphics resources interoperability. */
  bool should_use_graphics_interop(const GraphicsInteropDevice &interop_device,
                                   const bool log) override;

  unique_ptr<DeviceQueue> gpu_queue_create() override;

  /* NOTE(@nsirgien): Create this methods to avoid some compilation problems on Windows with host
   * side compilation (MSVC). */
  void *usm_aligned_alloc_host(const size_t memory_size, const size_t alignment);
  void usm_free(void *usm_ptr);

  static void architecture_information(const SyclDevice *device, string &name, bool &is_optimized);
  static char *device_capabilities();
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
                      const int kernel,
                      const size_t global_size,
                      const size_t local_size,
                      void **args);
  void get_adjusted_global_and_local_sizes(SyclQueue *queue,
                                           const DeviceKernel kernel,
                                           size_t &kernel_global_size,
                                           size_t &kernel_local_size);
  SyclQueue *sycl_queue();

 protected:
  bool can_use_hardware_raytracing_for_features(const uint requested_features) const;
  void check_usm(SyclQueue *queue, const void *usm_ptr, bool allow_host);
  bool create_queue(SyclQueue *&external_queue,
                    const int device_index,
                    void *embree_device,
                    bool *is_several_intel_dgpu_devices_detected_pointer);
  void free_queue(SyclQueue *queue);
  void *usm_aligned_alloc_host(SyclQueue *queue, const size_t memory_size, const size_t alignment);
  void *usm_alloc_device(SyclQueue *queue, const size_t memory_size);
  void usm_free(SyclQueue *queue, void *usm_ptr);
  bool usm_memcpy(SyclQueue *queue, void *dest, void *src, const size_t num_bytes);
  bool usm_memset(SyclQueue *queue, void *usm_ptr, unsigned char value, const size_t num_bytes);
};

CCL_NAMESPACE_END

#endif
