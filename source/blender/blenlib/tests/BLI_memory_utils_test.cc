/* Apache License, Version 2.0 */

#include "BLI_float3.hh"
#include "BLI_memory_utils.hh"
#include "BLI_strict_flags.h"
#include "testing/testing.h"

namespace blender::tests {

namespace {
struct MyValue {
  static inline int alive = 0;

  MyValue()
  {
    if (alive == 15) {
      throw std::exception();
    }

    alive++;
  }

  MyValue(const MyValue &UNUSED(other))
  {
    if (alive == 15) {
      throw std::exception();
    }

    alive++;
  }

  ~MyValue()
  {
    alive--;
  }
};
}  // namespace

TEST(memory_utils, DefaultConstructN_ActuallyCallsConstructor)
{
  constexpr int amount = 10;
  TypedBuffer<MyValue, amount> buffer;

  EXPECT_EQ(MyValue::alive, 0);
  default_construct_n(buffer.ptr(), amount);
  EXPECT_EQ(MyValue::alive, amount);
  destruct_n(buffer.ptr(), amount);
  EXPECT_EQ(MyValue::alive, 0);
}

TEST(memory_utils, DefaultConstructN_StrongExceptionSafety)
{
  constexpr int amount = 20;
  TypedBuffer<MyValue, amount> buffer;

  EXPECT_EQ(MyValue::alive, 0);
  EXPECT_THROW(default_construct_n(buffer.ptr(), amount), std::exception);
  EXPECT_EQ(MyValue::alive, 0);
}

TEST(memory_utils, UninitializedCopyN_ActuallyCopies)
{
  constexpr int amount = 5;
  TypedBuffer<MyValue, amount> buffer1;
  TypedBuffer<MyValue, amount> buffer2;

  EXPECT_EQ(MyValue::alive, 0);
  default_construct_n(buffer1.ptr(), amount);
  EXPECT_EQ(MyValue::alive, amount);
  uninitialized_copy_n(buffer1.ptr(), amount, buffer2.ptr());
  EXPECT_EQ(MyValue::alive, 2 * amount);
  destruct_n(buffer1.ptr(), amount);
  EXPECT_EQ(MyValue::alive, amount);
  destruct_n(buffer2.ptr(), amount);
  EXPECT_EQ(MyValue::alive, 0);
}

TEST(memory_utils, UninitializedCopyN_StrongExceptionSafety)
{
  constexpr int amount = 10;
  TypedBuffer<MyValue, amount> buffer1;
  TypedBuffer<MyValue, amount> buffer2;

  EXPECT_EQ(MyValue::alive, 0);
  default_construct_n(buffer1.ptr(), amount);
  EXPECT_EQ(MyValue::alive, amount);
  EXPECT_THROW(uninitialized_copy_n(buffer1.ptr(), amount, buffer2.ptr()), std::exception);
  EXPECT_EQ(MyValue::alive, amount);
  destruct_n(buffer1.ptr(), amount);
  EXPECT_EQ(MyValue::alive, 0);
}

TEST(memory_utils, UninitializedFillN_ActuallyCopies)
{
  constexpr int amount = 10;
  TypedBuffer<MyValue, amount> buffer;

  EXPECT_EQ(MyValue::alive, 0);
  {
    MyValue value;
    EXPECT_EQ(MyValue::alive, 1);
    uninitialized_fill_n(buffer.ptr(), amount, value);
    EXPECT_EQ(MyValue::alive, 1 + amount);
    destruct_n(buffer.ptr(), amount);
    EXPECT_EQ(MyValue::alive, 1);
  }
  EXPECT_EQ(MyValue::alive, 0);
}

TEST(memory_utils, UninitializedFillN_StrongExceptionSafety)
{
  constexpr int amount = 20;
  TypedBuffer<MyValue, amount> buffer;

  EXPECT_EQ(MyValue::alive, 0);
  {
    MyValue value;
    EXPECT_EQ(MyValue::alive, 1);
    EXPECT_THROW(uninitialized_fill_n(buffer.ptr(), amount, value), std::exception);
    EXPECT_EQ(MyValue::alive, 1);
  }
  EXPECT_EQ(MyValue::alive, 0);
}

class TestBaseClass {
  virtual void mymethod(){};
};

class TestChildClass : public TestBaseClass {
  void mymethod() override
  {
  }
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

}  // namespace blender::tests
