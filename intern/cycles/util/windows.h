/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef _WIN32

#  ifndef NOGDI
#    define NOGDI
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif

#  include <windows.h>

#endif /* _WIN32 */

CCL_NAMESPACE_BEGIN

bool system_windows_version_at_least(const int major, const int build);

CCL_NAMESPACE_END
