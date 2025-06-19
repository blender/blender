/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* So ImathMath is included before our kernel_cpu_compat. */
#ifdef WITH_OSL
#  include <cstdint> /* Needed before `sdlexec.h` for `int32_t` with GCC 15.1. */
/* So no context pollution happens from indirectly included windows.h */
#  ifdef _WIN32
#    include "util/windows.h"
#  endif
#  include <OSL/oslexec.h>
#endif

#ifdef WITH_EMBREE
#  include <embree4/rtcore.h>
#endif

#include "device/cpu/kernel.h"
#include "device/device.h"
#include "device/memory.h"

// clang-format off
#include "kernel/device/cpu/kernel.h"
#include "kernel/globals.h"

#include "kernel/osl/globals.h"
// clang-format on

#include "util/guiding.h"  // IWYU pragma: keep
#include "util/unique_ptr.h"

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
#  if RTC_VERSION >= 40400
  RTCTraversable embree_traversable = nullptr;
#  else
  RTCScene embree_traversable = nullptr;
#  endif
  RTCDevice embree_device;
#endif
#if defined(WITH_PATH_GUIDING)
  mutable unique_ptr<openpgl::cpp::Device> guiding_device;
#endif

  CPUDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_, bool headless_);
  ~CPUDevice() override;

  BVHLayoutMask get_bvh_layout_mask(uint /*kernel_features*/) const override;

  /* Returns true if the texture info was copied to the device (meaning, some more
   * re-initialization might be needed). */
  bool load_texture_info();

  void mem_alloc(device_memory &mem) override;
  void mem_copy_to(device_memory &mem) override;
  void mem_move_to_host(device_memory &mem) override;
  void mem_copy_from(
      device_memory &mem, const size_t y, size_t w, const size_t h, size_t elem) override;
  void mem_zero(device_memory &mem) override;
  void mem_free(device_memory &mem) override;
  device_ptr mem_alloc_sub_ptr(device_memory &mem, const size_t offset, size_t /*size*/) override;

  void const_copy_to(const char *name, void *host, const size_t size) override;

  void global_alloc(device_memory &mem);
  void global_free(device_memory &mem);

  void tex_alloc(device_texture &mem);
  void tex_free(device_texture &mem);

  void build_bvh(BVH *bvh, Progress &progress, bool refit) override;

  void *get_guiding_device() const override;

  void get_cpu_kernel_thread_globals(
      vector<ThreadKernelGlobalsCPU> &kernel_thread_globals) override;
  OSLGlobals *get_cpu_osl_memory() override;

 protected:
  bool load_kernels(uint /*kernel_features*/) override;
};

CCL_NAMESPACE_END
