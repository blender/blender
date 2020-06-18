// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_PUBLIC_ORDERED_GROUPS_H_
#define CERES_PUBLIC_ORDERED_GROUPS_H_

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include "ceres/internal/port.h"
#include "glog/logging.h"

namespace ceres {

// A class for storing and manipulating an ordered collection of
// groups/sets with the following semantics:
//
// Group ids are non-negative integer values. Elements are any type
// that can serve as a key in a map or an element of a set.
//
// An element can only belong to one group at a time. A group may
// contain an arbitrary number of elements.
//
// Groups are ordered by their group id.
template <typename T>
class OrderedGroups {
 public:
  // Add an element to a group. If a group with this id does not
  // exist, one is created. This method can be called any number of
  // times for the same element. Group ids should be non-negative
  // numbers.
  //
  // Return value indicates if adding the element was a success.
  bool AddElementToGroup(const T element, const int group) {
    if (group < 0) {
      return false;
    }

    auto it = element_to_group_.find(element);
    if (it != element_to_group_.end()) {
      if (it->second == group) {
        // Element is already in the right group, nothing to do.
        return true;
      }

      group_to_elements_[it->second].erase(element);
      if (group_to_elements_[it->second].size() == 0) {
        group_to_elements_.erase(it->second);
      }
    }

    element_to_group_[element] = group;
    group_to_elements_[group].insert(element);
    return true;
  }

  void Clear() {
    group_to_elements_.clear();
    element_to_group_.clear();
  }

  // Remove the element, no matter what group it is in. Return value
  // indicates if the element was actually removed.
  bool Remove(const T element) {
    const int current_group = GroupId(element);
    if (current_group < 0) {
      return false;
    }

    group_to_elements_[current_group].erase(element);

    if (group_to_elements_[current_group].size() == 0) {
      // If the group is empty, then get rid of it.
      group_to_elements_.erase(current_group);
    }

    element_to_group_.erase(element);
    return true;
  }

  // Bulk remove elements. The return value indicates the number of
  // elements successfully removed.
  int Remove(const std::vector<T>& elements) {
    if (NumElements() == 0 || elements.size() == 0) {
      return 0;
    }

    int num_removed = 0;
    for (int i = 0; i < elements.size(); ++i) {
      num_removed += Remove(elements[i]);
    }
    return num_removed;
  }

  // Reverse the order of the groups in place.
  void Reverse() {
    if (NumGroups() == 0) {
      return;
    }

    auto it = group_to_elements_.rbegin();
    std::map<int, std::set<T>> new_group_to_elements;
    new_group_to_elements[it->first] = it->second;

    int new_group_id = it->first + 1;
    for (++it; it != group_to_elements_.rend(); ++it) {
      for (const auto& element : it->second) {
        element_to_group_[element] = new_group_id;
      }
      new_group_to_elements[new_group_id] = it->second;
      new_group_id++;
    }

    group_to_elements_.swap(new_group_to_elements);
  }

  // Return the group id for the element. If the element is not a
  // member of any group, return -1.
  int GroupId(const T element) const {
    auto it = element_to_group_.find(element);
    if (it == element_to_group_.end()) {
      return -1;
    }
    return it->second;
  }

  bool IsMember(const T element) const {
    auto it = element_to_group_.find(element);
    return (it != element_to_group_.end());
  }

  // This function always succeeds, i.e., implicitly there exists a
  // group for every integer.
  int GroupSize(const int group) const {
    auto it = group_to_elements_.find(group);
    return (it == group_to_elements_.end()) ? 0 : it->second.size();
  }

  int NumElements() const { return element_to_group_.size(); }

  // Number of groups with one or more elements.
  int NumGroups() const { return group_to_elements_.size(); }

  // The first group with one or more elements. Calling this when
  // there are no groups with non-zero elements will result in a
  // crash.
  int MinNonZeroGroup() const {
    CHECK_NE(NumGroups(), 0);
    return group_to_elements_.begin()->first;
  }

  const std::map<int, std::set<T>>& group_to_elements() const {
    return group_to_elements_;
  }

  const std::map<T, int>& element_to_group() const { return element_to_group_; }

 private:
  std::map<int, std::set<T>> group_to_elements_;
  std::unordered_map<T, int> element_to_group_;
};

// Typedef for the most commonly used version of OrderedGroups.
typedef OrderedGroups<double*> ParameterBlockOrdering;

}  // namespace ceres

#endif  // CERES_PUBLIC_ORDERED_GROUP_H_
