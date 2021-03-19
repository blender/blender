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

#include "device/device.h"
#include "device/device_denoising.h"
#include "device/device_intern.h"
#include "device/device_split_kernel.h"

// clang-format off
#include "kernel/kernel.h"
#include "kernel/kernel_compat_cpu.h"
#include "kernel/kernel_types.h"
#include "kernel/split/kernel_split_data.h"
#include "kernel/kernel_globals.h"
#include "kernel/kernel_adaptive_sampling.h"

#include "kernel/filter/filter.h"

#include "kernel/osl/osl_shader.h"
#include "kernel/osl/osl_globals.h"
// clang-format on

#include "bvh/bvh_embree.h"

#include "render/buffers.h"
#include "render/coverage.h"

#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_function.h"
#include "util/util_logging.h"
#include "util/util_map.h"
#include "util/util_openimagedenoise.h"
#include "util/util_opengl.h"
#include "util/util_optimization.h"
#include "util/util_progress.h"
#include "util/util_system.h"
#include "util/util_task.h"
#include "util/util_thread.h"

CCL_NAMESPACE_BEGIN

class CPUDevice;

/* Has to be outside of the class to be shared across template instantiations. */
static const char *logged_architecture = "";

template<typename F> class KernelFunctions {
 public:
  KernelFunctions()
  {
    kernel = (F)NULL;
  }

  KernelFunctions(
      F kernel_default, F kernel_sse2, F kernel_sse3, F kernel_sse41, F kernel_avx, F kernel_avx2)
  {
    const char *architecture_name = "default";
    kernel = kernel_default;

    /* Silence potential warnings about unused variables
     * when compiling without some architectures. */
    (void)kernel_sse2;
    (void)kernel_sse3;
    (void)kernel_sse41;
    (void)kernel_avx;
    (void)kernel_avx2;
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX2
    if (DebugFlags().cpu.has_avx2() && system_cpu_support_avx2()) {
      architecture_name = "AVX2";
      kernel = kernel_avx2;
    }
    else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_AVX
        if (DebugFlags().cpu.has_avx() && system_cpu_support_avx()) {
      architecture_name = "AVX";
      kernel = kernel_avx;
    }
    else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE41
        if (DebugFlags().cpu.has_sse41() && system_cpu_support_sse41()) {
      architecture_name = "SSE4.1";
      kernel = kernel_sse41;
    }
    else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE3
        if (DebugFlags().cpu.has_sse3() && system_cpu_support_sse3()) {
      architecture_name = "SSE3";
      kernel = kernel_sse3;
    }
    else
#endif
#ifdef WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
        if (DebugFlags().cpu.has_sse2() && system_cpu_support_sse2()) {
      architecture_name = "SSE2";
      kernel = kernel_sse2;
    }
#else
    {
      /* Dummy to prevent the architecture if below become
       * conditional when WITH_CYCLES_OPTIMIZED_KERNEL_SSE2
       * is not defined. */
    }
#endif

    if (strcmp(architecture_name, logged_architecture) != 0) {
      VLOG(1) << "Will be using " << architecture_name << " kernels.";
      logged_architecture = architecture_name;
    }
  }

  inline F operator()() const
  {
    assert(kernel);
    return kernel;
  }

 protected:
  F kernel;
};

class CPUSplitKernel : public DeviceSplitKernel {
  CPUDevice *device;

 public:
  explicit CPUSplitKernel(CPUDevice *device);

  virtual bool enqueue_split_kernel_data_init(const KernelDimensions &dim,
                                              RenderTile &rtile,
                                              int num_global_elements,
                                              device_memory &kernel_globals,
                                              device_memory &kernel_data_,
                                              device_memory &split_data,
                                              device_memory &ray_state,
                                              device_memory &queue_index,
                                              device_memory &use_queues_flag,
                                              device_memory &work_pool_wgs);

  virtual SplitKernelFunction *get_split_kernel_function(const string &kernel_name,
                                                         const DeviceRequestedFeatures &);
  virtual int2 split_kernel_local_size();
  virtual int2 split_kernel_global_size(device_memory &kg, device_memory &data, DeviceTask &task);
  virtual uint64_t state_buffer_size(device_memory &kg, device_memory &data, size_t num_threads);
};

class CPUDevice : public Device {
 public:
  TaskPool task_pool;
  KernelGlobals kernel_globals;

  device_vector<TextureInfo> texture_info;
  bool need_texture_info;

#ifdef WITH_OSL
  OSLGlobals osl_globals;
#endif
#ifdef WITH_OPENIMAGEDENOISE
  oidn::DeviceRef oidn_device;
  oidn::FilterRef oidn_filter;
#endif
  thread_spin_lock oidn_task_lock;
#ifdef WITH_EMBREE
  RTCScene embree_scene = NULL;
  RTCDevice embree_device;
#endif

  bool use_split_kernel;

  DeviceRequestedFeatures requested_features;

