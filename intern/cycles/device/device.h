/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <stdlib.h>

#include "bvh/params.h"

#include "device/denoise.h"
#include "device/memory.h"

#include "util/function.h"
#include "util/list.h"
#include "util/log.h"
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
class Progress;
class CPUKernels;
class CPUKernelThreadGlobals;
class Scene;

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
  DeviceType type;
  string description;
  string id; /* used for user preferences, should stay fixed with changing hardware config */
  int num;
  bool display_device;          /* GPU is used as a display device. */
  bool has_nanovdb;             /* Support NanoVDB volumes. */
  bool has_light_tree;          /* Support light tree. */
  bool has_mnee;                /* Support MNEE. */
  bool has_osl;                 /* Support Open Shading Language. */
  bool has_guiding;             /* Support path guiding. */
  bool has_profiling;           /* Supports runtime collection of profiling info. */
  bool has_peer_memory;         /* GPU has P2P access to memory of another GPU. */
  bool has_gpu_queue;           /* Device supports GPU queue. */
  bool use_hardware_raytracing; /* Use hardware instructions to accelerate ray tracing. */
  bool use_metalrt_by_default;  /* Use MetalRT by default. */
  KernelOptimizationLevel kernel_optimization_level; /* Optimization level applied to path tracing
                                                      * kernels (Metal only). */
  DenoiserTypeMask denoisers;                        /* Supported denoiser types. */
  int cpu_threads;
  vector<DeviceInfo> multi_devices;
  string error_msg;

  DeviceInfo()
  {
    type = DEVICE_CPU;
    id = "CPU";
    num = 0;
    cpu_threads = 0;
    display_device = false;
    has_nanovdb = false;
    has_light_tree = true;
    has_mnee = true;
    has_osl = false;
    has_guiding = false;
    has_profiling = false;
    has_peer_memory = false;
    has_gpu_queue = false;
    use_hardware_raytracing = false;
    use_metalrt_by_default = false;
    denoisers = DENOISER_NONE;
  }

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
};

/* Device */

class Device {
  friend class device_sub_ptr;

 protected:
  Device(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_)
      : info(info_), stats(stats_), profiler(profiler_)
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
  virtual void set_error(const string &error)
  {
    if (!have_error()) {
      error_msg = error;
    }
    fprintf(stderr, "%s\n", error.c_str());
    fflush(stderr);
  }
  virtual BVHLayoutMask get_bvh_layout_mask(uint kernel_features) const = 0;

  /* statistics */
  Stats &stats;
  Profiler &profiler;

