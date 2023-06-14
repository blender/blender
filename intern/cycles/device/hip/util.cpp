/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_HIP

#  include "device/hip/util.h"
#  include "device/hip/device_impl.h"

CCL_NAMESPACE_BEGIN

HIPContextScope::HIPContextScope(HIPDevice *device) : device(device)
{
  hip_device_assert(device, hipCtxPushCurrent(device->hipContext));
}

HIPContextScope::~HIPContextScope()
{
  hip_device_assert(device, hipCtxPopCurrent(NULL));
}

#  ifndef WITH_HIP_DYNLOAD
const char *hipewErrorString(hipError_t result)
{
  /* We can only give error code here without major code duplication, that
   * should be enough since dynamic loading is only being disabled by folks
   * who knows what they're doing anyway.
   *
   * NOTE: Avoid call from several threads.
   */
  static string error;
  error = string_printf("%d", result);
  return error.c_str();
}

const char *hipewCompilerPath()
{
  return CYCLES_HIP_HIPCC_EXECUTABLE;
}

int hipewCompilerVersion()
{
  return (HIP_VERSION / 100) + (HIP_VERSION % 100 / 10);
}
#  endif

CCL_NAMESPACE_END

#endif /* WITH_HIP */
