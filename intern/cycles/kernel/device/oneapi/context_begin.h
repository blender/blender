/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "kernel/util/nanovdb.h"

/* clang-format off */
struct ONEAPIKernelContext : public KernelGlobalsGPU {
  public:
#    include "kernel/device/gpu/image.h"
  /* clang-format on */
