/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/string.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceInfo;
class Profiler;
class Stats;

bool device_cuda_init();

Device *device_cuda_create(const DeviceInfo &info, Stats &stats, Profiler &profiler);

void device_cuda_info(vector<DeviceInfo> &devices);

string device_cuda_capabilities();

CCL_NAMESPACE_END
