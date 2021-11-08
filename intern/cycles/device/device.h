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

/* Device Types */

enum DeviceType {
  DEVICE_NONE = 0,
  DEVICE_CPU,
  DEVICE_CUDA,
  DEVICE_MULTI,
  DEVICE_OPTIX,
  DEVICE_HIP,
  DEVICE_DUMMY,
};

enum DeviceTypeMask {
  DEVICE_MASK_CPU = (1 << DEVICE_CPU),
  DEVICE_MASK_CUDA = (1 << DEVICE_CUDA),
  DEVICE_MASK_OPTIX = (1 << DEVICE_OPTIX),
  DEVICE_MASK_HIP = (1 << DEVICE_HIP),
  DEVICE_MASK_ALL = ~0
};

#define DEVICE_MASK(type) (DeviceTypeMask)(1 << type)

class DeviceInfo {
 public:
  DeviceType type;
  string description;
  string id; /* used for user preferences, should stay fixed with changing hardware config */
  int num;
  bool display_device;        /* GPU is used as a display device. */
  bool has_nanovdb;           /* Support NanoVDB volumes. */
  bool has_osl;               /* Support Open Shading Language. */
  bool has_profiling;         /* Supports runtime collection of profiling info. */
  bool has_peer_memory;       /* GPU has P2P access to memory of another GPU. */
  bool has_gpu_queue;         /* Device supports GPU queue. */
  DenoiserTypeMask denoisers; /* Supported denoiser types. */
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
    has_osl = false;
    has_profiling = false;
    has_peer_memory = false;
    has_gpu_queue = false;
    denoisers = DENOISER_NONE;
  }

  bool operator==(const DeviceInfo &info) const
  {
    /* Multiple Devices with the same ID would be very bad. */
    assert(id != info.id ||
           (type == info.type && num == info.num && description == info.description));
    return id == info.id;
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
  virtual bool show_samples() const
  {
    return false;
  }
  virtual BVHLayoutMask get_bvh_layout_mask() const = 0;

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

  /* acceleration structure building */
  virtual void build_bvh(BVH *bvh, Progress &progress, bool refit);

  /* OptiX specific destructor. */
  virtual void release_optix_bvh(BVH * /*bvh*/){};

  /* multi device */
  virtual int device_number(Device * /*sub_device*/)
  {
    return 0;
  }

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

  /* Buffer denoising. */

  /* Returns true if task is fully handled. */
  virtual bool denoise_buffer(const DeviceDenoiseTask & /*task*/)
  {
    LOG(ERROR) << "Request buffer denoising from a device which does not support it.";
    return false;
  }

  virtual DeviceQueue *get_denoise_queue()
  {
    LOG(ERROR) << "Request denoising queue from a device which does not support it.";
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
  static uint devices_initialized_mask;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_H__ */
