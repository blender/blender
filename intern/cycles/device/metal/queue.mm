/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_METAL

#  include <algorithm>
#  include <mutex>

#  include "device/metal/queue.h"

#  include "device/metal/device_impl.h"
#  include "device/metal/graphics_interop.h"
#  include "device/metal/kernel.h"

#  include "util/path.h"
#  include "util/string.h"
#  include "util/time.h"

CCL_NAMESPACE_BEGIN

/* MetalDeviceQueue */

MetalDeviceQueue::MetalDeviceQueue(MetalDevice *device)
    : DeviceQueue(device), metal_device_(device), stats_(device->stats)
{
  @autoreleasepool {
    command_buffer_desc_ = [[MTLCommandBufferDescriptor alloc] init];
    command_buffer_desc_.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;

    mtlDevice_ = device->mtlDevice;
    mtlCommandQueue_ = device->mtlComputeCommandQueue;

    shared_event_ = [mtlDevice_ newSharedEvent];
    shared_event_id_ = 1;

    /* Shareable event listener */
    event_queue_ = dispatch_queue_create("com.cycles.metal.event_queue", nullptr);
    shared_event_listener_ = [[MTLSharedEventListener alloc] initWithDispatchQueue:event_queue_];

    wait_semaphore_ = dispatch_semaphore_create(0);

    if (auto *str = getenv("CYCLES_METAL_PROFILING")) {
      if (atoi(str) && [mtlDevice_ supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
      {
        /* Enable per-kernel timing breakdown (shown at end of render). */
        profiling_enabled_ = true;
        label_command_encoders_ = true;
      }
    }
    if (getenv("CYCLES_METAL_DEBUG")) {
      /* Enable very verbose tracing (shows every dispatch). */
      verbose_tracing_ = true;
      label_command_encoders_ = true;
    }

    setup_capture();
  }
}

void MetalDeviceQueue::setup_capture()
{
  capture_kernel_ = DeviceKernel(-1);

  if (auto *capture_kernel_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_KERNEL")) {
    /* CYCLES_DEBUG_METAL_CAPTURE_KERNEL captures a single dispatch of the specified kernel. */
    capture_kernel_ = DeviceKernel(atoi(capture_kernel_str));
    printf("Capture kernel: %d = %s\n", capture_kernel_, device_kernel_as_string(capture_kernel_));

    capture_dispatch_counter_ = 0;
    if (auto *capture_dispatch_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_DISPATCH")) {
      capture_dispatch_counter_ = atoi(capture_dispatch_str);

      printf("Capture dispatch number %d\n", capture_dispatch_counter_);
    }
  }
  else if (auto *capture_samples_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_SAMPLES")) {
    /* CYCLES_DEBUG_METAL_CAPTURE_SAMPLES captures a block of dispatches from reset#(N) to
     * reset#(N+1). */
    capture_samples_ = true;
    capture_reset_counter_ = atoi(capture_samples_str);

    capture_dispatch_counter_ = INT_MAX;
    if (auto *capture_limit_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_LIMIT")) {
      /* CYCLES_DEBUG_METAL_CAPTURE_LIMIT sets the maximum number of dispatches to capture. */
      capture_dispatch_counter_ = atoi(capture_limit_str);
    }

    printf("Capturing sample block %d (dispatch limit: %d)\n",
           capture_reset_counter_,
           capture_dispatch_counter_);
  }
  else {
    /* No capturing requested. */
    return;
  }

  /* Enable .gputrace capture for the specified DeviceKernel. */
  MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
  mtlCaptureScope_ = [captureManager newCaptureScopeWithDevice:mtlDevice_];
  mtlCaptureScope_.label = [NSString stringWithFormat:@"Cycles kernel dispatch"];
  [captureManager setDefaultCaptureScope:mtlCaptureScope_];

  label_command_encoders_ = true;

  if (auto *capture_url = getenv("CYCLES_DEBUG_METAL_CAPTURE_URL")) {
    if ([captureManager supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {

      MTLCaptureDescriptor *captureDescriptor = [[MTLCaptureDescriptor alloc] init];
      captureDescriptor.captureObject = mtlCaptureScope_;
      captureDescriptor.destination = MTLCaptureDestinationGPUTraceDocument;
      captureDescriptor.outputURL = [NSURL fileURLWithPath:@(capture_url)];

      NSError *error;
      if (![captureManager startCaptureWithDescriptor:captureDescriptor error:&error]) {
        NSString *err = [error localizedDescription];
        printf("Start capture failed: %s\n", [err UTF8String]);
      }
      else {
        printf("Capture started (URL: %s)\n", capture_url);
        is_capturing_to_disk_ = true;
      }
    }
    else {
      printf("Capture to file is not supported\n");
    }
  }
}

void MetalDeviceQueue::update_capture(DeviceKernel kernel)
{
  /* Handle capture end triggers. */
  if (is_capturing_) {
    capture_dispatch_counter_ -= 1;
    if (capture_dispatch_counter_ <= 0 || kernel == DEVICE_KERNEL_INTEGRATOR_RESET) {
      /* End capture if we've hit the dispatch limit or we hit a "reset". */
      end_capture();
    }
    return;
  }

  if (capture_dispatch_counter_ < 0) {
    /* We finished capturing. */
    return;
  }

  /* Handle single-capture start trigger. */
  if (kernel == capture_kernel_) {
    /* Start capturing when we hit the Nth dispatch of the specified kernel. */
    if (capture_dispatch_counter_ == 0) {
      begin_capture();
    }
    capture_dispatch_counter_ -= 1;
    return;
  }

  /* Handle multi-capture start trigger. */
  if (capture_samples_) {
    /* Start capturing when the reset countdown is at 0. */
    if (capture_reset_counter_ == 0) {
      begin_capture();
    }

    if (kernel == DEVICE_KERNEL_INTEGRATOR_RESET) {
      capture_reset_counter_ -= 1;
    }
    return;
  }
}

void MetalDeviceQueue::begin_capture()
{
  /* Start gputrace capture. */
  if (mtlCommandBuffer_) {
    synchronize();
  }
  [mtlCaptureScope_ beginScope];
  printf("[mtlCaptureScope_ beginScope]\n");
  is_capturing_ = true;
}

void MetalDeviceQueue::end_capture()
{
  [mtlCaptureScope_ endScope];
  is_capturing_ = false;
  printf("[mtlCaptureScope_ endScope]\n");

  if (is_capturing_to_disk_) {
    [[MTLCaptureManager sharedCaptureManager] stopCapture];
    has_captured_to_disk_ = true;
    is_capturing_to_disk_ = false;
    is_capturing_ = false;
    printf("Capture stopped\n");
  }
}

MetalDeviceQueue::~MetalDeviceQueue()
{
  /* Tidying up here isn't really practical - we should expect and require the work
   * queue to be empty here. */
  assert(mtlCommandBuffer_ == nil);
  assert(command_buffers_submitted_ == command_buffers_completed_);

  close_compute_encoder();
  close_blit_encoder();

  [shared_event_listener_ release];
  [shared_event_ release];
  [command_buffer_desc_ release];

  if (mtlCaptureScope_) {
    [mtlCaptureScope_ release];
  }

  double total_time = 0.0;

  /* Show per-kernel timings, if gathered (see CYCLES_METAL_PROFILING). */
  int64_t num_dispatches = 0;
  int64_t num_pathtracing_dispatches = 0;
  for (size_t i = 0; i < DEVICE_KERNEL_NUM; i++) {
    auto &stat = timing_stats_[i];
    bool pathtracing_kernel = (i <= DEVICE_KERNEL_INTEGRATOR_RESET) &&
                              (i != DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL);
    total_time += stat.total_time;
    num_dispatches += stat.num_dispatches;
    num_pathtracing_dispatches += pathtracing_kernel ? stat.num_dispatches : 0;
  }
  bool has_extra = (num_pathtracing_dispatches && num_dispatches > num_pathtracing_dispatches);

  if (num_dispatches) {
    printf("\nMetal %sdispatch stats:\n", num_pathtracing_dispatches ? "path-tracing " : "");
    auto header = string_printf("%-40s %16s %12s %12s %9s %9s",
                                "Kernel name",
                                "Total threads",
                                "Dispatches",
                                "Avg. T/D",
                                "Time/s",
                                "Time/%");
    auto divider = string(header.length(), '-');
    printf("%s\n%s\n%s\n", divider.c_str(), header.c_str(), divider.c_str());

    for (size_t i = 0; i < DEVICE_KERNEL_NUM; i++) {
      auto &stat = timing_stats_[i];

      bool pathtracing_kernel = (i <= DEVICE_KERNEL_INTEGRATOR_RESET) &&
                                (i != DEVICE_KERNEL_INTEGRATOR_MEGAKERNEL);
      if ((pathtracing_kernel && num_pathtracing_dispatches) || stat.num_dispatches > 0) {
        printf("%-40s %16llu %12llu %12llu %9.4f %9.2f\n",
               device_kernel_as_string(DeviceKernel(i)),
               stat.total_work_size,
               stat.num_dispatches,
               stat.total_work_size / stat.num_dispatches,
               stat.total_time,
               stat.total_time * 100.0 / total_time);
      }
      if (has_extra && i == DEVICE_KERNEL_INTEGRATOR_RESET) {
        printf("%s\n", divider.c_str());
      }
    }
    printf("%s\n", divider.c_str());
    printf("%-40s %16s %12llu %12s %9.4f %9.2f\n", "", "", num_dispatches, "", total_time, 100.0);
    printf("%s\n\n", divider.c_str());
  }
}

int MetalDeviceQueue::num_concurrent_states(const size_t state_size) const
{
  static int result = 0;
  if (result) {
    return result;
  }

  result = 4194304;

  /* Increasing the state count doesn't notably benefit M1-family systems. */
  if (MetalInfo::get_apple_gpu_architecture(metal_device_->mtlDevice) != APPLE_M1) {
    size_t system_ram = system_physical_ram();
    size_t allocated_so_far = [metal_device_->mtlDevice currentAllocatedSize];
    size_t max_recommended_working_set = [metal_device_->mtlDevice recommendedMaxWorkingSetSize];

    /* Determine whether we can double the state count, and leave enough GPU-available memory
     * (1/8 the system RAM or 1GB - whichever is largest). Enlarging the state size allows us to
     * keep dispatch sizes high and minimize work submission overheads. */
    size_t min_headroom = std::max(system_ram / 8, size_t(1024 * 1024 * 1024));
    size_t total_state_size = result * state_size;
    if (max_recommended_working_set - allocated_so_far - total_state_size * 2 >= min_headroom) {
      result *= 2;
      metal_printf("Doubling state count to exploit available RAM (new size = %d)", result);
    }
  }
  return result;
}

int MetalDeviceQueue::num_concurrent_busy_states(const size_t state_size) const
{
  /* A 1:4 busy:total ratio gives best rendering performance, independent of total state count. */
  return num_concurrent_states(state_size) / 4;
}

int MetalDeviceQueue::num_sort_partitions(int max_num_paths, uint max_scene_shaders) const
{
  int sort_partition_elements = MetalInfo::optimal_sort_partition_elements();
  /* Sort partitioning becomes less effective when more shaders are in the wavefront. In lieu of
   * a more sophisticated heuristic we simply disable sort partitioning if the shader count is
   * high.
   */
  if (max_scene_shaders < 300 && sort_partition_elements > 0) {
    return max(max_num_paths / sort_partition_elements, 1);
  }
  else {
    return 1;
  }
}

bool MetalDeviceQueue::supports_local_atomic_sort() const
{
  return metal_device_->use_local_atomic_sort();
}

static void zero_resource(void *address_in_arg_buffer, int index = 0)
{
  uint64_t *pptr = (uint64_t *)address_in_arg_buffer;
  pptr[index] = 0;
}

template<class T> void write_resource(void *address_in_arg_buffer, T resource, int index = 0)
{
  zero_resource(address_in_arg_buffer, index);
  uint64_t *pptr = (uint64_t *)address_in_arg_buffer;
  if (resource) {
    pptr[index] = metal_gpuResourceID(resource);
  }
}

template<> void write_resource(void *address_in_arg_buffer, id<MTLBuffer> buffer, int index)
{
  zero_resource(address_in_arg_buffer, index);
  uint64_t *pptr = (uint64_t *)address_in_arg_buffer;
  if (buffer) {
    pptr[index] = metal_gpuAddress(buffer);
  }
}

static id<MTLBuffer> patch_resource(void *address_in_arg_buffer, int index = 0)
{
  uint64_t *pptr = (uint64_t *)address_in_arg_buffer;
  if (MetalDevice::MetalMem *mmem = (MetalDevice::MetalMem *)pptr[index]) {
    write_resource<id<MTLBuffer>>(address_in_arg_buffer, mmem->mtlBuffer, index);
    return mmem->mtlBuffer;
  }
  return nil;
}

void MetalDeviceQueue::init_execution()
{
  /* Populate blas_array. */
  uint64_t *blas_array = (uint64_t *)metal_device_->blas_buffer.contents;
  for (uint64_t slot = 0; slot < metal_device_->blas_array.size(); ++slot) {
    write_resource(blas_array, metal_device_->blas_array[slot], slot);
  }

  device_vector<KernelImageInfo> &image_info = metal_device_->image_info;
  id<MTLBuffer> &image_bindings = metal_device_->image_bindings;
  std::vector<id<MTLResource>> &image_info_id_map = metal_device_->image_info_id_map;

  /* Ensure image_info is allocated before populating. */
  image_info.copy_to_device();

  /* Populate texture bindings. */
  uint64_t *bindings = (uint64_t *)image_bindings.contents;
  memset(bindings, 0, image_bindings.length);
  for (int image_info_id = 0; image_info_id < image_info.size(); ++image_info_id) {
    if (image_info_id_map[image_info_id]) {
      if (metal_device_->is_texture(image_info[image_info_id])) {
        write_resource(bindings, id<MTLTexture>(image_info_id_map[image_info_id]), image_info_id);
      }
      else {
        /* The GPU address of a 1D buffer texture is written into the image_info_id data field. */
        write_resource(
            &image_info[image_info_id].data, id<MTLBuffer>(image_info_id_map[image_info_id]), 0);
      }
    }
  }

  /* Synchronize memory copies. */
  synchronize();
}

bool MetalDeviceQueue::enqueue(DeviceKernel kernel,
                               const int work_size,
                               const DeviceKernelArguments &args)
{
  @autoreleasepool {
    update_capture(kernel);

    if (metal_device_->have_error()) {
      return false;
    }

    debug_enqueue_begin(kernel, work_size);

    LOG_TRACE << "Metal queue launch " << device_kernel_as_string(kernel) << ", work_size "
              << work_size;

    id<MTLComputeCommandEncoder> mtlComputeCommandEncoder = get_compute_encoder(kernel);

    if (profiling_enabled_) {
      command_encoder_labels_.push_back({kernel, work_size, current_encoder_idx_});
    }
    if (label_command_encoders_) {
      /* Add human-readable labels if we're doing any form of debugging / profiling. */
      mtlComputeCommandEncoder.label = [NSString
          stringWithFormat:@"Metal queue launch %s, work_size %d",
                           device_kernel_as_string(kernel),
                           work_size];
    }

    if (!active_pipelines_[kernel].update(metal_device_, kernel)) {
      metal_device_->set_error(
          string_printf("Could not activate pipeline for %s\n", device_kernel_as_string(kernel)));
      return false;
    }
    MetalDispatchPipeline &active_pipeline = active_pipelines_[kernel];

    uint8_t dynamic_args[512] = {0};

    /* Prepare the dynamic "enqueue" arguments */
    size_t dynamic_bytes_written = 0;
    size_t max_size_in_bytes = 0;
    for (size_t i = 0; i < args.count; i++) {
      size_t size_in_bytes = args.sizes[i];
      max_size_in_bytes = max(max_size_in_bytes, size_in_bytes);
      dynamic_bytes_written = round_up(dynamic_bytes_written, size_in_bytes);
      memcpy(dynamic_args + dynamic_bytes_written, args.values[i], size_in_bytes);
      if (args.types[i] == DeviceKernelArguments::POINTER) {
        if (id<MTLBuffer> buffer = patch_resource(dynamic_args + dynamic_bytes_written)) {
          [mtlComputeCommandEncoder useResource:buffer
                                          usage:MTLResourceUsageRead | MTLResourceUsageWrite];
        }
      }
      dynamic_bytes_written += size_in_bytes;
    }
    /* Apply conventional struct alignment (stops asserts firing when API validation is enabled).
     */
    dynamic_bytes_written = round_up(dynamic_bytes_written, max_size_in_bytes);

    /* Check that the dynamic args didn't overflow. */
    assert(dynamic_bytes_written <= sizeof(dynamic_args));

    uint64_t ancillary_args[ANCILLARY_SLOT_COUNT] = {0};

    /* Encode ancillaries */
    int ancillary_index = 0;
    write_resource(ancillary_args, metal_device_->image_bindings, ancillary_index++);

    if (metal_device_->use_metalrt) {
      write_resource(ancillary_args, metal_device_->accel_struct, ancillary_index++);
      write_resource(ancillary_args, metal_device_->blas_buffer, ancillary_index++);

      /* Write the intersection function table. */
      for (int table_idx = 0; table_idx < METALRT_TABLE_NUM; table_idx++) {
        write_resource(
            ancillary_args, active_pipeline.intersection_func_table[table_idx], ancillary_index++);
      }
      assert(ancillary_index == ANCILLARY_SLOT_COUNT);
    }

    /* Encode ancillaries */
    if (metal_device_->use_metalrt) {
      for (int table = 0; table < METALRT_TABLE_NUM; table++) {
        if (active_pipeline.intersection_func_table[table]) {
          [active_pipeline.intersection_func_table[table]
              setBuffer:metal_device_->launch_params_buffer
                 offset:0
                atIndex:1];
          [mtlComputeCommandEncoder useResource:active_pipeline.intersection_func_table[table]
                                          usage:MTLResourceUsageRead];
        }
      }
    }

    [mtlComputeCommandEncoder setBytes:dynamic_args length:dynamic_bytes_written atIndex:0];
    [mtlComputeCommandEncoder setBuffer:metal_device_->launch_params_buffer offset:0 atIndex:1];
    [mtlComputeCommandEncoder setBytes:ancillary_args length:sizeof(ancillary_args) atIndex:2];

    if (metal_device_->use_metalrt && device_kernel_has_intersection(kernel)) {
      if (@available(macos 12.0, *)) {

        if (id<MTLAccelerationStructure> accel_struct = metal_device_->accel_struct) {
          /* Mark all Accelerations resources as used */
          [mtlComputeCommandEncoder useResource:accel_struct usage:MTLResourceUsageRead];
          if (metal_device_->blas_buffer) {
            [mtlComputeCommandEncoder useResource:metal_device_->blas_buffer
                                            usage:MTLResourceUsageRead];
          }
          [mtlComputeCommandEncoder useResources:metal_device_->unique_blas_array.data()
                                           count:metal_device_->unique_blas_array.size()
                                           usage:MTLResourceUsageRead];
        }
      }
    }

    [mtlComputeCommandEncoder setComputePipelineState:active_pipeline.pipeline];

    /* Compute kernel launch parameters. */
    const int num_threads_per_block = active_pipeline.num_threads_per_block;

    int shared_mem_bytes = 0;

    switch (kernel) {
      case DEVICE_KERNEL_INTEGRATOR_QUEUED_PATHS_ARRAY:
      case DEVICE_KERNEL_INTEGRATOR_QUEUED_SHADOW_PATHS_ARRAY:
      case DEVICE_KERNEL_INTEGRATOR_ACTIVE_PATHS_ARRAY:
      case DEVICE_KERNEL_INTEGRATOR_TERMINATED_PATHS_ARRAY:
      case DEVICE_KERNEL_INTEGRATOR_SORTED_PATHS_ARRAY:
      case DEVICE_KERNEL_INTEGRATOR_COMPACT_PATHS_ARRAY:
      case DEVICE_KERNEL_INTEGRATOR_TERMINATED_SHADOW_PATHS_ARRAY:
      case DEVICE_KERNEL_INTEGRATOR_COMPACT_SHADOW_PATHS_ARRAY:
        /* See parallel_active_index.h for why this amount of shared memory is needed.
         * Rounded up to 16 bytes for Metal */
        shared_mem_bytes = (int)round_up((num_threads_per_block + 1) * sizeof(int), 16);
        break;

      case DEVICE_KERNEL_INTEGRATOR_SORT_BUCKET_PASS:
      case DEVICE_KERNEL_INTEGRATOR_SORT_WRITE_PASS: {
        int key_count = metal_device_->launch_params->data.max_shaders;
        shared_mem_bytes = (int)round_up(key_count * sizeof(int), 16);
        break;
      }

      default:
        break;
    }

    if (shared_mem_bytes) {
      assert(shared_mem_bytes <= 32 * 1024);
      [mtlComputeCommandEncoder setThreadgroupMemoryLength:shared_mem_bytes atIndex:0];
    }

    MTLSize size_threads_per_dispatch = MTLSizeMake(work_size, 1, 1);
    MTLSize size_threads_per_threadgroup = MTLSizeMake(num_threads_per_block, 1, 1);
    [mtlComputeCommandEncoder dispatchThreads:size_threads_per_dispatch
                        threadsPerThreadgroup:size_threads_per_threadgroup];

    [mtlCommandBuffer_ addCompletedHandler:^(id<MTLCommandBuffer> command_buffer) {
      /* Enhanced command buffer errors */
      string str;
      if (command_buffer.status != MTLCommandBufferStatusCompleted) {
        str = string_printf("Command buffer not completed. status = %d. ",
                            int(command_buffer.status));
      }
      if (command_buffer.error) {
        @autoreleasepool {
          const char *errCStr = [[NSString stringWithFormat:@"%@", command_buffer.error]
              UTF8String];
          str += string_printf("(%s.%s):\n%s\n",
                               kernel_type_as_string(active_pipeline.pso_type),
                               device_kernel_as_string(kernel),
                               errCStr);
        }
      }
      if (!str.empty()) {
        metal_device_->set_error(str);
      }
    }];

    if (verbose_tracing_ || is_capturing_) {
      /* Force a sync we've enabled step-by-step verbose tracing or if we're capturing. */
      synchronize();

      /* Show queue counters and dispatch timing. */
      if (verbose_tracing_) {
        if (kernel == DEVICE_KERNEL_INTEGRATOR_RESET) {
          printf(
              "_____________________________________.____________________.______________._________"
              "__"
              "______________________________________\n");
        }

        printf("%-40s| %7d threads |%5.2fms | buckets [",
               device_kernel_as_string(kernel),
               work_size,
               last_completion_time_ * 1000.0);
        std::lock_guard<std::recursive_mutex> lock(metal_device_->metal_mem_map_mutex);
        for (auto &it : metal_device_->metal_mem_map) {
          const string c_integrator_queue_counter = "integrator_queue_counter";
          if (it.first->global_name() == c_integrator_queue_counter) {
            if (IntegratorQueueCounter *queue_counter = (IntegratorQueueCounter *)
                                                            it.first->host_pointer)
            {
              for (int i = 0; i < DEVICE_KERNEL_INTEGRATOR_NUM; i++) {
                printf("%s%d", i == 0 ? "" : ",", queue_counter->num_queued[i]);
              }
            }
            break;
          }
        }
        printf("]\n");
      }
    }

    debug_enqueue_end();

    return !(metal_device_->have_error());
  }
}

void MetalDeviceQueue::flush_timing_stats()
{
  for (auto label : command_encoder_labels_) {
    TimingStats &stat = timing_stats_[label.kernel];

    double completion_time_gpu;
    NSData *computeTimeStamps = [metal_device_->mtlCounterSampleBuffer
        resolveCounterRange:NSMakeRange(label.timing_id, 2)];
    MTLCounterResultTimestamp *timestamps = (MTLCounterResultTimestamp *)(computeTimeStamps.bytes);

    uint64_t begTime = timestamps[0].timestamp;
    uint64_t endTime = timestamps[1].timestamp;
    completion_time_gpu = (endTime - begTime) / (double)NSEC_PER_SEC;

    stat.num_dispatches++;
    stat.total_time += completion_time_gpu;
    stat.total_work_size += label.work_size;
    last_completion_time_ = completion_time_gpu;
  }
  command_encoder_labels_.clear();
}

bool MetalDeviceQueue::synchronize()
{
  @autoreleasepool {
    if (has_captured_to_disk_ || metal_device_->have_error()) {
      return false;
    }

    close_compute_encoder();
    close_blit_encoder();

    if (mtlCommandBuffer_) {
      scoped_timer timer;

      uint64_t shared_event_id_ = this->shared_event_id_++;

      __block dispatch_semaphore_t block_sema = wait_semaphore_;
      [shared_event_ notifyListener:shared_event_listener_
                            atValue:shared_event_id_
                              block:^(id<MTLSharedEvent> /*sharedEvent*/, uint64_t /*value*/) {
                                dispatch_semaphore_signal(block_sema);
                              }];

      [mtlCommandBuffer_ encodeSignalEvent:shared_event_ value:shared_event_id_];
      [mtlCommandBuffer_ commit];
      dispatch_semaphore_wait(wait_semaphore_, DISPATCH_TIME_FOREVER);

      [mtlCommandBuffer_ release];

      metal_device_->flush_delayed_free_list();

      mtlCommandBuffer_ = nil;
      flush_timing_stats();
    }

    debug_synchronize();

    return !(metal_device_->have_error());
  }
}

void MetalDeviceQueue::zero_to_device(device_memory &mem)
{
  @autoreleasepool {
    if (metal_device_->have_error()) {
      return;
    }

    assert(mem.type != MEM_GLOBAL && mem.type != MEM_IMAGE_TEXTURE);

    if (mem.memory_size() == 0) {
      return;
    }

    /* Allocate on demand. */
    if (mem.device_pointer == 0) {
      metal_device_->mem_alloc(mem);
    }

    /* Zero memory on device. */
    assert(mem.device_pointer != 0);

    std::lock_guard<std::recursive_mutex> lock(metal_device_->metal_mem_map_mutex);
    MetalDevice::MetalMem &mmem = *metal_device_->metal_mem_map.at(&mem);
    if (mmem.mtlBuffer) {
      id<MTLBlitCommandEncoder> blitEncoder = get_blit_encoder();
      [blitEncoder fillBuffer:mmem.mtlBuffer range:NSMakeRange(mmem.offset, mmem.size) value:0];
    }
    else {
      metal_device_->mem_zero(mem);
    }
  }
}

void MetalDeviceQueue::copy_to_device(device_memory &mem)
{
  @autoreleasepool {
    if (metal_device_->have_error()) {
      return;
    }

    if (mem.memory_size() == 0) {
      return;
    }

    /* Allocate on demand. */
    if (mem.device_pointer == 0) {
      metal_device_->mem_alloc(mem);
    }

    assert(mem.device_pointer != 0);
    assert(mem.host_pointer != nullptr);
    /* No need to copy - Apple Silicon has Unified Memory Architecture. */
  }
}

void MetalDeviceQueue::copy_from_device(device_memory & /*mem*/)
{
  /* No need to copy - Apple Silicon has Unified Memory Architecture. */
}

void MetalDeviceQueue::prepare_resources(DeviceKernel /*kernel*/)
{
  std::lock_guard<std::recursive_mutex> lock(metal_device_->metal_mem_map_mutex);

  /* declare resource usage */
  for (auto &it : metal_device_->metal_mem_map) {
    device_memory *mem = it.first;

    MTLResourceUsage usage = MTLResourceUsageRead;
    if (mem->type != MEM_GLOBAL && mem->type != MEM_READ_ONLY && mem->type != MEM_IMAGE_TEXTURE) {
      usage |= MTLResourceUsageWrite;
    }

    if (it.second->mtlBuffer) {
      /* METAL_WIP - use array version (i.e. useResources) */
      [mtlComputeEncoder_ useResource:it.second->mtlBuffer usage:usage];
    }
    else if (it.second->mtlTexture) {
      /* METAL_WIP - use array version (i.e. useResources) */
      [mtlComputeEncoder_ useResource:it.second->mtlTexture usage:usage | MTLResourceUsageSample];
    }
  }

  /* ancillaries */
  [mtlComputeEncoder_ useResource:metal_device_->image_bindings usage:MTLResourceUsageRead];
}

id<MTLComputeCommandEncoder> MetalDeviceQueue::get_compute_encoder(DeviceKernel kernel)
{
  bool concurrent = int(kernel) < int(DEVICE_KERNEL_INTEGRATOR_NUM);

  if (profiling_enabled_) {
    /* Close the current encoder to ensure we're able to capture per-encoder timing data. */
    close_compute_encoder();
  }

  if (mtlComputeEncoder_) {
    if (mtlComputeEncoder_.dispatchType == concurrent ? MTLDispatchTypeConcurrent :
                                                        MTLDispatchTypeSerial)
    {
      /* declare usage of MTLBuffers etc */
      prepare_resources(kernel);

      return mtlComputeEncoder_;
    }
    close_compute_encoder();
  }

  close_blit_encoder();

  if (!mtlCommandBuffer_) {
    mtlCommandBuffer_ = [mtlCommandQueue_ commandBuffer];
    [mtlCommandBuffer_ retain];
  }

  if (profiling_enabled_) {
    MTLComputePassDescriptor *desc = [[MTLComputePassDescriptor alloc] init];

    current_encoder_idx_ = (counter_sample_buffer_curr_idx_.fetch_add(2) %
                            MAX_SAMPLE_BUFFER_LENGTH);
    [desc.sampleBufferAttachments[0] setSampleBuffer:metal_device_->mtlCounterSampleBuffer];
    [desc.sampleBufferAttachments[0] setStartOfEncoderSampleIndex:current_encoder_idx_];
    [desc.sampleBufferAttachments[0] setEndOfEncoderSampleIndex:current_encoder_idx_ + 1];

    [desc setDispatchType:concurrent ? MTLDispatchTypeConcurrent : MTLDispatchTypeSerial];

    mtlComputeEncoder_ = [mtlCommandBuffer_ computeCommandEncoderWithDescriptor:desc];
  }
  else {
    mtlComputeEncoder_ = [mtlCommandBuffer_
        computeCommandEncoderWithDispatchType:concurrent ? MTLDispatchTypeConcurrent :
                                                           MTLDispatchTypeSerial];
  }

  [mtlComputeEncoder_ retain];
  [mtlComputeEncoder_ setLabel:@(device_kernel_as_string(kernel))];

  /* declare usage of MTLBuffers etc */
  prepare_resources(kernel);

  return mtlComputeEncoder_;
}

id<MTLBlitCommandEncoder> MetalDeviceQueue::get_blit_encoder()
{
  if (mtlBlitEncoder_) {
    return mtlBlitEncoder_;
  }

  close_compute_encoder();

  if (!mtlCommandBuffer_) {
    mtlCommandBuffer_ = [mtlCommandQueue_ commandBuffer];
    [mtlCommandBuffer_ retain];
  }

  mtlBlitEncoder_ = [mtlCommandBuffer_ blitCommandEncoder];
  [mtlBlitEncoder_ retain];
  return mtlBlitEncoder_;
}

void MetalDeviceQueue::close_compute_encoder()
{
  if (mtlComputeEncoder_) {
    [mtlComputeEncoder_ endEncoding];
    [mtlComputeEncoder_ release];
    mtlComputeEncoder_ = nil;
  }
}

void MetalDeviceQueue::close_blit_encoder()
{
  if (mtlBlitEncoder_) {
    [mtlBlitEncoder_ endEncoding];
    [mtlBlitEncoder_ release];
    mtlBlitEncoder_ = nil;
  }
}

void *MetalDeviceQueue::native_queue()
{
  return mtlCommandQueue_;
}

unique_ptr<DeviceGraphicsInterop> MetalDeviceQueue::graphics_interop_create()
{
  return make_unique<MetalDeviceGraphicsInterop>(this);
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
