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

/* clang-format off */

/* #define static_assert triggers a bug in some clang-format versions, disable
 * format for entire file to keep results consistent. */

#ifndef __UTIL_STATIC_ASSERT_H__
#define __UTIL_STATIC_ASSERT_H__

CCL_NAMESPACE_BEGIN

#if defined(__KERNEL_OPENCL__) || defined(CYCLES_CUBIN_CC)
#  define static_assert(statement, message)
#endif /* __KERNEL_OPENCL__ */

#define static_assert_align(st, align) \
  static_assert((sizeof(st) % (align) == 0), "Structure must be strictly aligned")  // NOLINT

CCL_NAMESPACE_END

#endif /* __UTIL_STATIC_ASSERT_H__ */
