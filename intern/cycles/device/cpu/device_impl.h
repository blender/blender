/*
 * Copyright 2011-2021 Blender Foundation
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

#pragma once

/* So ImathMath is included before our kernel_cpu_compat. */
#ifdef WITH_OSL
/* So no context pollution happens from indirectly included windows.h */
#  include "util/util_windows.h"
#  include <OSL/oslexec.h>
#endif

#ifdef WITH_EMBREE
#  include <embree3/rtcore.h>
#endif

#include "device/cpu/kernel.h"
#include "device/device.h"
#include "device/device_memory.h"

// clang-format off
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/kernel.h"
#include "kernel/device/cpu/globals.h"

#include "kernel/osl/osl_shader.h"
#include "kernel/osl/osl_globals.h"
// clang-format on

CCL_NAMESPACE_BEGIN

class CPUDevice : public Device {
 public:
  KernelGlobalsCPU kernel_globals;

  device_vector<TextureInfo> texture_info;
  bool need_texture_info;

#ifdef WITH_OSL
  OSLGlobals osl_globals;
#endif
#ifdef WITH_EMBREE
  RTCScene embree_scene = NULL;
  RTCDevice embree_device;
#endif

  CPUKernels kernels;

  CPUDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_);
  ~CPUDevice();

  virtual bool show_samples() const override;

  virtual BVHLayoutMask get_bvh_layout_mask() const override;

  /* Returns true if the texture info was copied to the device (meaning, some more
   * re-initialization might be needed). */
  bool load_texture_info();

  virtual void mem_alloc(device_memory &mem) override;
  virtual void mem_copy_to(device_memory &mem) override;
  virtual void mem_copy_from(
      device_memory &mem, size_t y, size_t w, size_t h, size_t elem) override;
  virtual void mem_zero(device_memory &mem) override;
  virtual void mem_free(device_memory &mem) override;
  virtual device_ptr mem_alloc_sub_ptr(device_memory &mem,
                                       size_t offset,
                                       size_t /*size*/) override;

  virtual void const_copy_to(const char *name, void *host, size_t size) override;

  void global_alloc(device_memory &mem);
  void global_free(device_memory &mem);

  void tex_alloc(device_texture &mem);
  void tex_free(device_texture &mem);

  void build_bvh(BVH *bvh, Progress &progress, bool refit) override;

  virtual const CPUKernels *get_cpu_kernels() const override;
  virtual void get_cpu_kernel_thread_globals(
      vector<CPUKernelThreadGlobals> &kernel_thread_globals) override;
  virtual void *get_cpu_osl_memory() override;

 protected:
  virtual bool load_kernels(uint /*kernel_features*/) override;
};

CCL_NAMESPACE_END
