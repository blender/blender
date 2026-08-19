/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * SIMD instruction support.
 */

#if (defined(__ARM_NEON) || (defined(_M_ARM64) && defined(_MSC_VER))) && \
    defined(WITH_SSE2NEON) && !defined(DISABLE_SSE2NEON)
/* SSE/SSE2 emulation on ARM Neon. Match SSE precision. */
#  if !defined(SSE2NEON_PRECISE_MINMAX)
#    define SSE2NEON_PRECISE_MINMAX 1
#  endif
#  if !defined(SSE2NEON_PRECISE_DIV)
#    define SSE2NEON_PRECISE_DIV 1
#  endif
#  if !defined(SSE2NEON_PRECISE_SQRT)
#    define SSE2NEON_PRECISE_SQRT 1
#  endif
#  include <sse2neon.h>
#  define BLI_HAVE_SSE2 1
#  define BLI_HAVE_ARM_NEON 1
#elif defined(__SSE2__)
/* Native SSE2 on Intel/AMD. */
#  include <emmintrin.h>
#  define BLI_HAVE_SSE2 1
#  define BLI_HAVE_ARM_NEON 0
#else
#  define BLI_HAVE_SSE2 0
#  define BLI_HAVE_ARM_NEON 0
#endif

#if (defined(__ARM_NEON) || (defined(_M_ARM64) && defined(_MSC_VER))) && \
    defined(WITH_SSE2NEON) && !defined(DISABLE_SSE2NEON)
/* SSE4 is emulated via sse2neon. */
#  define BLI_HAVE_SSE4 1
#elif defined(__SSE4_2__)
/* Native SSE4.2. */
#  include <nmmintrin.h>
#  define BLI_HAVE_SSE4 1
#else
#  define BLI_HAVE_SSE4 0
#endif

/* Similar to Cycles simd.h. */

#include <cstdint>

#if defined(FREE_WINDOWS64)
#  include <windows.h>
#elif defined(_MSC_VER) && !defined(__KERNEL_NEON__)
#  include <intrin.h>
#elif (defined(__x86_64__) || defined(__i386__))
#  include <x86intrin.h>
#elif defined(__KERNEL_NEON__)
#  define SSE2NEON_PRECISE_MINMAX 1
#  include <sse2neon.h>
#endif

/* Floating Point Control, for Embree. */
#if defined(__x86_64__) || defined(_M_X64)
#  define SIMD_SET_FLUSH_TO_ZERO \
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); \
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#elif defined(__aarch64__) || defined(_M_ARM64)
/* The get/set denormals to zero was implemented in sse2neon v1.5.0.
 * Keep the compatibility code until the minimum library version is increased. */
#  if defined(_MM_SET_FLUSH_ZERO_MODE)
#    define SIMD_SET_FLUSH_TO_ZERO \
      _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); \
      _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#  elif !defined(_M_ARM64)
#    define _MM_FLUSH_ZERO_ON 24
#    define __get_fpcr(__fpcr) __asm__ __volatile__("mrs %0,fpcr" : "=r"(__fpcr))
#    define __set_fpcr(__fpcr) __asm__ __volatile__("msr fpcr,%0" : : "ri"(__fpcr))
#    define SIMD_SET_FLUSH_TO_ZERO blender::set_fz(_MM_FLUSH_ZERO_ON);
#    define SIMD_GET_FLUSH_TO_ZERO blender::get_fz(_MM_FLUSH_ZERO_ON)
#  else
#    define _MM_FLUSH_ZERO_ON 24
#    define __get_fpcr(__fpcr) __fpcr = _ReadStatusReg(0x5A20)
#    define __set_fpcr(__fpcr) _WriteStatusReg(0x5A20, __fpcr)
#    define SIMD_SET_FLUSH_TO_ZERO blender::set_fz(_MM_FLUSH_ZERO_ON);
#    define SIMD_GET_FLUSH_TO_ZERO blender::get_fz(_MM_FLUSH_ZERO_ON)
#  endif
#else
#  define SIMD_SET_FLUSH_TO_ZERO
#endif

namespace blender {

#if (defined(__aarch64__) || defined(_M_ARM64)) && !defined(_MM_SET_FLUSH_ZERO_MODE)
inline int set_fz(const uint32_t flag)
{
  uint64_t old_fpcr;
  uint64_t new_fpcr;
  __get_fpcr(old_fpcr);
  new_fpcr = old_fpcr | (uint64_t(1) << flag);
  __set_fpcr(new_fpcr);
  __get_fpcr(old_fpcr);
  return old_fpcr == new_fpcr;
}
inline int get_fz(const uint32_t flag)
{
  uint64_t cur_fpcr;
  __get_fpcr(cur_fpcr);
  return (cur_fpcr & (uint64_t(1) << flag)) > 0 ? 1 : 0;
}
#endif

/**
 * Enable flush-to-zero and denormals-are-zero for the lifetime of the object, then restore the
 * previous state. This is the mode #BLI_task_scheduler_init leaves all threads in, so this is
 * mainly useful to check that code behaves the same in both modes.
 *
 * Does nothing on platforms where the control register is not accessible this way. Those use the
 * `set_fz` path above, which can not read back the original state.
 */
class ScopedFlushToZero {
/* Same platforms that use the `_mm_*` intrinsics for #SIMD_SET_FLUSH_TO_ZERO above. */
#if defined(__x86_64__) || defined(_M_X64) || \
    ((defined(__aarch64__) || defined(_M_ARM64)) && defined(_MM_SET_FLUSH_ZERO_MODE))
  unsigned int orig_csr_;

 public:
  ScopedFlushToZero() : orig_csr_(_mm_getcsr())
  {
    SIMD_SET_FLUSH_TO_ZERO;
  }

  ~ScopedFlushToZero()
  {
    _mm_setcsr(orig_csr_);
  }

#endif
};

}  // namespace blender
