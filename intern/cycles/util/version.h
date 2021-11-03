/*
 * Copyright 2011-2016 Blender Foundation
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

#ifndef __UTIL_VERSION_H__
#define __UTIL_VERSION_H__

/* Cycles version number */

CCL_NAMESPACE_BEGIN

#define CYCLES_VERSION_MAJOR 1
#define CYCLES_VERSION_MINOR 13
#define CYCLES_VERSION_PATCH 0

#define CYCLES_MAKE_VERSION_STRING2(a, b, c) #a "." #b "." #c
#define CYCLES_MAKE_VERSION_STRING(a, b, c) CYCLES_MAKE_VERSION_STRING2(a, b, c)
#define CYCLES_VERSION_STRING \
  CYCLES_MAKE_VERSION_STRING(CYCLES_VERSION_MAJOR, CYCLES_VERSION_MINOR, CYCLES_VERSION_PATCH)

CCL_NAMESPACE_END

#endif /* __UTIL_VERSION_H__ */
