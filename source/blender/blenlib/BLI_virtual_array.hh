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

#include "BLI_array.hh"
#include "BLI_index_mask.hh"
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

  IndexRange index_range() const
  {
    return IndexRange(size_);
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
  Span<T> get_internal_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return {};
    }
    return this->get_internal_span_impl();
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
  T get_internal_single() const
  {
    BLI_assert(this->is_single());
    if (size_ == 1) {
      return this->get(0);
    }
    return this->get_internal_single_impl();
  }

  /* Get the element at a specific index. Note that this operator cannot be used to assign values
   * to an index, because the return value is not a reference. */
  T operator[](const int64_t index) const
  {
    return this->get(index);
  }

  /* Copy the entire virtual array into a span. */
  void materialize(MutableSpan<T> r_span) const
  {
    this->materialize(IndexMask(size_), r_span);
  }

  /* Copy some indices of the virtual array into a span. */
  void materialize(IndexMask mask, MutableSpan<T> r_span) const
  {
    BLI_assert(mask.min_array_size() <= size_);
    this->materialize_impl(mask, r_span);
  }

  void materialize_to_uninitialized(MutableSpan<T> r_span) const
  {
    this->materialize_to_uninitialized(IndexMask(size_), r_span);
  }

  void materialize_to_uninitialized(IndexMask mask, MutableSpan<T> r_span) const
  {
    BLI_assert(mask.min_array_size() <= size_);
    this->materialize_to_uninitialized_impl(mask, r_span);
  }

 protected:
  virtual T get_impl(const int64_t index) const = 0;

  virtual bool is_span_impl() const
  {
    return false;
  }

  virtual Span<T> get_internal_span_impl() const
  {
    BLI_assert_unreachable();
    return {};
  }

  virtual bool is_single_impl() const
  {
    return false;
  }

  virtual T get_internal_single_impl() const
  {
    /* Provide a default implementation, so that subclasses don't have to provide it. This method
     * should never be called because `is_single_impl` returns false by default. */
    BLI_assert_unreachable();
    return T();
  }

  virtual void materialize_impl(IndexMask mask, MutableSpan<T> r_span) const
  {
    T *dst = r_span.data();
    if (this->is_span()) {
      const T *src = this->get_internal_span().data();
      mask.foreach_index([&](const int64_t i) { dst[i] = src[i]; });
    }
    else if (this->is_single()) {
      const T single = this->get_internal_single();
      mask.foreach_index([&](const int64_t i) { dst[i] = single; });
    }
    else {
      mask.foreach_index([&](const int64_t i) { dst[i] = this->get(i); });
    }
  }

  virtual void materialize_to_uninitialized_impl(IndexMask mask, MutableSpan<T> r_span) const
  {
    T *dst = r_span.data();
    if (this->is_span()) {
      const T *src = this->get_internal_span().data();
      mask.foreach_index([&](const int64_t i) { new (dst + i) T(src[i]); });
    }
    else if (this->is_single()) {
      const T single = this->get_internal_single();
      mask.foreach_index([&](const int64_t i) { new (dst + i) T(single); });
    }
    else {
      mask.foreach_index([&](const int64_t i) { new (dst + i) T(this->get(i)); });
    }
  }
};

/* Similar to VArray, but the elements are mutable. */
template<typename T> class VMutableArray : public VArray<T> {
 public:
  VMutableArray(const int64_t size) : VArray<T>(size)
  {
  }

  void set(const int64_t index, T value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < this->size_);
    this->set_impl(index, std::move(value));
  }

  /* Copy the values from the source span to all elements in the virtual array. */
  void set_all(Span<T> src)
  {
    BLI_assert(src.size() == this->size_);
    this->set_all_impl(src);
  }

  MutableSpan<T> get_internal_span()
  {
    BLI_assert(this->is_span());
    Span<T> span = static_cast<const VArray<T> *>(this)->get_internal_span();
    return MutableSpan<T>(const_cast<T *>(span.data()), span.size());
  }

 protected:
  virtual void set_impl(const int64_t index, T value) = 0;

  virtual void set_all_impl(Span<T> src)
  {
    if (this->is_span()) {
      const MutableSpan<T> span = this->get_internal_span();
      initialized_copy_n(src.data(), this->size_, span.data());
    }
    else {
      const int64_t size = this->size_;
      for (int64_t i = 0; i < size; i++) {
        this->set(i, src[i]);
      }
    }
  }
};

template<typename T> using VArrayPtr = std::unique_ptr<VArray<T>>;
template<typename T> using VMutableArrayPtr = std::unique_ptr<VMutableArray<T>>;

/**
 * A virtual array implementation for a span. Methods in this class are final so that it can be
 * devirtualized by the compiler in some cases (e.g. when #devirtualize_varray is used).
 */
