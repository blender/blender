/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BLI_any_derived.hh"

#include "testing/testing.h"

namespace blender::tests {

class AnyVirtualTestBase {
 public:
  int value = 0;

  AnyVirtualTestBase() = default;
  virtual ~AnyVirtualTestBase() = default;

  AnyVirtualTestBase(const int v) : value(v) {}

  virtual int get_value() const
  {
    return this->value;
  }

  virtual void set_value(const int v)
  {
    this->value = v;
  }
};

class AnyVirtualTestTimes10 : public AnyVirtualTestBase {
 public:
  AnyVirtualTestTimes10(const int v) : AnyVirtualTestBase(v * 10) {}

  virtual void set_value(const int v) override
  {
    this->value = v * 10;
  }
};

TEST(any_virtual, DefaultConstructor)
{
  AnyDerived<AnyVirtualTestBase> a;
  EXPECT_FALSE(a);
}

TEST(any_virtual, Emplace)
{
  AnyDerived<AnyVirtualTestBase> a;
  a.emplace<AnyVirtualTestBase>(10);
  EXPECT_TRUE(a);
  EXPECT_EQ(a->value, 10);
  EXPECT_EQ(a->get_value(), 10);
  a->set_value(20);
  EXPECT_EQ(a->value, 20);
}

TEST(any_virtual, Subclass)
{
  AnyDerived<AnyVirtualTestBase> a;
  a.emplace<AnyVirtualTestTimes10>(10);
  EXPECT_EQ(a->value, 100);
  a->set_value(2);
  EXPECT_EQ(a->value, 20);
}

TEST(any_virtual, Const)
{
  AnyDerived<const AnyVirtualTestBase> a;
  a.emplace<AnyVirtualTestBase>(10);
  EXPECT_TRUE(a);
  EXPECT_EQ(a->value, 10);
}

}  // namespace blender::tests
