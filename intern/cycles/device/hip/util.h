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

#ifdef WITH_HIP

#  ifdef WITH_HIP_DYNLOAD
#    include "hipew.h"
#  endif

CCL_NAMESPACE_BEGIN

class HIPDevice;

/* Utility to push/pop HIP context. */
class HIPContextScope {
 public:
  HIPContextScope(HIPDevice *device);
  ~HIPContextScope();

 private:
  HIPDevice *device;
};

/* Utility for checking return values of HIP function calls. */
#  define hip_device_assert(hip_device, stmt) \
    { \
      hipError_t result = stmt; \
      if (result != hipSuccess) { \
        const char *name = hipewErrorString(result); \
        hip_device->set_error( \
            string_printf("%s in %s (%s:%d)", name, #stmt, __FILE__, __LINE__)); \
      } \
    } \
    (void)0

#  define hip_assert(stmt) hip_device_assert(this, stmt)

#  ifndef WITH_HIP_DYNLOAD
/* Transparently implement some functions, so majority of the file does not need
 * to worry about difference between dynamically loaded and linked HIP at all. */
const char *hipewErrorString(hipError_t result);
const char *hipewCompilerPath();
int hipewCompilerVersion();
#  endif /* WITH_HIP_DYNLOAD */

static inline bool hipSupportsDevice(const int hipDevId)
{
  int major, minor;
  hipDeviceGetAttribute(&major, hipDeviceAttributeComputeCapabilityMajor, hipDevId);
  hipDeviceGetAttribute(&minor, hipDeviceAttributeComputeCapabilityMinor, hipDevId);

  return (major > 10) || (major == 10 && minor >= 3);
}

CCL_NAMESPACE_END

#endif /* WITH_HIP */
