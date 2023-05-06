/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#ifdef WITH_METAL

#  include "device/metal/queue.h"

#  include "device/metal/device_impl.h"
#  include "device/metal/kernel.h"

#  include "util/path.h"
#  include "util/string.h"
#  include "util/time.h"

CCL_NAMESPACE_BEGIN

/* MetalDeviceQueue */

MetalDeviceQueue::MetalDeviceQueue(MetalDevice *device)
    : DeviceQueue(device), metal_device_(device), stats_(device->stats)
{
  if (@available(macos 11.0, *)) {
    command_buffer_desc_ = [[MTLCommandBufferDescriptor alloc] init];
    command_buffer_desc_.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
  }

  mtlDevice_ = device->mtlDevice;
  mtlCommandQueue_ = [mtlDevice_ newCommandQueue];

  if (@available(macos 10.14, *)) {
    shared_event_ = [mtlDevice_ newSharedEvent];
    shared_event_id_ = 1;

    /* Shareable event listener */
    event_queue_ = dispatch_queue_create("com.cycles.metal.event_queue", NULL);
    shared_event_listener_ = [[MTLSharedEventListener alloc] initWithDispatchQueue:event_queue_];
  }

  wait_semaphore_ = dispatch_semaphore_create(0);

  if (@available(macos 10.14, *)) {
    if (getenv("CYCLES_METAL_PROFILING")) {
      /* Enable per-kernel timing breakdown (shown at end of render). */
      timing_shared_event_ = [mtlDevice_ newSharedEvent];
      label_command_encoders_ = true;
    }
    if (getenv("CYCLES_METAL_DEBUG")) {
      /* Enable very verbose tracing (shows every dispatch). */
      verbose_tracing_ = true;
      label_command_encoders_ = true;
    }
    timing_shared_event_id_ = 1;
  }

  setup_capture();
}

