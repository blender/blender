// Copyright (c) 2007, 2008 libmv authors.
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

#include "libmv/image/tuple.h"
#include "testing/testing.h"

#include <algorithm>

using libmv::Tuple;

namespace {

TEST(Tuple, InitConstantValue) {
  Tuple<int, 3> t(5);
  EXPECT_EQ(t(0), 5);
  EXPECT_EQ(t(0), 5);
  EXPECT_EQ(t(0), 5);
}

TEST(Tuple, InitFromPointer) {
  float vals[3] = {1.0f, 2.0f, 3.0f};

  Tuple<float, 3> t(vals);
  for (int i = 0; i < 3; i++)
    EXPECT_EQ(t(i), vals[i]);

  Tuple<int, 3> b(t);
  EXPECT_EQ(b(0), int(vals[0]));
  EXPECT_EQ(b(1), int(vals[1]));
  EXPECT_EQ(b(2), int(vals[2]));
}

TEST(Tuple, Swap) {
  unsigned char vala[3] = {1, 2, 3};
  unsigned char valb[3] = {4, 5, 6};

  Tuple<unsigned char, 3> a(vala);
  Tuple<unsigned char, 3> b(valb);

  std::swap(a, b);

  EXPECT_EQ(a(0), int(valb[0]));
  EXPECT_EQ(a(1), int(valb[1]));
  EXPECT_EQ(a(2), int(valb[2]));
  EXPECT_EQ(b(0), int(vala[0]));
  EXPECT_EQ(b(1), int(vala[1]));
  EXPECT_EQ(b(2), int(vala[2]));
}

TEST(Tuple, IsEqualOperator) {
  Tuple<int, 3> a;
  a(0) = 1;
  a(1) = 2;
  a(2) = 3;
  Tuple<int, 3> b;
  b(0) = 1;
  b(1) = 2;
  b(2) = 3;
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
  b(1) = 5;
  EXPECT_TRUE(a != b);
  EXPECT_FALSE(a == b);
}

}  // namespace
