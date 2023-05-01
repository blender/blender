/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "device/dummy/device.h"

#include "device/device.h"
#include "device/queue.h"

CCL_NAMESPACE_BEGIN

/* Dummy device for when creating an appropriate rendering device fails. */

class DummyDevice : public Device {
 public:
  DummyDevice(const DeviceInfo &info_, Stats &stats_, Profiler &profiler_)
      : Device(info_, stats_, profiler_)
  {
    error_msg = info.error_msg;
  }

  ~DummyDevice() {}

  virtual BVHLayoutMask get_bvh_layout_mask(uint /*kernel_features*/) const override
  {
    return 0;
  }

  virtual void mem_alloc(device_memory &) override {}

  virtual void mem_copy_to(device_memory &) override {}

  virtual void mem_copy_from(device_memory &, size_t, size_t, size_t, size_t) override {}

  virtual void mem_zero(device_memory &) override {}

  virtual void mem_free(device_memory &) override {}

  virtual void const_copy_to(const char *, void *, size_t) override {}
};

Device *device_dummy_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
  return new DummyDevice(info, stats, profiler);
}

CCL_NAMESPACE_END
