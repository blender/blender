/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __KERNEL_ONEAPI__
#  include "kernel/types.h"

#  include "util/string.h"

#  include <bitset>
#  include <iosfwd>
#endif

CCL_NAMESPACE_BEGIN

/* DeviceKernel */

bool device_kernel_has_shading(DeviceKernel kernel);
bool device_kernel_has_intersection(DeviceKernel kernel);

const char *device_kernel_as_string(DeviceKernel kernel);

#ifndef __KERNEL_ONEAPI__
std::ostream &operator<<(std::ostream &os, DeviceKernel kernel);

/* DeviceKernelMask */

struct DeviceKernelMask : public std::bitset<DEVICE_KERNEL_NUM> {
  bool operator<(const DeviceKernelMask &other) const;
};

string device_kernel_mask_as_string(DeviceKernelMask mask);
#endif

CCL_NAMESPACE_END
