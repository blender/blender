/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_utildefines.h"

#include "testing/testing.h"

namespace blender::tests {

TEST(BLI_utildefines, ARRAY_SIZE)
{
  {
    int bounded_char[5];
    static_assert(ARRAY_SIZE(bounded_char) == 5);
  }

  {
    int *bounded_char[5];
    static_assert(ARRAY_SIZE(bounded_char) == 5);
  }
}

TEST(BLI_utildefines, BOUNDED_ARRAY_TYPE_SIZE)
{
  {
    int bounded_char[5];
    static_assert(BOUNDED_ARRAY_TYPE_SIZE<decltype(bounded_char)>() == 5);
  }

  {
    int *bounded_char[5];
    static_assert(BOUNDED_ARRAY_TYPE_SIZE<decltype(bounded_char)>() == 5);
  }

  {
    struct MyType {
      int array[12];
    };

    static_assert(BOUNDED_ARRAY_TYPE_SIZE<decltype(MyType::array)>() == 12);
  }
}

}  // namespace blender::tests
