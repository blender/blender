/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/cpu/device_impl.h"

#include <stdlib.h>
#include <string.h>

/* So ImathMath is included before our kernel_cpu_compat. */
#ifdef WITH_OSL
/* So no context pollution happens from indirectly included windows.h */
#  include "util/windows.h"
#  include <OSL/oslexec.h>
#endif

#ifdef WITH_EMBREE
#  if EMBREE_MAJOR_VERSION >= 4
#    include <embree4/rtcore.h>
#  else
#    include <embree3/rtcore.h>
#  endif
#endif

#include "device/cpu/kernel.h"
#include "device/cpu/kernel_thread_globals.h"

#include "device/device.h"

// clang-format off
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"
#include "kernel/device/cpu/kernel.h"
#include "kernel/types.h"

#include "kernel/osl/globals.h"
// clang-format on

#include "bvh/embree.h"

#include "session/buffers.h"

#include "util/debug.h"
#include "util/foreach.h"
#include "util/function.h"
#include "util/guiding.h"
#include "util/log.h"
#include "util/map.h"
#include "util/openimagedenoise.h"
#include "util/optimization.h"
#include "util/progress.h"
#include "util/system.h"
#include "util/task.h"
#include "util/thread.h"

CCL_NAMESPACE_BEGIN

CPUDevice::CPUDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_)
    : Device(info_, stats_, profiler_), texture_info(this, "texture_info", MEM_GLOBAL)
{
  /* Pick any kernel, all of them are supposed to have same level of microarchitecture
   * optimization. */
  VLOG_INFO << "Using " << get_cpu_kernels().integrator_init_from_camera.get_uarch_name()
            << " CPU kernels.";

  if (info.cpu_threads == 0) {
    info.cpu_threads = TaskScheduler::max_concurrency();
  }

#ifdef WITH_OSL
  kernel_globals.osl = &osl_globals;
#endif
#ifdef WITH_EMBREE
  embree_device = rtcNewDevice("verbose=0");
#endif
  need_texture_info = false;
}

CPUDevice::~CPUDevice()
{
#ifdef WITH_EMBREE
  rtcReleaseDevice(embree_device);
#endif

  texture_info.free();
}

BVHLayoutMask CPUDevice::get_bvh_layout_mask(uint /*kernel_features*/) const
{
  BVHLayoutMask bvh_layout_mask = BVH_LAYOUT_BVH2;
#ifdef WITH_EMBREE
  bvh_layout_mask |= BVH_LAYOUT_EMBREE;
#endif /* WITH_EMBREE */
  return bvh_layout_mask;
}

bool CPUDevice::load_texture_info()
{
  if (!need_texture_info) {
    return false;
  }

  texture_info.copy_to_device();
  need_texture_info = false;

  return true;
}

void CPUDevice::mem_alloc(device_memory &mem)
{
  if (mem.type == MEM_TEXTURE) {
    assert(!"mem_alloc not supported for textures.");
  }
  else if (mem.type == MEM_GLOBAL) {
    assert(!"mem_alloc not supported for global memory.");
  }
  else {
    if (mem.name) {
      VLOG_WORK << "Buffer allocate: " << mem.name << ", "
                << string_human_readable_number(mem.memory_size()) << " bytes. ("
                << string_human_readable_size(mem.memory_size()) << ")";
    }

    if (mem.type == MEM_DEVICE_ONLY || !mem.host_pointer) {
      size_t alignment = MIN_ALIGNMENT_CPU_DATA_TYPES;
      void *data = util_aligned_malloc(mem.memory_size(), alignment);
      mem.device_pointer = (device_ptr)data;
    }
    else {
      mem.device_pointer = (device_ptr)mem.host_pointer;
    }

    mem.device_size = mem.memory_size();
    stats.mem_alloc(mem.device_size);
  }
}

void CPUDevice::mem_copy_to(device_memory &mem)
{
  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
    global_alloc(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
    tex_alloc((device_texture &)mem);
  }
  else {
    if (!mem.device_pointer) {
      mem_alloc(mem);
    }

    /* copy is no-op */
  }
}

void CPUDevice::mem_copy_from(
    device_memory & /*mem*/, size_t /*y*/, size_t /*w*/, size_t /*h*/, size_t /*elem*/)
{
  /* no-op */
}

void CPUDevice::mem_zero(device_memory &mem)
{
  if (!mem.device_pointer) {
    mem_alloc(mem);
  }

  if (mem.device_pointer) {
    memset((void *)mem.device_pointer, 0, mem.memory_size());
  }
}

void CPUDevice::mem_free(device_memory &mem)
{
  if (mem.type == MEM_GLOBAL) {
    global_free(mem);
  }
  else if (mem.type == MEM_TEXTURE) {
    tex_free((device_texture &)mem);
  }
  else if (mem.device_pointer) {
    if (mem.type == MEM_DEVICE_ONLY || !mem.host_pointer) {
      util_aligned_free((void *)mem.device_pointer);
    }
    mem.device_pointer = 0;
    stats.mem_free(mem.device_size);
    mem.device_size = 0;
  }
}

