/*
 * Copyright 2011-2013 Blender Foundation
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

#include "device/cpu/device.h"
#include "device/cpu/device_impl.h"

/* Used for `info.denoisers`. */
/* TODO(sergey): The denoisers are probably to be moved completely out of the device into their
 * own class. But until then keep API consistent with how it used to work before. */
#include "util/openimagedenoise.h"

CCL_NAMESPACE_BEGIN

Device *device_cpu_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
  return new CPUDevice(info, stats, profiler);
}

void device_cpu_info(vector<DeviceInfo> &devices)
{
  DeviceInfo info;

  info.type = DEVICE_CPU;
  info.description = system_cpu_brand_string();
  info.id = "CPU";
  info.num = 0;
  info.has_osl = true;
  info.has_half_images = true;
  info.has_nanovdb = true;
  info.has_profiling = true;
  if (openimagedenoise_supported()) {
    info.denoisers |= DENOISER_OPENIMAGEDENOISE;
  }

  devices.insert(devices.begin(), info);
}

string device_cpu_capabilities()
{
  string capabilities = "";
  capabilities += system_cpu_support_sse2() ? "SSE2 " : "";
  capabilities += system_cpu_support_sse3() ? "SSE3 " : "";
  capabilities += system_cpu_support_sse41() ? "SSE41 " : "";
  capabilities += system_cpu_support_avx() ? "AVX " : "";
  capabilities += system_cpu_support_avx2() ? "AVX2" : "";
  if (capabilities[capabilities.size() - 1] == ' ')
    capabilities.resize(capabilities.size() - 1);
  return capabilities;
}

CCL_NAMESPACE_END