void MetalDeviceQueue::setup_capture()
{
  capture_kernel_ = DeviceKernel(-1);

  if (auto capture_kernel_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_KERNEL")) {
    /* CYCLES_DEBUG_METAL_CAPTURE_KERNEL captures a single dispatch of the specified kernel. */
    capture_kernel_ = DeviceKernel(atoi(capture_kernel_str));
    printf("Capture kernel: %d = %s\n", capture_kernel_, device_kernel_as_string(capture_kernel_));

    capture_dispatch_counter_ = 0;
    if (auto capture_dispatch_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_DISPATCH")) {
      capture_dispatch_counter_ = atoi(capture_dispatch_str);

      printf("Capture dispatch number %d\n", capture_dispatch_counter_);
    }
  }
  else if (auto capture_samples_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_SAMPLES")) {
    /* CYCLES_DEBUG_METAL_CAPTURE_SAMPLES captures a block of dispatches from reset#(N) to
     * reset#(N+1). */
    capture_samples_ = true;
    capture_reset_counter_ = atoi(capture_samples_str);

    capture_dispatch_counter_ = INT_MAX;
    if (auto capture_limit_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_LIMIT")) {
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

  if (auto capture_url = getenv("CYCLES_DEBUG_METAL_CAPTURE_URL")) {
    if (@available(macos 10.15, *)) {
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
    /* Start capturing when the we hit the Nth dispatch of the specified kernel. */
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
    if (@available(macos 10.15, *)) {
      [[MTLCaptureManager sharedCaptureManager] stopCapture];
      has_captured_to_disk_ = true;
      is_capturing_to_disk_ = false;
      is_capturing_ = false;
      printf("Capture stopped\n");
    }
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

  if (@available(macos 10.14, *)) {
    [shared_event_listener_ release];
    [shared_event_ release];
  }

  if (@available(macos 11.0, *)) {
    [command_buffer_desc_ release];
  }
  if (mtlCommandQueue_) {
    [mtlCommandQueue_ release];
    mtlCommandQueue_ = nil;
  }

  if (mtlCaptureScope_) {
    [mtlCaptureScope_ release];
  }

  double total_time = 0.0;

  /* Show per-kernel timings, if gathered (see CYCLES_METAL_PROFILING). */
  int64_t num_dispatches = 0;
  for (auto &stat : timing_stats_) {
    total_time += stat.total_time;
    num_dispatches += stat.num_dispatches;
  }

  if (num_dispatches) {
    printf("\nMetal dispatch stats:\n\n");
    auto header = string_printf("%-40s %16s %12s %12s %7s %7s",
                                "Kernel name",
                                "Total threads",
                                "Dispatches",
                                "Avg. T/D",
                                "Time",
                                "Time%");
    auto divider = string(header.length(), '-');
    printf("%s\n%s\n%s\n", divider.c_str(), header.c_str(), divider.c_str());

    for (size_t i = 0; i < DEVICE_KERNEL_NUM; i++) {
      auto &stat = timing_stats_[i];
      if (stat.num_dispatches > 0) {
        printf("%-40s %16s %12s %12s %6.2fs %6.2f%%\n",
               device_kernel_as_string(DeviceKernel(i)),
               string_human_readable_number(stat.total_work_size).c_str(),
               string_human_readable_number(stat.num_dispatches).c_str(),
               string_human_readable_number(stat.total_work_size / stat.num_dispatches).c_str(),
               stat.total_time,
               stat.total_time * 100.0 / total_time);
      }
    }
    printf("%s\n", divider.c_str());
    printf("%-40s %16s %12s %12s %6.2fs %6.2f%%\n",
           "",
           "",
           string_human_readable_number(num_dispatches).c_str(),
           "",
           total_time,
           100.0);
    printf("%s\n\n", divider.c_str());
  }
}

int MetalDeviceQueue::num_concurrent_states(const size_t state_size) const
{
  static int result = 0;
  if (result) {
    return result;
  }

  result = 1048576;
  if (metal_device_->device_vendor == METAL_GPU_APPLE) {
    result *= 4;

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
        metal_printf("Doubling state count to exploit available RAM (new size = %d)\n", result);
      }
    }
  }
  else if (metal_device_->device_vendor == METAL_GPU_AMD) {
    /* METAL_WIP */
    /* TODO: compute automatically. */
    /* TODO: must have at least num_threads_per_block. */
    result *= 2;
  }
  return result;
}

int MetalDeviceQueue::num_concurrent_busy_states(const size_t state_size) const
{
  /* A 1:4 busy:total ratio gives best rendering performance, independent of total state count. */
  return num_concurrent_states(state_size) / 4;
}

int MetalDeviceQueue::num_sort_partition_elements() const
{
  return MetalInfo::optimal_sort_partition_elements(metal_device_->mtlDevice);
}

bool MetalDeviceQueue::supports_local_atomic_sort() const
{
  return metal_device_->use_local_atomic_sort();
}

void MetalDeviceQueue::init_execution()
{
  /* Synchronize all textures and memory copies before executing task. */
  metal_device_->load_texture_info();

  synchronize();
}

bool MetalDeviceQueue::enqueue(DeviceKernel kernel,
                               const int work_size,
                               DeviceKernelArguments const &args)
{
  update_capture(kernel);

  if (metal_device_->have_error()) {
    return false;
  }

  VLOG_DEVICE_STATS << "Metal queue launch " << device_kernel_as_string(kernel) << ", work_size "
                    << work_size;

  id<MTLComputeCommandEncoder> mtlComputeCommandEncoder = get_compute_encoder(kernel);

  if (@available(macos 10.14, *)) {
    if (timing_shared_event_) {
      command_encoder_labels_.push_back({kernel, work_size, timing_shared_event_id_});
    }
  }

  /* Determine size requirement for argument buffer. */
  size_t arg_buffer_length = 0;
  for (size_t i = 0; i < args.count; i++) {
    size_t size_in_bytes = args.sizes[i];
    arg_buffer_length = round_up(arg_buffer_length, size_in_bytes) + size_in_bytes;
  }
  /* 256 is the Metal offset alignment for constant address space bindings */
  arg_buffer_length = round_up(arg_buffer_length, 256);

  /* Globals placed after "vanilla" arguments. */
  size_t globals_offsets = arg_buffer_length;
  arg_buffer_length += sizeof(KernelParamsMetal);
  arg_buffer_length = round_up(arg_buffer_length, 256);

  /* Metal ancillary bindless pointers. */
  size_t metal_offsets = arg_buffer_length;
  arg_buffer_length += metal_device_->mtlAncillaryArgEncoder.encodedLength;
  arg_buffer_length = round_up(arg_buffer_length, metal_device_->mtlAncillaryArgEncoder.alignment);

  /* Temporary buffer used to prepare arg_buffer */
  uint8_t *init_arg_buffer = (uint8_t *)alloca(arg_buffer_length);
  memset(init_arg_buffer, 0, arg_buffer_length);

  /* Prepare the non-pointer "enqueue" arguments */
  size_t bytes_written = 0;
  for (size_t i = 0; i < args.count; i++) {
    size_t size_in_bytes = args.sizes[i];
    bytes_written = round_up(bytes_written, size_in_bytes);
    if (args.types[i] != DeviceKernelArguments::POINTER) {
      memcpy(init_arg_buffer + bytes_written, args.values[i], size_in_bytes);
    }
    bytes_written += size_in_bytes;
  }

  /* Prepare any non-pointer (i.e. plain-old-data) KernelParamsMetal data */
  /* The plain-old-data is contiguous, continuing to the end of KernelParamsMetal */
  size_t plain_old_launch_data_offset = offsetof(KernelParamsMetal, integrator_state) +
                                        offsetof(IntegratorStateGPU, sort_partition_divisor);
  size_t plain_old_launch_data_size = sizeof(KernelParamsMetal) - plain_old_launch_data_offset;
  memcpy(init_arg_buffer + globals_offsets + plain_old_launch_data_offset,
         (uint8_t *)&metal_device_->launch_params + plain_old_launch_data_offset,
         plain_old_launch_data_size);

  /* Allocate an argument buffer. */
  MTLResourceOptions arg_buffer_options = MTLResourceStorageModeManaged;
  if (@available(macOS 11.0, *)) {
    if ([mtlDevice_ hasUnifiedMemory]) {
      arg_buffer_options = MTLResourceStorageModeShared;
    }
  }

  id<MTLBuffer> arg_buffer = temp_buffer_pool_.get_buffer(mtlDevice_,
                                                          mtlCommandBuffer_,
                                                          arg_buffer_length,
                                                          arg_buffer_options,
                                                          init_arg_buffer,
                                                          stats_);

  /* Encode the pointer "enqueue" arguments */
  bytes_written = 0;
  for (size_t i = 0; i < args.count; i++) {
    size_t size_in_bytes = args.sizes[i];
    bytes_written = round_up(bytes_written, size_in_bytes);
    if (args.types[i] == DeviceKernelArguments::POINTER) {
      [metal_device_->mtlBufferKernelParamsEncoder setArgumentBuffer:arg_buffer
                                                              offset:bytes_written];
      if (MetalDevice::MetalMem *mmem = *(MetalDevice::MetalMem **)args.values[i]) {
        [mtlComputeCommandEncoder useResource:mmem->mtlBuffer
                                        usage:MTLResourceUsageRead | MTLResourceUsageWrite];
        [metal_device_->mtlBufferKernelParamsEncoder setBuffer:mmem->mtlBuffer offset:0 atIndex:0];
      }
      else {
        if (@available(macos 12.0, *)) {
          [metal_device_->mtlBufferKernelParamsEncoder setBuffer:nil offset:0 atIndex:0];
        }
      }
    }
    bytes_written += size_in_bytes;
  }

  /* Encode KernelParamsMetal buffers */
  [metal_device_->mtlBufferKernelParamsEncoder setArgumentBuffer:arg_buffer
                                                          offset:globals_offsets];

  if (label_command_encoders_) {
    /* Add human-readable labels if we're doing any form of debugging / profiling. */
    mtlComputeCommandEncoder.label = [[NSString alloc]
        initWithFormat:@"Metal queue launch %s, work_size %d",
                       device_kernel_as_string(kernel),
                       work_size];
  }

  /* this relies on IntegratorStateGPU layout being contiguous device_ptrs  */
  const size_t pointer_block_end = offsetof(KernelParamsMetal, integrator_state) +
                                   offsetof(IntegratorStateGPU, sort_partition_divisor);
  for (size_t offset = 0; offset < pointer_block_end; offset += sizeof(device_ptr)) {
    int pointer_index = int(offset / sizeof(device_ptr));
    MetalDevice::MetalMem *mmem = *(
        MetalDevice::MetalMem **)((uint8_t *)&metal_device_->launch_params + offset);
    if (mmem && mmem->mem && (mmem->mtlBuffer || mmem->mtlTexture)) {
      [metal_device_->mtlBufferKernelParamsEncoder setBuffer:mmem->mtlBuffer
                                                      offset:0
                                                     atIndex:pointer_index];
    }
    else {
      if (@available(macos 12.0, *)) {
        [metal_device_->mtlBufferKernelParamsEncoder setBuffer:nil offset:0 atIndex:pointer_index];
      }
    }
  }
  bytes_written = globals_offsets + sizeof(KernelParamsMetal);

  const MetalKernelPipeline *metal_kernel_pso = MetalDeviceKernels::get_best_pipeline(
      metal_device_, kernel);
  if (!metal_kernel_pso) {
    metal_device_->set_error(
        string_printf("No MetalKernelPipeline for %s\n", device_kernel_as_string(kernel)));
    return false;
  }

  /* Encode ancillaries */
  [metal_device_->mtlAncillaryArgEncoder setArgumentBuffer:arg_buffer offset:metal_offsets];
  [metal_device_->mtlAncillaryArgEncoder setBuffer:metal_device_->texture_bindings_2d
                                            offset:0
                                           atIndex:0];
  [metal_device_->mtlAncillaryArgEncoder setBuffer:metal_device_->texture_bindings_3d
                                            offset:0
                                           atIndex:1];
  [metal_device_->mtlAncillaryArgEncoder setBuffer:metal_device_->buffer_bindings_1d
                                            offset:0
                                           atIndex:2];

  if (@available(macos 12.0, *)) {
    if (metal_device_->use_metalrt) {
      if (metal_device_->bvhMetalRT) {
        id<MTLAccelerationStructure> accel_struct = metal_device_->bvhMetalRT->accel_struct;
        [metal_device_->mtlAncillaryArgEncoder setAccelerationStructure:accel_struct atIndex:3];
        [metal_device_->mtlAncillaryArgEncoder setBuffer:metal_device_->blas_buffer
                                                  offset:0
                                                 atIndex:8];
        [metal_device_->mtlAncillaryArgEncoder setBuffer:metal_device_->blas_lookup_buffer
                                                  offset:0
                                                 atIndex:9];
      }

      for (int table = 0; table < METALRT_TABLE_NUM; table++) {
        if (metal_kernel_pso->intersection_func_table[table]) {
          [metal_kernel_pso->intersection_func_table[table] setBuffer:arg_buffer
                                                               offset:globals_offsets
                                                              atIndex:1];
          [metal_device_->mtlAncillaryArgEncoder
              setIntersectionFunctionTable:metal_kernel_pso->intersection_func_table[table]
                                   atIndex:4 + table];
          [mtlComputeCommandEncoder useResource:metal_kernel_pso->intersection_func_table[table]
                                          usage:MTLResourceUsageRead];
        }
        else {
          [metal_device_->mtlAncillaryArgEncoder setIntersectionFunctionTable:nil
                                                                      atIndex:4 + table];
        }
      }
    }
    bytes_written = metal_offsets + metal_device_->mtlAncillaryArgEncoder.encodedLength;
  }

  if (arg_buffer.storageMode == MTLStorageModeManaged) {
    [arg_buffer didModifyRange:NSMakeRange(0, bytes_written)];
  }

  [mtlComputeCommandEncoder setBuffer:arg_buffer offset:0 atIndex:0];
  [mtlComputeCommandEncoder setBuffer:arg_buffer offset:globals_offsets atIndex:1];
  [mtlComputeCommandEncoder setBuffer:arg_buffer offset:metal_offsets atIndex:2];

  if (metal_device_->use_metalrt) {
    if (@available(macos 12.0, *)) {

      auto bvhMetalRT = metal_device_->bvhMetalRT;
      switch (kernel) {
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_CLOSEST:
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SHADOW:
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE:
        case DEVICE_KERNEL_INTEGRATOR_INTERSECT_VOLUME_STACK:
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE:
        case DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE:
          break;
        default:
          bvhMetalRT = nil;
          break;
      }

      if (bvhMetalRT) {
        /* Mark all Accelerations resources as used */
        [mtlComputeCommandEncoder useResource:bvhMetalRT->accel_struct usage:MTLResourceUsageRead];
        [mtlComputeCommandEncoder useResource:metal_device_->blas_buffer
                                        usage:MTLResourceUsageRead];
        [mtlComputeCommandEncoder useResource:metal_device_->blas_lookup_buffer
                                        usage:MTLResourceUsageRead];
        [mtlComputeCommandEncoder useResources:bvhMetalRT->blas_array.data()
                                         count:bvhMetalRT->blas_array.size()
                                         usage:MTLResourceUsageRead];
      }
    }
  }

  [mtlComputeCommandEncoder setComputePipelineState:metal_kernel_pso->pipeline];

  /* Compute kernel launch parameters. */
  const int num_threads_per_block = metal_kernel_pso->num_threads_per_block;

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
      int key_count = metal_device_->launch_params.data.max_shaders;
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
    NSString *kernel_name = metal_kernel_pso->function.label;

    /* Enhanced command buffer errors are only available in 11.0+ */
    if (@available(macos 11.0, *)) {
      if (command_buffer.status == MTLCommandBufferStatusError && command_buffer.error != nil) {
        metal_device_->set_error(string("CommandBuffer Failed: ") + [kernel_name UTF8String]);
        NSArray<id<MTLCommandBufferEncoderInfo>> *encoderInfos = [command_buffer.error.userInfo
            valueForKey:MTLCommandBufferEncoderInfoErrorKey];
        if (encoderInfos != nil) {
          for (id<MTLCommandBufferEncoderInfo> encoderInfo : encoderInfos) {
            NSLog(@"%@", encoderInfo);
          }
        }
        id<MTLLogContainer> logs = command_buffer.logs;
        for (id<MTLFunctionLog> log in logs) {
          NSLog(@"%@", log);
        }
      }
      else if (command_buffer.error) {
        metal_device_->set_error(string("CommandBuffer Failed: ") + [kernel_name UTF8String]);
      }
    }
  }];

  if (verbose_tracing_ || is_capturing_) {
    /* Force a sync we've enabled step-by-step verbose tracing or if we're capturing. */
    synchronize();

    /* Show queue counters and dispatch timing. */
    if (verbose_tracing_) {
      if (kernel == DEVICE_KERNEL_INTEGRATOR_RESET) {
        printf(
            "_____________________________________.____________________.______________.___________"
            "______________________________________\n");
      }

      printf("%-40s| %7d threads |%5.2fms | buckets [",
             device_kernel_as_string(kernel),
             work_size,
             last_completion_time_ * 1000.0);
      std::lock_guard<std::recursive_mutex> lock(metal_device_->metal_mem_map_mutex);
      for (auto &it : metal_device_->metal_mem_map) {
        const string c_integrator_queue_counter = "integrator_queue_counter";
        if (it.first->name == c_integrator_queue_counter) {
          /* Workaround "device_copy_from" being protected. */
          struct MyDeviceMemory : device_memory {
            void device_copy_from__IntegratorQueueCounter()
            {
              device_copy_from(0, data_width, 1, sizeof(IntegratorQueueCounter));
            }
          };
          ((MyDeviceMemory *)it.first)->device_copy_from__IntegratorQueueCounter();

          if (IntegratorQueueCounter *queue_counter = (IntegratorQueueCounter *)
                                                          it.first->host_pointer) {
            for (int i = 0; i < DEVICE_KERNEL_INTEGRATOR_NUM; i++)
              printf("%s%d", i == 0 ? "" : ",", int(queue_counter->num_queued[i]));
          }
          break;
        }
      }
      printf("]\n");
    }
  }

  return !(metal_device_->have_error());
}

