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

#include "libmv/base/aligned_malloc.h"
#include "libmv/logging/logging.h"

#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
// Needed for memalign on Linux and _aligned_alloc on Windows.
#  ifdef FREE_WINDOWS
/* make sure _aligned_malloc is included */
#    ifdef __MSVCRT_VERSION__
#      undef __MSVCRT_VERSION__
#    endif

#    define __MSVCRT_VERSION__ 0x0700
#  endif  // FREE_WINDOWS

#  include <malloc.h>
#else
// Apple's malloc is 16-byte aligned, and does not have malloc.h, so include
// stdilb instead.
#  include <cstdlib>
#endif

namespace libmv {

void *aligned_malloc(int size, int alignment) {
#ifdef _WIN32
  return _aligned_malloc(size, alignment);
#elif __APPLE__
  // On Mac OS X, both the heap and the stack are guaranteed 16-byte aligned so
  // they work natively with SSE types with no further work.
  CHECK_EQ(alignment, 16);
  return malloc(size);
#elif defined(__FreeBSD__) || defined(__NetBSD__)
  void *result;

  if (posix_memalign(&result, alignment, size)) {
    // non-zero means allocation error
    // either no allocation or bad alignment value
    return NULL;
  }
  return result;
#else  // This is for Linux.
  return memalign(alignment, size);
#endif
}

void aligned_free(void *ptr) {
#ifdef _WIN32
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

}  // namespace libmv
