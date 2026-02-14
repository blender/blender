/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <cassert>
#include <utility>

#include "util/algorithm.h"
#include "util/random_access_iterator_mixin.h"
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

  T *back() const
  {
    return data.back().get();
  }

  void push_back(unique_ptr<T> &&value)
  {
    data.push_back(std::move(value));
  }

  void replace(const size_t i, unique_ptr<T> &&value)
  {
    data[i] = std::move(value);
  }

  void resize(const size_t new_size)
  {
    data.resize(new_size);
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

  void erase_by_swap(const size_t index)
  {
    swap(data[index], data[data.size() - 1]);
    data.resize(data.size() - 1);
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
        erase_by_swap(i);
        return;
      }
    }

    assert(0);
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

  /* Remove trailing null entries. */
  void trim()
  {
    while (!data.empty() && !data.back()) {
      data.pop_back();
    }
  }

  /* Basic iterators for range based for loop. */
  struct ConstIterator : public random_access_iterator_mixin<ConstIterator> {
   private:
    using It = typename vector<unique_ptr<T>>::const_iterator;
    It it_;

   public:
    using value_type = const T *;
    using pointer = const T **;
    /** For such derived iterators, this does not have to be an actual reference. */
    using reference = value_type;

    ConstIterator(It it) : it_(it) {}

    const T *operator*() const
    {
      return it_->get();
    }

    const It &iter_prop() const
    {
      return it_;
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

  struct Iterator : public random_access_iterator_mixin<Iterator> {
   private:
    using It = typename vector<unique_ptr<T>>::iterator;
    It it_;

   public:
    using value_type = T *;
    using pointer = T **;
    /** For such derived iterators, this does not have to be an actual reference. */
    using reference = value_type;

    Iterator(It it) : it_(it) {}

    T *operator*() const
    {
      return it_->get();
    }

    const It &iter_prop() const
    {
      return it_;
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

  /* Iterator over vector with index. */
  template<typename UniqueVectorU, typename U> struct EnumerateT {
    UniqueVectorU &vec;

    struct Iterator {
      size_t index;
      UniqueVectorU &vec;

      bool operator!=(const Iterator &other) const
      {
        return index != other.index;
      }

      void operator++()
      {
        index++;
      }

      std::pair<size_t, U *> operator*() const
      {
        return std::make_pair(index, vec[index]);
      }
    };

    Iterator begin()
    {
      return Iterator{0, vec};
    }
    Iterator end()
    {
      return Iterator{vec.size(), vec};
    }
  };

  using Enumerate = EnumerateT<unique_ptr_vector<T>, T>;
  using ConstEnumerate = EnumerateT<const unique_ptr_vector<T>, const T>;

  Enumerate enumerate()
  {
    return Enumerate{*this};
  }

  ConstEnumerate enumerate() const
  {
    return ConstEnumerate{*this};
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
