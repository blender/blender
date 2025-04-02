// Copyright (c) 2025 libmv authors.
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

#include "libmv/threading/parallel_for.h"

#include <algorithm>
#include <vector>

#include "libmv/threading/threading.h"
#include "testing/testing.h"

namespace libmv {

TEST(ParallelFor, SimpleRange) {
  const int num_elements = 100;

  std::vector<int> data;
  mutex data_mutex;

  parallel_for(0, num_elements, [&](const int i) {
    scoped_lock lock(data_mutex);
    data.push_back(i);
  });

  std::sort(data.begin(), data.end());

  for (int i = 0; i < num_elements; ++i) {
    EXPECT_EQ(data[i], i);
  }
}

}  // namespace libmv
