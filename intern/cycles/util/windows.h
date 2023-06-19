/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_WINDOWS_H__
#define __UTIL_WINDOWS_H__

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

bool system_windows_version_at_least(int major, int build);

CCL_NAMESPACE_END

#endif /* __UTIL_WINDOWS_H__ */
