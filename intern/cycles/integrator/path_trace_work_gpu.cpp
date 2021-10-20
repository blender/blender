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

#include "integrator/path_trace_work_gpu.h"
#include "integrator/path_trace_display.h"

#include "device/device.h"

#include "integrator/pass_accessor_gpu.h"
#include "render/buffers.h"
#include "render/scene.h"
#include "util/util_logging.h"
#include "util/util_string.h"
#include "util/util_tbb.h"
#include "util/util_time.h"

#include "kernel/kernel_types.h"

CCL_NAMESPACE_BEGIN

static size_t estimate_single_state_size()
{
  size_t state_size = 0;

#define KERNEL_STRUCT_BEGIN(name) for (int array_index = 0;; array_index++) {
#define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) state_size += sizeof(type);
#define KERNEL_STRUCT_ARRAY_MEMBER(parent_struct, type, name, feature) state_size += sizeof(type);
#define KERNEL_STRUCT_END(name) \
  break; \
  }
#define KERNEL_STRUCT_END_ARRAY(name, cpu_array_size, gpu_array_size) \
  if (array_index >= gpu_array_size - 1) { \
    break; \
  } \
  }
/* TODO(sergey): Look into better estimation for fields which depend on scene features. Maybe
 * maximum state calculation should happen as `alloc_work_memory()`, so that we can react to an
 * updated scene state here.
 * For until then use common value. Currently this size is only used for logging, but is weak to
 * rely on this. */
#define KERNEL_STRUCT_VOLUME_STACK_SIZE 4

#include "kernel/integrator/integrator_state_template.h"

#include "kernel/integrator/integrator_shadow_state_template.h"

#undef KERNEL_STRUCT_BEGIN
#undef KERNEL_STRUCT_MEMBER
#undef KERNEL_STRUCT_ARRAY_MEMBER
#undef KERNEL_STRUCT_END
#undef KERNEL_STRUCT_END_ARRAY
#undef KERNEL_STRUCT_VOLUME_STACK_SIZE

  return state_size;
}

PathTraceWorkGPU::PathTraceWorkGPU(Device *device,
                                   Film *film,
                                   DeviceScene *device_scene,
                                   bool *cancel_requested_flag)
    : PathTraceWork(device, film, device_scene, cancel_requested_flag),
      queue_(device->gpu_queue_create()),
      integrator_state_soa_kernel_features_(0),
      integrator_queue_counter_(device, "integrator_queue_counter", MEM_READ_WRITE),
      integrator_shader_sort_counter_(device, "integrator_shader_sort_counter", MEM_READ_WRITE),
      integrator_shader_raytrace_sort_counter_(
          device, "integrator_shader_raytrace_sort_counter", MEM_READ_WRITE),
      integrator_shader_sort_prefix_sum_(
          device, "integrator_shader_sort_prefix_sum", MEM_READ_WRITE),
      integrator_next_main_path_index_(device, "integrator_next_main_path_index", MEM_READ_WRITE),
      integrator_next_shadow_path_index_(
          device, "integrator_next_shadow_path_index", MEM_READ_WRITE),
      queued_paths_(device, "queued_paths", MEM_READ_WRITE),
      num_queued_paths_(device, "num_queued_paths", MEM_READ_WRITE),
      work_tiles_(device, "work_tiles", MEM_READ_WRITE),
      display_rgba_half_(device, "display buffer half", MEM_READ_WRITE),
      max_num_paths_(queue_->num_concurrent_states(estimate_single_state_size())),
      min_num_active_main_paths_(queue_->num_concurrent_busy_states()),
      max_active_main_path_index_(0)
{
  memset(&integrator_state_gpu_, 0, sizeof(integrator_state_gpu_));

  /* Limit number of active paths to the half of the overall state. This is due to the logic in the
   * path compaction which relies on the fact that regeneration does not happen sooner than half of
   * the states are available again. */
  min_num_active_main_paths_ = min(min_num_active_main_paths_, max_num_paths_ / 2);
}

