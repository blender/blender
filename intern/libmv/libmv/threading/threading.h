// Copyright (c) 2020 libmv authors.
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

#ifndef LIBMV_THREADING_THREADING_H_
#define LIBMV_THREADING_THREADING_H_

#include "libmv/build/build_config.h"

#if COMPILER_SUPPORTS_CXX11
#  include <condition_variable>
#  include <mutex>
#endif

namespace libmv {

#if COMPILER_SUPPORTS_CXX11
using mutex = std::mutex;
using scoped_lock = std::unique_lock<std::mutex>;
using condition_variable = std::condition_variable;
#else
#  error Please add support for threading in threading/threading.h
#endif

}  // namespace libmv

#endif  // LIBMV_THREADING_THREADING_H_
