/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_OPENIMAGEDENOISE_H__
#define __UTIL_OPENIMAGEDENOISE_H__

#ifdef WITH_OPENIMAGEDENOISE
#  include <OpenImageDenoise/oidn.hpp>
#endif

#include "util/system.h"

CCL_NAMESPACE_BEGIN

static inline bool openimagedenoise_supported()
{
#ifdef WITH_OPENIMAGEDENOISE
#  ifdef __APPLE__
  /* Always supported through Accelerate framework BNNS. */
  return true;
#  else
  return system_cpu_support_sse42();
#  endif
#else
  return false;
#endif
}

CCL_NAMESPACE_END

#endif /* __UTIL_OPENIMAGEDENOISE_H__ */