template<typename T> class VArray_For_Span : public VArray<T> {
 protected:
  const T *data_ = nullptr;

 public:
  VArray_For_Span(const Span<T> data) : VArray<T>(data.size()), data_(data.data())
  {
  }

 protected:
  VArray_For_Span(const int64_t size) : VArray<T>(size)
  {
  }

  T get_impl(const int64_t index) const final
  {
    return data_[index];
  }

  bool is_span_impl() const final
  {
    return true;
  }

  Span<T> get_internal_span_impl() const final
  {
    return Span<T>(data_, this->size_);
  }
};

template<typename T> class VMutableArray_For_MutableSpan : public VMutableArray<T> {
 protected:
  T *data_ = nullptr;

 public:
  VMutableArray_For_MutableSpan(const MutableSpan<T> data)
      : VMutableArray<T>(data.size()), data_(data.data())
  {
  }

 protected:
  VMutableArray_For_MutableSpan(const int64_t size) : VMutableArray<T>(size)
  {
  }

  T get_impl(const int64_t index) const final
  {
    return data_[index];
  }

  void set_impl(const int64_t index, T value) final
  {
    data_[index] = value;
  }

  bool is_span_impl() const override
  {
    return true;
  }

  Span<T> get_internal_span_impl() const override
  {
    return Span<T>(data_, this->size_);
  }
};

/**
 * A variant of `VArray_For_Span` that owns the underlying data.
 * The `Container` type has to implement a `size()` and `data()` method.
 * The `data()` method has to return a pointer to the first element in the continuous array of
 * elements.
 */
template<typename Container, typename T = typename Container::value_type>
class VArray_For_ArrayContainer : public VArray_For_Span<T> {
 private:
  Container container_;

 public:
  VArray_For_ArrayContainer(Container container)
      : VArray_For_Span<T>((int64_t)container.size()), container_(std::move(container))
  {
    this->data_ = container_.data();
  }
};

/**
 * A virtual array implementation that returns the same value for every index. This class is final
 * so that it can be devirtualized by the compiler in some cases (e.g. when #devirtualize_varray is
 * used).
 */
template<typename T> class VArray_For_Single final : public VArray<T> {
 private:
  T value_;

 public:
  VArray_For_Single(T value, const int64_t size) : VArray<T>(size), value_(std::move(value))
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

  Span<T> get_internal_span_impl() const override
  {
    return Span<T>(&value_, 1);
  }

  bool is_single_impl() const override
  {
    return true;
  }

  T get_internal_single_impl() const override
  {
    return value_;
  }
};

/**
 * In many cases a virtual array is a span internally. In those cases, access to individual could
 * be much more efficient than calling a virtual method. When the underlying virtual array is not a
 * span, this class allocates a new array and copies the values over.
 *
 * This should be used in those cases:
 *  - All elements in the virtual array are accessed multiple times.
 *  - In most cases, the underlying virtual array is a span, so no copy is necessary to benefit
 *    from faster access.
 *  - An API is called, that does not accept virtual arrays, but only spans.
 */
template<typename T> class VArray_Span final : public Span<T> {
 private:
  const VArray<T> &varray_;
  Array<T> owned_data_;

 public:
  VArray_Span(const VArray<T> &varray) : Span<T>(), varray_(varray)
  {
    this->size_ = varray_.size();
    if (varray_.is_span()) {
      this->data_ = varray_.get_internal_span().data();
    }
    else {
      owned_data_.~Array();
      new (&owned_data_) Array<T>(varray_.size(), NoInitialization{});
      varray_.materialize_to_uninitialized(owned_data_);
      this->data_ = owned_data_.data();
    }
  }
};

/**
 * Same as VArray_Span, but for a mutable span.
 * The important thing to note is that when changing this span, the results might not be
 * immediately reflected in the underlying virtual array (only when the virtual array is a span
 * internally). The #save method can be used to write all changes to the underlying virtual array,
 * if necessary.
 */
template<typename T> class VMutableArray_Span final : public MutableSpan<T> {
 private:
  VMutableArray<T> &varray_;
  Array<T> owned_data_;
  bool save_has_been_called_ = false;
  bool show_not_saved_warning_ = true;

 public:
  /* Create a span for any virtual array. This is cheap when the virtual array is a span itself. If
   * not, a new array has to be allocated as a wrapper for the underlying virtual array. */
  VMutableArray_Span(VMutableArray<T> &varray, const bool copy_values_to_span = true)
      : MutableSpan<T>(), varray_(varray)
  {
    this->size_ = varray_.size();
    if (varray_.is_span()) {
      this->data_ = varray_.get_internal_span().data();
    }
    else {
      if (copy_values_to_span) {
        owned_data_.~Array();
        new (&owned_data_) Array<T>(varray_.size(), NoInitialization{});
        varray_.materialize_to_uninitialized(owned_data_);
      }
      else {
        owned_data_.reinitialize(varray_.size());
      }
      this->data_ = owned_data_.data();
    }
  }

  ~VMutableArray_Span()
  {
    if (show_not_saved_warning_) {
      if (!save_has_been_called_) {
        std::cout << "Warning: Call `save()` to make sure that changes persist in all cases.\n";
      }
    }
  }

  /* Write back all values from a temporary allocated array to the underlying virtual array. */
  void save()
  {
    save_has_been_called_ = true;
    if (this->data_ != owned_data_.data()) {
      return;
    }
    varray_.set_all(owned_data_);
  }

  void disable_not_applied_warning()
  {
    show_not_saved_warning_ = false;
  }
};

