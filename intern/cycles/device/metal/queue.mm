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
    : DeviceQueue(device), metal_device(device), stats(device->stats)
{
  if (@available(macos 11.0, *)) {
    command_buffer_desc = [[MTLCommandBufferDescriptor alloc] init];
    command_buffer_desc.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
  }

  mtlDevice = device->mtlDevice;
  mtlCommandQueue = [mtlDevice newCommandQueue];

  if (@available(macos 10.14, *)) {
    shared_event = [mtlDevice newSharedEvent];
    shared_event_id = 1;

    /* Shareable event listener */
    event_queue = dispatch_queue_create("com.cycles.metal.event_queue", NULL);
    shared_event_listener = [[MTLSharedEventListener alloc] initWithDispatchQueue:event_queue];
  }

  wait_semaphore = dispatch_semaphore_create(0);

  if (@available(macos 10.14, *)) {
    if (getenv("CYCLES_METAL_PROFILING")) {
      /* Enable per-kernel timing breakdown (shown at end of render). */
      timing_shared_event = [mtlDevice newSharedEvent];
    }
    if (getenv("CYCLES_METAL_DEBUG")) {
      /* Enable very verbose tracing (shows every dispatch). */
      verbose_tracing = true;
    }
    timing_shared_event_id = 1;
  }

  capture_kernel = DeviceKernel(-1);
  if (auto capture_kernel_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_KERNEL")) {
    /* Enable .gputrace capture for the specified DeviceKernel. */
    MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
    mtlCaptureScope = [captureManager newCaptureScopeWithDevice:mtlDevice];
    mtlCaptureScope.label = [NSString stringWithFormat:@"Cycles kernel dispatch"];
    [captureManager setDefaultCaptureScope:mtlCaptureScope];

    capture_dispatch = -1;
    if (auto capture_dispatch_str = getenv("CYCLES_DEBUG_METAL_CAPTURE_DISPATCH")) {
      capture_dispatch = atoi(capture_dispatch_str);
      capture_dispatch_counter = 0;
    }

    capture_kernel = DeviceKernel(atoi(capture_kernel_str));
    printf("Capture kernel: %d = %s\n", capture_kernel, device_kernel_as_string(capture_kernel));

    if (auto capture_url = getenv("CYCLES_DEBUG_METAL_CAPTURE_URL")) {
      if (@available(macos 10.15, *)) {
        if ([captureManager supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {

          MTLCaptureDescriptor *captureDescriptor = [[MTLCaptureDescriptor alloc] init];
          captureDescriptor.captureObject = mtlCaptureScope;
          captureDescriptor.destination = MTLCaptureDestinationGPUTraceDocument;
          captureDescriptor.outputURL = [NSURL fileURLWithPath:@(capture_url)];

          NSError *error;
          if (![captureManager startCaptureWithDescriptor:captureDescriptor error:&error]) {
            NSString *err = [error localizedDescription];
            printf("Start capture failed: %s\n", [err UTF8String]);
          }
          else {
            printf("Capture started (URL: %s)\n", capture_url);
            is_capturing_to_disk = true;
          }
        }
        else {
          printf("Capture to file is not supported\n");
        }
      }
    }
  }
}

MetalDeviceQueue::~MetalDeviceQueue()
{
  /* Tidying up here isn't really practical - we should expect and require the work
   * queue to be empty here. */
  assert(mtlCommandBuffer == nil);
  assert(command_buffers_submitted == command_buffers_completed);

  if (@available(macos 10.14, *)) {
    [shared_event_listener release];
    [shared_event release];
  }

  if (@available(macos 11.0, *)) {
    [command_buffer_desc release];
  }
  if (mtlCommandQueue) {
    [mtlCommandQueue release];
    mtlCommandQueue = nil;
  }

  if (mtlCaptureScope) {
    [mtlCaptureScope release];
  }

  double total_time = 0.0;

  /* Show per-kernel timings, if gathered (see CYCLES_METAL_PROFILING). */
  int64_t num_dispatches = 0;
  for (auto &stat : timing_stats) {
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
      auto &stat = timing_stats[i];
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

int MetalDeviceQueue::num_concurrent_states(const size_t /*state_size*/) const
{
  /* METAL_WIP */
  /* TODO: compute automatically. */
  /* TODO: must have at least num_threads_per_block. */
  int result = 1048576;
  if (metal_device->device_vendor == METAL_GPU_AMD) {
    result *= 2;
  }
  else if (metal_device->device_vendor == METAL_GPU_APPLE) {
    result *= 4;
  }
  return result;
}

int MetalDeviceQueue::num_concurrent_busy_states() const
{
  /* METAL_WIP */
  /* TODO: compute automatically. */
  int result = 65536;
  if (metal_device->device_vendor == METAL_GPU_AMD) {
    result *= 2;
  }
  else if (metal_device->device_vendor == METAL_GPU_APPLE) {
    result *= 4;
  }
  return result;
}

void MetalDeviceQueue::init_execution()
{
  /* Synchronize all textures and memory copies before executing task. */
  metal_device->load_texture_info();

  synchronize();
}

bool MetalDeviceQueue::enqueue(DeviceKernel kernel,
                               const int work_size,
                               DeviceKernelArguments const &args)
{
  if (kernel == capture_kernel) {
    if (capture_dispatch < 0 || capture_dispatch == capture_dispatch_counter) {
      /* Start gputrace capture. */
      if (mtlCommandBuffer) {
        synchronize();
      }
      [mtlCaptureScope beginScope];
      printf("[mtlCaptureScope beginScope]\n");
      is_capturing = true;
    }
    capture_dispatch_counter += 1;
  }

  if (metal_device->have_error()) {
    return false;
  }

  VLOG(3) << "Metal queue launch " << device_kernel_as_string(kernel) << ", work_size "
          << work_size;

  id<MTLComputeCommandEncoder> mtlComputeCommandEncoder = get_compute_encoder(kernel);

  if (timing_shared_event) {
    command_encoder_labels.push_back({kernel, work_size, timing_shared_event_id});
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
  arg_buffer_length += metal_device->mtlAncillaryArgEncoder.encodedLength;
  arg_buffer_length = round_up(arg_buffer_length, metal_device->mtlAncillaryArgEncoder.alignment);

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
  size_t plain_old_launch_data_offset = offsetof(KernelParamsMetal, __integrator_state) +
                                        sizeof(IntegratorStateGPU);
  size_t plain_old_launch_data_size = sizeof(KernelParamsMetal) - plain_old_launch_data_offset;
  memcpy(init_arg_buffer + globals_offsets + plain_old_launch_data_offset,
         (uint8_t *)&metal_device->launch_params + plain_old_launch_data_offset,
         plain_old_launch_data_size);

  /* Allocate an argument buffer. */
  MTLResourceOptions arg_buffer_options = MTLResourceStorageModeManaged;
  if (@available(macOS 11.0, *)) {
    if ([mtlDevice hasUnifiedMemory]) {
      arg_buffer_options = MTLResourceStorageModeShared;
    }
  }

  id<MTLBuffer> arg_buffer = temp_buffer_pool.get_buffer(
      mtlDevice, mtlCommandBuffer, arg_buffer_length, arg_buffer_options, init_arg_buffer, stats);

  /* Encode the pointer "enqueue" arguments */
  bytes_written = 0;
  for (size_t i = 0; i < args.count; i++) {
    size_t size_in_bytes = args.sizes[i];
    bytes_written = round_up(bytes_written, size_in_bytes);
    if (args.types[i] == DeviceKernelArguments::POINTER) {
      [metal_device->mtlBufferKernelParamsEncoder setArgumentBuffer:arg_buffer
                                                             offset:bytes_written];
      if (MetalDevice::MetalMem *mmem = *(MetalDevice::MetalMem **)args.values[i]) {
        [mtlComputeCommandEncoder useResource:mmem->mtlBuffer
                                        usage:MTLResourceUsageRead | MTLResourceUsageWrite];
        [metal_device->mtlBufferKernelParamsEncoder setBuffer:mmem->mtlBuffer offset:0 atIndex:0];
      }
      else {
        if (@available(macos 12.0, *)) {
          [metal_device->mtlBufferKernelParamsEncoder setBuffer:nil offset:0 atIndex:0];
        }
      }
    }
    bytes_written += size_in_bytes;
  }

  /* Encode KernelParamsMetal buffers */
  [metal_device->mtlBufferKernelParamsEncoder setArgumentBuffer:arg_buffer offset:globals_offsets];

  if (verbose_tracing || timing_shared_event || is_capturing) {
    /* Add human-readable labels if we're doing any form of debugging / profiling. */
    mtlComputeCommandEncoder.label = [[NSString alloc]
        initWithFormat:@"Metal queue launch %s, work_size %d",
                       device_kernel_as_string(kernel),
                       work_size];
  }

  /* this relies on IntegratorStateGPU layout being contiguous device_ptrs  */
  const size_t pointer_block_end = offsetof(KernelParamsMetal, __integrator_state) +
                                   sizeof(IntegratorStateGPU);
  for (size_t offset = 0; offset < pointer_block_end; offset += sizeof(device_ptr)) {
    int pointer_index = offset / sizeof(device_ptr);
    MetalDevice::MetalMem *mmem = *(
        MetalDevice::MetalMem **)((uint8_t *)&metal_device->launch_params + offset);
    if (mmem && mmem->mem && (mmem->mtlBuffer || mmem->mtlTexture)) {
      [metal_device->mtlBufferKernelParamsEncoder setBuffer:mmem->mtlBuffer
                                                     offset:0
                                                    atIndex:pointer_index];
    }
    else {
      if (@available(macos 12.0, *)) {
        [metal_device->mtlBufferKernelParamsEncoder setBuffer:nil offset:0 atIndex:pointer_index];
      }
    }
  }
  bytes_written = globals_offsets + sizeof(KernelParamsMetal);

  const MetalKernelPipeline *metal_kernel_pso = MetalDeviceKernels::get_best_pipeline(metal_device,
                                                                                      kernel);
  if (!metal_kernel_pso) {
    metal_device->set_error(
        string_printf("No MetalKernelPipeline for %s\n", device_kernel_as_string(kernel)));
    return false;
  }

  /* Encode ancillaries */
  [metal_device->mtlAncillaryArgEncoder setArgumentBuffer:arg_buffer offset:metal_offsets];
  [metal_device->mtlAncillaryArgEncoder setBuffer:metal_device->texture_bindings_2d
                                           offset:0
                                          atIndex:0];
  [metal_device->mtlAncillaryArgEncoder setBuffer:metal_device->texture_bindings_3d
                                           offset:0
                                          atIndex:1];
  if (@available(macos 12.0, *)) {
    if (metal_device->use_metalrt) {
      if (metal_device->bvhMetalRT) {
        id<MTLAccelerationStructure> accel_struct = metal_device->bvhMetalRT->accel_struct;
        [metal_device->mtlAncillaryArgEncoder setAccelerationStructure:accel_struct atIndex:2];
      }

      for (int table = 0; table < METALRT_TABLE_NUM; table++) {
        if (metal_kernel_pso->intersection_func_table[table]) {
          [metal_kernel_pso->intersection_func_table[table] setBuffer:arg_buffer
                                                               offset:globals_offsets
                                                              atIndex:1];
          [metal_device->mtlAncillaryArgEncoder
              setIntersectionFunctionTable:metal_kernel_pso->intersection_func_table[table]
                                   atIndex:3 + table];
          [mtlComputeCommandEncoder useResource:metal_kernel_pso->intersection_func_table[table]
                                          usage:MTLResourceUsageRead];
        }
        else {
          [metal_device->mtlAncillaryArgEncoder setIntersectionFunctionTable:nil
                                                                     atIndex:3 + table];
        }
      }
    }
    bytes_written = metal_offsets + metal_device->mtlAncillaryArgEncoder.encodedLength;
  }

  if (arg_buffer.storageMode == MTLStorageModeManaged) {
    [arg_buffer didModifyRange:NSMakeRange(0, bytes_written)];
  }

  [mtlComputeCommandEncoder setBuffer:arg_buffer offset:0 atIndex:0];
  [mtlComputeCommandEncoder setBuffer:arg_buffer offset:globals_offsets atIndex:1];
  [mtlComputeCommandEncoder setBuffer:arg_buffer offset:metal_offsets atIndex:2];

  if (metal_device->use_metalrt) {
    if (@available(macos 12.0, *)) {

      auto bvhMetalRT = metal_device->bvhMetalRT;
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
      shared_mem_bytes = round_up((num_threads_per_block + 1) * sizeof(int), 16);
      [mtlComputeCommandEncoder setThreadgroupMemoryLength:shared_mem_bytes atIndex:0];
      break;

    default:
      break;
  }

  MTLSize size_threadgroups_per_dispatch = MTLSizeMake(
      divide_up(work_size, num_threads_per_block), 1, 1);
  MTLSize size_threads_per_threadgroup = MTLSizeMake(num_threads_per_block, 1, 1);
  [mtlComputeCommandEncoder dispatchThreadgroups:size_threadgroups_per_dispatch
                           threadsPerThreadgroup:size_threads_per_threadgroup];

  [mtlCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> command_buffer) {
    NSString *kernel_name = metal_kernel_pso->function.label;

    /* Enhanced command buffer errors are only available in 11.0+ */
    if (@available(macos 11.0, *)) {
      if (command_buffer.status == MTLCommandBufferStatusError && command_buffer.error != nil) {
        printf("CommandBuffer Failed: %s\n", [kernel_name UTF8String]);
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
        printf("CommandBuffer Failed: %s\n", [kernel_name UTF8String]);
      }
    }
  }];

  if (verbose_tracing || is_capturing) {
    /* Force a sync we've enabled step-by-step verbose tracing or if we're capturing. */
    synchronize();

    /* Show queue counters and dispatch timing. */
    if (verbose_tracing) {
      if (kernel == DEVICE_KERNEL_INTEGRATOR_RESET) {
        printf(
            "_____________________________________.____________________.______________.___________"
            "______________________________________\n");
      }

      printf("%-40s| %7d threads |%5.2fms | buckets [",
             device_kernel_as_string(kernel),
             work_size,
             last_completion_time * 1000.0);
      std::lock_guard<std::recursive_mutex> lock(metal_device->metal_mem_map_mutex);
      for (auto &it : metal_device->metal_mem_map) {
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

  return !(metal_device->have_error());
}

bool MetalDeviceQueue::synchronize()
{
  if (has_captured_to_disk || metal_device->have_error()) {
    return false;
  }

  if (mtlComputeEncoder) {
    close_compute_encoder();
  }
  close_blit_encoder();

  if (mtlCommandBuffer) {
    scoped_timer timer;
    if (timing_shared_event) {
      /* For per-kernel timing, add event handlers to measure & accumulate dispatch times. */
      __block double completion_time = 0;
      for (uint64_t i = command_buffer_start_timing_id; i < timing_shared_event_id; i++) {
        [timing_shared_event notifyListener:shared_event_listener
                                    atValue:i
                                      block:^(id<MTLSharedEvent> sharedEvent, uint64_t value) {
                                        completion_time = timer.get_time() - completion_time;
                                        last_completion_time = completion_time;
                                        for (auto label : command_encoder_labels) {
                                          if (label.timing_id == value) {
                                            TimingStats &stat = timing_stats[label.kernel];
                                            stat.num_dispatches++;
                                            stat.total_time += completion_time;
                                            stat.total_work_size += label.work_size;
                                          }
                                        }
                                      }];
      }
    }

    uint64_t shared_event_id = this->shared_event_id++;

    if (@available(macos 10.14, *)) {
      __block dispatch_semaphore_t block_sema = wait_semaphore;
      [shared_event notifyListener:shared_event_listener
                           atValue:shared_event_id
                             block:^(id<MTLSharedEvent> sharedEvent, uint64_t value) {
                               dispatch_semaphore_signal(block_sema);
                             }];

      [mtlCommandBuffer encodeSignalEvent:shared_event value:shared_event_id];
      [mtlCommandBuffer commit];
      dispatch_semaphore_wait(wait_semaphore, DISPATCH_TIME_FOREVER);
    }

    if (is_capturing) {
      [mtlCaptureScope endScope];
      is_capturing = false;
      printf("[mtlCaptureScope endScope]\n");

      if (is_capturing_to_disk) {
        if (@available(macos 10.15, *)) {
          [[MTLCaptureManager sharedCaptureManager] stopCapture];
          has_captured_to_disk = true;
          is_capturing_to_disk = false;
          is_capturing = false;
          printf("Capture stopped\n");
        }
      }
    }

    [mtlCommandBuffer release];

    for (const CopyBack &mmem : copy_back_mem) {
      memcpy((uchar *)mmem.host_pointer, (uchar *)mmem.gpu_mem, mmem.size);
    }
    copy_back_mem.clear();

    temp_buffer_pool.process_command_buffer_completion(mtlCommandBuffer);
    metal_device->flush_delayed_free_list();

    mtlCommandBuffer = nil;
    command_encoder_labels.clear();
  }

  return !(metal_device->have_error());
}

void MetalDeviceQueue::zero_to_device(device_memory &mem)
{
  assert(mem.type != MEM_GLOBAL && mem.type != MEM_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  /* Allocate on demand. */
  if (mem.device_pointer == 0) {
    metal_device->mem_alloc(mem);
  }

  /* Zero memory on device. */
  assert(mem.device_pointer != 0);

  std::lock_guard<std::recursive_mutex> lock(metal_device->metal_mem_map_mutex);
  MetalDevice::MetalMem &mmem = *metal_device->metal_mem_map.at(&mem);
  if (mmem.mtlBuffer) {
    id<MTLBlitCommandEncoder> blitEncoder = get_blit_encoder();
    [blitEncoder fillBuffer:mmem.mtlBuffer range:NSMakeRange(mmem.offset, mmem.size) value:0];
  }
  else {
    metal_device->mem_zero(mem);
  }
}

void MetalDeviceQueue::copy_to_device(device_memory &mem)
{
  if (mem.memory_size() == 0) {
    return;
  }

  /* Allocate on demand. */
  if (mem.device_pointer == 0) {
    metal_device->mem_alloc(mem);
  }

  assert(mem.device_pointer != 0);
  assert(mem.host_pointer != nullptr);

  std::lock_guard<std::recursive_mutex> lock(metal_device->metal_mem_map_mutex);
  auto result = metal_device->metal_mem_map.find(&mem);
  if (result != metal_device->metal_mem_map.end()) {
    if (mem.host_pointer == mem.shared_pointer) {
      return;
    }

    MetalDevice::MetalMem &mmem = *result->second;
    id<MTLBlitCommandEncoder> blitEncoder = get_blit_encoder();

    id<MTLBuffer> buffer = temp_buffer_pool.get_buffer(mtlDevice,
                                                       mtlCommandBuffer,
                                                       mmem.size,
                                                       MTLResourceStorageModeShared,
                                                       mem.host_pointer,
                                                       stats);

    [blitEncoder copyFromBuffer:buffer
                   sourceOffset:0
                       toBuffer:mmem.mtlBuffer
              destinationOffset:mmem.offset
                           size:mmem.size];
  }
  else {
    metal_device->mem_copy_to(mem);
  }
}

void MetalDeviceQueue::copy_from_device(device_memory &mem)
{
  assert(mem.type != MEM_GLOBAL && mem.type != MEM_TEXTURE);

  if (mem.memory_size() == 0) {
    return;
  }

  assert(mem.device_pointer != 0);
  assert(mem.host_pointer != nullptr);

  std::lock_guard<std::recursive_mutex> lock(metal_device->metal_mem_map_mutex);
  MetalDevice::MetalMem &mmem = *metal_device->metal_mem_map.at(&mem);
  if (mmem.mtlBuffer) {
    const size_t size = mem.memory_size();

    if (mem.device_pointer) {
      if ([mmem.mtlBuffer storageMode] == MTLStorageModeManaged) {
        id<MTLBlitCommandEncoder> blitEncoder = get_blit_encoder();
        [blitEncoder synchronizeResource:mmem.mtlBuffer];
      }
      if (mem.host_pointer != mmem.hostPtr) {
        if (mtlCommandBuffer) {
          copy_back_mem.push_back({mem.host_pointer, mmem.hostPtr, size});
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
    metal_device->mem_copy_from(mem);
  }
}

void MetalDeviceQueue::prepare_resources(DeviceKernel kernel)
{
  std::lock_guard<std::recursive_mutex> lock(metal_device->metal_mem_map_mutex);

  /* declare resource usage */
  for (auto &it : metal_device->metal_mem_map) {
    device_memory *mem = it.first;

    MTLResourceUsage usage = MTLResourceUsageRead;
    if (mem->type != MEM_GLOBAL && mem->type != MEM_READ_ONLY && mem->type != MEM_TEXTURE) {
      usage |= MTLResourceUsageWrite;
    }

    if (it.second->mtlBuffer) {
      /* METAL_WIP - use array version (i.e. useResources) */
      [mtlComputeEncoder useResource:it.second->mtlBuffer usage:usage];
    }
    else if (it.second->mtlTexture) {
      /* METAL_WIP - use array version (i.e. useResources) */
      [mtlComputeEncoder useResource:it.second->mtlTexture usage:usage | MTLResourceUsageSample];
    }
  }

  /* ancillaries */
  [mtlComputeEncoder useResource:metal_device->texture_bindings_2d usage:MTLResourceUsageRead];
  [mtlComputeEncoder useResource:metal_device->texture_bindings_3d usage:MTLResourceUsageRead];
}

id<MTLComputeCommandEncoder> MetalDeviceQueue::get_compute_encoder(DeviceKernel kernel)
{
  bool concurrent = (kernel < DEVICE_KERNEL_INTEGRATOR_NUM);

  if (timing_shared_event) {
    /* Close the current encoder to ensure we're able to capture per-encoder timing data. */
    if (mtlComputeEncoder) {
      close_compute_encoder();
    }
  }

  if (@available(macos 10.14, *)) {
    if (mtlComputeEncoder) {
      if (mtlComputeEncoder.dispatchType == concurrent ? MTLDispatchTypeConcurrent :
                                                         MTLDispatchTypeSerial) {
        /* declare usage of MTLBuffers etc */
        prepare_resources(kernel);

        return mtlComputeEncoder;
      }
      close_compute_encoder();
    }

    close_blit_encoder();

    if (!mtlCommandBuffer) {
      mtlCommandBuffer = [mtlCommandQueue commandBuffer];
      [mtlCommandBuffer retain];
    }

    mtlComputeEncoder = [mtlCommandBuffer
        computeCommandEncoderWithDispatchType:concurrent ? MTLDispatchTypeConcurrent :
                                                           MTLDispatchTypeSerial];

    [mtlComputeEncoder setLabel:@(device_kernel_as_string(kernel))];

    /* declare usage of MTLBuffers etc */
    prepare_resources(kernel);
  }

  return mtlComputeEncoder;
}

id<MTLBlitCommandEncoder> MetalDeviceQueue::get_blit_encoder()
{
  if (mtlBlitEncoder) {
    return mtlBlitEncoder;
  }

  if (mtlComputeEncoder) {
    close_compute_encoder();
  }

  if (!mtlCommandBuffer) {
    mtlCommandBuffer = [mtlCommandQueue commandBuffer];
    [mtlCommandBuffer retain];
    command_buffer_start_timing_id = timing_shared_event_id;
  }

  mtlBlitEncoder = [mtlCommandBuffer blitCommandEncoder];
  return mtlBlitEncoder;
}

void MetalDeviceQueue::close_compute_encoder()
{
  [mtlComputeEncoder endEncoding];
  mtlComputeEncoder = nil;

  if (timing_shared_event) {
    [mtlCommandBuffer encodeSignalEvent:timing_shared_event value:timing_shared_event_id++];
  }
}

void MetalDeviceQueue::close_blit_encoder()
{
  if (mtlBlitEncoder) {
    [mtlBlitEncoder endEncoding];
    mtlBlitEncoder = nil;
  }
}

CCL_NAMESPACE_END

#endif /* WITH_METAL */