void PathTraceWorkGPU::alloc_integrator_soa()
{
  /* IntegrateState allocated as structure of arrays. */

  /* Check if we already allocated memory for the required features. */
  const int requested_volume_stack_size = device_scene_->data.volume_stack_size;
  const uint kernel_features = device_scene_->data.kernel_features;
  if ((integrator_state_soa_kernel_features_ & kernel_features) == kernel_features &&
      integrator_state_soa_volume_stack_size_ >= requested_volume_stack_size) {
    return;
  }
  integrator_state_soa_kernel_features_ = kernel_features;
  integrator_state_soa_volume_stack_size_ = max(integrator_state_soa_volume_stack_size_,
                                                requested_volume_stack_size);

  /* Allocate a device only memory buffer before for each struct member, and then
   * write the pointers into a struct that resides in constant memory.
   *
   * TODO: store float3 in separate XYZ arrays. */
#define KERNEL_STRUCT_BEGIN(name) for (int array_index = 0;; array_index++) {
#define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) \
  if ((kernel_features & feature) && (integrator_state_gpu_.parent_struct.name == nullptr)) { \
    device_only_memory<type> *array = new device_only_memory<type>(device_, \
                                                                   "integrator_state_" #name); \
    array->alloc_to_device(max_num_paths_); \
    integrator_state_soa_.emplace_back(array); \
    integrator_state_gpu_.parent_struct.name = (type *)array->device_pointer; \
  }
#define KERNEL_STRUCT_ARRAY_MEMBER(parent_struct, type, name, feature) \
  if ((kernel_features & feature) && \
      (integrator_state_gpu_.parent_struct[array_index].name == nullptr)) { \
    device_only_memory<type> *array = new device_only_memory<type>(device_, \
                                                                   "integrator_state_" #name); \
    array->alloc_to_device(max_num_paths_); \
    integrator_state_soa_.emplace_back(array); \
    integrator_state_gpu_.parent_struct[array_index].name = (type *)array->device_pointer; \
  }
#define KERNEL_STRUCT_END(name) \
  break; \
  }
#define KERNEL_STRUCT_END_ARRAY(name, cpu_array_size, gpu_array_size) \
  if (array_index >= gpu_array_size - 1) { \
    break; \
  } \
  }
#define KERNEL_STRUCT_VOLUME_STACK_SIZE (integrator_state_soa_volume_stack_size_)

#include "kernel/integrator/integrator_state_template.h"

#include "kernel/integrator/integrator_shadow_state_template.h"

#undef KERNEL_STRUCT_BEGIN
#undef KERNEL_STRUCT_MEMBER
#undef KERNEL_STRUCT_ARRAY_MEMBER
#undef KERNEL_STRUCT_END
#undef KERNEL_STRUCT_END_ARRAY
#undef KERNEL_STRUCT_VOLUME_STACK_SIZE

  if (VLOG_IS_ON(3)) {
    size_t total_soa_size = 0;
    for (auto &&soa_memory : integrator_state_soa_) {
      total_soa_size += soa_memory->memory_size();
    }

    VLOG(3) << "GPU SoA state size: " << string_human_readable_size(total_soa_size);
  }
}

void PathTraceWorkGPU::alloc_integrator_queue()
{
  if (integrator_queue_counter_.size() == 0) {
    integrator_queue_counter_.alloc(1);
    integrator_queue_counter_.zero_to_device();
    integrator_queue_counter_.copy_from_device();
    integrator_state_gpu_.queue_counter = (IntegratorQueueCounter *)
                                              integrator_queue_counter_.device_pointer;
  }

  /* Allocate data for active path index arrays. */
  if (num_queued_paths_.size() == 0) {
    num_queued_paths_.alloc(1);
    num_queued_paths_.zero_to_device();
  }

  if (queued_paths_.size() == 0) {
    queued_paths_.alloc(max_num_paths_);
    /* TODO: this could be skip if we had a function to just allocate on device. */
    queued_paths_.zero_to_device();
  }
}

void PathTraceWorkGPU::alloc_integrator_sorting()
{
  /* Allocate arrays for shader sorting. */
  const int max_shaders = device_scene_->data.max_shaders;
  if (integrator_shader_sort_counter_.size() < max_shaders) {
    integrator_shader_sort_counter_.alloc(max_shaders);
    integrator_shader_sort_counter_.zero_to_device();

    integrator_shader_raytrace_sort_counter_.alloc(max_shaders);
    integrator_shader_raytrace_sort_counter_.zero_to_device();

    integrator_shader_sort_prefix_sum_.alloc(max_shaders);
    integrator_shader_sort_prefix_sum_.zero_to_device();

    integrator_state_gpu_.sort_key_counter[DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE] =
        (int *)integrator_shader_sort_counter_.device_pointer;
    integrator_state_gpu_.sort_key_counter[DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE] =
        (int *)integrator_shader_raytrace_sort_counter_.device_pointer;
  }
}

void PathTraceWorkGPU::alloc_integrator_path_split()
{
  if (integrator_next_shadow_path_index_.size() == 0) {
    integrator_next_shadow_path_index_.alloc(1);
    integrator_next_shadow_path_index_.zero_to_device();

    integrator_state_gpu_.next_shadow_path_index =
        (int *)integrator_next_shadow_path_index_.device_pointer;
  }

  if (integrator_next_main_path_index_.size() == 0) {
    integrator_next_main_path_index_.alloc(1);
    integrator_next_shadow_path_index_.data()[0] = 0;
    integrator_next_main_path_index_.zero_to_device();

    integrator_state_gpu_.next_main_path_index =
        (int *)integrator_next_main_path_index_.device_pointer;
  }
}

