/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_SET_H__
#define __UTIL_SET_H__

#include <set>
#include <unordered_set>

#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#  include <iterator>
#endif

CCL_NAMESPACE_BEGIN

using std::set;
using std::unordered_set;

CCL_NAMESPACE_END

#endif /* __UTIL_SET_H__ */
