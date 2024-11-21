/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_binary_search.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "testing/testing.h"

namespace blender::binary_search::tests {

static bool value_pass(const bool value)
{
  return value;
}

TEST(binary_search, Empty)
{
  EXPECT_EQ(first_if(Span<bool>{}, value_pass), 0);
  EXPECT_EQ(last_if(Span<bool>{}, value_pass), -1);
}

TEST(binary_search, One)
{
  EXPECT_EQ(first_if(Span{true}, value_pass), 0);
  EXPECT_EQ(last_if(Span{true}, value_pass), 0);

  EXPECT_EQ(first_if(Span{false}, value_pass), 1);
  EXPECT_EQ(last_if(Span{false}, value_pass), -1);
}

TEST(binary_search, Multiple)
{
  EXPECT_EQ(first_if(Span{true, true, true, true, true, true}, value_pass), 0);
  EXPECT_EQ(first_if(Span{false, true, true, true, true, true}, value_pass), 1);
  EXPECT_EQ(first_if(Span{false, false, true, true, true, true}, value_pass), 2);
  EXPECT_EQ(first_if(Span{false, false, false, true, true, true}, value_pass), 3);
  EXPECT_EQ(first_if(Span{false, false, false, false, true, true}, value_pass), 4);
  EXPECT_EQ(first_if(Span{false, false, false, false, false, true}, value_pass), 5);
  EXPECT_EQ(first_if(Span{false, false, false, false, false, false}, value_pass), 6);

  EXPECT_EQ(last_if(Span{false, false, false, false, false, false}, value_pass), -1);
  EXPECT_EQ(last_if(Span{true, false, false, false, false, false}, value_pass), 0);
  EXPECT_EQ(last_if(Span{true, true, false, false, false, false}, value_pass), 1);
  EXPECT_EQ(last_if(Span{true, true, true, false, false, false}, value_pass), 2);
  EXPECT_EQ(last_if(Span{true, true, true, true, false, false}, value_pass), 3);
  EXPECT_EQ(last_if(Span{true, true, true, true, true, false}, value_pass), 4);
  EXPECT_EQ(last_if(Span{true, true, true, true, true, true}, value_pass), 5);
}

}  // namespace blender::binary_search::tests
