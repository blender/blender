/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bli
 *
 * A virtual array is a data structure that behaves similar to an array, but its elements are
 * accessed through virtual methods. This improves the decoupling of a function from its callers,
 * because it does not have to know exactly how the data is laid out in memory, or if it is stored
 * in memory at all. It could just as well be computed on the fly.
 *
 * Taking a virtual array as parameter instead of a more specific non-virtual type has some
 * tradeoffs. Access to individual elements of the individual elements is higher due to function
 * call overhead. On the other hand, potential callers don't have to convert the data into the
 * specific format required for the function. This can be a costly conversion if only few of the
 * elements are accessed in the end.
 *
 * Functions taking a virtual array as input can still optimize for different data layouts. For
 * example, they can check if the array is stored as an array internally or if it is the same
 * element for all indices. Whether it is worth to optimize for different data layouts in a
 * function has to be decided on a case by case basis. One should always do some benchmarking to
 * see of the increased compile time and binary size is worth it.
 */

#include "BLI_span.hh"

namespace blender {

/* An immutable virtual array. */
template<typename T> class VArray {
 protected:
  int64_t size_;

 public:
  VArray(const int64_t size) : size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~VArray() = default;

  T get(const int64_t index) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    return this->get_impl(index);
  }

  int64_t size() const
  {
    return size_;
  }

  bool is_empty() const
  {
    return size_ == 0;
  }

  /* Returns true when the virtual array is stored as a span internally. */
  bool is_span() const
  {
    if (size_ == 0) {
      return true;
    }
    return this->is_span_impl();
  }

  /* Returns the internally used span of the virtual array. This invokes undefined behavior is the
   * virtual array is not stored as a span internally. */
  Span<T> get_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return {};
    }
    return this->get_span_impl();
  }

  /* Returns true when the virtual array returns the same value for every index. */
  bool is_single() const
  {
    if (size_ == 1) {
      return true;
    }
    return this->is_single_impl();
  }

  /* Returns the value that is returned for every index. This invokes undefined behavior if the
   * virtual array would not return the same value for every index. */
  T get_single() const
  {
    BLI_assert(this->is_single());
    if (size_ == 1) {
      return this->get(0);
    }
    return this->get_single_impl();
  }

  T operator[](const int64_t index) const
  {
    return this->get(index);
  }

 protected:
  virtual T get_impl(const int64_t index) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual Span<T> get_span_impl() const
  {
    BLI_assert_unreachable();
    return {};
  }

  virtual bool is_single_impl() const
  {
    return false;
  }

  virtual T get_single_impl() const
  {
    /* Provide a default implementation, so that subclasses don't have to provide it. This method
     * should never be called because `is_single_impl` returns false by default. */
    BLI_assert_unreachable();
    return T();
  }
};

/**
 * A virtual array implementation for a span. This class is final so that it can be devirtualized
 * by the compiler in some cases (e.g. when #devirtualize_varray is used).
 */
template<typename T> class VArrayForSpan final : public VArray<T> {
 private:
  const T *data_;

 public:
  VArrayForSpan(const Span<T> data) : VArray<T>(data.size()), data_(data.data())
  {
  }

 protected:
  T get_impl(const int64_t index) const override
  {
    return data_[index];
  }

  bool is_span_impl() const override
  {
    return true;
  }

  Span<T> get_span_impl() const override
  {
    return Span<T>(data_, this->size_);
  }
};

/**
 * A virtual array implementation that returns the same value for every index. This class is final
 * so that it can be devirtualized by the compiler in some cases (e.g. when #devirtualize_varray is
 * used).
 */
template<typename T> class VArrayForSingle final : public VArray<T> {
 private:
  T value_;

 public:
  VArrayForSingle(T value, const int64_t size) : VArray<T>(size), value_(std::move(value))
  {
  }

 protected:
  T get_impl(const int64_t UNUSED(index)) const override
  {
    return value_;
  }

  bool is_span_impl() const override
  {
    return this->size_ == 1;
  }

  Span<T> get_span_impl() const override
  {
    return Span<T>(&value_, 1);
  }

  bool is_single_impl() const override
  {
    return true;
  }

  T get_single_impl() const override
  {
    return value_;
  }
};

/**
 * Generate multiple versions of the given function optimized for different virtual arrays.
 * One has to be careful with nesting multiple devirtualizations, because that results in an
 * exponential number of function instantiations (increasing compile time and binary size).
 *
 * Generally, this function should only be used when the virtual method call overhead to get an
 * element from a virtual array is significant.
 */
template<typename T, typename Func>
inline void devirtualize_varray(const VArray<T> &varray, const Func &func, bool enable = true)
{
  /* Support disabling the devirtualization to simplify benchmarking. */
  if (enable) {
    if (varray.is_single()) {
      /* `VArrayForSingle` can be used for devirtualization, because it is declared `final`. */
      const VArrayForSingle<T> varray_single{varray.get_single(), varray.size()};
      func(varray_single);
      return;
    }
    if (varray.is_span()) {
      /* `VArrayForSpan` can be used for devirtualization, because it is declared `final`. */
      const VArrayForSpan<T> varray_span{varray.get_span()};
      func(varray_span);
      return;
    }
  }
  func(varray);
}

/**
 * Same as `devirtualize_varray`, but devirtualizes two virtual arrays at the same time.
 * This is better than nesting two calls to `devirtualize_varray`, because it instantiates fewer
 * cases.
 */
template<typename T1, typename T2, typename Func>
inline void devirtualize_varray2(const VArray<T1> &varray1,
                                 const VArray<T2> &varray2,
                                 const Func &func,
                                 bool enable = true)
{
  /* Support disabling the devirtualization to simplify benchmarking. */
  if (enable) {
    const bool is_span1 = varray1.is_span();
    const bool is_span2 = varray2.is_span();
    const bool is_single1 = varray1.is_single();
    const bool is_single2 = varray2.is_single();
    if (is_span1 && is_span2) {
      const VArrayForSpan<T1> varray1_span{varray1.get_span()};
      const VArrayForSpan<T2> varray2_span{varray2.get_span()};
      func(varray1_span, varray2_span);
      return;
    }
    if (is_span1 && is_single2) {
      const VArrayForSpan<T1> varray1_span{varray1.get_span()};
      const VArrayForSingle<T2> varray2_single{varray2.get_single(), varray2.size()};
      func(varray1_span, varray2_single);
      return;
    }
    if (is_single1 && is_span2) {
      const VArrayForSingle<T1> varray1_single{varray1.get_single(), varray1.size()};
      const VArrayForSpan<T2> varray2_span{varray2.get_span()};
      func(varray1_single, varray2_span);
      return;
    }
    if (is_single1 && is_single2) {
      const VArrayForSingle<T1> varray1_single{varray1.get_single(), varray1.size()};
      const VArrayForSingle<T2> varray2_single{varray2.get_single(), varray2.size()};
      func(varray1_single, varray2_single);
      return;
    }
  }
  /* This fallback is used even when one of the inputs could be optimized. It's probably not worth
   * it to optimize just one of the inputs, because then the compiler still has to call into
   * unknown code, which inhibits many compiler optimizations. */
  func(varray1, varray2);
}

}  // namespace blender
