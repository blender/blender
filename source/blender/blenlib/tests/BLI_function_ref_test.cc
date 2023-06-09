/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

TEST(function_ref, CallSafe)
{
  FunctionRef<int()> f;
  EXPECT_FALSE(f.call_safe().has_value());
  auto func = []() { return 10; };
  f = func;
  EXPECT_TRUE(f.call_safe().has_value());
  EXPECT_EQ(*f.call_safe(), 10);
  f = {};
  EXPECT_FALSE(f.call_safe().has_value());
  BLI_STATIC_ASSERT((std::is_same_v<decltype(f.call_safe()), std::optional<int>>), "");
}

TEST(function_ref, CallSafeVoid)
{
  FunctionRef<void()> f;
  BLI_STATIC_ASSERT((std::is_same_v<decltype(f.call_safe()), void>), "");
  f.call_safe();
  int value = 0;
  auto func = [&]() { value++; };
  f = func;
  f.call_safe();
  EXPECT_EQ(value, 1);
}

TEST(function_ref, InitializeWithNull)
{
  FunctionRef<int(int, int)> f{nullptr};
  EXPECT_FALSE(f);
}

static int overload_test(const FunctionRef<void(std::string)> /*fn*/)
{
  return 1;
}

static int overload_test(const FunctionRef<void(int)> /*fn*/)
{
  return 2;
}

TEST(function_ref, OverloadSelection)
{
  const auto fn_1 = [](std::string /*x*/) {};
  const auto fn_2 = [](int /*x*/) {};

  EXPECT_EQ(overload_test(fn_1), 1);
  EXPECT_EQ(overload_test(fn_2), 2);
}

}  // namespace blender::tests
