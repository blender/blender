/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/cpu/device.h"
#include "device/cpu/device_impl.h"
#include "device/device.h"

/* Used for `info.denoisers`. */
/* TODO(sergey): The denoisers are probably to be moved completely out of the device into their
 * own class. But until then keep API consistent with how it used to work before. */
#include "util/guiding.h"
#include "util/openimagedenoise.h"

CCL_NAMESPACE_BEGIN

unique_ptr<Device> device_cpu_create(const DeviceInfo &info,
                                     Stats &stats,
                                     Profiler &profiler,
                                     bool headless)
{
  return make_unique<CPUDevice>(info, stats, profiler, headless);
}

void device_cpu_info(vector<DeviceInfo> &devices)
{
  DeviceInfo info;

  info.type = DEVICE_CPU;
  info.description = system_cpu_brand_string();
  info.id = "CPU";
  info.num = 0;
  info.has_osl = true;
  info.has_nanovdb = true;
  info.has_profiling = true;
  if (guiding_supported()) {
    info.has_guiding = true;
  }
  else {
    info.has_guiding = false;
  }
  if (openimagedenoise_supported()) {
    info.denoisers |= DENOISER_OPENIMAGEDENOISE;
  }

  devices.insert(devices.begin(), info);
}

string device_cpu_capabilities()
{
  return system_cpu_support_avx2() ? "AVX2" : "";
}

CCL_NAMESPACE_END
