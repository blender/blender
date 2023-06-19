/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#define __KERNEL_SSE__
#define __KERNEL_AVX__

#define TEST_CATEGORY_NAME util_avx

#if (defined(i386) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)) && \
    defined(__AVX__)
#  include "util_float8_test.h"
#endif
