/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstdlib>
#include <functional>

#include "bvh/params.h"

#include "device/denoise.h"
#include "device/memory.h"

#include "util/profiling.h"
#include "util/stats.h"
#include "util/string.h"
#include "util/texture.h"
#include "util/thread.h"
#include "util/types.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class BVH;
class DeviceQueue;
class GraphicsInteropDevice;
class Progress;
class CPUKernels;
class Scene;

struct OSLGlobals;
struct ThreadKernelGlobalsCPU;

/* Device Types */

enum DeviceType {
  DEVICE_NONE = 0,
  DEVICE_CPU,
  DEVICE_CUDA,
  DEVICE_MULTI,
  DEVICE_OPTIX,
  DEVICE_HIP,
  DEVICE_HIPRT,
  DEVICE_METAL,
  DEVICE_ONEAPI,
  DEVICE_DUMMY,
};

enum DeviceTypeMask {
  DEVICE_MASK_CPU = (1 << DEVICE_CPU),
  DEVICE_MASK_CUDA = (1 << DEVICE_CUDA),
  DEVICE_MASK_OPTIX = (1 << DEVICE_OPTIX),
  DEVICE_MASK_HIP = (1 << DEVICE_HIP),
  DEVICE_MASK_METAL = (1 << DEVICE_METAL),
  DEVICE_MASK_ONEAPI = (1 << DEVICE_ONEAPI),
  DEVICE_MASK_ALL = ~0
};

#define DEVICE_MASK(type) (DeviceTypeMask)(1 << type)

enum KernelOptimizationLevel {
  KERNEL_OPTIMIZATION_LEVEL_OFF = 0,
  KERNEL_OPTIMIZATION_LEVEL_INTERSECT = 1,
  KERNEL_OPTIMIZATION_LEVEL_FULL = 2,

  KERNEL_OPTIMIZATION_NUM_LEVELS
};

enum MetalRTSetting {
  METALRT_OFF = 0,
  METALRT_ON = 1,
  METALRT_AUTO = 2,

  METALRT_NUM_SETTINGS
};

class DeviceInfo {
 public:
  DeviceType type = DEVICE_CPU;
  string description;
  /* used for user preferences, should stay fixed with changing hardware config */
  string id = "CPU";
  int num = 0;
  bool display_device = false;          /* GPU is used as a display device. */
  bool has_nanovdb = false;             /* Support NanoVDB volumes. */
  bool has_mnee = true;                 /* Support MNEE. */
  bool has_osl = false;                 /* Support Open Shading Language. */
  bool has_guiding = false;             /* Support path guiding. */
  bool has_profiling = false;           /* Supports runtime collection of profiling info. */
  bool has_peer_memory = false;         /* GPU has P2P access to memory of another GPU. */
  bool has_gpu_queue = false;           /* Device supports GPU queue. */
  bool use_hardware_raytracing = false; /* Use hardware instructions to accelerate ray tracing. */
  bool use_metalrt_by_default = false;  /* Use MetalRT by default. */
  /* Indicate that device execution has been optimized by Blender or vendor developers.
   * For LTS versions, this helps communicate that newer versions may have better performance. */
  bool has_execution_optimization = true;

  KernelOptimizationLevel kernel_optimization_level =
      KERNEL_OPTIMIZATION_LEVEL_FULL;         /* Optimization level applied to path tracing
                                               * kernels (Metal only). */
  DenoiserTypeMask denoisers = DENOISER_NONE; /* Supported denoiser types. */
  int cpu_threads = 0;
  vector<DeviceInfo> multi_devices;
  string error_msg;

  DeviceInfo() = default;

  bool operator==(const DeviceInfo &info) const
  {
    /* Multiple Devices with the same ID would be very bad. */
    assert(id != info.id ||
           (type == info.type && num == info.num && description == info.description));
    return id == info.id && use_hardware_raytracing == info.use_hardware_raytracing &&
           kernel_optimization_level == info.kernel_optimization_level;
  }
  bool operator!=(const DeviceInfo &info) const
  {
    return !(*this == info);
  }

  bool contains_device_type(const DeviceType type) const;
};

/* Device */

class Device {
  friend class device_sub_ptr;

 protected:
  Device(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_, bool headless_)
      : info(info_), stats(stats_), profiler(profiler_), headless(headless_)
  {
  }

  string error_msg;

  virtual device_ptr mem_alloc_sub_ptr(device_memory & /*mem*/, size_t /*offset*/, size_t /*size*/)
  {
    /* Only required for devices that implement denoising. */
    assert(false);
    return (device_ptr)0;
  }
  virtual void mem_free_sub_ptr(device_ptr /*ptr*/){};