/**
 * This class makes it easy to create a virtual array for an existing function or lambda. The
 * `GetFunc` should take a single `index` argument and return the value at that index.
 */
template<typename T, typename GetFunc> class VArray_For_Func final : public VArray<T> {
 private:
  GetFunc get_func_;

 public:
  VArray_For_Func(const int64_t size, GetFunc get_func)
      : VArray<T>(size), get_func_(std::move(get_func))
  {
  }

 private:
  T get_impl(const int64_t index) const override
  {
    return get_func_(index);
  }

  void materialize_impl(IndexMask mask, MutableSpan<T> r_span) const override
  {
    T *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { dst[i] = get_func_(i); });
  }

  void materialize_to_uninitialized_impl(IndexMask mask, MutableSpan<T> r_span) const override
  {
    T *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { new (dst + i) T(get_func_(i)); });
  }
};

template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
class VArray_For_DerivedSpan : public VArray<ElemT> {
 private:
  const StructT *data_;

 public:
  VArray_For_DerivedSpan(const Span<StructT> data) : VArray<ElemT>(data.size()), data_(data.data())
  {
  }

 private:
  ElemT get_impl(const int64_t index) const override
  {
    return GetFunc(data_[index]);
  }

  void materialize_impl(IndexMask mask, MutableSpan<ElemT> r_span) const override
  {
    ElemT *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { dst[i] = GetFunc(data_[i]); });
  }

  void materialize_to_uninitialized_impl(IndexMask mask, MutableSpan<ElemT> r_span) const override
  {
    ElemT *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { new (dst + i) ElemT(GetFunc(data_[i])); });
  }
};

template<typename StructT,
         typename ElemT,
         ElemT (*GetFunc)(const StructT &),
         void (*SetFunc)(StructT &, ElemT)>
class VMutableArray_For_DerivedSpan : public VMutableArray<ElemT> {
 private:
  StructT *data_;

 public:
  VMutableArray_For_DerivedSpan(const MutableSpan<StructT> data)
      : VMutableArray<ElemT>(data.size()), data_(data.data())
  {
  }

 private:
  ElemT get_impl(const int64_t index) const override
  {
    return GetFunc(data_[index]);
  }

  void set_impl(const int64_t index, ElemT value) override
  {
    SetFunc(data_[index], std::move(value));
  }

  void materialize_impl(IndexMask mask, MutableSpan<ElemT> r_span) const override
  {
    ElemT *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { dst[i] = GetFunc(data_[i]); });
  }

  void materialize_to_uninitialized_impl(IndexMask mask, MutableSpan<ElemT> r_span) const override
  {
    ElemT *dst = r_span.data();
    mask.foreach_index([&](const int64_t i) { new (dst + i) ElemT(GetFunc(data_[i])); });
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
      /* `VArray_For_Single` can be used for devirtualization, because it is declared `final`. */
      const VArray_For_Single<T> varray_single{varray.get_internal_single(), varray.size()};
      func(varray_single);
      return;
    }
    if (varray.is_span()) {
      /* `VArray_For_Span` can be used for devirtualization, because it is declared `final`. */
      const VArray_For_Span<T> varray_span{varray.get_internal_span()};
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
      const VArray_For_Span<T1> varray1_span{varray1.get_internal_span()};
      const VArray_For_Span<T2> varray2_span{varray2.get_internal_span()};
      func(varray1_span, varray2_span);
      return;
    }
    if (is_span1 && is_single2) {
      const VArray_For_Span<T1> varray1_span{varray1.get_internal_span()};
      const VArray_For_Single<T2> varray2_single{varray2.get_internal_single(), varray2.size()};
      func(varray1_span, varray2_single);
      return;
    }
    if (is_single1 && is_span2) {
      const VArray_For_Single<T1> varray1_single{varray1.get_internal_single(), varray1.size()};
      const VArray_For_Span<T2> varray2_span{varray2.get_internal_span()};
      func(varray1_single, varray2_span);
      return;
    }
    if (is_single1 && is_single2) {
      const VArray_For_Single<T1> varray1_single{varray1.get_internal_single(), varray1.size()};
      const VArray_For_Single<T2> varray2_single{varray2.get_internal_single(), varray2.size()};
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
