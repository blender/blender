/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

extern "C" {
#include <libavutil/cpu.h>
}

namespace ffmpeg::tests {

TEST(ffmpeg, correct_av_cpu_flags)
{
  int flags = av_get_cpu_flags();
#if defined(_M_X64) || defined(__x86_64__)
  /* x64 expected to have at least up to SSE4.2. */
  EXPECT_TRUE((flags & AV_CPU_FLAG_SSE2) != 0);
  EXPECT_TRUE((flags & AV_CPU_FLAG_SSE4) != 0);
  EXPECT_TRUE((flags & AV_CPU_FLAG_SSE42) != 0);
#elif defined(__aarch64__) || defined(_M_ARM64)
  /* arm64 expected to have at least NEON. */
  EXPECT_TRUE((flags & AV_CPU_FLAG_ARMV8) != 0);
  EXPECT_TRUE((flags & AV_CPU_FLAG_NEON) != 0);
#endif
}

}  // namespace ffmpeg::tests
