// Copyright (c) 2009 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "libmv/base/vector.h"
#include "libmv/numeric/numeric.h"
#include "testing/testing.h"
#include <algorithm>

namespace {
using namespace libmv;

// This uses a Vec2d which is a fixed-size vectorizable Eigen type. It is
// necessary to test vectorizable types to ensure that the alignment asserts
// trigger if the alignment is not correct.
TEST(VectorAlignmentTest, PushBack) {
  Vec2 x1, x2;
  x1 << 1, 2;
  x2 << 3, 4;

  vector<Vec2> vs;
  vs.push_back(x1);
  EXPECT_EQ(1, vs.size());
  EXPECT_EQ(1, vs.capacity());

  vs.push_back(x2);
  EXPECT_EQ(2, vs.size());
  EXPECT_EQ(2, vs.capacity());

  // The following is necessary because of some bug in gtest; the expected
  // parameter can't be a fixed size vectorizable type with alignment
  // requirements.
  Vec x1r = x1;
  Vec x2r = x2;
  EXPECT_EQ(x1r, vs[0]);
  EXPECT_EQ(x2r, vs[1]);

  vs.push_back(x2);
  vs.push_back(x2);
  vs.push_back(x2);
  EXPECT_EQ(5, vs.size());
  EXPECT_EQ(8, vs.capacity());
}

// Count the number of destruct calls to test that the destructor gets called.
int foo_construct_calls = 0;
int foo_destruct_calls = 0;

struct Foo {
 public:
  Foo() : value(5) { foo_construct_calls++; }
  ~Foo()           { foo_destruct_calls++;  }
  int value;
};

struct VectorTest : public testing::Test {
  VectorTest() {
    foo_construct_calls = 0;
    foo_destruct_calls = 0;
  }
};

TEST_F(VectorTest, EmptyVectorDoesNotConstruct) {
  {
    vector<Foo> v;
    EXPECT_EQ(0, v.size());
    EXPECT_EQ(0, v.capacity());
  }
  EXPECT_EQ(0, foo_construct_calls);
  EXPECT_EQ(0, foo_destruct_calls);
}

TEST_F(VectorTest, DestructorGetsCalled) {
  {
    vector<Foo> v;
    v.resize(5);
  }
  EXPECT_EQ(5, foo_construct_calls);
  EXPECT_EQ(5, foo_destruct_calls);
}

TEST_F(VectorTest, ReserveDoesNotCallConstructorsOrDestructors) {
  vector<Foo> v;
  EXPECT_EQ(0, v.size());
  EXPECT_EQ(0, v.capacity());
  EXPECT_EQ(0, foo_construct_calls);
  EXPECT_EQ(0, foo_destruct_calls);

  v.reserve(5);
  EXPECT_EQ(0, v.size());
  EXPECT_EQ(5, v.capacity());
  EXPECT_EQ(0, foo_construct_calls);
  EXPECT_EQ(0, foo_destruct_calls);
}

TEST_F(VectorTest, ResizeConstructsAndDestructsAsExpected) {
  vector<Foo> v;

  // Create one object.
  v.resize(1);
  EXPECT_EQ(1, v.size());
  EXPECT_EQ(1, v.capacity());
  EXPECT_EQ(1, foo_construct_calls);
  EXPECT_EQ(0, foo_destruct_calls);
  EXPECT_EQ(5, v[0].value);

  // Create two more.
  v.resize(3);
  EXPECT_EQ(3, v.size());
  EXPECT_EQ(3, v.capacity());
  EXPECT_EQ(3, foo_construct_calls);
  EXPECT_EQ(0, foo_destruct_calls);

  // Delete the last one.
  v.resize(2);
  EXPECT_EQ(2, v.size());
  EXPECT_EQ(3, v.capacity());
  EXPECT_EQ(3, foo_construct_calls);
  EXPECT_EQ(1, foo_destruct_calls);

  // Delete the remaining two.
  v.resize(0);
  EXPECT_EQ(0, v.size());
  EXPECT_EQ(3, v.capacity());
  EXPECT_EQ(3, foo_construct_calls);
  EXPECT_EQ(3, foo_destruct_calls);
}

TEST_F(VectorTest, PushPopBack) {
  vector<Foo> v;

  Foo foo;
  foo.value = 10;
  v.push_back(foo);
  EXPECT_EQ(1, v.size());
  EXPECT_EQ(10, v.back().value);

  v.pop_back();
  EXPECT_EQ(0, v.size());
  EXPECT_EQ(1, foo_construct_calls);
  EXPECT_EQ(1, foo_destruct_calls);
}

TEST_F(VectorTest, CopyConstructor) {
  vector<int> a;
  a.push_back(1);
  a.push_back(5);
  a.push_back(3);

  vector<int> b(a);
  EXPECT_EQ(a.size(),     b.size());
  //EXPECT_EQ(a.capacity(), b.capacity());
  for (int i = 0; i < a.size(); ++i) {
    EXPECT_EQ(a[i], b[i]);
  }
}

TEST_F(VectorTest, OperatorEquals) {
  vector<int> a, b;
  a.push_back(1);
  a.push_back(5);
  a.push_back(3);

  b = a;

  EXPECT_EQ(a.size(),     b.size());
  //EXPECT_EQ(a.capacity(), b.capacity());
  for (int i = 0; i < a.size(); ++i) {
    EXPECT_EQ(a[i], b[i]);
  }
}

TEST_F(VectorTest, STLFind) {
  vector<int> a;
  a.push_back(1);
  a.push_back(5);
  a.push_back(3);

  // Find return an int *
  EXPECT_EQ(std::find(&a[0], &a[2], 1) == &a[0], true);
  EXPECT_EQ(std::find(&a[0], &a[2], 5) == &a[1], true);
  EXPECT_EQ(std::find(&a[0], &a[2], 3) == &a[2], true);

  // Find return a const int *
  EXPECT_EQ(std::find(a.begin(), a.end(), 1) == &a[0], true);
  EXPECT_EQ(std::find(a.begin(), a.end(), 5) == &a[1], true);
  EXPECT_EQ(std::find(a.begin(), a.end(), 3) == &a[2], true);

  // Search value that are not in the vector
  EXPECT_EQ(std::find(a.begin(), a.end(), 0) == a.end(), true);
  EXPECT_EQ(std::find(a.begin(), a.end(), 52) == a.end(), true);
}

TEST(Vector, swap) {
  vector<int> a, b;
  a.push_back(1);
  a.push_back(2);
  b.push_back(3);
  a.swap(b);
  EXPECT_EQ(1, a.size());
  EXPECT_EQ(3, a[0]);
  EXPECT_EQ(2, b.size());
  EXPECT_EQ(1, b[0]);
  EXPECT_EQ(2, b[1]);
}

}  // namespace