bool MetalDeviceQueue::synchronize()
{
  if (has_captured_to_disk_ || metal_device_->have_error()) {
    return false;
  }

  close_compute_encoder();
  close_blit_encoder();

  if (mtlCommandBuffer_) {
    scoped_timer timer;

    if (@available(macos 10.14, *)) {
      if (timing_shared_event_) {
        /* For per-kernel timing, add event handlers to measure & accumulate dispatch times. */
        __block double completion_time = 0;
        for (uint64_t i = command_buffer_start_timing_id_; i < timing_shared_event_id_; i++) {
          [timing_shared_event_ notifyListener:shared_event_listener_
                                       atValue:i
                                         block:^(id<MTLSharedEvent> sharedEvent, uint64_t value) {
                                           completion_time = timer.get_time() - completion_time;
                                           last_completion_time_ = completion_time;
                                           for (auto label : command_encoder_labels_) {
                                             if (label.timing_id == value) {
                                               TimingStats &stat = timing_stats_[label.kernel];
                                               stat.num_dispatches++;
                                               stat.total_time += completion_time;
                                               stat.total_work_size += label.work_size;
                                             }
                                           }
                                         }];
        }
      }
    }

    uint64_t shared_event_id_ = this->shared_event_id_++;

    if (@available(macos 10.14, *)) {
      __block dispatch_semaphore_t block_sema = wait_semaphore_;
      [shared_event_ notifyListener:shared_event_listener_
                            atValue:shared_event_id_
                              block:^(id<MTLSharedEvent> sharedEvent, uint64_t value) {
                                dispatch_semaphore_signal(block_sema);
                              }];

      [mtlCommandBuffer_ encodeSignalEvent:shared_event_ value:shared_event_id_];
      [mtlCommandBuffer_ commit];
      dispatch_semaphore_wait(wait_semaphore_, DISPATCH_TIME_FOREVER);
    }

    [mtlCommandBuffer_ release];

    for (const CopyBack &mmem : copy_back_mem_) {
      memcpy((uchar *)mmem.host_pointer, (uchar *)mmem.gpu_mem, mmem.size);
    }
    copy_back_mem_.clear();

    temp_buffer_pool_.process_command_buffer_completion(mtlCommandBuffer_);
    metal_device_->flush_delayed_free_list();

    mtlCommandBuffer_ = nil;
    command_encoder_labels_.clear();
  }

  return !(metal_device_->have_error());
}

