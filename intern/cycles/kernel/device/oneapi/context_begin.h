/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#ifdef WITH_NANOVDB
#  include <nanovdb/NanoVDB.h>
#  include <nanovdb/util/SampleFromVoxels.h>
#endif

/* clang-format off */
struct ONEAPIKernelContext : public KernelGlobalsGPU {
  public:
#    include "kernel/device/oneapi/image.h"
  /* clang-format on */
