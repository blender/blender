/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_OPENIMAGEDENOISE

#  include "BLI_span.hh"

#  include "COM_context.hh"

#  include <OpenImageDenoise/oidn.hpp>

namespace blender::compositor {

/* Create an appropriate device based on the device preferences in the given context. Special
 * attention is given to GPU devices, as multiple GPUs could exist, so the same GPU device used in
 * the active GPU context is chosen. If no GPU context is active, OIDN chooses the best device,
 * which is typically the fastest in the system. Such device selection makes execution more
 * predictable and allows interoperability across APIs. */
oidn::DeviceRef create_oidn_device(const Context &context);

/* Creates a buffer on the given device that represents the given image. If the device can access
 * host-side data, the returned buffer is a simple wrapper around the data, otherwise, the data is
 * copied to a device-only buffer. It is thus expected that the given image data will outlive the
 * returned buffer. */
oidn::BufferRef create_oidn_buffer(const oidn::DeviceRef &device, const MutableSpan<float> image);

}  // namespace blender::compositor

#endif
