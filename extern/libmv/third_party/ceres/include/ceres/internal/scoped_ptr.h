// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: jorg@google.com (Jorg Brown)
//
// This is an implementation designed to match the anticipated future TR2
// implementation of the scoped_ptr class, and its closely-related brethren,
// scoped_array, scoped_ptr_malloc, and make_scoped_ptr.

#ifndef CERES_PUBLIC_INTERNAL_SCOPED_PTR_H_
#define CERES_PUBLIC_INTERNAL_SCOPED_PTR_H_

#include <assert.h>
#include <stdlib.h>
#include <cstddef>
#include <algorithm>

namespace ceres {
namespace internal {

template <class C> class scoped_ptr;
template <class C, class Free> class scoped_ptr_malloc;
template <class C> class scoped_array;

template <class C>
scoped_ptr<C> make_scoped_ptr(C *);

// A scoped_ptr<T> is like a T*, except that the destructor of
// scoped_ptr<T> automatically deletes the pointer it holds (if
// any). That is, scoped_ptr<T> owns the T object that it points
// to. Like a T*, a scoped_ptr<T> may hold either NULL or a pointer to
// a T object. Also like T*, scoped_ptr<T> is thread-compatible, and
// once you dereference it, you get the threadsafety guarantees of T.
//
// The size of a scoped_ptr is small: sizeof(scoped_ptr<C>) == sizeof(C*)
template <class C>
class scoped_ptr {
 public:
  // The element type
  typedef C element_type;

  // Constructor.  Defaults to intializing with NULL.
  // There is no way to create an uninitialized scoped_ptr.
  // The input parameter must be allocated with new.
  explicit scoped_ptr(C* p = NULL) : ptr_(p) { }

  // Destructor.  If there is a C object, delete it.
  // We don't need to test ptr_ == NULL because C++ does that for us.
  ~scoped_ptr() {
    enum { type_must_be_complete = sizeof(C) };
    delete ptr_;
  }

  // Reset.  Deletes the current owned object, if any.
  // Then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(C* p = NULL) {
    if (p != ptr_) {
      enum { type_must_be_complete = sizeof(C) };
      delete ptr_;
      ptr_ = p;
    }
  }

  // Accessors to get the owned object.
  // operator* and operator-> will assert() if there is no current object.
  C& operator*() const {
    assert(ptr_ != NULL);
    return *ptr_;
  }
  C* operator->() const  {
    assert(ptr_ != NULL);
    return ptr_;
  }
  C* get() const { return ptr_; }

  // Comparison operators.
  // These return whether a scoped_ptr and a raw pointer refer to
  // the same object, not just to two different but equal objects.
  bool operator==(const C* p) const { return ptr_ == p; }
  bool operator!=(const C* p) const { return ptr_ != p; }

  // Swap two scoped pointers.
  void swap(scoped_ptr& p2) {
    C* tmp = ptr_;
    ptr_ = p2.ptr_;
    p2.ptr_ = tmp;
  }

  // Release a pointer.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  C* release() {
    C* retVal = ptr_;
    ptr_ = NULL;
    return retVal;
  }

 private:
  C* ptr_;

  // google3 friend class that can access copy ctor (although if it actually
  // calls a copy ctor, there will be a problem) see below
  friend scoped_ptr<C> make_scoped_ptr<C>(C *p);

  // Forbid comparison of scoped_ptr types.  If C2 != C, it totally doesn't
  // make sense, and if C2 == C, it still doesn't make sense because you should
  // never have the same object owned by two different scoped_ptrs.
  template <class C2> bool operator==(scoped_ptr<C2> const& p2) const;
  template <class C2> bool operator!=(scoped_ptr<C2> const& p2) const;

  // Disallow evil constructors
  scoped_ptr(const scoped_ptr&);
  void operator=(const scoped_ptr&);
};

// Free functions
template <class C>
inline void swap(scoped_ptr<C>& p1, scoped_ptr<C>& p2) {
  p1.swap(p2);
}

template <class C>
inline bool operator==(const C* p1, const scoped_ptr<C>& p2) {
  return p1 == p2.get();
}

template <class C>
inline bool operator==(const C* p1, const scoped_ptr<const C>& p2) {
  return p1 == p2.get();
}

template <class C>
inline bool operator!=(const C* p1, const scoped_ptr<C>& p2) {
  return p1 != p2.get();
}

