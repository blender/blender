/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPTIX

#  include "device/cuda/queue.h"

CCL_NAMESPACE_BEGIN

class OptiXDevice;

/* Base class for CUDA queues. */
class OptiXDeviceQueue : public CUDADeviceQueue {
 public:
  OptiXDeviceQueue(OptiXDevice *device);

  virtual void init_execution() override;

  virtual bool enqueue(DeviceKernel kernel,
                       const int work_size,
                       DeviceKernelArguments const &args) override;
};

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
