/*
 * Copyright 2011-2013 Blender Foundation
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

#ifdef WITH_CUDA

#  include "device/cuda/util.h"
#  include "device/cuda/device_impl.h"

CCL_NAMESPACE_BEGIN

CUDAContextScope::CUDAContextScope(CUDADevice *device) : device(device)
{
  cuda_device_assert(device, cuCtxPushCurrent(device->cuContext));
}

CUDAContextScope::~CUDAContextScope()
{
  cuda_device_assert(device, cuCtxPopCurrent(NULL));
}

#  ifndef WITH_CUDA_DYNLOAD
const char *cuewErrorString(CUresult result)
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

const char *cuewCompilerPath()
{
  return CYCLES_CUDA_NVCC_EXECUTABLE;
}

int cuewCompilerVersion()
{
  return (CUDA_VERSION / 100) + (CUDA_VERSION % 100 / 10);
}
#  endif

CCL_NAMESPACE_END

#endif /* WITH_CUDA */
