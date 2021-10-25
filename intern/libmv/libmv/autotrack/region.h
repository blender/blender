// Copyright (c) 2014 libmv authors.
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
//
// Author: mierle@gmail.com (Keir Mierle)

#ifndef LIBMV_AUTOTRACK_REGION_H_
#define LIBMV_AUTOTRACK_REGION_H_

#include "libmv/numeric/numeric.h"

namespace mv {

using libmv::Vec2f;

// A region is a bounding box within an image.
//
//    +----------> x
//    |
//    |   (min.x, min.y)           (max.x, min.y)
//    |        +-------------------------+
//    |        |                         |
//    |        |                         |
//    |        |                         |
//    |        +-------------------------+
//    v   (min.x, max.y)           (max.x, max.y)
//    y
//
struct Region {
  Vec2f min;
  Vec2f max;

  template<typename T>
  void Offset(const T& offset) {
    min += offset.template cast<float>();
    max += offset.template cast<float>();
  }

  Region Rounded() const {
    Region result;
    result.min(0) = ceil(this->min(0));
    result.min(1) = ceil(this->min(1));
    result.max(0) = ceil(this->max(0));
    result.max(1) = ceil(this->max(1));
    return result;
  }
};

}  // namespace mv

#endif  // LIBMV_AUTOTRACK_REGION_H_
