/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types.h"

CCL_NAMESPACE_BEGIN

class GraphicsInteropBuffer;

/* Device-side graphics interoperability support.
 *
 * Takes care of holding all the handlers needed by the device to implement interoperability with
 * the graphics library. */
class DeviceGraphicsInterop {
 public:
  DeviceGraphicsInterop() = default;
  virtual ~DeviceGraphicsInterop() = default;

  /* Update this device-side graphics interoperability buffer with the given destination
   * resource information. */
  virtual void set_buffer(GraphicsInteropBuffer &interop_buffer) = 0;

  virtual device_ptr map() = 0;
  virtual void unmap() = 0;
};

CCL_NAMESPACE_END