  KernelFunctions<void (*)(KernelGlobals *, float *, int, int, int, int, int)> path_trace_kernel;
  KernelFunctions<void (*)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int)>
      convert_to_half_float_kernel;
  KernelFunctions<void (*)(KernelGlobals *, uchar4 *, float *, float, int, int, int, int)>
      convert_to_byte_kernel;
  KernelFunctions<void (*)(KernelGlobals *, uint4 *, float4 *, int, int, int, int, int)>
      shader_kernel;
  KernelFunctions<void (*)(KernelGlobals *, float *, int, int, int, int, int)> bake_kernel;

  KernelFunctions<void (*)(
      int, TileInfo *, int, int, float *, float *, float *, float *, float *, int *, int, int)>
      filter_divide_shadow_kernel;
  KernelFunctions<void (*)(
      int, TileInfo *, int, int, int, int, float *, float *, float, int *, int, int)>
      filter_get_feature_kernel;
  KernelFunctions<void (*)(int, int, int, int *, float *, float *, int, int *)>
      filter_write_feature_kernel;
  KernelFunctions<void (*)(int, int, float *, float *, float *, float *, int *, int)>
      filter_detect_outliers_kernel;
  KernelFunctions<void (*)(int, int, float *, float *, float *, float *, int *, int)>
      filter_combine_halves_kernel;

  KernelFunctions<void (*)(
      int, int, float *, float *, float *, float *, int *, int, int, int, float, float)>
      filter_nlm_calc_difference_kernel;
  KernelFunctions<void (*)(float *, float *, int *, int, int)> filter_nlm_blur_kernel;
  KernelFunctions<void (*)(float *, float *, int *, int, int)> filter_nlm_calc_weight_kernel;
  KernelFunctions<void (*)(
      int, int, float *, float *, float *, float *, float *, int *, int, int, int)>
      filter_nlm_update_output_kernel;
  KernelFunctions<void (*)(float *, float *, int *, int)> filter_nlm_normalize_kernel;

  KernelFunctions<void (*)(
      float *, TileInfo *, int, int, int, float *, int *, int *, int, int, bool, int, float)>
      filter_construct_transform_kernel;
  KernelFunctions<void (*)(int,
                           int,
                           int,
                           float *,
                           float *,
                           float *,
                           int *,
                           float *,
                           float3 *,
                           int *,
                           int *,
                           int,
                           int,
                           int,
                           int,
                           bool)>
      filter_nlm_construct_gramian_kernel;
  KernelFunctions<void (*)(int, int, int, float *, int *, float *, float3 *, int *, int)>
      filter_finalize_kernel;

  KernelFunctions<void (*)(KernelGlobals *,
                           ccl_constant KernelData *,
                           ccl_global void *,
                           int,
                           ccl_global char *,
                           int,
                           int,
                           int,
                           int,
                           int,
                           int,
                           int,
                           int,
                           ccl_global int *,
                           int,
                           ccl_global char *,
                           ccl_global unsigned int *,
                           unsigned int,
                           ccl_global float *)>
      data_init_kernel;
  unordered_map<string, KernelFunctions<void (*)(KernelGlobals *, KernelData *)>> split_kernels;

#define KERNEL_FUNCTIONS(name) \
  KERNEL_NAME_EVAL(cpu, name), KERNEL_NAME_EVAL(cpu_sse2, name), \
      KERNEL_NAME_EVAL(cpu_sse3, name), KERNEL_NAME_EVAL(cpu_sse41, name), \
      KERNEL_NAME_EVAL(cpu_avx, name), KERNEL_NAME_EVAL(cpu_avx2, name)

  CPUDevice(DeviceInfo &info_, Stats &stats_, Profiler &profiler_, bool background_)
      : Device(info_, stats_, profiler_, background_),
        texture_info(this, "__texture_info", MEM_GLOBAL),
#define REGISTER_KERNEL(name) name##_kernel(KERNEL_FUNCTIONS(name))
        REGISTER_KERNEL(path_trace),
        REGISTER_KERNEL(convert_to_half_float),
        REGISTER_KERNEL(convert_to_byte),
        REGISTER_KERNEL(shader),
        REGISTER_KERNEL(bake),
        REGISTER_KERNEL(filter_divide_shadow),
        REGISTER_KERNEL(filter_get_feature),
        REGISTER_KERNEL(filter_write_feature),
        REGISTER_KERNEL(filter_detect_outliers),
        REGISTER_KERNEL(filter_combine_halves),
        REGISTER_KERNEL(filter_nlm_calc_difference),
        REGISTER_KERNEL(filter_nlm_blur),
        REGISTER_KERNEL(filter_nlm_calc_weight),
        REGISTER_KERNEL(filter_nlm_update_output),
        REGISTER_KERNEL(filter_nlm_normalize),
        REGISTER_KERNEL(filter_construct_transform),
        REGISTER_KERNEL(filter_nlm_construct_gramian),
        REGISTER_KERNEL(filter_finalize),
        REGISTER_KERNEL(data_init)
