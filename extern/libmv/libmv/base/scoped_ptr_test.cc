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

#include "libmv/base/scoped_ptr.h"
#include "testing/testing.h"

namespace libmv {
namespace {

struct FreeMe {
  FreeMe(int *freed) : freed(freed) {}
  ~FreeMe() { (*freed)++; }
  int *freed;
};

TEST(ScopedPtr, NullDoesNothing) {
  scoped_ptr<FreeMe> x(NULL);
  // Does nothing.
}

TEST(ScopedPtr, FreesWhenOutOfScope) {
  int frees = 0;
  {
    scoped_ptr<FreeMe> scoped(new FreeMe(&frees));
    EXPECT_EQ(0, frees);
  }
  EXPECT_EQ(1, frees);
}

TEST(ScopedPtr, Operators) {
  int tag = 123;
  scoped_ptr<FreeMe> scoped(new FreeMe(&tag));
  EXPECT_EQ(123, *(scoped->freed));
  EXPECT_EQ(123, *((*scoped).freed));
}

TEST(ScopedPtr, Reset) {
  int frees = 0;
  scoped_ptr<FreeMe> scoped(new FreeMe(&frees));
  EXPECT_EQ(0, frees);
  scoped.reset(new FreeMe(&frees));
  EXPECT_EQ(1, frees);
}

TEST(ScopedPtr, ReleaseAndGet) {
  int frees = 0;
  FreeMe *allocated = new FreeMe(&frees);
  FreeMe *released = NULL;
  {
    scoped_ptr<FreeMe> scoped(allocated);
    EXPECT_EQ(0, frees);
    EXPECT_EQ(allocated, scoped.get());
    released = scoped.release();
    EXPECT_EQ(0, frees);
    EXPECT_EQ(released, allocated);
  }
  EXPECT_EQ(0, frees);
  delete released;
}

}  // namespace
}  // namespace libmv
