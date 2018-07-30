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
#include <unordered_set>

#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#  include <iterator>
#endif

CCL_NAMESPACE_BEGIN

using std::set;
using std::unordered_set;

CCL_NAMESPACE_END

#endif /* __UTIL_SET_H__ */
