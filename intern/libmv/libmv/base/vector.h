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
//
// Get an aligned vector implementation. Must be included before <vector>. The
// Eigen guys went through some trouble to make a portable override for the
// fixed size vector types.

#ifndef LIBMV_BASE_VECTOR_H
#define LIBMV_BASE_VECTOR_H

#include <vector>

#include <Eigen/Core>

namespace libmv {

// A simple container class, which guarantees the correct memory alignment
// needed for most eigen vectorization. Don't use this container for classes
// that cannot be copied via memcpy.

template <class ElementType>
using vector = std::vector<ElementType, Eigen::aligned_allocator<ElementType>>;

}  // namespace libmv

#endif  // LIBMV_BASE_VECTOR_H
