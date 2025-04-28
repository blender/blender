/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* TBB includes <windows.h>, do it ourselves first so we are sure
 * WIN32_LEAN_AND_MEAN and similar are defined beforehand. */
#ifdef _WIN32
#  include "util/windows.h"
#endif

#include <tbb/concurrent_set.h>

CCL_NAMESPACE_BEGIN

using tbb::concurrent_set;

CCL_NAMESPACE_END
