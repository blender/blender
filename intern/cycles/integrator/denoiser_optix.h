/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "integrator/denoiser_device.h"

CCL_NAMESPACE_BEGIN

class OptiXDenoiser : public DeviceDenoiser {
 public:
  OptiXDenoiser(Device *path_trace_device, const DenoiseParams &params);

 protected:
  virtual uint get_device_type_mask() const override;
};

CCL_NAMESPACE_END
