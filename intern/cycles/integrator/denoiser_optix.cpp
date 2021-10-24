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
