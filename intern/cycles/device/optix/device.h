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

bool device_optix_init();

Device *device_optix_create(const DeviceInfo &info, Stats &stats, Profiler &profiler);

void device_optix_info(const vector<DeviceInfo> &cuda_devices, vector<DeviceInfo> &devices);

CCL_NAMESPACE_END
