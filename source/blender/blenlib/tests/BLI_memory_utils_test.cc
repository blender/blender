/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_vector_types.hh"
#include "BLI_memory_utils.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender::tests {

class TestBaseClass {
  virtual void mymethod() {};
};

class TestChildClass : public TestBaseClass {
  void mymethod() override {}
};

static_assert(is_convertible_pointer_v<int *, int *>);
static_assert(is_convertible_pointer_v<int *, const int *>);
static_assert(is_convertible_pointer_v<int *, int *const>);
static_assert(is_convertible_pointer_v<int *, const int *const>);
static_assert(!is_convertible_pointer_v<const int *, int *>);
static_assert(!is_convertible_pointer_v<int, int *>);
static_assert(!is_convertible_pointer_v<int *, int>);
static_assert(is_convertible_pointer_v<TestBaseClass *, const TestBaseClass *>);
static_assert(!is_convertible_pointer_v<const TestBaseClass *, TestBaseClass *>);
static_assert(is_convertible_pointer_v<TestChildClass *, TestBaseClass *>);
static_assert(!is_convertible_pointer_v<TestBaseClass *, TestChildClass *>);
static_assert(is_convertible_pointer_v<const TestChildClass *, const TestBaseClass *>);
static_assert(!is_convertible_pointer_v<TestBaseClass, const TestChildClass *>);
static_assert(!is_convertible_pointer_v<float3, float *>);
static_assert(!is_convertible_pointer_v<float *, float3>);
static_assert(!is_convertible_pointer_v<int **, int *>);
static_assert(!is_convertible_pointer_v<int *, int **>);
static_assert(is_convertible_pointer_v<int **, int **>);
static_assert(is_convertible_pointer_v<const int **, const int **>);
static_assert(!is_convertible_pointer_v<const int **, int **>);
static_assert(!is_convertible_pointer_v<int *const *, int **>);
static_assert(!is_convertible_pointer_v<int *const *const, int **>);
static_assert(is_convertible_pointer_v<int **, int **const>);
static_assert(is_convertible_pointer_v<int **, int *const *>);
static_assert(is_convertible_pointer_v<int **, int const *const *>);

static_assert(is_span_convertible_pointer_v<int *, int *>);
static_assert(is_span_convertible_pointer_v<int *, const int *>);
static_assert(!is_span_convertible_pointer_v<const int *, int *>);
static_assert(is_span_convertible_pointer_v<const int *, const int *>);
static_assert(is_span_convertible_pointer_v<const int *, const void *>);
static_assert(!is_span_convertible_pointer_v<const int *, void *>);
static_assert(is_span_convertible_pointer_v<int *, void *>);
static_assert(is_span_convertible_pointer_v<int *, const void *>);
static_assert(!is_span_convertible_pointer_v<TestBaseClass *, TestChildClass *>);
static_assert(!is_span_convertible_pointer_v<TestChildClass *, TestBaseClass *>);

static_assert(is_same_any_v<int, float, bool, int>);
static_assert(is_same_any_v<int, int, float>);
static_assert(is_same_any_v<int, int>);
static_assert(!is_same_any_v<int, float, bool>);
static_assert(!is_same_any_v<int, float>);
static_assert(!is_same_any_v<int>);

TEST(memory_utils, ScopedDefer1)
{
  int a = 0;
  {
    BLI_SCOPED_DEFER([&]() { a -= 5; });
    {
      BLI_SCOPED_DEFER([&]() { a *= 10; });
      a = 5;
    }
  }
  EXPECT_EQ(a, 45);
}

TEST(memory_utils, ScopedDefer2)
{
  std::string s;
  {
    BLI_SCOPED_DEFER([&]() { s += "A"; });
    BLI_SCOPED_DEFER([&]() { s += "B"; });
    BLI_SCOPED_DEFER([&]() { s += "C"; });
    BLI_SCOPED_DEFER([&]() { s += "D"; });
  }
  EXPECT_EQ(s, "DCBA");
}

}  // namespace blender::tests