void PathTraceWorkGPU::alloc_work_memory()
{
  alloc_integrator_soa();
  alloc_integrator_queue();
  alloc_integrator_sorting();
  alloc_integrator_path_split();
}

void PathTraceWorkGPU::init_execution()
{
  queue_->init_execution();

  /* Copy to device side struct in constant memory. */
  device_->const_copy_to(
      "__integrator_state", &integrator_state_gpu_, sizeof(integrator_state_gpu_));
}

void PathTraceWorkGPU::render_samples(RenderStatistics &statistics,
                                      int start_sample,
                                      int samples_num)
{
  /* Limit number of states for the tile and rely on a greedy scheduling of tiles. This allows to
   * add more work (because tiles are smaller, so there is higher chance that more paths will
   * become busy after adding new tiles). This is especially important for the shadow catcher which
   * schedules work in halves of available number of paths. */
  work_tile_scheduler_.set_max_num_path_states(max_num_paths_ / 8);

  work_tile_scheduler_.reset(effective_buffer_params_, start_sample, samples_num);

  enqueue_reset();

  int num_iterations = 0;
  uint64_t num_busy_accum = 0;

  /* TODO: set a hard limit in case of undetected kernel failures? */
  while (true) {
    /* Enqueue work from the scheduler, on start or when there are not enough
     * paths to keep the device occupied. */
    bool finished;
    if (enqueue_work_tiles(finished)) {
      /* Copy stats from the device. */
      queue_->copy_from_device(integrator_queue_counter_);

      if (!queue_->synchronize()) {
        break; /* Stop on error. */
      }
    }

    if (is_cancel_requested()) {
      break;
    }

    /* Stop if no more work remaining. */
    if (finished) {
      break;
    }

    /* Enqueue on of the path iteration kernels. */
    if (enqueue_path_iteration()) {
      /* Copy stats from the device. */
      queue_->copy_from_device(integrator_queue_counter_);

      if (!queue_->synchronize()) {
        break; /* Stop on error. */
      }
    }

    if (is_cancel_requested()) {
      break;
    }

    num_busy_accum += num_active_main_paths_paths();
    ++num_iterations;
  }

  statistics.occupancy = static_cast<float>(num_busy_accum) / num_iterations / max_num_paths_;
}

DeviceKernel PathTraceWorkGPU::get_most_queued_kernel() const
{
  const IntegratorQueueCounter *queue_counter = integrator_queue_counter_.data();

  int max_num_queued = 0;
  DeviceKernel kernel = DEVICE_KERNEL_NUM;

  for (int i = 0; i < DEVICE_KERNEL_INTEGRATOR_NUM; i++) {
    if (queue_counter->num_queued[i] > max_num_queued) {
      kernel = (DeviceKernel)i;
      max_num_queued = queue_counter->num_queued[i];
    }
  }

  return kernel;
}

void PathTraceWorkGPU::enqueue_reset()
{
  void *args[] = {&max_num_paths_};
  queue_->enqueue(DEVICE_KERNEL_INTEGRATOR_RESET, max_num_paths_, args);
  queue_->zero_to_device(integrator_queue_counter_);
  queue_->zero_to_device(integrator_shader_sort_counter_);
  queue_->zero_to_device(integrator_shader_raytrace_sort_counter_);

  /* Tiles enqueue need to know number of active paths, which is based on this counter. Zero the
   * counter on the host side because `zero_to_device()` is not doing it. */
  if (integrator_queue_counter_.host_pointer) {
    memset(integrator_queue_counter_.data(), 0, integrator_queue_counter_.memory_size());
  }
}

bool PathTraceWorkGPU::enqueue_path_iteration()
{
  /* Find kernel to execute, with max number of queued paths. */
  const IntegratorQueueCounter *queue_counter = integrator_queue_counter_.data();

  int num_active_paths = 0;
  for (int i = 0; i < DEVICE_KERNEL_INTEGRATOR_NUM; i++) {
    num_active_paths += queue_counter->num_queued[i];
  }

  if (num_active_paths == 0) {
    return false;
  }

  /* Find kernel to execute, with max number of queued paths. */
  const DeviceKernel kernel = get_most_queued_kernel();
  if (kernel == DEVICE_KERNEL_NUM) {
    return false;
  }

  /* If the number of shadow kernels dropped to zero, set the next shadow path
   * index to zero as well.
   *
   * TODO: use shadow path compaction to lower it more often instead of letting
   * it fill up entirely? */
  const int num_queued_shadow =
      queue_counter->num_queued[DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW] +
      queue_counter->num_queued[DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW];
  if (num_queued_shadow == 0) {
    if (integrator_next_shadow_path_index_.data()[0] != 0) {
      integrator_next_shadow_path_index_.data()[0] = 0;
      queue_->copy_to_device(integrator_next_shadow_path_index_);
    }
  }

  /* For kernels that add shadow paths, check if there is enough space available.
   * If not, schedule shadow kernels first to clear out the shadow paths. */
  int num_paths_limit = INT_MAX;

  if (kernel_creates_shadow_paths(kernel)) {
    const int available_shadow_paths = max_num_paths_ -
                                       integrator_next_shadow_path_index_.data()[0];
    if (available_shadow_paths < queue_counter->num_queued[kernel]) {
      if (queue_counter->num_queued[DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW]) {
        enqueue_path_iteration(DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW);
        return true;
      }
      else if (queue_counter->num_queued[DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW]) {
        enqueue_path_iteration(DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW);
        return true;
      }
    }
    else if (kernel_creates_ao_paths(kernel)) {
      /* AO kernel creates two shadow paths, so limit number of states to schedule. */
      num_paths_limit = available_shadow_paths / 2;
    }
  }

  /* Schedule kernel with maximum number of queued items. */
  enqueue_path_iteration(kernel, num_paths_limit);

  /* Update next shadow path index for kernels that can add shadow paths. */
  if (kernel_creates_shadow_paths(kernel)) {
    queue_->copy_from_device(integrator_next_shadow_path_index_);
  }

  return true;
}

