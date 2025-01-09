/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/dummy/device.h"
#include "device/device.h"
#include "device/queue.h"

CCL_NAMESPACE_BEGIN

/* Dummy device for when creating an appropriate rendering device fails. */

class DummyDevice : public Device {
 public:
  DummyDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_, bool headless_)
      : Device(info_, stats_, profiler_, headless_)
  {
    error_msg = info.error_msg;
  }

  ~DummyDevice() override = default;

  BVHLayoutMask get_bvh_layout_mask(uint /*kernel_features*/) const override
  {
    return 0;
  }

  void mem_alloc(device_memory & /*mem*/) override {}

  void mem_copy_to(device_memory & /*mem*/) override {}

  void mem_move_to_host(device_memory & /*mem*/) override {}

  void mem_copy_from(
      device_memory & /*mem*/, size_t /*y*/, size_t /*w*/, size_t /*h*/, size_t /*elem*/) override
  {
  }

  void mem_zero(device_memory & /*mem*/) override {}

  void mem_free(device_memory & /*mem*/) override {}

  void const_copy_to(const char * /*name*/, void * /*host*/, size_t /*size*/) override {}
};

unique_ptr<Device> device_dummy_create(const DeviceInfo &info,
                                       Stats &stats,
                                       Profiler &profiler,
                                       bool headless)
{
  return make_unique<DummyDevice>(info, stats, profiler, headless);
}

CCL_NAMESPACE_END
