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

bool device_hip_init();

Device *device_hip_create(const DeviceInfo &info, Stats &stats, Profiler &profiler);

void device_hip_info(vector<DeviceInfo> &devices);

string device_hip_capabilities();

CCL_NAMESPACE_END