 public:
  /* noexcept needed to silence TBB warning. */
  virtual ~Device() noexcept(false);

  /* info */
  DeviceInfo info;
  virtual const string &error_message()
  {
    return error_msg;
  }
  bool have_error()
  {
    return !error_message().empty();
  }
  virtual void set_error(const string &error);
  virtual BVHLayoutMask get_bvh_layout_mask(const uint kernel_features) const = 0;

  /* statistics */
  Stats &stats;
  Profiler &profiler;
  bool headless = true;

  /* constant memory */
  virtual void const_copy_to(const char *name, void *host, const size_t size) = 0;

  /* load/compile kernels, must be called before adding tasks */
  virtual bool load_kernels(uint /*kernel_features*/)
  {
    return true;
  }

  virtual bool load_osl_kernels()
  {
    return true;
  }

  /* Request cancellation of any long-running work. */
  virtual void cancel() {}

  /* Report status and return true if device is ready for rendering. */
  virtual bool is_ready(string & /*status*/) const
  {
    return true;
  }

  /* GPU device only functions.
   * These may not be used on CPU or multi-devices. */

  /* Create new queue for executing kernels in. */
  virtual unique_ptr<DeviceQueue> gpu_queue_create();

  /* CPU device only functions.
   * These may not be used on GPU or multi-devices. */

  /* Get CPU kernel functions for native instruction set. */
  static const CPUKernels &get_cpu_kernels();
  /* Get kernel globals to pass to kernels. */
  virtual void get_cpu_kernel_thread_globals(
      vector<ThreadKernelGlobalsCPU> & /*kernel_thread_globals*/);
  /* Get OpenShadingLanguage memory buffer. */
  virtual OSLGlobals *get_cpu_osl_memory();

  /* Acceleration structure building. */
  virtual void build_bvh(BVH *bvh, Progress &progress, bool refit);
  /* Used by Metal and OptiX. */
  virtual void release_bvh(BVH * /*bvh*/) {}

  /* Inform of BVH limits, return true to force-rebuild all BVHs and kernels. */
  virtual bool set_bvh_limits(size_t /*instance_count*/, size_t /*max_prim_count*/)
  {
    return false;
  }

  /* multi device */
  virtual int device_number(Device * /*sub_device*/)
  {
    return 0;
  }

  /* Called after kernel texture setup, and prior to integrator state setup. */
  virtual void optimize_for_scene(Scene * /*scene*/) {}

  virtual bool is_resident(device_ptr /*key*/, Device *sub_device)
  {
    /* Memory is always resident if this is not a multi device, regardless of whether the pointer
     * is valid or not (since it may not have been allocated yet). */
    return sub_device == this;
  }
  virtual bool check_peer_access(Device * /*peer_device*/)
  {
    return false;
  }

  virtual bool is_shared(const void * /*shared_pointer*/,
                         const device_ptr /*device_pointer*/,
                         Device * /*sub_device*/)
  {
    return false;
  }

  /* Graphics resources interoperability.
   *
   * The interoperability comes here by the meaning that the device is capable of computing result
   * directly into a OpenGL, Vulkan or Metal buffer. */

  /* Check display is to be updated using graphics interoperability.
   * The interoperability can not be used is it is not supported by the device. But the device
   * might also force disable the interoperability if it detects that it will be slower than
   * copying pixels from the render buffer. */
  virtual bool should_use_graphics_interop(const GraphicsInteropDevice & /*interop_device*/,
                                           const bool /*log*/ = false)
  {
    return false;
  }

  /* Returns native buffer handle for device pointer. */
  virtual void *get_native_buffer(device_ptr /*ptr*/)
  {
    return nullptr;
  }

  /* Guiding */

  /* Returns path guiding device handle. */
  virtual void *get_guiding_device() const;

  /* Sub-devices */

  /* Run given callback for every individual device which will be handling rendering.
   * For the single device the callback is called for the device itself. For the multi-device the
   * callback is only called for the sub-devices. */
  virtual void foreach_device(const std::function<void(Device *)> &callback)
  {
    callback(this);
  }

  /* static */
  static unique_ptr<Device> create(const DeviceInfo &info,
                                   Stats &stats,
                                   Profiler &profiler,
                                   bool headless);