void PathTraceWorkGPU::enqueue_path_iteration(DeviceKernel kernel, const int num_paths_limit)
{
  void *d_path_index = (void *)NULL;

  /* Create array of path indices for which this kernel is queued to be executed. */
  int work_size = kernel_max_active_main_path_index(kernel);

  IntegratorQueueCounter *queue_counter = integrator_queue_counter_.data();
  int num_queued = queue_counter->num_queued[kernel];

  if (kernel_uses_sorting(kernel)) {
    /* Compute array of active paths, sorted by shader. */
    work_size = num_queued;
    d_path_index = (void *)queued_paths_.device_pointer;

    compute_sorted_queued_paths(
        DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY, kernel, num_paths_limit);
  }
  else if (num_queued < work_size) {
    work_size = num_queued;
    d_path_index = (void *)queued_paths_.device_pointer;

    if (kernel_is_shadow_path(kernel)) {
      /* Compute array of active shadow paths for specific kernel. */
      compute_queued_paths(DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY, kernel);
    }
    else {
      /* Compute array of active paths for specific kernel. */
      compute_queued_paths(DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY, kernel);
    }
  }

  work_size = min(work_size, num_paths_limit);

  DCHECK_LE(work_size, max_num_paths_);

  switch (kernel) {
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST:
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW:
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE:
    case DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK: {
      /* Ray intersection kernels with integrator state. */
      void *args[] = {&d_path_index, const_cast<int *>(&work_size)};

      queue_->enqueue(kernel, work_size, args);
      break;
    }
    case DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_LIGHT:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE:
    case DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME: {
      /* Shading kernels with integrator state and render buffer. */
      void *d_render_buffer = (void *)buffers_->buffer.device_pointer;
      void *args[] = {&d_path_index, &d_render_buffer, const_cast<int *>(&work_size)};

      queue_->enqueue(kernel, work_size, args);
      break;
    }

    default:
      LOG(FATAL) << "Unhandled kernel " << device_kernel_as_string(kernel)
                 << " used for path iteration, should never happen.";
      break;
  }
}

void PathTraceWorkGPU::compute_sorted_queued_paths(DeviceKernel kernel,
                                                   DeviceKernel queued_kernel,
                                                   const int num_paths_limit)
{
  int d_queued_kernel = queued_kernel;
  void *d_counter = integrator_state_gpu_.sort_key_counter[d_queued_kernel];
  void *d_prefix_sum = (void *)integrator_shader_sort_prefix_sum_.device_pointer;
  assert(d_counter != nullptr && d_prefix_sum != nullptr);

  /* Compute prefix sum of number of active paths with each shader. */
  {
    const int work_size = 1;
    int max_shaders = device_scene_->data.max_shaders;
    void *args[] = {&d_counter, &d_prefix_sum, &max_shaders};
    queue_->enqueue(DEVICE_KERNEL_PREFIX_SUM, work_size, args);
  }

  queue_->zero_to_device(num_queued_paths_);

  /* Launch kernel to fill the active paths arrays. */
  {
    /* TODO: this could be smaller for terminated paths based on amount of work we want
     * to schedule, and also based on num_paths_limit.
     *
     * Also, when the number paths is limited it may be better to prefer paths from the
     * end of the array since compaction would need to do less work. */
    const int work_size = kernel_max_active_main_path_index(queued_kernel);

    void *d_queued_paths = (void *)queued_paths_.device_pointer;
    void *d_num_queued_paths = (void *)num_queued_paths_.device_pointer;
    void *args[] = {const_cast<int *>(&work_size),
                    const_cast<int *>(&num_paths_limit),
                    &d_queued_paths,
                    &d_num_queued_paths,
                    &d_counter,
                    &d_prefix_sum,
                    &d_queued_kernel};

    queue_->enqueue(kernel, work_size, args);
  }
}

