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

#ifndef __UTIL_OPENIMAGEDENOISE_H__
#define __UTIL_OPENIMAGEDENOISE_H__

#ifdef WITH_OPENIMAGEDENOISE
#  include <OpenImageDenoise/oidn.hpp>
#endif

#include "util_system.h"

CCL_NAMESPACE_BEGIN

static inline bool openimagedenoise_supported()
{
#ifdef WITH_OPENIMAGEDENOISE
  return system_cpu_support_sse41();
#else
  return false;
#endif
}

CCL_NAMESPACE_END

#endif /* __UTIL_OPENIMAGEDENOISE_H__ */
