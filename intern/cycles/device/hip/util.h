/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cstring>
#include <string>

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

static std::string hipDeviceArch(const int hipDevId)
{
  hipDeviceProp_t props;
  hipGetDeviceProperties(&props, hipDevId);
  const char *arch = strtok(props.gcnArchName, ":");
  return (arch == nullptr) ? props.gcnArchName : arch;
}

static inline bool hipSupportsDevice(const int hipDevId)
{
  int major, minor;
  hipDeviceGetAttribute(&major, hipDeviceAttributeComputeCapabilityMajor, hipDevId);
  hipDeviceGetAttribute(&minor, hipDeviceAttributeComputeCapabilityMinor, hipDevId);

  return (major >= 9);
}

static inline bool hipSupportsDeviceOIDN(const int hipDevId)
{
  /* Matches HIPDevice::getArch in HIP. */
  const std::string arch = hipDeviceArch(hipDevId);
  return (arch == "gfx1030" || arch == "gfx1100" || arch == "gfx1101" || arch == "gfx1102");
}

CCL_NAMESPACE_END

#endif /* WITH_HIP */
