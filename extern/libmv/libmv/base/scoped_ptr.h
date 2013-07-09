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

#ifndef LIBMV_BASE_SCOPED_PTR_H
#define LIBMV_BASE_SCOPED_PTR_H

namespace libmv {

/**
 * A handle for a heap-allocated resource that should be freed when it goes out
 * of scope. This looks similar to the one found in TR1.
 */
template<typename T>
class scoped_ptr {
 public:
  scoped_ptr(T *resource) : resource_(resource) {}
  ~scoped_ptr() { reset(0); }

  T *get()             const { return resource_;  }
  T *operator->()      const { return resource_;  }
  T &operator*()       const { return *resource_; }

  void reset(T *new_resource) {
    if (sizeof(T)) {
      delete resource_;
    }
    resource_ = new_resource;
  }

  T *release() {
    T *released_resource = resource_;
    resource_ = 0;
    return released_resource;
  }

 private:
  // No copying allowed.
  T *resource_;
};

}  // namespace libmv

#endif  // LIBMV_BASE_SCOPED_PTR_H
