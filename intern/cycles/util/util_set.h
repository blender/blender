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

#ifndef __UTIL_SET_H__
#define __UTIL_SET_H__

#include <set>
#if (__cplusplus > 199711L) || (defined(_MSC_VER) && _MSC_VER >= 1800)
#  include <unordered_set>
#else
#  if defined(CYCLES_TR1_UNORDERED_MAP)
#    include <tr1/unordered_set>
#  endif
#  if defined(CYCLES_STD_UNORDERED_MAP) || \
      defined(CYCLES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
#    include <unordered_set>
#  endif
#  if !defined(CYCLES_NO_UNORDERED_MAP) && \
      !defined(CYCLES_TR1_UNORDERED_MAP) && \
      !defined(CYCLES_STD_UNORDERED_MAP) && \
      !defined(CYCLES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
#    error One of: CYCLES_NO_UNORDERED_MAP, CYCLES_TR1_UNORDERED_MAP,\
 CYCLES_STD_UNORDERED_MAP, CYCLES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE must be defined!  // NOLINT
#  endif
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#  include <iterator>
#endif

CCL_NAMESPACE_BEGIN

using std::set;
#if (__cplusplus > 199711L) || (defined(_MSC_VER) && _MSC_VER >= 1800)
using std::unordered_set;
#else
#  if defined(CYCLES_NO_UNORDERED_MAP)
typedef std::set unordered_set;
#  endif
#  if defined(CYCLES_TR1_UNORDERED_MAP) || defined(CYCLES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
using std::tr1::unordered_set;
#  endif
#  if defined(CYCLES_STD_UNORDERED_MAP)
using std::unordered_set;
#  endif
#endif
CCL_NAMESPACE_END

#endif /* __UTIL_SET_H__ */
