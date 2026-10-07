/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "kernel/util/nanovdb.h"

/* clang-format off */
struct ONEAPIKernelContext : public KernelGlobalsGPU {
  public:
#ifndef WITH_CYCLES_ONEAPI_SOFTWARE_TEXTURING
#    include "kernel/device/gpu/image.h"
#else
#    include "kernel/util/image.h"
#endif
  /* clang-format on */