  /* constant memory */
  virtual void const_copy_to(const char *name, void *host, size_t size) = 0;

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
      vector<CPUKernelThreadGlobals> & /*kernel_thread_globals*/);
  /* Get OpenShadingLanguage memory buffer. */
  virtual void *get_cpu_osl_memory();

  /* Acceleration structure building. */
  virtual void build_bvh(BVH *bvh, Progress &progress, bool refit);
  /* Used by Metal and OptiX. */
  virtual void release_bvh(BVH * /*bvh*/) {}

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

  /* Graphics resources interoperability.
   *
   * The interoperability comes here by the meaning that the device is capable of computing result
   * directly into an OpenGL (or other graphics library) buffer. */

  /* Check display is to be updated using graphics interoperability.
   * The interoperability can not be used is it is not supported by the device. But the device
   * might also force disable the interoperability if it detects that it will be slower than
   * copying pixels from the render buffer. */
  virtual bool should_use_graphics_interop()
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
  virtual void *get_guiding_device() const
  {
    LOG(ERROR) << "Request guiding field from a device which does not support it.";
    return nullptr;
  }

  /* Sub-devices */

  /* Run given callback for every individual device which will be handling rendering.
   * For the single device the callback is called for the device itself. For the multi-device the
   * callback is only called for the sub-devices. */
  virtual void foreach_device(const function<void(Device *)> &callback)
  {
    callback(this);
  }

  /* static */
  static Device *create(const DeviceInfo &info, Stats &stats, Profiler &profiler);

  static DeviceType type_from_string(const char *name);
  static string string_from_type(DeviceType type);
  static vector<DeviceType> available_types();
  static vector<DeviceInfo> available_devices(uint device_type_mask = DEVICE_MASK_ALL);
  static DeviceInfo dummy_device(const string &error_msg = "");
  static string device_capabilities(uint device_type_mask = DEVICE_MASK_ALL);
  static DeviceInfo get_multi_device(const vector<DeviceInfo> &subdevices,
                                     int threads,
                                     bool background);

  /* Tag devices lists for update. */
  static void tag_update();

  static void free_memory();

 protected:
  /* Memory allocation, only accessed through device_memory. */
  friend class MultiDevice;
  friend class DeviceServer;
  friend class device_memory;

  virtual void mem_alloc(device_memory &mem) = 0;
  virtual void mem_copy_to(device_memory &mem) = 0;
  virtual void mem_copy_from(device_memory &mem, size_t y, size_t w, size_t h, size_t elem) = 0;
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
  GPUDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_)
      : Device(info_, stats_, profiler_),
        texture_info(this, "texture_info", MEM_GLOBAL),
        need_texture_info(false),
        can_map_host(false),
        map_host_used(0),
        map_host_limit(0),
        device_texture_headroom(0),
        device_working_headroom(0),
        device_mem_map(),
        device_mem_map_mutex(),
        move_texture_to_host(false),
        device_mem_in_use(0)
  {
  }

 public:
  virtual ~GPUDevice() noexcept(false);

  /* For GPUs that can use bindless textures in some way or another. */
  device_vector<TextureInfo> texture_info;
  bool need_texture_info;
  /* Returns true if the texture info was copied to the device (meaning, some more
   * re-initialization might be needed). */
  virtual bool load_texture_info();

 protected:
  /* Memory allocation, only accessed through device_memory. */
  friend class device_memory;

  bool can_map_host;
  size_t map_host_used;
  size_t map_host_limit;
  size_t device_texture_headroom;
  size_t device_working_headroom;
  typedef unsigned long long texMemObject;
  typedef unsigned long long arrayMemObject;
  struct Mem {
    Mem() : texobject(0), array(0), use_mapped_host(false) {}

    texMemObject texobject;
    arrayMemObject array;

    /* If true, a mapped host memory in shared_pointer is being used. */
    bool use_mapped_host;
  };
  typedef map<device_memory *, Mem> MemMap;
  MemMap device_mem_map;
  thread_mutex device_mem_map_mutex;
  bool move_texture_to_host;
  /* Simple counter which will try to track amount of used device memory */
  size_t device_mem_in_use;

  virtual void init_host_memory(size_t preferred_texture_headroom = 0,
                                size_t preferred_working_headroom = 0);
  virtual void move_textures_to_host(size_t size, bool for_texture);

  /* Allocation, deallocation and copy functions, with corresponding
   * support of device/host allocations. */
  virtual GPUDevice::Mem *generic_alloc(device_memory &mem, size_t pitch_padding = 0);
  virtual void generic_free(device_memory &mem);
  virtual void generic_copy_to(device_memory &mem);

  /* total - amount of device memory, free - amount of available device memory */
  virtual void get_device_memory_info(size_t &total, size_t &free) = 0;

  virtual bool alloc_device(void *&device_pointer, size_t size) = 0;

  virtual void free_device(void *device_pointer) = 0;

  virtual bool alloc_host(void *&shared_pointer, size_t size) = 0;

  virtual void free_host(void *shared_pointer) = 0;

  /* This function should return device pointer corresponding to shared pointer, which
   * is host buffer, allocated in `alloc_host`. The function should `true`, if such
   * address transformation is possible and `false` otherwise. */
  virtual void transform_host_pointer(void *&device_pointer, void *&shared_pointer) = 0;

  virtual void copy_host_to_device(void *device_pointer, void *host_pointer, size_t size) = 0;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_H__ */
