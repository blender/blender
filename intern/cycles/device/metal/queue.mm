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
  if (metal_device->have_error()) {
    return false;
  }

  VLOG(3) << "Metal queue launch " << device_kernel_as_string(kernel) << ", work_size "
          << work_size;

  id<MTLComputeCommandEncoder> mtlComputeCommandEncoder = get_compute_encoder(kernel);

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

  /* this relies on IntegratorStateGPU layout being contiguous device_ptrs  */
  const size_t pointer_block_end = offsetof(KernelParamsMetal, __integrator_state) +
                                   sizeof(IntegratorStateGPU);
  for (size_t offset = 0; offset < pointer_block_end; offset += sizeof(device_ptr)) {
    int pointer_index = offset / sizeof(device_ptr);
    MetalDevice::MetalMem *mmem = *(
        MetalDevice::MetalMem **)((uint8_t *)&metal_device->launch_params + offset);
    if (mmem && (mmem->mtlBuffer || mmem->mtlTexture)) {
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

  return !(metal_device->have_error());
}

bool MetalDeviceQueue::synchronize()
{
  if (metal_device->have_error()) {
    return false;
  }

  if (mtlComputeEncoder) {
    close_compute_encoder();
  }
  close_blit_encoder();

  if (mtlCommandBuffer) {
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

    [mtlCommandBuffer release];

    for (const CopyBack &mmem : copy_back_mem) {
      memcpy((uchar *)mmem.host_pointer, (uchar *)mmem.gpu_mem, mmem.size);
    }
    copy_back_mem.clear();

    temp_buffer_pool.process_command_buffer_completion(mtlCommandBuffer);
    metal_device->flush_delayed_free_list();

    mtlCommandBuffer = nil;
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
  }

  mtlBlitEncoder = [mtlCommandBuffer blitCommandEncoder];
  return mtlBlitEncoder;
}

void MetalDeviceQueue::close_compute_encoder()
{
  [mtlComputeEncoder endEncoding];
  mtlComputeEncoder = nil;
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
