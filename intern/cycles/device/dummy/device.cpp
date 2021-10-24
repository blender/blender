/*
 * Copyright 2011-2020 Blender Foundation
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

  ~DummyDevice()
  {
  }

  virtual BVHLayoutMask get_bvh_layout_mask() const override
  {
    return 0;
  }

  virtual void mem_alloc(device_memory &) override
  {
  }

  virtual void mem_copy_to(device_memory &) override
  {
  }

  virtual void mem_copy_from(device_memory &, size_t, size_t, size_t, size_t) override
  {
  }

  virtual void mem_zero(device_memory &) override
  {
  }

  virtual void mem_free(device_memory &) override
  {
  }

  virtual void const_copy_to(const char *, void *, size_t) override
  {
  }
};

Device *device_dummy_create(const DeviceInfo &info, Stats &stats, Profiler &profiler)
{
  return new DummyDevice(info, stats, profiler);
}

CCL_NAMESPACE_END
