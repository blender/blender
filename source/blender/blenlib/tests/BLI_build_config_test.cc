/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_build_config.h"
#include "BLI_endian_defines.h"

namespace blender::tests {

/* Some basic tests to ensure simple code compiles, and that there are no obvious mistakes in the
 * build_config implementation. */
TEST(BLI_build_config, Basic)
{
  static_assert(ARCH_CPU_31_BITS || ARCH_CPU_32_BITS || ARCH_CPU_64_BITS || ARCH_CPU_128_BITS);
}

TEST(BLI_build_config, Endian)
{
  /* TODO: This could become more comprehensive test when C++20 is available: we can check that
   * std::endian::native is aligned with the compile-time defines. */
  static_assert(ARCH_CPU_LITTLE_ENDIAN || ARCH_CPU_BIG_ENDIAN);

  /* Verify the build_config is aligned with the CMake configuration. */
#if defined(__BIG_ENDIAN__)
  static_assert(ARCH_CPU_BIG_ENDIAN);
  static_assert(!ARCH_CPU_LITTLE_ENDIAN);
#endif
#if defined(__LITTLE_ENDIAN__)
  static_assert(!ARCH_CPU_BIG_ENDIAN);
  static_assert(ARCH_CPU_LITTLE_ENDIAN);
#endif
}

}  // namespace blender::tests