void PathTraceWorkGPU::compute_queued_paths(DeviceKernel kernel, DeviceKernel queued_kernel)
{
  int d_queued_kernel = queued_kernel;

  /* Launch kernel to fill the active paths arrays. */
  const int work_size = kernel_max_active_main_path_index(queued_kernel);
  void *d_queued_paths = (void *)queued_paths_.device_pointer;
  void *d_num_queued_paths = (void *)num_queued_paths_.device_pointer;
  void *args[] = {
      const_cast<int *>(&work_size), &d_queued_paths, &d_num_queued_paths, &d_queued_kernel};

  queue_->zero_to_device(num_queued_paths_);
  queue_->enqueue(kernel, work_size, args);
}

void PathTraceWorkGPU::compact_states(const int num_active_paths)
{
  if (num_active_paths == 0) {
    max_active_main_path_index_ = 0;
  }

  /* Compact fragmented path states into the start of the array, moving any paths
   * with index higher than the number of active paths into the gaps. */
  if (max_active_main_path_index_ == num_active_paths) {
    return;
  }

  void *d_compact_paths = (void *)queued_paths_.device_pointer;
  void *d_num_queued_paths = (void *)num_queued_paths_.device_pointer;

  /* Create array with terminated paths that we can write to. */
  {
    /* TODO: can the work size be reduced here? */
    int offset = num_active_paths;
    int work_size = num_active_paths;
    void *args[] = {&work_size, &d_compact_paths, &d_num_queued_paths, &offset};
    queue_->zero_to_device(num_queued_paths_);
    queue_->enqueue(DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY, work_size, args);
  }

  /* Create array of paths that we need to compact, where the path index is bigger
   * than the number of active paths. */
  {
    int work_size = max_active_main_path_index_;
    void *args[] = {
        &work_size, &d_compact_paths, &d_num_queued_paths, const_cast<int *>(&num_active_paths)};
    queue_->zero_to_device(num_queued_paths_);
    queue_->enqueue(DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY, work_size, args);
  }

  queue_->copy_from_device(num_queued_paths_);
  queue_->synchronize();

  int num_compact_paths = num_queued_paths_.data()[0];

  /* Move paths into gaps. */
  if (num_compact_paths > 0) {
    int work_size = num_compact_paths;
    int active_states_offset = 0;
    int terminated_states_offset = num_active_paths;
    void *args[] = {
        &d_compact_paths, &active_states_offset, &terminated_states_offset, &work_size};
    queue_->enqueue(DEVICE_KERNEL_INTEGRATOR_COMPACT_STATES, work_size, args);
  }

  queue_->synchronize();

  /* Adjust max active path index now we know which part of the array is actually used. */
  max_active_main_path_index_ = num_active_paths;
}

bool PathTraceWorkGPU::enqueue_work_tiles(bool &finished)
{
  /* If there are existing paths wait them to go to intersect closest kernel, which will align the
   * wavefront of the existing and newly added paths. */
  /* TODO: Check whether counting new intersection kernels here will have positive affect on the
   * performance. */
  const DeviceKernel kernel = get_most_queued_kernel();
  if (kernel != DEVICE_KERNEL_NUM && kernel != DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST) {
    return false;
  }

  int num_active_paths = num_active_main_paths_paths();

  /* Don't schedule more work if canceling. */
  if (is_cancel_requested()) {
    if (num_active_paths == 0) {
      finished = true;
    }
    return false;
  }

  finished = false;

  vector<KernelWorkTile> work_tiles;

  int max_num_camera_paths = max_num_paths_;
  int num_predicted_splits = 0;

  if (has_shadow_catcher()) {
    /* When there are shadow catchers in the scene bounce from them will split the state. So we
     * make sure there is enough space in the path states array to fit split states.
     *
     * Basically, when adding N new paths we ensure that there is 2*N available path states, so
     * that all the new paths can be split.
     *
     * Note that it is possible that some of the current states can still split, so need to make
     * sure there is enough space for them as well. */

    /* Number of currently in-flight states which can still split. */
    const int num_scheduled_possible_split = shadow_catcher_count_possible_splits();

    const int num_available_paths = max_num_paths_ - num_active_paths;
    const int num_new_paths = num_available_paths / 2;
    max_num_camera_paths = max(num_active_paths,
                               num_active_paths + num_new_paths - num_scheduled_possible_split);
    num_predicted_splits += num_scheduled_possible_split + num_new_paths;
  }

  /* Schedule when we're out of paths or there are too few paths to keep the
   * device occupied. */
  int num_paths = num_active_paths;
  if (num_paths == 0 || num_paths < min_num_active_main_paths_) {
    /* Get work tiles until the maximum number of path is reached. */
    while (num_paths < max_num_camera_paths) {
      KernelWorkTile work_tile;
      if (work_tile_scheduler_.get_work(&work_tile, max_num_camera_paths - num_paths)) {
        work_tiles.push_back(work_tile);
        num_paths += work_tile.w * work_tile.h * work_tile.num_samples;
      }
      else {
        break;
      }
    }

    /* If we couldn't get any more tiles, we're done. */
    if (work_tiles.size() == 0 && num_paths == 0) {
      finished = true;
      return false;
    }
  }

  /* Initialize paths from work tiles. */
  if (work_tiles.size() == 0) {
    return false;
  }

  /* Compact state array when number of paths becomes small relative to the
   * known maximum path index, which makes computing active index arrays slow. */
  compact_states(num_active_paths);

  if (has_shadow_catcher()) {
    integrator_next_main_path_index_.data()[0] = num_paths;
    queue_->copy_to_device(integrator_next_main_path_index_);
  }

  enqueue_work_tiles((device_scene_->data.bake.use) ? DEVICE_KERNEL_INTEGRATOR_INIT_FROM_BAKE :
                                                      DEVICE_KERNEL_INTEGRATOR_INIT_FROM_CAMERA,
                     work_tiles.data(),
                     work_tiles.size(),
                     num_active_paths,
                     num_predicted_splits);

  return true;
}