void MetalDeviceQueue::zero_to_device(device_memory &mem)
{
  if (metal_device_->have_error()) {
    return;
  }

  assert(mem.type != MEM_GLOBAL && mem.type != MEM_TEXTURE);

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

void MetalDeviceQueue::copy_to_device(device_memory &mem)
{
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

  std::lock_guard<std::recursive_mutex> lock(metal_device_->metal_mem_map_mutex);
  auto result = metal_device_->metal_mem_map.find(&mem);
  if (result != metal_device_->metal_mem_map.end()) {
    if (mem.host_pointer == mem.shared_pointer) {
      return;
    }

    MetalDevice::MetalMem &mmem = *result->second;
    id<MTLBlitCommandEncoder> blitEncoder = get_blit_encoder();

    id<MTLBuffer> buffer = temp_buffer_pool_.get_buffer(mtlDevice_,
                                                        mtlCommandBuffer_,
                                                        mmem.size,
                                                        MTLResourceStorageModeShared,
                                                        mem.host_pointer,
                                                        stats_);

    [blitEncoder copyFromBuffer:buffer
                   sourceOffset:0
                       toBuffer:mmem.mtlBuffer
              destinationOffset:mmem.offset
                           size:mmem.size];
  }
  else {
    metal_device_->mem_copy_to(mem);
  }
}

void MetalDeviceQueue::copy_from_device(device_memory &mem)
{
  if (metal_device_->have_error()) {
    return;
  }

  assert(mem.type != MEM_GLOBAL && mem.type != MEM_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  assert(mem.device_pointer != 0);
  assert(mem.host_pointer != nullptr);

  std::lock_guard<std::recursive_mutex> lock(metal_device_->metal_mem_map_mutex);
  MetalDevice::MetalMem &mmem = *metal_device_->metal_mem_map.at(&mem);
  if (mmem.mtlBuffer) {
    const size_t size = mem.memory_size();

    if (mem.device_pointer) {
      if ([mmem.mtlBuffer storageMode] == MTLStorageModeManaged) {
        id<MTLBlitCommandEncoder> blitEncoder = get_blit_encoder();
        [blitEncoder synchronizeResource:mmem.mtlBuffer];
      }
      if (mem.host_pointer != mmem.hostPtr) {
        if (mtlCommandBuffer_) {
          copy_back_mem_.push_back({mem.host_pointer, mmem.hostPtr, size});
        }
        else {
          memcpy((uchar *)mem.host_pointer, (uchar *)mmem.hostPtr, size);
        }
      }
    }
    else {
      memset((char *)mem.host_pointer, 0, size);
    }
  }
  else {
    metal_device_->mem_copy_from(mem);
  }
}

void MetalDeviceQueue::prepare_resources(DeviceKernel kernel)
{
  std::lock_guard<std::recursive_mutex> lock(metal_device_->metal_mem_map_mutex);

  /* declare resource usage */
  for (auto &it : metal_device_->metal_mem_map) {
    device_memory *mem = it.first;

    MTLResourceUsage usage = MTLResourceUsageRead;
    if (mem->type != MEM_GLOBAL && mem->type != MEM_READ_ONLY && mem->type != MEM_TEXTURE) {
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
  [mtlComputeEncoder_ useResource:metal_device_->texture_bindings_2d usage:MTLResourceUsageRead];
  [mtlComputeEncoder_ useResource:metal_device_->texture_bindings_3d usage:MTLResourceUsageRead];
  [mtlComputeEncoder_ useResource:metal_device_->buffer_bindings_1d usage:MTLResourceUsageRead];
}

id<MTLComputeCommandEncoder> MetalDeviceQueue::get_compute_encoder(DeviceKernel kernel)
{
  bool concurrent = (kernel < DEVICE_KERNEL_INTEGRATOR_NUM);

  if (@available(macos 10.14, *)) {
    if (timing_shared_event_) {
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

    mtlComputeEncoder_ = [mtlCommandBuffer_
        computeCommandEncoderWithDispatchType:concurrent ? MTLDispatchTypeConcurrent :
                                                           MTLDispatchTypeSerial];

    [mtlComputeEncoder_ setLabel:@(device_kernel_as_string(kernel))];

    /* declare usage of MTLBuffers etc */
    prepare_resources(kernel);
  }

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
    command_buffer_start_timing_id_ = timing_shared_event_id_;
  }

  mtlBlitEncoder_ = [mtlCommandBuffer_ blitCommandEncoder];
  return mtlBlitEncoder_;
}

void MetalDeviceQueue::close_compute_encoder()
{
  if (mtlComputeEncoder_) {
    [mtlComputeEncoder_ endEncoding];
    mtlComputeEncoder_ = nil;

    if (@available(macos 10.14, *)) {
      if (timing_shared_event_) {
        [mtlCommandBuffer_ encodeSignalEvent:timing_shared_event_ value:timing_shared_event_id_++];
      }
    }
  }
}

void MetalDeviceQueue::close_blit_encoder()
{
  if (mtlBlitEncoder_) {
    [mtlBlitEncoder_ endEncoding];
    mtlBlitEncoder_ = nil;
  }
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
