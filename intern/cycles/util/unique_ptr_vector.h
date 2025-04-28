/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cassert>

#include "util/algorithm.h"
#include "util/set.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Convenient vector of unique_ptr.
 * - Indexing and iterators return the pointer value directly.
 * - Utility functions for erase by swapping elements, for nodes.
 */
template<typename T> class unique_ptr_vector {
 protected:
  vector<unique_ptr<T>> data;

 public:
  T *operator[](const size_t i) const
  {
    return data[i].get();
  }

  unique_ptr<T> steal(const size_t i)
  {
    unique_ptr<T> local;
    swap(data[i], local);
    return local;
  }

  void push_back(unique_ptr<T> &&value)
  {
    data.push_back(std::move(value));
  }

  bool empty() const
  {
    return data.empty();
  }

  size_t size() const
  {
    return data.size();
  }

  void clear()
  {
    data.clear();
  }

  void free_memory()
  {
    data.free_memory();
  }

  void erase(const T *value)
  {
    const size_t size = data.size();
    for (size_t i = 0; i < size; i++) {
      if (data[i].get() == value) {
        data.erase(data.begin() + i);
        return;
      }
    }

    assert(0);
  }

  /* Slightly faster erase swapping with other element instead of moving many,
   * but will change element order. */
  void erase_by_swap(const T *value)
  {
    const size_t size = data.size();
    for (size_t i = 0; i < size; i++) {
      if (data[i].get() == value) {
        swap(data[i], data[data.size() - 1]);
        break;
      }
    }
    data.resize(data.size() - 1);
  }

  void erase_in_set(const set<T *> &values)
  {
    size_t new_size = data.size();

    for (size_t i = 0; i < new_size; i++) {
      T *value = data[i].get();
      if (values.find(value) != values.end()) {
        swap(data[i], data[new_size - 1]);
        i -= 1;
        new_size -= 1;
      }
    }

    data.resize(new_size);
  }

  /* Basic iterators for range based for loop. */
  struct ConstIterator {
    typename vector<unique_ptr<T>>::const_iterator it;

    const T *operator*() const
    {
      return it->get();
    }
    bool operator!=(const ConstIterator &other) const
    {
      return it != other.it;
    }
    void operator++()
    {
      ++it;
    }
  };

  ConstIterator begin() const
  {
    return ConstIterator{data.begin()};
  }
  ConstIterator end() const
  {
    return ConstIterator{data.end()};
  }

  struct Iterator {
    typename vector<unique_ptr<T>>::const_iterator it;

    T *operator*() const
    {
      return it->get();
    }
    bool operator!=(const Iterator &other) const
    {
      return it != other.it;
    }
    void operator++()
    {
      ++it;
    }
  };
  Iterator begin()
  {
    return Iterator{data.begin()};
  }
  Iterator end()
  {
    return Iterator{data.end()};
  }

  /* Cast to read-only regular vector for easier interop.
   * Assumes unique_ptr is zero overhead. */
  operator const vector<T *> &()
  {
    static_assert(sizeof(unique_ptr<T>) == sizeof(T *));
    return reinterpret_cast<vector<T *> &>(*this);
  }

  /* For sorting unique_ptr instead of pointer. */
  template<typename Compare> void stable_sort(Compare compare)
  {
    auto compare_unique_ptr = [compare](const unique_ptr<T> &a, const unique_ptr<T> &b) {
      return compare(a.get(), b.get());
    };

    std::stable_sort(data.begin(), data.end(), compare_unique_ptr);
  }
};

CCL_NAMESPACE_END
