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

#include "util/string.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceInfo;
class Profiler;
class Stats;

Device *device_cpu_create(const DeviceInfo &info, Stats &stats, Profiler &profiler);

void device_cpu_info(vector<DeviceInfo> &devices);

string device_cpu_capabilities();

CCL_NAMESPACE_END