  static DeviceType type_from_string(const char *name);
  static string string_from_type(DeviceType type);
  static vector<DeviceType> available_types();
  static vector<DeviceInfo> available_devices(const uint device_type_mask = DEVICE_MASK_ALL);
  static DeviceInfo dummy_device(const string &error_msg = "");
  static string device_capabilities(const uint device_type_mask = DEVICE_MASK_ALL);
  static DeviceInfo get_multi_device(const vector<DeviceInfo> &subdevices,
                                     const int threads,
                                     bool background);

  /* Tag devices lists for update. */
  static void tag_update();

  static void free_memory();

 protected:
  /* Memory allocation, only accessed through device_memory. */
  friend class MultiDevice;
  friend class DeviceServer;
  friend class device_memory;

  virtual void *host_alloc(const MemoryType type, const size_t size);
  virtual void host_free(const MemoryType type, void *host_pointer, const size_t size);

  virtual void mem_alloc(device_memory &mem) = 0;
  virtual void mem_copy_to(device_memory &mem) = 0;
  virtual void mem_move_to_host(device_memory &mem) = 0;
  virtual void mem_copy_from(
      device_memory &mem, const size_t y, size_t w, const size_t h, size_t elem) = 0;
  virtual void mem_zero(device_memory &mem) = 0;
  virtual void mem_free(device_memory &mem) = 0;

 private:
  /* Indicted whether device types and devices lists were initialized. */
  static bool need_types_update, need_devices_update;
  static thread_mutex device_mutex;
  static vector<DeviceInfo> cuda_devices;
  static vector<DeviceInfo> optix_devices;
  static vector<DeviceInfo> cpu_devices;
  static vector<DeviceInfo> hip_devices;
  static vector<DeviceInfo> metal_devices;
  static vector<DeviceInfo> oneapi_devices;
  static uint devices_initialized_mask;
};

/* Device, which is GPU, with some common functionality for GPU back-ends. */
class GPUDevice : public Device {
 protected:
  GPUDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_, bool headless_)
      : Device(info_, stats_, profiler_, headless_), texture_info(this, "texture_info", MEM_GLOBAL)
  {
  }

 public:
  ~GPUDevice() noexcept(false) override;

  /* For GPUs that can use bindless textures in some way or another. */
  device_vector<TextureInfo> texture_info;
  thread_mutex texture_info_mutex;
  bool need_texture_info = false;
  /* Returns true if the texture info was copied to the device (meaning, some more
   * re-initialization might be needed). */
  virtual bool load_texture_info();

 protected:
  /* Memory allocation, only accessed through device_memory. */
  friend class device_memory;

  bool can_map_host = false;
  size_t map_host_used = 0;
  size_t map_host_limit = 0;
  size_t device_texture_headroom = 0;
  size_t device_working_headroom = 0;
  using texMemObject = unsigned long long;
  using arrayMemObject = unsigned long long;
  struct Mem {
    Mem() = default;

    texMemObject texobject = 0;
    arrayMemObject array = 0;
  };
  using MemMap = map<device_memory *, Mem>;
  MemMap device_mem_map;
  thread_mutex device_mem_map_mutex;
  /* Simple counter which will try to track amount of used device memory */
  size_t device_mem_in_use = 0;

  virtual void init_host_memory(const size_t preferred_texture_headroom = 0,
                                const size_t preferred_working_headroom = 0);
  virtual void move_textures_to_host(const size_t size,
                                     const size_t headroom,
                                     const bool for_texture);

  /* Allocation, deallocation and copy functions, with corresponding
   * support of device/host allocations. */
  virtual GPUDevice::Mem *generic_alloc(device_memory &mem, const size_t pitch_padding = 0);
  virtual void generic_free(device_memory &mem);
  virtual void generic_copy_to(device_memory &mem);

  /* total - amount of device memory, free - amount of available device memory */
  virtual void get_device_memory_info(size_t &total, size_t &free) = 0;

  /* Device side memory. */
  virtual bool alloc_device(void *&device_pointer, const size_t size) = 0;
  virtual void free_device(void *device_pointer) = 0;

  /* Shared memory. */
  virtual bool shared_alloc(void *&shared_pointer, const size_t size) = 0;
  virtual void shared_free(void *shared_pointer) = 0;
  bool is_shared(const void *shared_pointer,
                 const device_ptr device_pointer,
                 Device *sub_device) override;
  /* This function should return device pointer corresponding to shared pointer, which
   * is host buffer, allocated in `shared_alloc`. */
  virtual void *shared_to_device_pointer(const void *shared_pointer) = 0;

  /* Memory copy. */
  virtual void copy_host_to_device(void *device_pointer,
                                   void *host_pointer,
                                   const size_t size) = 0;
};

CCL_NAMESPACE_END
