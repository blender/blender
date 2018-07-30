
/*
 * Copyright 2011-2017 Blender Foundation
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

#ifndef __UTIL_DEFINES_H__
#define __UTIL_DEFINES_H__

/* Bitness */

#if defined(__ppc64__) || defined(__PPC64__) || defined(__x86_64__) || defined(__ia64__) || defined(_M_X64)
#  define __KERNEL_64_BIT__
#endif

/* Qualifiers for kernel code shared by CPU and GPU */

#ifndef __KERNEL_GPU__
#  define ccl_device static inline
#  define ccl_device_noinline static
#  define ccl_global
#  define ccl_static_constant static const
#  define ccl_constant const
#  define ccl_local
#  define ccl_local_param
#  define ccl_private
#  define ccl_restrict __restrict
#  define ccl_ref &
#  define __KERNEL_WITH_SSE_ALIGN__

#  if defined(_WIN32) && !defined(FREE_WINDOWS)
#    define ccl_device_inline static __forceinline
#    define ccl_device_forceinline static __forceinline
#    define ccl_align(...) __declspec(align(__VA_ARGS__))
#    ifdef __KERNEL_64_BIT__
#      define ccl_try_align(...) __declspec(align(__VA_ARGS__))
#    else  /* __KERNEL_64_BIT__ */
#      undef __KERNEL_WITH_SSE_ALIGN__
/* No support for function arguments (error C2719). */
#      define ccl_try_align(...)
#    endif  /* __KERNEL_64_BIT__ */
#    define ccl_may_alias
#    define ccl_always_inline __forceinline
#    define ccl_never_inline __declspec(noinline)
#    define ccl_maybe_unused
#  else  /* _WIN32 && !FREE_WINDOWS */
#    define ccl_device_inline static inline __attribute__((always_inline))
#    define ccl_device_forceinline static inline __attribute__((always_inline))
#    define ccl_align(...) __attribute__((aligned(__VA_ARGS__)))
#    ifndef FREE_WINDOWS64
#      define __forceinline inline __attribute__((always_inline))
#    endif
#    define ccl_try_align(...) __attribute__((aligned(__VA_ARGS__)))
#    define ccl_may_alias __attribute__((__may_alias__))
#    define ccl_always_inline __attribute__((always_inline))
#    define ccl_never_inline __attribute__((noinline))
#    define ccl_maybe_unused __attribute__((used))
#  endif  /* _WIN32 && !FREE_WINDOWS */

/* Use to suppress '-Wimplicit-fallthrough' (in place of 'break'). */
#  if defined(__GNUC__) && (__GNUC__ >= 7)  /* gcc7.0+ only */
#    define ATTR_FALLTHROUGH __attribute__((fallthrough))
#  else
#    define ATTR_FALLTHROUGH ((void)0)
#  endif
#endif  /* __KERNEL_GPU__ */

/* macros */

/* hints for branch prediction, only use in code that runs a _lot_ */
#if defined(__GNUC__) && defined(__KERNEL_CPU__)
#  define LIKELY(x)       __builtin_expect(!!(x), 1)
#  define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x)       (x)
#  define UNLIKELY(x)     (x)
#endif

#if defined(__GNUC__) || defined(__clang__)
/* Some magic to be sure we don't have reference in the type. */
template<typename T> static inline T decltype_helper(T x) { return x; }
#  define TYPEOF(x) decltype(decltype_helper(x))
#endif

/* Causes warning:
 * incompatible types when assigning to type 'Foo' from type 'Bar'
 * ... the compiler optimizes away the temp var */
#ifdef __GNUC__
#define CHECK_TYPE(var, type)  {  \
	TYPEOF(var) *__tmp;           \
	__tmp = (type *)NULL;         \
	(void)__tmp;                  \
} (void)0

#define CHECK_TYPE_PAIR(var_a, var_b)  {  \
	TYPEOF(var_a) *__tmp;                 \
	__tmp = (typeof(var_b) *)NULL;        \
	(void)__tmp;                          \
} (void)0
#else
#  define CHECK_TYPE(var, type)
#  define CHECK_TYPE_PAIR(var_a, var_b)
#endif

/* can be used in simple macros */
#define CHECK_TYPE_INLINE(val, type) \
	((void)(((type)0) != (val)))

#ifndef __KERNEL_GPU__
#  include <cassert>
#  define util_assert(statement)  assert(statement)
#else
#  define util_assert(statement)
#endif

#endif /* __UTIL_DEFINES_H__ */