void PathTraceWorkGPU::enqueue_work_tiles(DeviceKernel kernel,
                                          const KernelWorkTile work_tiles[],
                                          const int num_work_tiles,
                                          const int num_active_paths,
                                          const int num_predicted_splits)
{
  /* Copy work tiles to device. */
  if (work_tiles_.size() < num_work_tiles) {
    work_tiles_.alloc(num_work_tiles);
  }

  int path_index_offset = num_active_paths;
  int max_tile_work_size = 0;
  for (int i = 0; i < num_work_tiles; i++) {
    KernelWorkTile &work_tile = work_tiles_.data()[i];
    work_tile = work_tiles[i];

    const int tile_work_size = work_tile.w * work_tile.h * work_tile.num_samples;

    work_tile.path_index_offset = path_index_offset;
    work_tile.work_size = tile_work_size;

    path_index_offset += tile_work_size;

    max_tile_work_size = max(max_tile_work_size, tile_work_size);
  }

  queue_->copy_to_device(work_tiles_);

  void *d_work_tiles = (void *)work_tiles_.device_pointer;
  void *d_render_buffer = (void *)buffers_->buffer.device_pointer;

  /* Launch kernel. */
  void *args[] = {&d_work_tiles,
                  const_cast<int *>(&num_work_tiles),
                  &d_render_buffer,
                  const_cast<int *>(&max_tile_work_size)};

  queue_->enqueue(kernel, max_tile_work_size * num_work_tiles, args);

  max_active_main_path_index_ = path_index_offset + num_predicted_splits;
}

int PathTraceWorkGPU::num_active_main_paths_paths()
{
  IntegratorQueueCounter *queue_counter = integrator_queue_counter_.data();

  int num_paths = 0;
  for (int i = 0; i < DEVICE_KERNEL_INTEGRATOR_NUM; i++) {
    DCHECK_GE(queue_counter->num_queued[i], 0)
        << "Invalid number of queued states for kernel "
        << device_kernel_as_string(static_cast<DeviceKernel>(i));

    if (!kernel_is_shadow_path((DeviceKernel)i)) {
      num_paths += queue_counter->num_queued[i];
    }
  }

  return num_paths;
}

bool PathTraceWorkGPU::should_use_graphics_interop()
{
  /* There are few aspects with the graphics interop when using multiple devices caused by the fact
   * that the PathTraceDisplay has a single texture:
   *
   *   CUDA will return `CUDA_ERROR_NOT_SUPPORTED` from `cuGraphicsGLRegisterBuffer()` when
   *   attempting to register OpenGL PBO which has been mapped. Which makes sense, because
   *   otherwise one would run into a conflict of where the source of truth is. */
  if (has_multiple_works()) {
    return false;
  }

  if (!interop_use_checked_) {
    Device *device = queue_->device;
    interop_use_ = device->should_use_graphics_interop();

    if (interop_use_) {
      VLOG(2) << "Will be using graphics interop GPU display update.";
    }
    else {
      VLOG(2) << "Will be using naive GPU display update.";
    }

    interop_use_checked_ = true;
  }

  return interop_use_;
}

