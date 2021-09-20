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

#ifdef WITH_CUDA

#  ifdef WITH_CUDA_DYNLOAD
#    include "cuew.h"
#  else
#    include <cuda.h>
#  endif

CCL_NAMESPACE_BEGIN

class CUDADevice;

/* Utility to push/pop CUDA context. */
class CUDAContextScope {
 public:
  CUDAContextScope(CUDADevice *device);
  ~CUDAContextScope();

 private:
  CUDADevice *device;
};

/* Utility for checking return values of CUDA function calls. */
#  define cuda_device_assert(cuda_device, stmt) \
    { \
      CUresult result = stmt; \
      if (result != CUDA_SUCCESS) { \
        const char *name = cuewErrorString(result); \
        cuda_device->set_error( \
            string_printf("%s in %s (%s:%d)", name, #stmt, __FILE__, __LINE__)); \
      } \
    } \
    (void)0

#  define cuda_assert(stmt) cuda_device_assert(this, stmt)

#  ifndef WITH_CUDA_DYNLOAD
/* Transparently implement some functions, so majority of the file does not need
 * to worry about difference between dynamically loaded and linked CUDA at all. */
const char *cuewErrorString(CUresult result);
const char *cuewCompilerPath();
int cuewCompilerVersion();
#  endif /* WITH_CUDA_DYNLOAD */

CCL_NAMESPACE_END

#endif /* WITH_CUDA */
