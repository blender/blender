/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

/* clang-format off */

/* #define __forceinline triggers a bug in some clang-format versions, disable
 * format for entire file to keep results consistent. */

#ifndef __UTIL_DEFINES_H__
#define __UTIL_DEFINES_H__

/* Bitness */

#if defined(__ppc64__) || defined(__PPC64__) || defined(__x86_64__) || defined(__ia64__) || \
    defined(_M_X64) || defined(__aarch64__)
#  define __KERNEL_64_BIT__
#endif

/* Qualifiers for kernel code shared by CPU and GPU */

#ifndef __KERNEL_GPU__

/* Leave inlining decisions to compiler for these, the inline keyword here
 * is not about performance but including function definitions in headers. */
#  define ccl_device static inline
#  define ccl_device_extern extern "C"
#  define ccl_device_noinline static inline
#  define ccl_device_noinline_cpu ccl_device_noinline

/* Forced inlining. */
#  if defined(_WIN32) && !defined(FREE_WINDOWS)
#    define ccl_device_inline static __forceinline
#    define ccl_device_forceinline static __forceinline
#    define ccl_device_inline_method __forceinline
#    define ccl_align(...) __declspec(align(__VA_ARGS__))
#    ifdef __KERNEL_64_BIT__
#      define ccl_try_align(...) __declspec(align(__VA_ARGS__))
#    else /* __KERNEL_64_BIT__ */
#      undef __KERNEL_WITH_SSE_ALIGN__
/* No support for function arguments (error C2719). */
#      define ccl_try_align(...)
#    endif /* __KERNEL_64_BIT__ */
#    define ccl_may_alias
#    define ccl_always_inline __forceinline
#    define ccl_never_inline __declspec(noinline)
#  else /* _WIN32 && !FREE_WINDOWS */
#    define ccl_device_inline static inline __attribute__((always_inline))
#    define ccl_device_forceinline static inline __attribute__((always_inline))
#    define ccl_device_inline_method __attribute__((always_inline))
#    define ccl_align(...) __attribute__((aligned(__VA_ARGS__)))
#    ifndef FREE_WINDOWS64
#      define __forceinline inline __attribute__((always_inline))
#    endif
#    define ccl_try_align(...) __attribute__((aligned(__VA_ARGS__)))
#    define ccl_may_alias __attribute__((__may_alias__))
#    define ccl_always_inline __attribute__((always_inline))
#    define ccl_never_inline __attribute__((noinline))
#  endif /* _WIN32 && !FREE_WINDOWS */

/* Address spaces for GPU. */
#  define ccl_global
#  define ccl_inline_constant inline constexpr
#  define ccl_constant const
#  define ccl_private

#  define ccl_restrict __restrict
#  define ccl_optional_struct_init
#  define ccl_loop_no_unroll
#  define ccl_attr_maybe_unused [[maybe_unused]]
#  define __KERNEL_WITH_SSE_ALIGN__

/* Use to suppress '-Wimplicit-fallthrough' (in place of 'break'). */
#  ifndef ATTR_FALLTHROUGH
#    if defined(__GNUC__) && (__GNUC__ >= 7) /* gcc7.0+ only */
#      define ATTR_FALLTHROUGH __attribute__((fallthrough))
#    else
#      define ATTR_FALLTHROUGH ((void)0)
#    endif
#  endif
#endif /* __KERNEL_GPU__ */

/* macros */

/* hints for branch prediction, only use in code that runs a _lot_ */
#if defined(__GNUC__) && !defined(__KERNEL_GPU__)
#  define LIKELY(x) __builtin_expect(!!(x), 1)
#  define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x) (x)
#  define UNLIKELY(x) (x)
#endif

#ifndef __KERNEL_GPU__
#  include <cassert>
#  define util_assert(statement) assert(statement)
#else
#  define util_assert(statement)
#endif

#define CONCAT_HELPER(a, ...) a##__VA_ARGS__
#define CONCAT(a, ...) CONCAT_HELPER(a, __VA_ARGS__)

#endif /* __UTIL_DEFINES_H__ */
