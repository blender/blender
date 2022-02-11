/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "integrator/denoiser_optix.h"

#include "device/denoise.h"
#include "device/device.h"

CCL_NAMESPACE_BEGIN

OptiXDenoiser::OptiXDenoiser(Device *path_trace_device, const DenoiseParams &params)
    : DeviceDenoiser(path_trace_device, params)
{
}

uint OptiXDenoiser::get_device_type_mask() const
{
  return DEVICE_MASK_OPTIX;
}

CCL_NAMESPACE_END
