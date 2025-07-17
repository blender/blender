/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_array_state.hh"

namespace blender::tests {

TEST(array_state, Empty)
{
  ArrayState<int> state{};
  EXPECT_TRUE(state.is_empty());
  EXPECT_TRUE(state.same_as({}, nullptr));
  EXPECT_FALSE(state.same_as(VArray<int>::from_span({3, 4}), nullptr));
}

TEST(array_state, NoSharing)
{
  ArrayState<int> state{VArray<int>::from_span({1, 2, 3}), nullptr};
  EXPECT_FALSE(state.is_empty());
  EXPECT_TRUE(state.same_as(VArray<int>::from_span({1, 2, 3}), nullptr));
  EXPECT_FALSE(state.same_as(VArray<int>::from_span({1, 2, 4}), nullptr));
  EXPECT_FALSE(state.same_as(VArray<int>::from_span({1, 2, 3, 4}), nullptr));
}

TEST(array_state, WithSharing)
{
  int *data = MEM_calloc_arrayN<int>(3, __func__);
  data[0] = 0;
  data[1] = 10;
  data[2] = 20;
  ImplicitSharingPtr sharing_info{implicit_sharing::info_for_mem_free(data)};

  ArrayState<int> state{VArray<int>::from_span({data, 3}), sharing_info.get()};
  EXPECT_FALSE(state.is_empty());
  EXPECT_TRUE(state.same_as(VArray<int>::from_span({data, 3}), sharing_info.get()));
  EXPECT_TRUE(state.same_as(VArray<int>::from_span({0, 10, 20}), nullptr));
  EXPECT_FALSE(state.same_as(VArray<int>::from_span({0, 1, 2}), nullptr));
}

TEST(array_state, DifferentSharingInfoButSameData)
{
  int *data1 = MEM_calloc_arrayN<int>(3, __func__);
  data1[0] = 0;
  data1[1] = 10;
  data1[2] = 20;
  ImplicitSharingPtr sharing_info1{implicit_sharing::info_for_mem_free(data1)};

  int *data2 = MEM_calloc_arrayN<int>(3, __func__);
  data2[0] = 0;
  data2[1] = 10;
  data2[2] = 20;
  ImplicitSharingPtr sharing_info2{implicit_sharing::info_for_mem_free(data2)};

  ArrayState<int> state{VArray<int>::from_span({data1, 3}), sharing_info1.get()};
  EXPECT_TRUE(state.same_as(VArray<int>::from_span({data2, 3}), sharing_info2.get()));
}

}  // namespace blender::tests
