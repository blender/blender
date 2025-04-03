// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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
// Author: keir@google.com (Keir Mierle)

#ifndef CERES_INTERNAL_STL_UTIL_H_
#define CERES_INTERNAL_STL_UTIL_H_

#include <algorithm>

namespace ceres {

// STLDeleteContainerPointers()
//  For a range within a container of pointers, calls delete
//  (non-array version) on these pointers.
// NOTE: for these three functions, we could just implement a DeleteObject
// functor and then call for_each() on the range and functor, but this
// requires us to pull in all of algorithm.h, which seems expensive.
// For hash_[multi]set, it is important that this deletes behind the iterator
// because the hash_set may call the hash function on the iterator when it is
// advanced, which could result in the hash function trying to deference a
// stale pointer.
template <class ForwardIterator>
void STLDeleteContainerPointers(ForwardIterator begin, ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

// Variant of STLDeleteContainerPointers which allows the container to
// contain duplicates.
template <class ForwardIterator>
void STLDeleteUniqueContainerPointers(ForwardIterator begin,
                                      ForwardIterator end) {
  sort(begin, end);
  ForwardIterator new_end = unique(begin, end);
  while (begin != new_end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

// STLDeleteElements() deletes all the elements in an STL container and clears
// the container.  This function is suitable for use with a vector, set,
// hash_set, or any other STL container which defines sensible begin(), end(),
// and clear() methods.
//
// If container is nullptr, this function is a no-op.
//
// As an alternative to calling STLDeleteElements() directly, consider
// ElementDeleter (defined below), which ensures that your container's elements
// are deleted when the ElementDeleter goes out of scope.
template <class T>
void STLDeleteElements(T* container) {
  if (!container) return;
  STLDeleteContainerPointers(container->begin(), container->end());
  container->clear();
}

}  // namespace ceres

#endif  // CERES_INTERNAL_STL_UTIL_H_
