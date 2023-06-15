/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
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

bool device_metal_init();

Device *device_metal_create(const DeviceInfo &info, Stats &stats, Profiler &profiler);

void device_metal_info(vector<DeviceInfo> &devices);

string device_metal_capabilities();

CCL_NAMESPACE_END