#undef REGISTER_KERNEL
  {
    if (info.cpu_threads == 0) {
      info.cpu_threads = TaskScheduler::num_threads();
    }

#ifdef WITH_OSL
    kernel_globals.osl = &osl_globals;
#endif
#ifdef WITH_EMBREE
    embree_device = rtcNewDevice("verbose=0");
#endif
    use_split_kernel = DebugFlags().cpu.split_kernel;
    if (use_split_kernel) {
      VLOG(1) << "Will be using split kernel.";
    }
    need_texture_info = false;

#define REGISTER_SPLIT_KERNEL(name) \
  split_kernels[#name] = KernelFunctions<void (*)(KernelGlobals *, KernelData *)>( \
      KERNEL_FUNCTIONS(name))
    REGISTER_SPLIT_KERNEL(path_init);
    REGISTER_SPLIT_KERNEL(scene_intersect);
    REGISTER_SPLIT_KERNEL(lamp_emission);
    REGISTER_SPLIT_KERNEL(do_volume);
    REGISTER_SPLIT_KERNEL(queue_enqueue);
    REGISTER_SPLIT_KERNEL(indirect_background);
    REGISTER_SPLIT_KERNEL(shader_setup);
    REGISTER_SPLIT_KERNEL(shader_sort);
    REGISTER_SPLIT_KERNEL(shader_eval);
    REGISTER_SPLIT_KERNEL(holdout_emission_blurring_pathtermination_ao);
    REGISTER_SPLIT_KERNEL(subsurface_scatter);
    REGISTER_SPLIT_KERNEL(direct_lighting);
    REGISTER_SPLIT_KERNEL(shadow_blocked_ao);
    REGISTER_SPLIT_KERNEL(shadow_blocked_dl);
    REGISTER_SPLIT_KERNEL(enqueue_inactive);
    REGISTER_SPLIT_KERNEL(next_iteration_setup);
    REGISTER_SPLIT_KERNEL(indirect_subsurface);
    REGISTER_SPLIT_KERNEL(buffer_update);
    REGISTER_SPLIT_KERNEL(adaptive_stopping);
    REGISTER_SPLIT_KERNEL(adaptive_filter_x);
    REGISTER_SPLIT_KERNEL(adaptive_filter_y);
    REGISTER_SPLIT_KERNEL(adaptive_adjust_samples);
#undef REGISTER_SPLIT_KERNEL
#undef KERNEL_FUNCTIONS
  }

  ~CPUDevice()
  {
#ifdef WITH_EMBREE
    rtcReleaseDevice(embree_device);
#endif
    task_pool.cancel();
    texture_info.free();
  }

  virtual bool show_samples() const override
  {
    return (info.cpu_threads == 1);
  }

  virtual BVHLayoutMask get_bvh_layout_mask() const override
  {
    BVHLayoutMask bvh_layout_mask = BVH_LAYOUT_BVH2;
#ifdef WITH_EMBREE
    bvh_layout_mask |= BVH_LAYOUT_EMBREE;
#endif /* WITH_EMBREE */
    return bvh_layout_mask;
  }

  void load_texture_info()
  {
    if (need_texture_info) {
      texture_info.copy_to_device();
      need_texture_info = false;
    }
  }

  virtual void mem_alloc(device_memory &mem) override
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

  virtual void mem_copy_to(device_memory &mem) override
  {
    if (mem.type == MEM_GLOBAL) {
      global_free(mem);
      global_alloc(mem);
    }
    else if (mem.type == MEM_TEXTURE) {
      tex_free((device_texture &)mem);
      tex_alloc((device_texture &)mem);
    }
    else if (mem.type == MEM_PIXELS) {
      assert(!"mem_copy_to not supported for pixels.");
    }
    else {
      if (!mem.device_pointer) {
        mem_alloc(mem);
      }

      /* copy is no-op */
    }
  }

  virtual void mem_copy_from(
      device_memory & /*mem*/, int /*y*/, int /*w*/, int /*h*/, int /*elem*/) override
  {
    /* no-op */
  }

  virtual void mem_zero(device_memory &mem) override
  {
    if (!mem.device_pointer) {
      mem_alloc(mem);
    }

    if (mem.device_pointer) {
      memset((void *)mem.device_pointer, 0, mem.memory_size());
    }
  }

  virtual void mem_free(device_memory &mem) override
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

  virtual device_ptr mem_alloc_sub_ptr(device_memory &mem, int offset, int /*size*/) override
  {
    return (device_ptr)(((char *)mem.device_pointer) + mem.memory_elements_size(offset));
  }

  virtual void const_copy_to(const char *name, void *host, size_t size) override
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

  void global_alloc(device_memory &mem)
  {
    VLOG(1) << "Global memory allocate: " << mem.name << ", "
            << string_human_readable_number(mem.memory_size()) << " bytes. ("
            << string_human_readable_size(mem.memory_size()) << ")";

    kernel_global_memory_copy(&kernel_globals, mem.name, mem.host_pointer, mem.data_size);

    mem.device_pointer = (device_ptr)mem.host_pointer;
    mem.device_size = mem.memory_size();
    stats.mem_alloc(mem.device_size);
  }

  void global_free(device_memory &mem)
  {
    if (mem.device_pointer) {
      mem.device_pointer = 0;
      stats.mem_free(mem.device_size);
      mem.device_size = 0;
    }
  }

  void tex_alloc(device_texture &mem)
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

  void tex_free(device_texture &mem)
  {
    if (mem.device_pointer) {
      mem.device_pointer = 0;
      stats.mem_free(mem.device_size);
      mem.device_size = 0;
      need_texture_info = true;
    }
  }

  virtual void *osl_memory() override
  {
#ifdef WITH_OSL
    return &osl_globals;
#else
    return NULL;
#endif
  }

  void build_bvh(BVH *bvh, Progress &progress, bool refit) override
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

  void thread_run(DeviceTask &task)
  {
    if (task.type == DeviceTask::RENDER)
      thread_render(task);
    else if (task.type == DeviceTask::SHADER)
      thread_shader(task);
    else if (task.type == DeviceTask::FILM_CONVERT)
      thread_film_convert(task);
    else if (task.type == DeviceTask::DENOISE_BUFFER)
      thread_denoise(task);
  }

  bool denoising_non_local_means(device_ptr image_ptr,
                                 device_ptr guide_ptr,
                                 device_ptr variance_ptr,
                                 device_ptr out_ptr,
                                 DenoisingTask *task)
  {
    ProfilingHelper profiling(task->profiler, PROFILING_DENOISING_NON_LOCAL_MEANS);

    int4 rect = task->rect;
    int r = task->nlm_state.r;
    int f = task->nlm_state.f;
    float a = task->nlm_state.a;
    float k_2 = task->nlm_state.k_2;

    int w = align_up(rect.z - rect.x, 4);
    int h = rect.w - rect.y;
    int stride = task->buffer.stride;
    int channel_offset = task->nlm_state.is_color ? task->buffer.pass_stride : 0;

    float *temporary_mem = (float *)task->buffer.temporary_mem.device_pointer;
    float *blurDifference = temporary_mem;
    float *difference = temporary_mem + task->buffer.pass_stride;
    float *weightAccum = temporary_mem + 2 * task->buffer.pass_stride;

    memset(weightAccum, 0, sizeof(float) * w * h);
    memset((float *)out_ptr, 0, sizeof(float) * w * h);

    for (int i = 0; i < (2 * r + 1) * (2 * r + 1); i++) {
      int dy = i / (2 * r + 1) - r;
      int dx = i % (2 * r + 1) - r;

      int local_rect[4] = {
          max(0, -dx), max(0, -dy), rect.z - rect.x - max(0, dx), rect.w - rect.y - max(0, dy)};
      filter_nlm_calc_difference_kernel()(dx,
                                          dy,
                                          (float *)guide_ptr,
                                          (float *)variance_ptr,
                                          NULL,
                                          difference,
                                          local_rect,
                                          w,
                                          channel_offset,
                                          0,
                                          a,
                                          k_2);

      filter_nlm_blur_kernel()(difference, blurDifference, local_rect, w, f);
      filter_nlm_calc_weight_kernel()(blurDifference, difference, local_rect, w, f);
      filter_nlm_blur_kernel()(difference, blurDifference, local_rect, w, f);

      filter_nlm_update_output_kernel()(dx,
                                        dy,
                                        blurDifference,
                                        (float *)image_ptr,
                                        difference,
                                        (float *)out_ptr,
                                        weightAccum,
                                        local_rect,
                                        channel_offset,
                                        stride,
                                        f);
    }

    int local_rect[4] = {0, 0, rect.z - rect.x, rect.w - rect.y};
    filter_nlm_normalize_kernel()((float *)out_ptr, weightAccum, local_rect, w);

    return true;
  }

  bool denoising_construct_transform(DenoisingTask *task)
  {
    ProfilingHelper profiling(task->profiler, PROFILING_DENOISING_CONSTRUCT_TRANSFORM);

    for (int y = 0; y < task->filter_area.w; y++) {
      for (int x = 0; x < task->filter_area.z; x++) {
        filter_construct_transform_kernel()((float *)task->buffer.mem.device_pointer,
                                            task->tile_info,
                                            x + task->filter_area.x,
                                            y + task->filter_area.y,
                                            y * task->filter_area.z + x,
                                            (float *)task->storage.transform.device_pointer,
                                            (int *)task->storage.rank.device_pointer,
                                            &task->rect.x,
                                            task->buffer.pass_stride,
                                            task->buffer.frame_stride,
                                            task->buffer.use_time,
                                            task->radius,
                                            task->pca_threshold);
      }
    }
    return true;
  }

  bool denoising_accumulate(device_ptr color_ptr,
                            device_ptr color_variance_ptr,
                            device_ptr scale_ptr,
                            int frame,
                            DenoisingTask *task)
  {
    ProfilingHelper profiling(task->profiler, PROFILING_DENOISING_RECONSTRUCT);

    float *temporary_mem = (float *)task->buffer.temporary_mem.device_pointer;
    float *difference = temporary_mem;
    float *blurDifference = temporary_mem + task->buffer.pass_stride;

    int r = task->radius;
    int frame_offset = frame * task->buffer.frame_stride;
    for (int i = 0; i < (2 * r + 1) * (2 * r + 1); i++) {
      int dy = i / (2 * r + 1) - r;
      int dx = i % (2 * r + 1) - r;

      int local_rect[4] = {max(0, -dx),
                           max(0, -dy),
                           task->reconstruction_state.source_w - max(0, dx),
                           task->reconstruction_state.source_h - max(0, dy)};
      filter_nlm_calc_difference_kernel()(dx,
                                          dy,
                                          (float *)color_ptr,
                                          (float *)color_variance_ptr,
                                          (float *)scale_ptr,
                                          difference,
                                          local_rect,
                                          task->buffer.stride,
                                          task->buffer.pass_stride,
                                          frame_offset,
                                          1.0f,
                                          task->nlm_k_2);
      filter_nlm_blur_kernel()(difference, blurDifference, local_rect, task->buffer.stride, 4);
      filter_nlm_calc_weight_kernel()(
          blurDifference, difference, local_rect, task->buffer.stride, 4);
      filter_nlm_blur_kernel()(difference, blurDifference, local_rect, task->buffer.stride, 4);
      filter_nlm_construct_gramian_kernel()(dx,
                                            dy,
                                            task->tile_info->frames[frame],
                                            blurDifference,
                                            (float *)task->buffer.mem.device_pointer,
                                            (float *)task->storage.transform.device_pointer,
                                            (int *)task->storage.rank.device_pointer,
                                            (float *)task->storage.XtWX.device_pointer,
                                            (float3 *)task->storage.XtWY.device_pointer,
                                            local_rect,
                                            &task->reconstruction_state.filter_window.x,
                                            task->buffer.stride,
                                            4,
                                            task->buffer.pass_stride,
                                            frame_offset,
                                            task->buffer.use_time);
    }

    return true;
  }

  bool denoising_solve(device_ptr output_ptr, DenoisingTask *task)
  {
    for (int y = 0; y < task->filter_area.w; y++) {
      for (int x = 0; x < task->filter_area.z; x++) {
        filter_finalize_kernel()(x,
                                 y,
                                 y * task->filter_area.z + x,
                                 (float *)output_ptr,
                                 (int *)task->storage.rank.device_pointer,
                                 (float *)task->storage.XtWX.device_pointer,
                                 (float3 *)task->storage.XtWY.device_pointer,
                                 &task->reconstruction_state.buffer_params.x,
                                 task->render_buffer.samples);
      }
    }
    return true;
  }

  bool denoising_combine_halves(device_ptr a_ptr,
                                device_ptr b_ptr,
                                device_ptr mean_ptr,
                                device_ptr variance_ptr,
                                int r,
                                int4 rect,
                                DenoisingTask *task)
  {
    ProfilingHelper profiling(task->profiler, PROFILING_DENOISING_COMBINE_HALVES);

    for (int y = rect.y; y < rect.w; y++) {
      for (int x = rect.x; x < rect.z; x++) {
        filter_combine_halves_kernel()(x,
                                       y,
                                       (float *)mean_ptr,
                                       (float *)variance_ptr,
                                       (float *)a_ptr,
                                       (float *)b_ptr,
                                       &rect.x,
                                       r);
      }
    }
    return true;
  }

  bool denoising_divide_shadow(device_ptr a_ptr,
                               device_ptr b_ptr,
                               device_ptr sample_variance_ptr,
                               device_ptr sv_variance_ptr,
                               device_ptr buffer_variance_ptr,
                               DenoisingTask *task)
  {
    ProfilingHelper profiling(task->profiler, PROFILING_DENOISING_DIVIDE_SHADOW);

    for (int y = task->rect.y; y < task->rect.w; y++) {
      for (int x = task->rect.x; x < task->rect.z; x++) {
        filter_divide_shadow_kernel()(task->render_buffer.samples,
                                      task->tile_info,
                                      x,
                                      y,
                                      (float *)a_ptr,
                                      (float *)b_ptr,
                                      (float *)sample_variance_ptr,
                                      (float *)sv_variance_ptr,
                                      (float *)buffer_variance_ptr,
                                      &task->rect.x,
                                      task->render_buffer.pass_stride,
                                      task->render_buffer.offset);
      }
    }
    return true;
  }

  bool denoising_get_feature(int mean_offset,
                             int variance_offset,
                             device_ptr mean_ptr,
                             device_ptr variance_ptr,
                             float scale,
                             DenoisingTask *task)
  {
    ProfilingHelper profiling(task->profiler, PROFILING_DENOISING_GET_FEATURE);

    for (int y = task->rect.y; y < task->rect.w; y++) {
      for (int x = task->rect.x; x < task->rect.z; x++) {
        filter_get_feature_kernel()(task->render_buffer.samples,
                                    task->tile_info,
                                    mean_offset,
                                    variance_offset,
                                    x,
                                    y,
                                    (float *)mean_ptr,
                                    (float *)variance_ptr,
                                    scale,
                                    &task->rect.x,
                                    task->render_buffer.pass_stride,
                                    task->render_buffer.offset);
      }
    }
    return true;
  }

  bool denoising_write_feature(int out_offset,
                               device_ptr from_ptr,
                               device_ptr buffer_ptr,
                               DenoisingTask *task)
  {
    for (int y = 0; y < task->filter_area.w; y++) {
      for (int x = 0; x < task->filter_area.z; x++) {
        filter_write_feature_kernel()(task->render_buffer.samples,
                                      x + task->filter_area.x,
                                      y + task->filter_area.y,
                                      &task->reconstruction_state.buffer_params.x,
                                      (float *)from_ptr,
                                      (float *)buffer_ptr,
                                      out_offset,
                                      &task->rect.x);
      }
    }
    return true;
  }

  bool denoising_detect_outliers(device_ptr image_ptr,
                                 device_ptr variance_ptr,
                                 device_ptr depth_ptr,
                                 device_ptr output_ptr,
                                 DenoisingTask *task)
  {
    ProfilingHelper profiling(task->profiler, PROFILING_DENOISING_DETECT_OUTLIERS);

    for (int y = task->rect.y; y < task->rect.w; y++) {
      for (int x = task->rect.x; x < task->rect.z; x++) {
        filter_detect_outliers_kernel()(x,
                                        y,
                                        (float *)image_ptr,
                                        (float *)variance_ptr,
                                        (float *)depth_ptr,
                                        (float *)output_ptr,
                                        &task->rect.x,
                                        task->buffer.pass_stride);
      }
    }
    return true;
  }

  bool adaptive_sampling_filter(KernelGlobals *kg, RenderTile &tile, int sample)
  {
    WorkTile wtile;
    wtile.x = tile.x;
    wtile.y = tile.y;
    wtile.w = tile.w;
    wtile.h = tile.h;
    wtile.offset = tile.offset;
    wtile.stride = tile.stride;
    wtile.buffer = (float *)tile.buffer;

    /* For CPU we do adaptive stopping per sample so we can stop earlier, but
     * for combined CPU + GPU rendering we match the GPU and do it per tile
     * after a given number of sample steps. */
    if (!kernel_data.integrator.adaptive_stop_per_sample) {
      for (int y = wtile.y; y < wtile.y + wtile.h; ++y) {
        for (int x = wtile.x; x < wtile.x + wtile.w; ++x) {
          const int index = wtile.offset + x + y * wtile.stride;
          float *buffer = wtile.buffer + index * kernel_data.film.pass_stride;
          kernel_do_adaptive_stopping(kg, buffer, sample);
        }
      }
    }

    bool any = false;
    for (int y = wtile.y; y < wtile.y + wtile.h; ++y) {
      any |= kernel_do_adaptive_filter_x(kg, y, &wtile);
    }
    for (int x = wtile.x; x < wtile.x + wtile.w; ++x) {
      any |= kernel_do_adaptive_filter_y(kg, x, &wtile);
    }
    return (!any);
  }

  void adaptive_sampling_post(const RenderTile &tile, KernelGlobals *kg)
  {
    float *render_buffer = (float *)tile.buffer;
    for (int y = tile.y; y < tile.y + tile.h; y++) {
      for (int x = tile.x; x < tile.x + tile.w; x++) {
        int index = tile.offset + x + y * tile.stride;
        ccl_global float *buffer = render_buffer + index * kernel_data.film.pass_stride;
        if (buffer[kernel_data.film.pass_sample_count] < 0.0f) {
          buffer[kernel_data.film.pass_sample_count] = -buffer[kernel_data.film.pass_sample_count];
          float sample_multiplier = tile.sample / buffer[kernel_data.film.pass_sample_count];
          if (sample_multiplier != 1.0f) {
            kernel_adaptive_post_adjust(kg, buffer, sample_multiplier);
          }
        }
        else {
          kernel_adaptive_post_adjust(kg, buffer, tile.sample / (tile.sample - 1.0f));
        }
      }
    }
  }

  void render(DeviceTask &task, RenderTile &tile, KernelGlobals *kg)
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
            path_trace_kernel()(kg, render_buffer, sample, x, y, tile.offset, tile.stride);
          }
        }
      }
      else {
        for (int y = tile.y; y < tile.y + tile.h; y++) {
          for (int x = tile.x; x < tile.x + tile.w; x++) {
            bake_kernel()(kg, render_buffer, sample, x, y, tile.offset, tile.stride);
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

  void denoise_openimagedenoise_buffer(DeviceTask &task,
                                       float *buffer,
                                       const size_t offset,
                                       const size_t stride,
                                       const size_t x,
                                       const size_t y,
                                       const size_t w,
                                       const size_t h,
                                       const float scale)
  {
#ifdef WITH_OPENIMAGEDENOISE
    assert(openimagedenoise_supported());

    /* Only one at a time, since OpenImageDenoise itself is multithreaded for full
     * buffers, and for tiled rendering because creating multiple devices and filters
     * is slow and memory hungry as well.
     *
     * TODO: optimize tiled rendering case, by batching together denoising of many
     * tiles somehow? */
    static thread_mutex mutex;
    thread_scoped_lock lock(mutex);

    /* Create device and filter, cached for reuse. */
    if (!oidn_device) {
      oidn_device = oidn::newDevice();
      oidn_device.commit();
    }
    if (!oidn_filter) {
      oidn_filter = oidn_device.newFilter("RT");
      oidn_filter.set("hdr", true);
      oidn_filter.set("srgb", false);
    }

    /* Set images with appropriate stride for our interleaved pass storage. */
    struct {
      const char *name;
      const int offset;
      const bool scale;
      const bool use;
      array<float> scaled_buffer;
    } passes[] = {{"color", task.pass_denoising_data + DENOISING_PASS_COLOR, false, true},
                  {"albedo",
                   task.pass_denoising_data + DENOISING_PASS_ALBEDO,
                   true,
                   task.denoising.input_passes >= DENOISER_INPUT_RGB_ALBEDO},
                  {"normal",
                   task.pass_denoising_data + DENOISING_PASS_NORMAL,
                   true,
                   task.denoising.input_passes >= DENOISER_INPUT_RGB_ALBEDO_NORMAL},
                  {"output", 0, false, true},
                  { NULL,
                    0 }};

    for (int i = 0; passes[i].name; i++) {
      if (!passes[i].use) {
        continue;
      }

      const int64_t pixel_offset = offset + x + y * stride;
      const int64_t buffer_offset = (pixel_offset * task.pass_stride + passes[i].offset);
      const int64_t pixel_stride = task.pass_stride;
      const int64_t row_stride = stride * pixel_stride;

      if (passes[i].scale && scale != 1.0f) {
        /* Normalize albedo and normal passes as they are scaled by the number of samples.
         * For the color passes OIDN will perform auto-exposure making it unnecessary. */
        array<float> &scaled_buffer = passes[i].scaled_buffer;
        scaled_buffer.resize(w * h * 3);

        for (int y = 0; y < h; y++) {
          const float *pass_row = buffer + buffer_offset + y * row_stride;
          float *scaled_row = scaled_buffer.data() + y * w * 3;

          for (int x = 0; x < w; x++) {
            scaled_row[x * 3 + 0] = pass_row[x * pixel_stride + 0] * scale;
            scaled_row[x * 3 + 1] = pass_row[x * pixel_stride + 1] * scale;
            scaled_row[x * 3 + 2] = pass_row[x * pixel_stride + 2] * scale;
          }
        }

        oidn_filter.setImage(
            passes[i].name, scaled_buffer.data(), oidn::Format::Float3, w, h, 0, 0, 0);
      }
      else {
        oidn_filter.setImage(passes[i].name,
                             buffer + buffer_offset,
                             oidn::Format::Float3,
                             w,
                             h,
                             0,
                             pixel_stride * sizeof(float),
                             row_stride * sizeof(float));
      }
    }

    /* Execute filter. */
    oidn_filter.commit();
    oidn_filter.execute();
#else
    (void)task;
    (void)buffer;
    (void)offset;
    (void)stride;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)scale;
#endif
  }

  void denoise_openimagedenoise(DeviceTask &task, RenderTile &rtile)
  {
    if (task.type == DeviceTask::DENOISE_BUFFER) {
      /* Copy pixels from compute device to CPU (no-op for CPU device). */
      rtile.buffers->buffer.copy_from_device();

      denoise_openimagedenoise_buffer(task,
                                      (float *)rtile.buffer,
                                      rtile.offset,
                                      rtile.stride,
                                      rtile.x,
                                      rtile.y,
                                      rtile.w,
                                      rtile.h,
                                      1.0f / rtile.sample);

      /* todo: it may be possible to avoid this copy, but we have to ensure that
       * when other code copies data from the device it doesn't overwrite the
       * denoiser buffers. */
      rtile.buffers->buffer.copy_to_device();
    }
    else {
      /* Per-tile denoising. */
      rtile.sample = rtile.start_sample + rtile.num_samples;
      const float scale = 1.0f / rtile.sample;
      const float invscale = rtile.sample;
      const size_t pass_stride = task.pass_stride;

      /* Map neighboring tiles into one buffer for denoising. */
      RenderTileNeighbors neighbors(rtile);
      task.map_neighbor_tiles(neighbors, this);
      RenderTile &center_tile = neighbors.tiles[RenderTileNeighbors::CENTER];
      rtile = center_tile;

      /* Calculate size of the tile to denoise (including overlap). The overlap
       * size was chosen empirically. OpenImageDenoise specifies an overlap size
       * of 128 but this is significantly bigger than typical tile size. */
      const int4 rect = rect_clip(rect_expand(center_tile.bounds(), 64), neighbors.bounds());
      const int2 rect_size = make_int2(rect.z - rect.x, rect.w - rect.y);

      /* Adjacent tiles are in separate memory regions, copy into single buffer. */
      array<float> merged(rect_size.x * rect_size.y * task.pass_stride);

      for (int i = 0; i < RenderTileNeighbors::SIZE; i++) {
        RenderTile &ntile = neighbors.tiles[i];
        if (!ntile.buffer) {
          continue;
        }

        const int xmin = max(ntile.x, rect.x);
        const int ymin = max(ntile.y, rect.y);
        const int xmax = min(ntile.x + ntile.w, rect.z);
        const int ymax = min(ntile.y + ntile.h, rect.w);

        const size_t tile_offset = ntile.offset + xmin + ymin * ntile.stride;
        const float *tile_buffer = (float *)ntile.buffer + tile_offset * pass_stride;

        const size_t merged_stride = rect_size.x;
        const size_t merged_offset = (xmin - rect.x) + (ymin - rect.y) * merged_stride;
        float *merged_buffer = merged.data() + merged_offset * pass_stride;

        for (int y = ymin; y < ymax; y++) {
          for (int x = 0; x < pass_stride * (xmax - xmin); x++) {
            merged_buffer[x] = tile_buffer[x] * scale;
          }
          tile_buffer += ntile.stride * pass_stride;
          merged_buffer += merged_stride * pass_stride;
        }
      }

      /* Denoise */
      denoise_openimagedenoise_buffer(
          task, merged.data(), 0, rect_size.x, 0, 0, rect_size.x, rect_size.y, 1.0f);

      /* Copy back result from merged buffer. */
      RenderTile &ntile = neighbors.target;
      if (ntile.buffer) {
        const int xmin = max(ntile.x, rect.x);
        const int ymin = max(ntile.y, rect.y);
        const int xmax = min(ntile.x + ntile.w, rect.z);
        const int ymax = min(ntile.y + ntile.h, rect.w);

        const size_t tile_offset = ntile.offset + xmin + ymin * ntile.stride;
        float *tile_buffer = (float *)ntile.buffer + tile_offset * pass_stride;

        const size_t merged_stride = rect_size.x;
        const size_t merged_offset = (xmin - rect.x) + (ymin - rect.y) * merged_stride;
        const float *merged_buffer = merged.data() + merged_offset * pass_stride;

        for (int y = ymin; y < ymax; y++) {
          for (int x = 0; x < pass_stride * (xmax - xmin); x += pass_stride) {
            tile_buffer[x + 0] = merged_buffer[x + 0] * invscale;
            tile_buffer[x + 1] = merged_buffer[x + 1] * invscale;
            tile_buffer[x + 2] = merged_buffer[x + 2] * invscale;
          }
          tile_buffer += ntile.stride * pass_stride;
          merged_buffer += merged_stride * pass_stride;
        }
      }

      task.unmap_neighbor_tiles(neighbors, this);
    }
  }

  void denoise_nlm(DenoisingTask &denoising, RenderTile &tile)
  {
    ProfilingHelper profiling(denoising.profiler, PROFILING_DENOISING);

    tile.sample = tile.start_sample + tile.num_samples;

    denoising.functions.construct_transform = function_bind(
        &CPUDevice::denoising_construct_transform, this, &denoising);
    denoising.functions.accumulate = function_bind(
        &CPUDevice::denoising_accumulate, this, _1, _2, _3, _4, &denoising);
    denoising.functions.solve = function_bind(&CPUDevice::denoising_solve, this, _1, &denoising);
    denoising.functions.divide_shadow = function_bind(
        &CPUDevice::denoising_divide_shadow, this, _1, _2, _3, _4, _5, &denoising);
    denoising.functions.non_local_means = function_bind(
        &CPUDevice::denoising_non_local_means, this, _1, _2, _3, _4, &denoising);
    denoising.functions.combine_halves = function_bind(
        &CPUDevice::denoising_combine_halves, this, _1, _2, _3, _4, _5, _6, &denoising);
    denoising.functions.get_feature = function_bind(
        &CPUDevice::denoising_get_feature, this, _1, _2, _3, _4, _5, &denoising);
    denoising.functions.write_feature = function_bind(
        &CPUDevice::denoising_write_feature, this, _1, _2, _3, &denoising);
    denoising.functions.detect_outliers = function_bind(
        &CPUDevice::denoising_detect_outliers, this, _1, _2, _3, _4, &denoising);

    denoising.filter_area = make_int4(tile.x, tile.y, tile.w, tile.h);
    denoising.render_buffer.samples = tile.sample;
    denoising.buffer.gpu_temporary_mem = false;

    denoising.run_denoising(tile);
  }

  void thread_render(DeviceTask &task)
  {
    if (TaskPool::canceled()) {
      if (task.need_finish_queue == false)
        return;
    }

    /* allocate buffer for kernel globals */
    device_only_memory<KernelGlobals> kgbuffer(this, "kernel_globals");
    kgbuffer.alloc_to_device(1);

    KernelGlobals *kg = new ((void *)kgbuffer.device_pointer)
        KernelGlobals(thread_kernel_globals_init());

    profiler.add_state(&kg->profiler);

    CPUSplitKernel *split_kernel = NULL;
    if (use_split_kernel) {
      split_kernel = new CPUSplitKernel(this);
      if (!split_kernel->load_kernels(requested_features)) {
        thread_kernel_globals_free((KernelGlobals *)kgbuffer.device_pointer);
        kgbuffer.free();
        delete split_kernel;
        return;
      }
    }

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
        if (use_split_kernel) {
          device_only_memory<uchar> void_buffer(this, "void_buffer");
          split_kernel->path_trace(task, tile, kgbuffer, void_buffer);
        }
        else {
          render(task, tile, kg);
        }
      }
      else if (tile.task == RenderTile::BAKE) {
        render(task, tile, kg);
      }
      else if (tile.task == RenderTile::DENOISE) {
        if (task.denoising.type == DENOISER_OPENIMAGEDENOISE) {
          denoise_openimagedenoise(task, tile);
        }
        else if (task.denoising.type == DENOISER_NLM) {
          if (denoising == NULL) {
            denoising = new DenoisingTask(this, task);
            denoising->profiler = &kg->profiler;
          }
          denoise_nlm(*denoising, tile);
        }
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

    profiler.remove_state(&kg->profiler);

    thread_kernel_globals_free((KernelGlobals *)kgbuffer.device_pointer);
    kg->~KernelGlobals();
    kgbuffer.free();
    delete split_kernel;
    delete denoising;
  }

  void thread_denoise(DeviceTask &task)
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

    if (task.denoising.type == DENOISER_OPENIMAGEDENOISE) {
      denoise_openimagedenoise(task, tile);
    }
    else {
      DenoisingTask denoising(this, task);

      ProfilingState denoising_profiler_state;
      profiler.add_state(&denoising_profiler_state);
      denoising.profiler = &denoising_profiler_state;

      denoise_nlm(denoising, tile);

      profiler.remove_state(&denoising_profiler_state);
    }

    task.update_progress(&tile, tile.w * tile.h);
  }

  void thread_film_convert(DeviceTask &task)
  {
    float sample_scale = 1.0f / (task.sample + 1);

    if (task.rgba_half) {
      for (int y = task.y; y < task.y + task.h; y++)
        for (int x = task.x; x < task.x + task.w; x++)
          convert_to_half_float_kernel()(&kernel_globals,
                                         (uchar4 *)task.rgba_half,
                                         (float *)task.buffer,
                                         sample_scale,
                                         x,
                                         y,
                                         task.offset,
                                         task.stride);
    }
    else {
      for (int y = task.y; y < task.y + task.h; y++)
        for (int x = task.x; x < task.x + task.w; x++)
          convert_to_byte_kernel()(&kernel_globals,
                                   (uchar4 *)task.rgba_byte,
                                   (float *)task.buffer,
                                   sample_scale,
                                   x,
                                   y,
                                   task.offset,
                                   task.stride);
    }
  }

  void thread_shader(DeviceTask &task)
  {
    KernelGlobals *kg = new KernelGlobals(thread_kernel_globals_init());

    for (int sample = 0; sample < task.num_samples; sample++) {
      for (int x = task.shader_x; x < task.shader_x + task.shader_w; x++)
        shader_kernel()(kg,
                        (uint4 *)task.shader_input,
                        (float4 *)task.shader_output,
                        task.shader_eval_type,
                        task.shader_filter,
                        x,
                        task.offset,
                        sample);

      if (task.get_cancel() || TaskPool::canceled())
        break;

      task.update_progress(NULL);
    }

    thread_kernel_globals_free(kg);
    delete kg;
  }

  virtual int get_split_task_count(DeviceTask &task) override
  {
    if (task.type == DeviceTask::SHADER)
      return task.get_subtask_count(info.cpu_threads, 256);
    else
      return task.get_subtask_count(info.cpu_threads);
  }

  virtual void task_add(DeviceTask &task) override
  {
    /* Load texture info. */
    load_texture_info();

    /* split task into smaller ones */
    list<DeviceTask> tasks;

    if (task.type == DeviceTask::DENOISE_BUFFER &&
        task.denoising.type == DENOISER_OPENIMAGEDENOISE) {
      /* Denoise entire buffer at once with OIDN, it has own threading. */
      tasks.push_back(task);
    }
    else if (task.type == DeviceTask::SHADER) {
      task.split(tasks, info.cpu_threads, 256);
    }
    else {
      task.split(tasks, info.cpu_threads);
    }

    foreach (DeviceTask &task, tasks) {
      task_pool.push([=] {
        DeviceTask task_copy = task;
        thread_run(task_copy);
      });
    }
  }

  virtual void task_wait() override
  {
    task_pool.wait_work();
  }

  virtual void task_cancel() override
  {
    task_pool.cancel();
  }

 protected:
  inline KernelGlobals thread_kernel_globals_init()
  {
    KernelGlobals kg = kernel_globals;
    kg.transparent_shadow_intersections = NULL;
    const int decoupled_count = sizeof(kg.decoupled_volume_steps) /
                                sizeof(*kg.decoupled_volume_steps);
    for (int i = 0; i < decoupled_count; ++i) {
      kg.decoupled_volume_steps[i] = NULL;
    }
    kg.decoupled_volume_steps_index = 0;
    kg.coverage_asset = kg.coverage_object = kg.coverage_material = NULL;
#ifdef WITH_OSL
    OSLShader::thread_init(&kg, &kernel_globals, &osl_globals);
#endif
    return kg;
  }

  inline void thread_kernel_globals_free(KernelGlobals *kg)
  {
    if (kg == NULL) {
      return;
    }

    if (kg->transparent_shadow_intersections != NULL) {
      free(kg->transparent_shadow_intersections);
    }
    const int decoupled_count = sizeof(kg->decoupled_volume_steps) /
                                sizeof(*kg->decoupled_volume_steps);
    for (int i = 0; i < decoupled_count; ++i) {
      if (kg->decoupled_volume_steps[i] != NULL) {
        free(kg->decoupled_volume_steps[i]);
      }
    }
#ifdef WITH_OSL
    OSLShader::thread_free(kg);
#endif
  }

  virtual bool load_kernels(const DeviceRequestedFeatures &requested_features_) override
  {
    requested_features = requested_features_;

    return true;
  }
};