void PathTraceWorkGPU::copy_to_display(PathTraceDisplay *display,
                                       PassMode pass_mode,
                                       int num_samples)
{
  if (device_->have_error()) {
    /* Don't attempt to update GPU display if the device has errors: the error state will make
     * wrong decisions to happen about interop, causing more chained bugs. */
    return;
  }

  if (!buffers_->buffer.device_pointer) {
    LOG(WARNING) << "Request for GPU display update without allocated render buffers.";
    return;
  }

  if (should_use_graphics_interop()) {
    if (copy_to_display_interop(display, pass_mode, num_samples)) {
      return;
    }

    /* If error happens when trying to use graphics interop fallback to the native implementation
     * and don't attempt to use interop for the further updates. */
    interop_use_ = false;
  }

  copy_to_display_naive(display, pass_mode, num_samples);
}

void PathTraceWorkGPU::copy_to_display_naive(PathTraceDisplay *display,
                                             PassMode pass_mode,
                                             int num_samples)
{
  const int full_x = effective_buffer_params_.full_x;
  const int full_y = effective_buffer_params_.full_y;
  const int width = effective_buffer_params_.window_width;
  const int height = effective_buffer_params_.window_height;
  const int final_width = buffers_->params.window_width;
  const int final_height = buffers_->params.window_height;

  const int texture_x = full_x - effective_full_params_.full_x + effective_buffer_params_.window_x;
  const int texture_y = full_y - effective_full_params_.full_y + effective_buffer_params_.window_y;

  /* Re-allocate display memory if needed, and make sure the device pointer is allocated.
   *
   * NOTE: allocation happens to the final resolution so that no re-allocation happens on every
   * change of the resolution divider. However, if the display becomes smaller, shrink the
   * allocated memory as well. */
  if (display_rgba_half_.data_width != final_width ||
      display_rgba_half_.data_height != final_height) {
    display_rgba_half_.alloc(final_width, final_height);
    /* TODO(sergey): There should be a way to make sure device-side memory is allocated without
     * transferring zeroes to the device. */
    queue_->zero_to_device(display_rgba_half_);
  }

  PassAccessor::Destination destination(film_->get_display_pass());
  destination.d_pixels_half_rgba = display_rgba_half_.device_pointer;

  get_render_tile_film_pixels(destination, pass_mode, num_samples);

  queue_->copy_from_device(display_rgba_half_);
  queue_->synchronize();

  display->copy_pixels_to_texture(display_rgba_half_.data(), texture_x, texture_y, width, height);
}

bool PathTraceWorkGPU::copy_to_display_interop(PathTraceDisplay *display,
                                               PassMode pass_mode,
                                               int num_samples)
{
  if (!device_graphics_interop_) {
    device_graphics_interop_ = queue_->graphics_interop_create();
  }

  const DisplayDriver::GraphicsInterop graphics_interop_dst = display->graphics_interop_get();
  device_graphics_interop_->set_display_interop(graphics_interop_dst);

  const device_ptr d_rgba_half = device_graphics_interop_->map();
  if (!d_rgba_half) {
    return false;
  }

  PassAccessor::Destination destination = get_display_destination_template(display);
  destination.d_pixels_half_rgba = d_rgba_half;

  get_render_tile_film_pixels(destination, pass_mode, num_samples);

  device_graphics_interop_->unmap();

  return true;
}

void PathTraceWorkGPU::destroy_gpu_resources(PathTraceDisplay *display)
{
  if (!device_graphics_interop_) {
    return;
  }
  display->graphics_interop_activate();
  device_graphics_interop_ = nullptr;
  display->graphics_interop_deactivate();
}

void PathTraceWorkGPU::get_render_tile_film_pixels(const PassAccessor::Destination &destination,
                                                   PassMode pass_mode,
                                                   int num_samples)
{
  const KernelFilm &kfilm = device_scene_->data.film;

  const PassAccessor::PassAccessInfo pass_access_info = get_display_pass_access_info(pass_mode);
  const PassAccessorGPU pass_accessor(queue_.get(), pass_access_info, kfilm.exposure, num_samples);

  pass_accessor.get_render_tile_pixels(buffers_.get(), effective_buffer_params_, destination);
}

int PathTraceWorkGPU::adaptive_sampling_converge_filter_count_active(float threshold, bool reset)
{
  const int num_active_pixels = adaptive_sampling_convergence_check_count_active(threshold, reset);

  if (num_active_pixels) {
    enqueue_adaptive_sampling_filter_x();
    enqueue_adaptive_sampling_filter_y();
    queue_->synchronize();
  }

  return num_active_pixels;
}

