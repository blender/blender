/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Intel Corporation */

#ifdef WITH_NANOVDB
/* Data type to replace `double` used in the NanoVDB headers. Cycles don't need doubles, and is
 * safer and more portable to never use double datatype on GPU.
 * Use a special structure, so that the following is true:
 * - No unnoticed implicit cast or mathematical operations used on scalar 64bit type
 *   (which rules out trick like using `uint64_t` as a drop-in replacement for double).
 * - Padding rules are matching exactly `double`
 *   (which rules out array of `uint8_t`). */
typedef struct ccl_vdb_double_t {
  union ccl_vdb_helper_t {
    double d;
    uint64_t i;
  };

  uint64_t i;
  ccl_vdb_double_t(double value)
  {
    ccl_vdb_helper_t helper;
    helper.d = value;
    i = helper.i;
  }
  /* We intentionally allow conversion to float in order to workaround compilation errors
   * for defined math functions that take doubles. */
  operator float() const
  {
    ccl_vdb_helper_t helper;
    helper.i = i;
    return (float)helper.d;
  }
} ccl_vdb_double_t;

#  define double ccl_vdb_double_t
#  include <nanovdb/NanoVDB.h>
#  include <nanovdb/util/SampleFromVoxels.h>
#  undef double
#endif

/* clang-format off */
struct ONEAPIKernelContext : public KernelGlobalsGPU {
  public:
#    include "kernel/device/oneapi/image.h"
  /* clang-format on */