/* split kernel */

class CPUSplitKernelFunction : public SplitKernelFunction {
 public:
  CPUDevice *device;
  void (*func)(KernelGlobals *kg, KernelData *data);

  CPUSplitKernelFunction(CPUDevice *device) : device(device), func(NULL)
  {
  }
  ~CPUSplitKernelFunction()
  {
  }

  virtual bool enqueue(const KernelDimensions &dim,
                       device_memory &kernel_globals,
                       device_memory &data)
  {
    if (!func) {
      return false;
    }

    KernelGlobals *kg = (KernelGlobals *)kernel_globals.device_pointer;
    kg->global_size = make_int2(dim.global_size[0], dim.global_size[1]);

    for (int y = 0; y < dim.global_size[1]; y++) {
      for (int x = 0; x < dim.global_size[0]; x++) {
        kg->global_id = make_int2(x, y);

        func(kg, (KernelData *)data.device_pointer);
      }
    }

    return true;
  }
};

CPUSplitKernel::CPUSplitKernel(CPUDevice *device) : DeviceSplitKernel(device), device(device)
{
}

bool CPUSplitKernel::enqueue_split_kernel_data_init(const KernelDimensions &dim,
                                                    RenderTile &rtile,
                                                    int num_global_elements,
                                                    device_memory &kernel_globals,
                                                    device_memory &data,
                                                    device_memory &split_data,
                                                    device_memory &ray_state,
                                                    device_memory &queue_index,
                                                    device_memory &use_queues_flags,
                                                    device_memory &work_pool_wgs)
{
  KernelGlobals *kg = (KernelGlobals *)kernel_globals.device_pointer;
  kg->global_size = make_int2(dim.global_size[0], dim.global_size[1]);

  for (int y = 0; y < dim.global_size[1]; y++) {
    for (int x = 0; x < dim.global_size[0]; x++) {
      kg->global_id = make_int2(x, y);

      device->data_init_kernel()((KernelGlobals *)kernel_globals.device_pointer,
                                 (KernelData *)data.device_pointer,
                                 (void *)split_data.device_pointer,
                                 num_global_elements,
                                 (char *)ray_state.device_pointer,
                                 rtile.start_sample,
                                 rtile.start_sample + rtile.num_samples,
                                 rtile.x,
                                 rtile.y,
                                 rtile.w,
                                 rtile.h,
                                 rtile.offset,
                                 rtile.stride,
                                 (int *)queue_index.device_pointer,
                                 dim.global_size[0] * dim.global_size[1],
                                 (char *)use_queues_flags.device_pointer,
                                 (uint *)work_pool_wgs.device_pointer,
                                 rtile.num_samples,
                                 (float *)rtile.buffer);
    }
  }

  return true;
}