int PathTraceWorkGPU::adaptive_sampling_convergence_check_count_active(float threshold, bool reset)
{
  device_vector<uint> num_active_pixels(device_, "num_active_pixels", MEM_READ_WRITE);
  num_active_pixels.alloc(1);

  queue_->zero_to_device(num_active_pixels);

  const int work_size = effective_buffer_params_.width * effective_buffer_params_.height;

  void *args[] = {&buffers_->buffer.device_pointer,
                  const_cast<int *>(&effective_buffer_params_.full_x),
                  const_cast<int *>(&effective_buffer_params_.full_y),
                  const_cast<int *>(&effective_buffer_params_.width),
                  const_cast<int *>(&effective_buffer_params_.height),
                  &threshold,
                  &reset,
                  &effective_buffer_params_.offset,
                  &effective_buffer_params_.stride,
                  &num_active_pixels.device_pointer};

  queue_->enqueue(DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_CHECK, work_size, args);

  queue_->copy_from_device(num_active_pixels);
  queue_->synchronize();

  return num_active_pixels.data()[0];
}

void PathTraceWorkGPU::enqueue_adaptive_sampling_filter_x()
{
  const int work_size = effective_buffer_params_.height;

  void *args[] = {&buffers_->buffer.device_pointer,
                  &effective_buffer_params_.full_x,
                  &effective_buffer_params_.full_y,
                  &effective_buffer_params_.width,
                  &effective_buffer_params_.height,
                  &effective_buffer_params_.offset,
                  &effective_buffer_params_.stride};

  queue_->enqueue(DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_FILTER_X, work_size, args);
}

void PathTraceWorkGPU::enqueue_adaptive_sampling_filter_y()
{
  const int work_size = effective_buffer_params_.width;

  void *args[] = {&buffers_->buffer.device_pointer,
                  &effective_buffer_params_.full_x,
                  &effective_buffer_params_.full_y,
                  &effective_buffer_params_.width,
                  &effective_buffer_params_.height,
                  &effective_buffer_params_.offset,
                  &effective_buffer_params_.stride};

  queue_->enqueue(DEVICE_KERNEL_ADAPTIVE_SAMPLING_CONVERGENCE_FILTER_Y, work_size, args);
}

void PathTraceWorkGPU::cryptomatte_postproces()
{
  const int work_size = effective_buffer_params_.width * effective_buffer_params_.height;

  void *args[] = {&buffers_->buffer.device_pointer,
                  const_cast<int *>(&work_size),
                  &effective_buffer_params_.offset,
                  &effective_buffer_params_.stride};

  queue_->enqueue(DEVICE_KERNEL_CRYPTOMATTE_POSTPROCESS, work_size, args);
}

bool PathTraceWorkGPU::copy_render_buffers_from_device()
{
  queue_->copy_from_device(buffers_->buffer);

  /* Synchronize so that the CPU-side buffer is available at the exit of this function. */
  return queue_->synchronize();
}

bool PathTraceWorkGPU::copy_render_buffers_to_device()
{
  queue_->copy_to_device(buffers_->buffer);

  /* NOTE: The direct device access to the buffers only happens within this path trace work. The
   * rest of communication happens via API calls which involves `copy_render_buffers_from_device()`
   * which will perform synchronization as needed. */

  return true;
}

bool PathTraceWorkGPU::zero_render_buffers()
{
  queue_->zero_to_device(buffers_->buffer);

  return true;
}

bool PathTraceWorkGPU::has_shadow_catcher() const
{
  return device_scene_->data.integrator.has_shadow_catcher;
}

int PathTraceWorkGPU::shadow_catcher_count_possible_splits()
{
  if (max_active_main_path_index_ == 0) {
    return 0;
  }

  if (!has_shadow_catcher()) {
    return 0;
  }

  queue_->zero_to_device(num_queued_paths_);

  const int work_size = max_active_main_path_index_;
  void *d_num_queued_paths = (void *)num_queued_paths_.device_pointer;
  void *args[] = {const_cast<int *>(&work_size), &d_num_queued_paths};

  queue_->enqueue(DEVICE_KERNEL_INTEGRATOR_SHADOW_CATCHER_COUNT_POSSIBLE_SPLITS, work_size, args);
  queue_->copy_from_device(num_queued_paths_);
  queue_->synchronize();

  return num_queued_paths_.data()[0];
}

bool PathTraceWorkGPU::kernel_uses_sorting(DeviceKernel kernel)
{
  return (kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE);
}

bool PathTraceWorkGPU::kernel_creates_shadow_paths(DeviceKernel kernel)
{
  return (kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_VOLUME);
}

bool PathTraceWorkGPU::kernel_creates_ao_paths(DeviceKernel kernel)
{
  return (device_scene_->data.film.pass_ao != PASS_UNUSED) &&
         (kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE);
}

bool PathTraceWorkGPU::kernel_is_shadow_path(DeviceKernel kernel)
{
  return (kernel == DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW ||
          kernel == DEVICE_KERNEL_INTEGRATOR_SHADE_SHADOW);
}

int PathTraceWorkGPU::kernel_max_active_main_path_index(DeviceKernel kernel)
{
  return (kernel_is_shadow_path(kernel)) ? integrator_next_shadow_path_index_.data()[0] :
                                           max_active_main_path_index_;
}

CCL_NAMESPACE_END
