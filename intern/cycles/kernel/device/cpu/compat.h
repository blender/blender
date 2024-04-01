/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

/* Release kernel has too much false-positive maybe-uninitialized warnings,
 * which makes it possible to miss actual warnings.
 */
#if (defined(__GNUC__) && !defined(__clang__)) && defined(NDEBUG)
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#  pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#include "util/half.h"
#include "util/math.h"
#include "util/simd.h"
#include "util/texture.h"
#include "util/types.h"

/* On x86_64, versions of GLIBC < 2.16 have an issue where `expf` is
 * much slower than the double version. This was fixed in GLIBC 2.16. */
#if !defined(__KERNEL_GPU__) && defined(__x86_64__) && defined(__x86_64__) && \
    defined(__GNU_LIBRARY__) && defined(__GLIBC__) && defined(__GLIBC_MINOR__) && \
    (__GLIBC__ <= 2 && __GLIBC_MINOR__ < 16)
#  define expf(x) ((float)exp((double)(x)))
#endif

CCL_NAMESPACE_BEGIN

/* Assertions inside the kernel only work for the CPU device, so we wrap it in
 * a macro which is empty for other devices */

#define kernel_assert(cond) assert(cond)

CCL_NAMESPACE_END