SplitKernelFunction *CPUSplitKernel::get_split_kernel_function(const string &kernel_name,
                                                               const DeviceRequestedFeatures &)
{
  CPUSplitKernelFunction *kernel = new CPUSplitKernelFunction(device);

  kernel->func = device->split_kernels[kernel_name]();
  if (!kernel->func) {
    delete kernel;
    return NULL;
  }

  return kernel;
}

int2 CPUSplitKernel::split_kernel_local_size()
{
  return make_int2(1, 1);
}

int2 CPUSplitKernel::split_kernel_global_size(device_memory & /*kg*/,
                                              device_memory & /*data*/,
                                              DeviceTask & /*task*/)
{
  return make_int2(1, 1);
}

uint64_t CPUSplitKernel::state_buffer_size(device_memory &kernel_globals,
                                           device_memory & /*data*/,
                                           size_t num_threads)
{
  KernelGlobals *kg = (KernelGlobals *)kernel_globals.device_pointer;

  return split_data_buffer_size(kg, num_threads);
}

Device *device_cpu_create(DeviceInfo &info, Stats &stats, Profiler &profiler, bool background)
{
  return new CPUDevice(info, stats, profiler, background);
}

void device_cpu_info(vector<DeviceInfo> &devices)
{
  DeviceInfo info;

  info.type = DEVICE_CPU;
  info.description = system_cpu_brand_string();
  info.id = "CPU";
  info.num = 0;
  info.has_volume_decoupled = true;
  info.has_adaptive_stop_per_sample = true;
  info.has_osl = true;
  info.has_half_images = true;
  info.has_profiling = true;
  info.denoisers = DENOISER_NLM;
  if (openimagedenoise_supported()) {
    info.denoisers |= DENOISER_OPENIMAGEDENOISE;
  }

  devices.insert(devices.begin(), info);
}

string device_cpu_capabilities()
{
  string capabilities = "";
  capabilities += system_cpu_support_sse2() ? "SSE2 " : "";
  capabilities += system_cpu_support_sse3() ? "SSE3 " : "";
  capabilities += system_cpu_support_sse41() ? "SSE41 " : "";
  capabilities += system_cpu_support_avx() ? "AVX " : "";
  capabilities += system_cpu_support_avx2() ? "AVX2" : "";
  if (capabilities[capabilities.size() - 1] == ' ')
    capabilities.resize(capabilities.size() - 1);
  return capabilities;
}

CCL_NAMESPACE_END
