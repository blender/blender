/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPENIMAGEDENOISE
#  include <OpenImageDenoise/oidn.hpp>  // IWYU pragma: export
#endif

#include "util/system.h"  // IWYU pragma: keep

CCL_NAMESPACE_BEGIN

static inline bool openimagedenoise_supported()
{
#ifdef WITH_OPENIMAGEDENOISE
#  if defined(__APPLE__)
  /* Always supported through Accelerate framework BNNS. */
  return true;
#  elif defined(__aarch64__) || defined(_M_ARM64)
  /* OIDN 2.2 and up supports ARM64 on Windows and Linux. */
  return true;
#  else
  return system_cpu_support_sse42();
#  endif
#else
  return false;
#endif
}

CCL_NAMESPACE_END
