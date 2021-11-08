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

#pragma once

#include "kernel/types.h"

#include "util/string.h"

#include <ostream>  // NOLINT

CCL_NAMESPACE_BEGIN

const char *device_kernel_as_string(DeviceKernel kernel);
std::ostream &operator<<(std::ostream &os, DeviceKernel kernel);

typedef uint64_t DeviceKernelMask;
string device_kernel_mask_as_string(DeviceKernelMask mask);

CCL_NAMESPACE_END
