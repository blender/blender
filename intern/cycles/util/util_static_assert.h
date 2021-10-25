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

#ifndef __UTIL_STATIC_ASSERT_H__
#define __UTIL_STATIC_ASSERT_H__

CCL_NAMESPACE_BEGIN

/* TODO(sergey): In theory CUDA might work with own static assert
 * implementation since it's just pure C++.
 */
#ifndef __KERNEL_GPU__
#  if (__cplusplus > 199711L) || (defined(_MSC_VER) && _MSC_VER >= 1800)
/* C++11 has built-in static_assert() */
#  elif defined(static_assert)
/* Some platforms might have static_assert() defined even tho their
 * C++ support wouldn't be declared to be C++11.
 */
#  else  /* C++11 or MSVC2015 */
template <bool Test> class StaticAssertFailure;
template <> class StaticAssertFailure<true> {};
#    define _static_assert_private_glue_impl(A, B) A ## B
#    define _static_assert_glue(A, B) _static_assert_private_glue_impl(A, B)
#    ifdef __COUNTER__
#      define static_assert(condition, message) \
  enum {_static_assert_glue(q_static_assert_result, __COUNTER__) = sizeof(StaticAssertFailure<!!(condition)>)}  // NOLINT
#    else  /* __COUNTER__ */
#      define static_assert(condition, message) \
  enum {_static_assert_glue(q_static_assert_result, __LINE__) = sizeof(StaticAssertFailure<!!(condition)>)}  // NOLINT
#    endif  /* __COUNTER__ */
#  endif  /* C++11 or MSVC2015 */
#else  /* __KERNEL_GPU__ */
#  ifndef static_assert
#    define static_assert(statement, message)
#  endif
#endif  /* __KERNEL_GPU__ */

/* TODO(sergey): For until C++11 is a bare minimum for us,
 * we do a bit of a trickery to show meaningful message so
 * it's more or less clear what's wrong when building without
 * C++11.
 *
 * The thing here is: our non-C++11 implementation doesn't
 * have a way to print any message after preprocessor
 * substitution so we rely on the message which is passed to
 * static_assert() since that's the only message visible when
 * compilation fails.
 *
 * After C++11 bump it should be possible to glue structure
 * name to the error message,
 */
#  define static_assert_align(st, align) \
  static_assert((sizeof(st) % (align) == 0), "Structure must be strictly aligned")  // NOLINT

CCL_NAMESPACE_END

#endif /* __UTIL_STATIC_ASSERT_H__ */
