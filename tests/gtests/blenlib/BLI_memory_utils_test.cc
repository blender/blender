#include "BLI_memory_utils.hh"
#include "BLI_strict_flags.h"
#include "testing/testing.h"

namespace blender {

struct MyValue {
  static inline int alive = 0;

  MyValue()
  {
    if (alive == 15) {
      throw std::exception();
    }

    alive++;
  }

  MyValue(const MyValue &other)
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

}  // namespace blender
