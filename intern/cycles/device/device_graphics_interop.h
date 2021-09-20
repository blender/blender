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

#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* Information about interoperability destination.
 * Is provided by the GPUDisplay. */
class DeviceGraphicsInteropDestination {
 public:
  /* Dimensions of the buffer, in pixels. */
  int buffer_width = 0;
  int buffer_height = 0;

  /* OpenGL pixel buffer object. */
  int opengl_pbo_id = 0;

  /* Clear the entire destination before doing partial write to it. */
  bool need_clear = false;
};

/* Device-side graphics interoperability support.
 *
 * Takes care of holding all the handlers needed by the device to implement interoperability with
 * the graphics library. */
class DeviceGraphicsInterop {
 public:
  DeviceGraphicsInterop() = default;
  virtual ~DeviceGraphicsInterop() = default;

  /* Update this device-side graphics interoperability object with the given destination resource
   * information. */
  virtual void set_destination(const DeviceGraphicsInteropDestination &destination) = 0;

  virtual device_ptr map() = 0;
  virtual void unmap() = 0;
};

CCL_NAMESPACE_END
