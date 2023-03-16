/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#ifndef __KERNEL_ONEAPI__
#  include "kernel/types.h"

#  include "util/string.h"

#  include <ostream>  // NOLINT
#endif

CCL_NAMESPACE_BEGIN

bool device_kernel_has_shading(DeviceKernel kernel);
bool device_kernel_has_intersection(DeviceKernel kernel);

const char *device_kernel_as_string(DeviceKernel kernel);

#ifndef __KERNEL_ONEAPI__
std::ostream &operator<<(std::ostream &os, DeviceKernel kernel);

typedef uint64_t DeviceKernelMask;
string device_kernel_mask_as_string(DeviceKernelMask mask);
#endif

CCL_NAMESPACE_END
