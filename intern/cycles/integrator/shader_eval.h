/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/memory.h"

#include "kernel/types.h"

#include "util/function.h"

CCL_NAMESPACE_BEGIN

class Device;
class Progress;

enum ShaderEvalType {
  SHADER_EVAL_DISPLACE,
  SHADER_EVAL_BACKGROUND,
  SHADER_EVAL_CURVE_SHADOW_TRANSPARENCY,
};

/* ShaderEval class performs shader evaluation for background light and displacement. */
class ShaderEval {
 public:
  ShaderEval(Device *device, Progress &progress);

  /* Evaluate shader at points specified by KernelShaderEvalInput and write out
   * RGBA colors to output. */
  bool eval(const ShaderEvalType type,
            const int max_num_inputs,
            const int num_channels,
            const function<int(device_vector<KernelShaderEvalInput> &)> &fill_input,
            const function<void(device_vector<float> &)> &read_output);

 protected:
  bool eval_cpu(Device *device,
                const ShaderEvalType type,
                device_vector<KernelShaderEvalInput> &input,
                device_vector<float> &output,
                const int64_t work_size);
  bool eval_gpu(Device *device,
                const ShaderEvalType type,
                device_vector<KernelShaderEvalInput> &input,
                device_vector<float> &output,
                const int64_t work_size);

  Device *device_;
  Progress &progress_;
};

CCL_NAMESPACE_END
