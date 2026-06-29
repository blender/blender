/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_normalized_int_types.hh"

namespace blender::tests {

TEST(normalized_int, roundtrip)
{
  {
    const float4 a(-1.0f, 1.0f / 127.0f, 64.0f / 127.0f, 1.0f);
    EXPECT_EQ(float4(char4_norm(a)), a);
  }
  {
    const float4 a(0.0f, 1.0f / 255.0f, 128.0f / 255.0f, 1.0f);
    EXPECT_EQ(float4(uchar4_norm(a)), a);
  }
  {
    const float4 a(-1.0f, 1.0f / 32767.0f, 128.0f / 32767.0f, 1.0f);
    EXPECT_EQ(float4(short4_norm(a)), a);
  }
  {
    const float2 a(-1.0f, 1.0f / 32767.0f);
    EXPECT_EQ(float2(short2_norm(a)), a);
  }
  {
    const float4 a(0.0f, 1.0f / 65535.0f, 64.0f / 65535.0f, 1.0f);
    EXPECT_EQ(float4(ushort4_norm(a)), a);
  }
  {
    const float4 a(-1.0f, 1.0f / 511.0f, 128.0f / 511.0f, 1.0f);
    EXPECT_EQ(float4(int1010102_norm(a)), a);
  }
  {
    const float4 a(0.0f, 1.0f / 1023.0f, 64.0f / 1023.0f, 1.0f);
    EXPECT_EQ(float4(uint1010102_norm(a)), a);
  }
}

}  // namespace blender::tests