template <class C>
inline bool operator!=(const C* p1, const scoped_ptr<const C>& p2) {
  return p1 != p2.get();
}

template <class C>
scoped_ptr<C> make_scoped_ptr(C *p) {
  // This does nothing but to return a scoped_ptr of the type that the passed
  // pointer is of.  (This eliminates the need to specify the name of T when
  // making a scoped_ptr that is used anonymously/temporarily.)  From an
  // access control point of view, we construct an unnamed scoped_ptr here
  // which we return and thus copy-construct.  Hence, we need to have access
  // to scoped_ptr::scoped_ptr(scoped_ptr const &).  However, it is guaranteed
  // that we never actually call the copy constructor, which is a good thing
  // as we would call the temporary's object destructor (and thus delete p)
  // if we actually did copy some object, here.
  return scoped_ptr<C>(p);
}

// scoped_array<C> is like scoped_ptr<C>, except that the caller must allocate
// with new [] and the destructor deletes objects with delete [].
//
// As with scoped_ptr<C>, a scoped_array<C> either points to an object
// or is NULL.  A scoped_array<C> owns the object that it points to.
// scoped_array<T> is thread-compatible, and once you index into it,
// the returned objects have only the threadsafety guarantees of T.
//
// Size: sizeof(scoped_array<C>) == sizeof(C*)
template <class C>
class scoped_array {
 public:
  // The element type
  typedef C element_type;

  // Constructor.  Defaults to intializing with NULL.
  // There is no way to create an uninitialized scoped_array.
  // The input parameter must be allocated with new [].
  explicit scoped_array(C* p = NULL) : array_(p) { }

  // Destructor.  If there is a C object, delete it.
  // We don't need to test ptr_ == NULL because C++ does that for us.
  ~scoped_array() {
    enum { type_must_be_complete = sizeof(C) };
    delete[] array_;
  }

  // Reset. Deletes the current owned object, if any.
  // Then takes ownership of a new object, if given.
  // this->reset(this->get()) works.
  void reset(C* p = NULL) {
    if (p != array_) {
      enum { type_must_be_complete = sizeof(C) };
      delete[] array_;
      array_ = p;
    }
  }

  // Get one element of the current object.
  // Will assert() if there is no current object, or index i is negative.
  C& operator[](std::ptrdiff_t i) const {
    assert(i >= 0);
    assert(array_ != NULL);
    return array_[i];
  }

  // Get a pointer to the zeroth element of the current object.
  // If there is no current object, return NULL.
  C* get() const {
    return array_;
  }

  // Comparison operators.
  // These return whether a scoped_array and a raw pointer refer to
  // the same array, not just to two different but equal arrays.
  bool operator==(const C* p) const { return array_ == p; }
  bool operator!=(const C* p) const { return array_ != p; }

  // Swap two scoped arrays.
  void swap(scoped_array& p2) {
    C* tmp = array_;
    array_ = p2.array_;
    p2.array_ = tmp;
  }

  // Release an array.
  // The return value is the current pointer held by this object.
  // If this object holds a NULL pointer, the return value is NULL.
  // After this operation, this object will hold a NULL pointer,
  // and will not own the object any more.
  C* release() {
    C* retVal = array_;
    array_ = NULL;
    return retVal;
  }

 private:
  C* array_;

  // Forbid comparison of different scoped_array types.
  template <class C2> bool operator==(scoped_array<C2> const& p2) const;
  template <class C2> bool operator!=(scoped_array<C2> const& p2) const;

  // Disallow evil constructors
  scoped_array(const scoped_array&);
  void operator=(const scoped_array&);
};

// Free functions
template <class C>
inline void swap(scoped_array<C>& p1, scoped_array<C>& p2) {
  p1.swap(p2);
}

template <class C>
inline bool operator==(const C* p1, const scoped_array<C>& p2) {
  return p1 == p2.get();
}

template <class C>
inline bool operator==(const C* p1, const scoped_array<const C>& p2) {
  return p1 == p2.get();
}

template <class C>
inline bool operator!=(const C* p1, const scoped_array<C>& p2) {
  return p1 != p2.get();
}

template <class C>
inline bool operator!=(const C* p1, const scoped_array<const C>& p2) {
  return p1 != p2.get();
}

// This class wraps the c library function free() in a class that can be
// passed as a template argument to scoped_ptr_malloc below.
class ScopedPtrMallocFree {
 public:
  inline void operator()(void* x) const {
    free(x);
  }
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_SCOPED_PTR_H_
