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

#ifndef __UTIL_FOREACH_H__
#define __UTIL_FOREACH_H__

/* Use Boost to get nice foreach() loops for STL data structures. */

#if (__cplusplus > 199711L) || (defined(_MSC_VER) && _MSC_VER >= 1800)
#  define foreach(x, y) for(x : y)
#else
#  include <boost/foreach.hpp>
#  define foreach BOOST_FOREACH
#endif

#endif /* __UTIL_FOREACH_H__ */
