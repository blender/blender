/* Apache License, Version 2.0 */

#include "BLI_function_ref.hh"

#include "testing/testing.h"

namespace blender::tests {

static int perform_binary_operation(int a, int b, FunctionRef<int(int, int)> operation)
{
  return operation(a, b);
}

TEST(function_ref, StatelessLambda)
{
  const int result = perform_binary_operation(4, 6, [](int a, int b) { return a - b; });
  EXPECT_EQ(result, -2);
}

TEST(function_ref, StatefullLambda)
{
  const int factor = 10;
  const int result = perform_binary_operation(
      2, 3, [&](int a, int b) { return factor * (a + b); });
  EXPECT_EQ(result, 50);
}

static int add_two_numbers(int a, int b)
{
  return a + b;
}

TEST(function_ref, StandaloneFunction)
{
  const int result = perform_binary_operation(10, 5, add_two_numbers);
  EXPECT_EQ(result, 15);
}

TEST(function_ref, ConstantFunction)
{
  auto f = []() { return 42; };
  FunctionRef<int()> ref = f;
  EXPECT_EQ(ref(), 42);
}

TEST(function_ref, MutableStatefullLambda)
{
  int counter = 0;
  auto f = [&]() mutable { return counter++; };
  FunctionRef<int()> ref = f;
  EXPECT_EQ(ref(), 0);
  EXPECT_EQ(ref(), 1);
  EXPECT_EQ(ref(), 2);
}

TEST(function_ref, Null)
{
  FunctionRef<int()> ref;
  EXPECT_FALSE(ref);

  auto f = []() { return 1; };
  ref = f;
  EXPECT_TRUE(ref);

  ref = {};
  EXPECT_FALSE(ref);
}

TEST(function_ref, CopyDoesNotReferenceFunctionRef)
{
  auto f1 = []() { return 1; };
  auto f2 = []() { return 2; };
  FunctionRef<int()> x = f1;
  FunctionRef<int()> y = x;
  x = f2;
  EXPECT_EQ(y(), 1);
}

TEST(function_ref, CopyDoesNotReferenceFunctionRef2)
{
  auto f = []() { return 1; };
  FunctionRef<int()> x;
  FunctionRef<int()> y = f;
  FunctionRef<int()> z = static_cast<const FunctionRef<int()> &&>(y);
  x = z;
  y = {};
  EXPECT_EQ(x(), 1);
}

TEST(function_ref, ReferenceAnotherFunctionRef)
{
  auto f1 = []() { return 1; };
  auto f2 = []() { return 2; };
  FunctionRef<int()> x = f1;
  auto f3 = [&]() { return x(); };
  FunctionRef<int()> y = f3;
  EXPECT_EQ(y(), 1);
  x = f2;
  EXPECT_EQ(y(), 2);
}

}  // namespace blender::tests
