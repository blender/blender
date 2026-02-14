/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/cpu/device_impl.h"

#include <cstdlib>
#include <cstring>

/* So ImathMath is included before our kernel_cpu_compat. */
#ifdef WITH_OSL
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

#include "kernel/device/cpu/kernel.h"
#include "kernel/globals.h"
#include "kernel/types.h"

#include "bvh/embree.h"

#include "session/buffers.h"

#include "util/guiding.h"
#include "util/log.h"
#include "util/progress.h"
#include "util/task.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

CPUDevice::CPUDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_, bool headless_)
    : Device(info_, stats_, profiler_, headless_), image_info(this, "image_info", MEM_GLOBAL)
{
  /* Pick any kernel, all of them are supposed to have same level of microarchitecture
   * optimization. */
  LOG_INFO << "Using " << get_cpu_kernels().integrator_init_from_camera.get_uarch_name()
           << " CPU kernels.";

  if (info.cpu_threads == 0) {
    info.cpu_threads = TaskScheduler::max_concurrency();
  }

#ifdef WITH_EMBREE
  embree_device = rtcNewDevice("verbose=0");
#endif
  need_image_info = false;
}

CPUDevice::~CPUDevice()
{
#ifdef WITH_EMBREE
  rtcReleaseDevice(embree_device);
#endif

  image_info.free();
}

BVHLayoutMask CPUDevice::get_bvh_layout_mask(uint /*kernel_features*/) const
{
  BVHLayoutMask bvh_layout_mask = BVH_LAYOUT_BVH2;
#ifdef WITH_EMBREE
  bvh_layout_mask |= BVH_LAYOUT_EMBREE;
#endif /* WITH_EMBREE */
  return bvh_layout_mask;
}

bool CPUDevice::load_image_info()
{
  if (!need_image_info) {
    return false;
  }

  image_info.copy_to_device();
  need_image_info = false;

  return true;
}

void CPUDevice::mem_alloc(device_memory &mem)
{
  if (mem.type == MEM_IMAGE_TEXTURE) {
    assert(!"mem_alloc not supported for images.");
  }
  else if (mem.type == MEM_GLOBAL) {
    assert(!"mem_alloc not supported for global memory.");
  }
  else {
    LOG_DEBUG << "Buffer allocate: " << mem.log_name() << ", "
              << string_human_readable_number(mem.memory_size()) << " bytes. ("
              << string_human_readable_size(mem.memory_size()) << ")";

    if (mem.type == MEM_DEVICE_ONLY) {
      size_t alignment = MIN_ALIGNMENT_DEVICE_MEMORY;
      void *data = util_aligned_malloc(mem.memory_size(), alignment);
      mem.device_pointer = (device_ptr)data;
    }
    else {
      assert(!(mem.host_pointer == nullptr && mem.memory_size() > 0));
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
  else if (mem.type == MEM_IMAGE_TEXTURE) {
    image_free((device_image &)mem);
    image_alloc((device_image &)mem);
  }
  else {
    if (!mem.device_pointer) {
      mem_alloc(mem);
    }

    /* copy is no-op */
  }
}

void CPUDevice::mem_move_to_host(device_memory & /*mem*/)
{
  /* no-op */
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
  else if (mem.type == MEM_IMAGE_TEXTURE) {
    image_free((device_image &)mem);
  }
  else if (mem.device_pointer) {
    if (mem.type == MEM_DEVICE_ONLY) {
      util_aligned_free((void *)mem.device_pointer, mem.memory_size());
    }
    mem.device_pointer = 0;
    stats.mem_free(mem.device_size);
    mem.device_size = 0;
  }
}

device_ptr CPUDevice::mem_alloc_sub_ptr(device_memory &mem, const size_t offset, size_t /*size*/)
{
  return (device_ptr)(((char *)mem.device_pointer) + mem.memory_elements_size(offset));
}

void CPUDevice::const_copy_to(const char *name, void *host, const size_t size)
{
#ifdef WITH_EMBREE
  if (strcmp(name, "data") == 0) {
    assert(size <= sizeof(KernelData));

    /* Update scene handle (since it is different for each device on multi devices).
     * This must be a raw pointer copy since at some points during scene update this
     * pointer may be invalid. */
    KernelData *const data = (KernelData *)host;
    data->device_bvh = embree_traversable;
  }
#endif
  kernel_const_copy(&kernel_globals, name, host, size);
}

void CPUDevice::global_alloc(device_memory &mem)
{
  LOG_DEBUG << "Global memory allocate: " << mem.log_name() << ", "
            << string_human_readable_number(mem.memory_size()) << " bytes. ("
            << string_human_readable_size(mem.memory_size()) << ")";

  kernel_global_memory_copy(&kernel_globals, mem.global_name(), mem.host_pointer, mem.data_size);

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

void CPUDevice::image_alloc(device_image &mem)
{
  LOG_DEBUG << "Texture allocate: " << mem.log_name() << ", "
            << string_human_readable_number(mem.memory_size()) << " bytes. ("
            << string_human_readable_size(mem.memory_size()) << ")";

  mem.device_pointer = (device_ptr)mem.host_pointer;
  mem.device_size = mem.memory_size();
  stats.mem_alloc(mem.device_size);

  const uint slot = mem.slot;
  if (slot >= image_info.size()) {
    /* Allocate some slots in advance, to reduce amount of re-allocations. */
    image_info.resize(slot + 128);
  }

  image_info[slot] = mem.info;
  image_info[slot].data = (uint64_t)mem.host_pointer;
  need_image_info = true;
}

void CPUDevice::image_free(device_image &mem)
{
  if (mem.device_pointer) {
    mem.device_pointer = 0;
    stats.mem_free(mem.device_size);
    mem.device_size = 0;
    need_image_info = true;
  }
}

void CPUDevice::build_bvh(BVH *bvh, Progress &progress, bool refit)
{
#ifdef WITH_EMBREE
  if (bvh->params.bvh_layout == BVH_LAYOUT_EMBREE ||
      bvh->params.bvh_layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE ||
      bvh->params.bvh_layout == BVH_LAYOUT_MULTI_METAL_EMBREE ||
      bvh->params.bvh_layout == BVH_LAYOUT_MULTI_HIPRT_EMBREE ||
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
#  if RTC_VERSION >= 40400
      embree_traversable = rtcGetSceneTraversable(bvh_embree->scene);
#  else
      embree_traversable = bvh_embree->scene;
#  endif
    }
  }
  else
#endif
  {
    Device::build_bvh(bvh, progress, refit);
  }
}

void *CPUDevice::get_guiding_device() const
{
#if defined(WITH_PATH_GUIDING)
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
    vector<ThreadKernelGlobalsCPU> &kernel_thread_globals)
{
  /* Ensure latest image info is loaded into kernel globals before returning. */
  load_image_info();

  kernel_thread_globals.clear();
  OSLGlobals *osl_globals = get_cpu_osl_memory();
  for (int i = 0; i < info.cpu_threads; i++) {
    kernel_thread_globals.emplace_back(kernel_globals, osl_globals, profiler, i);
  }
}

OSLGlobals *CPUDevice::get_cpu_osl_memory()
{
#ifdef WITH_OSL
  return &osl_globals;
#else
  return nullptr;
#endif
}

bool CPUDevice::load_kernels(const uint /*kernel_features*/)
{
  return true;
}

CCL_NAMESPACE_END
