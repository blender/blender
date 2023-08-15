/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_generic_vector_array.hh"

namespace blender::tests {

TEST(generic_vector_array, Construct)
{
  GVectorArray vector_array{CPPType::get<int>(), 4};
  EXPECT_EQ(vector_array.size(), 4);
  EXPECT_FALSE(vector_array.is_empty());
}

TEST(generic_vector_array, Append)
{
  GVectorArray vector_array{CPPType::get<int>(), 3};
  int value1 = 2;
  vector_array.append(1, &value1);
  vector_array.append(1, &value1);
  int value2 = 3;
  vector_array.append(0, &value2);
  vector_array.append(1, &value2);

  EXPECT_EQ(vector_array[0].size(), 1);
  EXPECT_EQ(vector_array[1].size(), 3);
  EXPECT_EQ(vector_array[2].size(), 0);
}

TEST(generic_vector_array, Extend)
{
  GVectorArray vector_array{CPPType::get<int>(), 3};
  vector_array.extend(0, Span<int>({1, 4, 6, 4}));
  vector_array.extend(1, Span<int>());
  vector_array.extend(0, Span<int>({10, 20, 30}));

  EXPECT_EQ(vector_array[0].size(), 7);
  EXPECT_EQ(vector_array[1].size(), 0);
  EXPECT_EQ(vector_array[2].size(), 0);
}

}  // namespace blender::tests
