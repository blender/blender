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

#include "device/cpu/device_impl.h"

#include <stdlib.h>
#include <string.h>

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
#include "device/cpu/kernel_thread_globals.h"

#include "device/device.h"

// clang-format off
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"
#include "kernel/device/cpu/kernel.h"
#include "kernel/kernel_types.h"

#include "kernel/osl/osl_shader.h"
#include "kernel/osl/osl_globals.h"
// clang-format on

#include "bvh/bvh_embree.h"

#include "render/buffers.h"

#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_function.h"
#include "util/util_logging.h"
#include "util/util_map.h"
#include "util/util_opengl.h"
#include "util/util_openimagedenoise.h"
#include "util/util_optimization.h"
#include "util/util_progress.h"
#include "util/util_system.h"
#include "util/util_task.h"
#include "util/util_thread.h"

CCL_NAMESPACE_BEGIN

CPUDevice::CPUDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_)
    : Device(info_, stats_, profiler_), texture_info(this, "__texture_info", MEM_GLOBAL)
{
  /* Pick any kernel, all of them are supposed to have same level of microarchitecture
   * optimization. */
  VLOG(1) << "Will be using " << kernels.integrator_init_from_camera.get_uarch_name()
          << " kernels.";

  if (info.cpu_threads == 0) {
    info.cpu_threads = TaskScheduler::num_threads();
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

bool CPUDevice::show_samples() const
{
  return (info.cpu_threads == 1);
}

BVHLayoutMask CPUDevice::get_bvh_layout_mask() const
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
      VLOG(1) << "Buffer allocate: " << mem.name << ", "
              << string_human_readable_number(mem.memory_size()) << " bytes. ("
              << string_human_readable_size(mem.memory_size()) << ")";
    }

    if (mem.type == MEM_DEVICE_ONLY) {
      assert(!mem.host_pointer);
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
    if (mem.type == MEM_DEVICE_ONLY) {
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
#if WITH_EMBREE
  if (strcmp(name, "__data") == 0) {
    assert(size <= sizeof(KernelData));

    // Update scene handle (since it is different for each device on multi devices)
    KernelData *const data = (KernelData *)host;
    data->bvh.scene = embree_scene;
  }
#endif
  kernel_const_copy(&kernel_globals, name, host, size);
}

void CPUDevice::global_alloc(device_memory &mem)
{
  VLOG(1) << "Global memory allocate: " << mem.name << ", "
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
  VLOG(1) << "Texture allocate: " << mem.name << ", "
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
      bvh->params.bvh_layout == BVH_LAYOUT_MULTI_OPTIX_EMBREE) {
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

#if 0
void CPUDevice::render(DeviceTask &task, RenderTile &tile, KernelGlobals *kg)
{
  const bool use_coverage = kernel_data.film.cryptomatte_passes & CRYPT_ACCURATE;

  scoped_timer timer(&tile.buffers->render_time);

  Coverage coverage(kg, tile);
  if (use_coverage) {
    coverage.init_path_trace();
  }

  float *render_buffer = (float *)tile.buffer;
  int start_sample = tile.start_sample;
  int end_sample = tile.start_sample + tile.num_samples;

  /* Needed for Embree. */
  SIMD_SET_FLUSH_TO_ZERO;

  for (int sample = start_sample; sample < end_sample; sample++) {
    if (task.get_cancel() || TaskPool::canceled()) {
      if (task.need_finish_queue == false)
        break;
    }

    if (tile.stealing_state == RenderTile::CAN_BE_STOLEN && task.get_tile_stolen()) {
      tile.stealing_state = RenderTile::WAS_STOLEN;
      break;
    }

    if (tile.task == RenderTile::PATH_TRACE) {
      for (int y = tile.y; y < tile.y + tile.h; y++) {
        for (int x = tile.x; x < tile.x + tile.w; x++) {
          if (use_coverage) {
            coverage.init_pixel(x, y);
          }
          kernels.path_trace(kg, render_buffer, sample, x, y, tile.offset, tile.stride);
        }
      }
    }
    else {
      for (int y = tile.y; y < tile.y + tile.h; y++) {
        for (int x = tile.x; x < tile.x + tile.w; x++) {
          kernels.bake(kg, render_buffer, sample, x, y, tile.offset, tile.stride);
        }
      }
    }
    tile.sample = sample + 1;

    if (task.adaptive_sampling.use && task.adaptive_sampling.need_filter(sample)) {
      const bool stop = adaptive_sampling_filter(kg, tile, sample);
      if (stop) {
        const int num_progress_samples = end_sample - sample;
        tile.sample = end_sample;
        task.update_progress(&tile, tile.w * tile.h * num_progress_samples);
        break;
      }
    }

    task.update_progress(&tile, tile.w * tile.h);
  }
  if (use_coverage) {
    coverage.finalize();
  }

  if (task.adaptive_sampling.use && (tile.stealing_state != RenderTile::WAS_STOLEN)) {
    adaptive_sampling_post(tile, kg);
  }
}

void CPUDevice::thread_render(DeviceTask &task)
{
  if (TaskPool::canceled()) {
    if (task.need_finish_queue == false)
      return;
  }

  /* allocate buffer for kernel globals */
  CPUKernelThreadGlobals kg(kernel_globals, get_cpu_osl_memory());

  profiler.add_state(&kg.profiler);

  /* NLM denoiser. */
  DenoisingTask *denoising = NULL;

  /* OpenImageDenoise: we can only denoise with one thread at a time, so to
   * avoid waiting with mutex locks in the denoiser, we let only a single
   * thread acquire denoising tiles. */
  uint tile_types = task.tile_types;
  bool hold_denoise_lock = false;
  if ((tile_types & RenderTile::DENOISE) && task.denoising.type == DENOISER_OPENIMAGEDENOISE) {
    if (!oidn_task_lock.try_lock()) {
      tile_types &= ~RenderTile::DENOISE;
      hold_denoise_lock = true;
    }
  }

  RenderTile tile;
  while (task.acquire_tile(this, tile, tile_types)) {
    if (tile.task == RenderTile::PATH_TRACE) {
      render(task, tile, &kg);
    }
    else if (tile.task == RenderTile::BAKE) {
      render(task, tile, &kg);
    }
    else if (tile.task == RenderTile::DENOISE) {
      denoise_openimagedenoise(task, tile);
      task.update_progress(&tile, tile.w * tile.h);
    }

    task.release_tile(tile);

    if (TaskPool::canceled()) {
      if (task.need_finish_queue == false)
        break;
    }
  }

  if (hold_denoise_lock) {
    oidn_task_lock.unlock();
  }

  profiler.remove_state(&kg.profiler);

  delete denoising;
}

void CPUDevice::thread_denoise(DeviceTask &task)
{
  RenderTile tile;
  tile.x = task.x;
  tile.y = task.y;
  tile.w = task.w;
  tile.h = task.h;
  tile.buffer = task.buffer;
  tile.sample = task.sample + task.num_samples;
  tile.num_samples = task.num_samples;
  tile.start_sample = task.sample;
  tile.offset = task.offset;
  tile.stride = task.stride;
  tile.buffers = task.buffers;

  denoise_openimagedenoise(task, tile);

  task.update_progress(&tile, tile.w * tile.h);
}
#endif

const CPUKernels *CPUDevice::get_cpu_kernels() const
{
  return &kernels;
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
