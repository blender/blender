/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "integrator/shader_eval.h"

#include "device/device.h"
#include "device/queue.h"

#include "device/cpu/kernel.h"
#include "device/cpu/kernel_thread_globals.h"

#include "util/log.h"
#include "util/progress.h"
#include "util/tbb.h"

CCL_NAMESPACE_BEGIN

ShaderEval::ShaderEval(Device *device, Progress &progress) : device_(device), progress_(progress)
{
  DCHECK_NE(device_, nullptr);
}

bool ShaderEval::eval(const ShaderEvalType type,
                      const int max_num_inputs,
                      const int num_channels,
                      const function<int(device_vector<KernelShaderEvalInput> &)> &fill_input,
                      const function<void(device_vector<float> &)> &read_output)
{
  bool first_device = true;
  bool success = true;

  device_->foreach_device([&](Device *device) {
    if (!first_device) {
      VLOG_WORK << "Multi-devices are not yet fully implemented, will evaluate shader on a "
                   "single device.";
      return;
    }
    first_device = false;

    device_vector<KernelShaderEvalInput> input(device, "ShaderEval input", MEM_READ_ONLY);
    device_vector<float> output(device, "ShaderEval output", MEM_READ_WRITE);

    /* Allocate and copy device buffers. */
    DCHECK_EQ(input.device, device);
    DCHECK_EQ(output.device, device);
    DCHECK_LE(output.size(), input.size());

    input.alloc(max_num_inputs);
    int num_points = fill_input(input);
    if (num_points == 0) {
      return;
    }

    input.copy_to_device();
    output.alloc(num_points * num_channels);
    output.zero_to_device();

    /* Evaluate on CPU or GPU. */
    success = (device->info.type == DEVICE_CPU) ?
                  eval_cpu(device, type, input, output, num_points) :
                  eval_gpu(device, type, input, output, num_points);

    /* Copy data back from device if not canceled. */
    if (success) {
      output.copy_from_device(0, 1, output.size());
      read_output(output);
    }

    input.free();
    output.free();
  });

  return success;
}

bool ShaderEval::eval_cpu(Device *device,
                          const ShaderEvalType type,
                          device_vector<KernelShaderEvalInput> &input,
                          device_vector<float> &output,
                          const int64_t work_size)
{
  vector<CPUKernelThreadGlobals> kernel_thread_globals;
  device->get_cpu_kernel_thread_globals(kernel_thread_globals);

  /* Find required kernel function. */
  const CPUKernels &kernels = Device::get_cpu_kernels();

  /* Simple parallel_for over all work items. */
  KernelShaderEvalInput *input_data = input.data();
  float *output_data = output.data();
  bool success = true;

  tbb::task_arena local_arena(device->info.cpu_threads);
  local_arena.execute([&]() {
    parallel_for(int64_t(0), work_size, [&](int64_t work_index) {
      /* TODO: is this fast enough? */
      if (progress_.get_cancel()) {
        success = false;
        return;
      }

      const int thread_index = tbb::this_task_arena::current_thread_index();
      const KernelGlobalsCPU *kg = &kernel_thread_globals[thread_index];

      switch (type) {
        case SHADER_EVAL_DISPLACE:
          kernels.shader_eval_displace(kg, input_data, output_data, work_index);
          break;
        case SHADER_EVAL_BACKGROUND:
          kernels.shader_eval_background(kg, input_data, output_data, work_index);
          break;
        case SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY:
          kernels.shader_eval_curve_shadow_transparency(kg, input_data, output_data, work_index);
          break;
      }
    });
  });

  return success;
}

bool ShaderEval::eval_gpu(Device *device,
                          const ShaderEvalType type,
                          device_vector<KernelShaderEvalInput> &input,
                          device_vector<float> &output,
                          const int64_t work_size)
{
  /* Find required kernel function. */
  DeviceKernel kernel;
  switch (type) {
    case SHADER_EVAL_DISPLACE:
      kernel = DEVICE_KERNEL_SHADER_EVAL_DISPLACE;
      break;
    case SHADER_EVAL_BACKGROUND:
      kernel = DEVICE_KERNEL_SHADER_EVAL_BACKGROUND;
      break;
    case SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY:
      kernel = DEVICE_KERNEL_SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY;
      break;
  };

  /* Create device queue. */
  unique_ptr<DeviceQueue> queue = device->gpu_queue_create();
  queue->init_execution();

  /* Execute work on GPU in chunk, so we can cancel.
   * TODO: query appropriate size from device. */
  const int32_t chunk_size = 65536;

  device_ptr d_input = input.device_pointer;
  device_ptr d_output = output.device_pointer;

  assert(work_size <= 0x7fffffff);
  for (int32_t d_offset = 0; d_offset < int32_t(work_size); d_offset += chunk_size) {
    int32_t d_work_size = std::min(chunk_size, int32_t(work_size) - d_offset);

    DeviceKernelArguments args(&d_input, &d_output, &d_offset, &d_work_size);

    queue->enqueue(kernel, d_work_size, args);
    queue->synchronize();

    if (progress_.get_cancel()) {
      return false;
    }
  }

  return true;
}

CCL_NAMESPACE_END
