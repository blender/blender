/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENIMAGEDENOISE

#  include <cstdint>

#  include "BLI_array.hh"
#  include "BLI_assert.h"
#  include "BLI_span.hh"

#  include "GPU_platform.hh"

#  include "COM_context.hh"
#  include "COM_utilities_oidn.hh"

#  include <OpenImageDenoise/oidn.hpp>

namespace blender::compositor {

static oidn::DeviceRef create_oidn_gpu_device(const Context &context)
{

  /* The compositor uses CPU execution and does not have an active GPU context or device, so let
   * OIDN select the best device, which is typically the fastest. */
  if (!context.use_gpu()) {
    return oidn::newDevice(oidn::DeviceType::Default);
  }

  /* Try to select the device that is used by the currently active GPU context. First, try to
   * select the device based on the device LUID. */
  const Span<uint8_t> platform_luid = GPU_platform_luid();
  const uint32_t platform_luid_node_mask = GPU_platform_luid_node_mask();
  const int devices_count = oidn::getNumPhysicalDevices();
  for (int i = 0; i < devices_count; i++) {
    oidn::PhysicalDeviceRef physical_device(i);
    if (!physical_device.get<bool>("luidSupported")) {
      continue;
    }

    oidn::LUID luid = physical_device.get<oidn::LUID>("luid");
    uint32_t luid_node_mask = physical_device.get<uint32_t>("nodeMask");
    if (platform_luid == Span<uint8_t>(luid.bytes, sizeof(luid.bytes)) &&
        platform_luid_node_mask == luid_node_mask)
    {
      return physical_device.newDevice();
    }
  }

  /* If LUID matching was unsuccessful, try to match based on UUID. We rely on multiple selection
   * methods because not all platforms support both UUID and LUID, but all platforms support either
   * one of them. UUID supports all except MacOS Metal, while LUID only supports Windows and MacOS
   * Metal. Note that we prefer LUID as a first match because UUID is unreliable in practice as
   * some implementations report the same UUID for different devices in the same machine. */
  const Span<uint8_t> platform_uuid = GPU_platform_uuid();
  for (int i = 0; i < devices_count; i++) {
    oidn::PhysicalDeviceRef physical_device(i);
    if (!physical_device.get<bool>("uuidSupported")) {
      continue;
    }

    oidn::UUID uuid = physical_device.get<oidn::UUID>("uuid");
    if (platform_uuid == Span<uint8_t>(uuid.bytes, sizeof(uuid.bytes))) {
      return physical_device.newDevice();
    }
  }

  return oidn::newDevice(oidn::DeviceType::Default);
}

oidn::DeviceRef create_oidn_device(const Context &context)
{
  const eCompositorDenoiseDevice preferred_denoise_device = static_cast<eCompositorDenoiseDevice>(
      context.get_render_data().compositor_denoise_device);

  switch (preferred_denoise_device) {
    case SCE_COMPOSITOR_DENOISE_DEVICE_CPU:
      return oidn::newDevice(oidn::DeviceType::CPU);
    case SCE_COMPOSITOR_DENOISE_DEVICE_GPU:
      return create_oidn_gpu_device(context);
    case SCE_COMPOSITOR_DENOISE_DEVICE_AUTO:
      if (!context.use_gpu()) {
        return oidn::newDevice(oidn::DeviceType::CPU);
      }
      else {
        return create_oidn_gpu_device(context);
      }
  }

  BLI_assert_unreachable();
  return oidn::newDevice(oidn::DeviceType::Default);
}

oidn::BufferRef create_oidn_buffer(const oidn::DeviceRef &device, const MutableSpan<float> image)
{
  /* The device can access host-side data, so create a shared buffer that wraps the data. */
  const bool can_access_host_memory = device.get<bool>("systemMemorySupported");
  if (can_access_host_memory) {
    return device.newBuffer(image.data(), image.size_in_bytes());
  }

  /* Otherwise, create a device-only buffer and copy the data to it. */
  oidn::BufferRef buffer = device.newBuffer(image.size_in_bytes(), oidn::Storage::Device);
  buffer.write(0, image.size_in_bytes(), image.data());
  return buffer;
}

}  // namespace blender::compositor

#endif