device_ptr CPUDevice::mem_alloc_sub_ptr(device_memory &mem, size_t offset, size_t /*size*/)
{
  return (device_ptr)(((char *)mem.device_pointer) + mem.memory_elements_size(offset));
}

void CPUDevice::const_copy_to(const char *name, void *host, size_t size)
{
#ifdef WITH_EMBREE
  if (strcmp(name, "data") == 0) {
    assert(size <= sizeof(KernelData));

    // Update scene handle (since it is different for each device on multi devices)
    KernelData *const data = (KernelData *)host;
    data->device_bvh = embree_scene;
  }
#endif
  kernel_const_copy(&kernel_globals, name, host, size);
}

void CPUDevice::global_alloc(device_memory &mem)
{
  VLOG_WORK << "Global memory allocate: " << mem.name << ", "
            << string_human_readable_number(mem.memory_size()) << " bytes. ("
            << string_human_readable_size(mem.memory_size()) << ")";

  kernel_global_memory_copy(&kernel_globals, mem.name, mem.host_pointer, mem.data_size);

  mem.device_pointer = (device_ptr)mem.host_pointer;
  mem.device_size = mem.memory_size();
  stats.mem_alloc(mem.device_size);
}

void CPUDevice::global_free(device_memory &mem)
{
  if (mem.device_pointer) {
    mem.device_pointer = 0;
    stats.mem_free(mem.device_size);
    mem.device_size = 0;
  }
}

void CPUDevice::tex_alloc(device_texture &mem)
{
  VLOG_WORK << "Texture allocate: " << mem.name << ", "
            << string_human_readable_number(mem.memory_size()) << " bytes. ("
            << string_human_readable_size(mem.memory_size()) << ")";

  mem.device_pointer = (device_ptr)mem.host_pointer;
  mem.device_size = mem.memory_size();
  stats.mem_alloc(mem.device_size);

  const uint slot = mem.slot;
  if (slot >= texture_info.size()) {
    /* Allocate some slots in advance, to reduce amount of re-allocations. */
    texture_info.resize(slot + 128);
  }

  texture_info[slot] = mem.info;
  texture_info[slot].data = (uint64_t)mem.host_pointer;
  need_texture_info = true;
}

void CPUDevice::tex_free(device_texture &mem)
{
  if (mem.device_pointer) {
    mem.device_pointer = 0;
    stats.mem_free(mem.device_size);
    mem.device_size = 0;
    need_texture_info = true;
  }
}

void CPUDevice::build_bvh(BVH *bvh, Progress &progress, bool refit)
{
#ifdef WITH_EMBREE
  if (bvh->params.bvh_layout == BVH_LAYOUT_EMBREE ||
      bvh->params.bvh_layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE ||
      bvh->params.bvh_layout == BVH_LAYOUT_MULTI_METAL_EMBREE ||
      bvh->params.bvh_layout == BVH_LAYOUT_MULTI_EMBREEGPU_EMBREE)
  {
    BVHEmbree *const bvh_embree = static_cast<BVHEmbree *>(bvh);
    if (refit) {
      bvh_embree->refit(progress);
    }
    else {
      bvh_embree->build(progress, &stats, embree_device);
    }

    if (bvh->params.top_level) {
      embree_scene = bvh_embree->scene;
    }
  }
  else
#endif
    Device::build_bvh(bvh, progress, refit);
}

void *CPUDevice::get_guiding_device() const
{
#ifdef WITH_PATH_GUIDING
  if (!guiding_device) {
    if (guiding_device_type() == 8) {
      guiding_device = make_unique<openpgl::cpp::Device>(PGL_DEVICE_TYPE_CPU_8);
    }
    else if (guiding_device_type() == 4) {
      guiding_device = make_unique<openpgl::cpp::Device>(PGL_DEVICE_TYPE_CPU_4);
    }
  }
  return guiding_device.get();
#else
  return nullptr;
#endif
}

void CPUDevice::get_cpu_kernel_thread_globals(
    vector<CPUKernelThreadGlobals> &kernel_thread_globals)
{
  /* Ensure latest texture info is loaded into kernel globals before returning. */
  load_texture_info();

  kernel_thread_globals.clear();
  void *osl_memory = get_cpu_osl_memory();
  for (int i = 0; i < info.cpu_threads; i++) {
    kernel_thread_globals.emplace_back(kernel_globals, osl_memory, profiler);
  }
}

void *CPUDevice::get_cpu_osl_memory()
{
#ifdef WITH_OSL
  return &osl_globals;
#else
  return NULL;
#endif
}

bool CPUDevice::load_kernels(const uint /*kernel_features*/)
{
  return true;
}

CCL_NAMESPACE_END
