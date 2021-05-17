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
 * \ingroup fn
 *
 * A generic virtual array is the same as a virtual array from blenlib, except for the fact that
 * the data type is only known at runtime.
 */

#include <optional>

#include "BLI_virtual_array.hh"

#include "FN_generic_span.hh"

namespace blender::fn {

template<typename T> class GVArray_Typed;
template<typename T> class GVMutableArray_Typed;

class GVArray;
class GVMutableArray;

using GVArrayPtr = std::unique_ptr<GVArray>;
using GVMutableArrayPtr = std::unique_ptr<GVMutableArray>;

/* A generically typed version of `VArray<T>`. */
class GVArray {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVArray(const CPPType &type, const int64_t size) : type_(&type), size_(size)
  {
    BLI_assert(size_ >= 0);
  }

  virtual ~GVArray() = default;

  const CPPType &type() const
  {
    return *type_;
  }

  int64_t size() const
  {
    return size_;
  }

  bool is_empty() const
  {
    return size_;
  }

  /* Copies the value at the given index into the provided storage. The `r_value` pointer is
   * expected to point to initialized memory. */
  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->get_impl(index, r_value);
  }

  /* Same as `get`, but `r_value` is expected to point to uninitialized memory. */
  void get_to_uninitialized(const int64_t index, void *r_value) const
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->get_to_uninitialized_impl(index, r_value);
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
  GSpan get_internal_span() const
  {
    BLI_assert(this->is_span());
    if (size_ == 0) {
      return GSpan(*type_);
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

  /* Copies the value that is used for every element into `r_value`, which is expected to point to
   * initialized memory. This invokes undefined behavior if the virtual array would not return the
   * same value for every index. */
  void get_internal_single(void *r_value) const
  {
    BLI_assert(this->is_single());
    if (size_ == 1) {
      this->get(0, r_value);
      return;
    }
    this->get_internal_single_impl(r_value);
  }

  /* Same as `get_internal_single`, but `r_value` points to initialized memory. */
  void get_single_to_uninitialized(void *r_value) const
  {
    type_->construct_default(r_value);
    this->get_internal_single(r_value);
  }

  void materialize(void *dst) const;
  void materialize(const IndexMask mask, void *dst) const;

  void materialize_to_uninitialized(void *dst) const;
  void materialize_to_uninitialized(const IndexMask mask, void *dst) const;

  template<typename T> const VArray<T> *try_get_internal_varray() const
  {
    BLI_assert(type_->is<T>());
    return (const VArray<T> *)this->try_get_internal_varray_impl();
  }

  /* Create a typed virtual array for this generic virtual array. */
  template<typename T> GVArray_Typed<T> typed() const
  {
    return GVArray_Typed<T>(*this);
  }

  GVArrayPtr shallow_copy() const;

 protected:
  virtual void get_impl(const int64_t index, void *r_value) const;
  virtual void get_to_uninitialized_impl(const int64_t index, void *r_value) const = 0;

  virtual bool is_span_impl() const;
  virtual GSpan get_internal_span_impl() const;

  virtual bool is_single_impl() const;
  virtual void get_internal_single_impl(void *UNUSED(r_value)) const;

  virtual void materialize_impl(const IndexMask mask, void *dst) const;
  virtual void materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const;

  virtual const void *try_get_internal_varray_impl() const;
};

/* Similar to GVArray, but supports changing the elements in the virtual array. */
class GVMutableArray : public GVArray {
 public:
  GVMutableArray(const CPPType &type, const int64_t size) : GVArray(type, size)
  {
  }

  void set_by_copy(const int64_t index, const void *value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->set_by_copy_impl(index, value);
  }

  void set_by_move(const int64_t index, void *value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->set_by_move_impl(index, value);
  }

  void set_by_relocate(const int64_t index, void *value)
  {
    BLI_assert(index >= 0);
    BLI_assert(index < size_);
    this->set_by_relocate_impl(index, value);
  }

  GMutableSpan get_internal_span()
  {
    BLI_assert(this->is_span());
    GSpan span = static_cast<const GVArray *>(this)->get_internal_span();
    return GMutableSpan(span.type(), const_cast<void *>(span.data()), span.size());
  }

  template<typename T> VMutableArray<T> *try_get_internal_mutable_varray()
  {
    BLI_assert(type_->is<T>());
    return (VMutableArray<T> *)this->try_get_internal_mutable_varray_impl();
  }

  /* Create a typed virtual array for this generic virtual array. */
  template<typename T> GVMutableArray_Typed<T> typed()
  {
    return GVMutableArray_Typed<T>(*this);
  }

  void fill(const void *value);

  /* Copy the values from the source buffer to all elements in the virtual array. */
  void set_all(const void *src)
  {
    this->set_all_impl(src);
  }

 protected:
  virtual void set_by_copy_impl(const int64_t index, const void *value);
  virtual void set_by_relocate_impl(const int64_t index, void *value);
  virtual void set_by_move_impl(const int64_t index, void *value) = 0;

  virtual void set_all_impl(const void *src);

  virtual void *try_get_internal_mutable_varray_impl();
};

class GVArray_For_GSpan : public GVArray {
 protected:
  const void *data_ = nullptr;
  const int64_t element_size_;

 public:
  GVArray_For_GSpan(const GSpan span)
      : GVArray(span.type(), span.size()), data_(span.data()), element_size_(span.type().size())
  {
  }

 protected:
  GVArray_For_GSpan(const CPPType &type, const int64_t size)
      : GVArray(type, size), element_size_(type.size())
  {
  }

  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  bool is_span_impl() const override;
  GSpan get_internal_span_impl() const override;
};

class GVArray_For_Empty : public GVArray {
 public:
  GVArray_For_Empty(const CPPType &type) : GVArray(type, 0)
  {
  }

 protected:
  void get_to_uninitialized_impl(const int64_t UNUSED(index), void *UNUSED(r_value)) const override
  {
    BLI_assert(false);
  }
};

class GVMutableArray_For_GMutableSpan : public GVMutableArray {
 protected:
  void *data_ = nullptr;
  const int64_t element_size_;

 public:
  GVMutableArray_For_GMutableSpan(const GMutableSpan span)
      : GVMutableArray(span.type(), span.size()),
        data_(span.data()),
        element_size_(span.type().size())
  {
  }

 protected:
  GVMutableArray_For_GMutableSpan(const CPPType &type, const int64_t size)
      : GVMutableArray(type, size), element_size_(type.size())
  {
  }

  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  void set_by_copy_impl(const int64_t index, const void *value) override;
  void set_by_move_impl(const int64_t index, void *value) override;
  void set_by_relocate_impl(const int64_t index, void *value) override;

  bool is_span_impl() const override;
  GSpan get_internal_span_impl() const override;
};

/* Generic virtual array where each element has the same value. The value is not owned. */
class GVArray_For_SingleValueRef : public GVArray {
 protected:
  const void *value_ = nullptr;

 public:
  GVArray_For_SingleValueRef(const CPPType &type, const int64_t size, const void *value)
      : GVArray(type, size), value_(value)
  {
  }

 protected:
  GVArray_For_SingleValueRef(const CPPType &type, const int64_t size) : GVArray(type, size)
  {
  }

  void get_impl(const int64_t index, void *r_value) const override;
  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override;

  bool is_span_impl() const override;
  GSpan get_internal_span_impl() const override;

  bool is_single_impl() const override;
  void get_internal_single_impl(void *r_value) const override;
};

/* Same as GVArray_For_SingleValueRef, but the value is owned. */
class GVArray_For_SingleValue : public GVArray_For_SingleValueRef {
 public:
  GVArray_For_SingleValue(const CPPType &type, const int64_t size, const void *value);
  ~GVArray_For_SingleValue();
};

/* Used to convert a typed virtual array into a generic one. */
template<typename T> class GVArray_For_VArray : public GVArray {
 protected:
  const VArray<T> *varray_ = nullptr;

 public:
  GVArray_For_VArray(const VArray<T> &varray)
      : GVArray(CPPType::get<T>(), varray.size()), varray_(&varray)
  {
  }

 protected:
  GVArray_For_VArray(const int64_t size) : GVArray(CPPType::get<T>(), size)
  {
  }

  void get_impl(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = varray_->get(index);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) T(varray_->get(index));
  }

  bool is_span_impl() const override
  {
    return varray_->is_span();
  }

  GSpan get_internal_span_impl() const override
  {
    return GSpan(varray_->get_internal_span());
  }

  bool is_single_impl() const override
  {
    return varray_->is_single();
  }

  void get_internal_single_impl(void *r_value) const override
  {
    *(T *)r_value = varray_->get_internal_single();
  }

  void materialize_impl(const IndexMask mask, void *dst) const override
  {
    varray_->materialize(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  void materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const override
  {
    varray_->materialize_to_uninitialized(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  const void *try_get_internal_varray_impl() const override
  {
    return varray_;
  }
};

/* Used to convert any generic virtual array into a typed one. */
template<typename T> class VArray_For_GVArray : public VArray<T> {
 protected:
  const GVArray *varray_ = nullptr;

 public:
  VArray_For_GVArray(const GVArray &varray) : VArray<T>(varray.size()), varray_(&varray)
  {
    BLI_assert(varray_->type().template is<T>());
  }

 protected:
  VArray_For_GVArray(const int64_t size) : VArray<T>(size)
  {
  }

  T get_impl(const int64_t index) const override
  {
    T value;
    varray_->get(index, &value);
    return value;
  }

  bool is_span_impl() const override
  {
    return varray_->is_span();
  }

  Span<T> get_internal_span_impl() const override
  {
    return varray_->get_internal_span().template typed<T>();
  }

  bool is_single_impl() const override
  {
    return varray_->is_single();
  }

  T get_internal_single_impl() const override
  {
    T value;
    varray_->get_internal_single(&value);
    return value;
  }
};

/* Used to convert an generic mutable virtual array into a typed one. */
template<typename T> class VMutableArray_For_GVMutableArray : public VMutableArray<T> {
 protected:
  GVMutableArray *varray_ = nullptr;

 public:
  VMutableArray_For_GVMutableArray(GVMutableArray &varray)
      : VMutableArray<T>(varray.size()), varray_(&varray)
  {
    BLI_assert(varray.type().template is<T>());
  }

  VMutableArray_For_GVMutableArray(const int64_t size) : VMutableArray<T>(size)
  {
  }

 private:
  T get_impl(const int64_t index) const override
  {
    T value;
    varray_->get(index, &value);
    return value;
  }

  void set_impl(const int64_t index, T value) override
  {
    varray_->set_by_relocate(index, &value);
  }

  bool is_span_impl() const override
  {
    return varray_->is_span();
  }

  Span<T> get_internal_span_impl() const override
  {
    return varray_->get_internal_span().template typed<T>();
  }

  bool is_single_impl() const override
  {
    return varray_->is_single();
  }

  T get_internal_single_impl() const override
  {
    T value;
    varray_->get_internal_single(&value);
    return value;
  }
};

/* Used to convert any typed virtual mutable array into a generic one. */
template<typename T> class GVMutableArray_For_VMutableArray : public GVMutableArray {
 protected:
  VMutableArray<T> *varray_ = nullptr;

 public:
  GVMutableArray_For_VMutableArray(VMutableArray<T> &varray)
      : GVMutableArray(CPPType::get<T>(), varray.size()), varray_(&varray)
  {
  }

 protected:
  GVMutableArray_For_VMutableArray(const int64_t size) : GVMutableArray(CPPType::get<T>(), size)
  {
  }

  void get_impl(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = varray_->get(index);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    new (r_value) T(varray_->get(index));
  }

  bool is_span_impl() const override
  {
    return varray_->is_span();
  }

  GSpan get_internal_span_impl() const override
  {
    Span<T> span = varray_->get_internal_span();
    return span;
  }

  bool is_single_impl() const override
  {
    return varray_->is_single();
  }

  void get_internal_single_impl(void *r_value) const override
  {
    *(T *)r_value = varray_->get_internal_single();
  }

  void set_by_copy_impl(const int64_t index, const void *value) override
  {
    const T &value_ = *(const T *)value;
    varray_->set(index, value_);
  }

  void set_by_relocate_impl(const int64_t index, void *value) override
  {
    T &value_ = *(T *)value;
    varray_->set(index, std::move(value_));
    value_.~T();
  }

  void set_by_move_impl(const int64_t index, void *value) override
  {
    T &value_ = *(T *)value;
    varray_->set(index, std::move(value_));
  }

  void set_all_impl(const void *src) override
  {
    varray_->set_all(Span((T *)src, size_));
  }

  void materialize_impl(const IndexMask mask, void *dst) const override
  {
    varray_->materialize(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  void materialize_to_uninitialized_impl(const IndexMask mask, void *dst) const override
  {
    varray_->materialize_to_uninitialized(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  const void *try_get_internal_varray_impl() const override
  {
    return (const VArray<T> *)varray_;
  }

  void *try_get_internal_mutable_varray_impl() override
  {
    return varray_;
  }
};

/* A generic version of VArray_Span. */
class GVArray_GSpan : public GSpan {
 private:
  const GVArray &varray_;
  void *owned_data_ = nullptr;

 public:
  GVArray_GSpan(const GVArray &varray);
  ~GVArray_GSpan();
};

/* A generic version of VMutableArray_Span. */
class GVMutableArray_GSpan : public GMutableSpan {
 private:
  GVMutableArray &varray_;
  void *owned_data_ = nullptr;
  bool save_has_been_called_ = false;
  bool show_not_saved_warning_ = true;

 public:
  GVMutableArray_GSpan(GVMutableArray &varray, bool copy_values_to_span = true);
  ~GVMutableArray_GSpan();

  void save();
  void disable_not_applied_warning();
};

/* Similar to GVArray_GSpan, but the resulting span is typed. */
template<typename T> class GVArray_Span : public Span<T> {
 private:
  GVArray_GSpan varray_gspan_;

 public:
  GVArray_Span(const GVArray &varray) : varray_gspan_(varray)
  {
    BLI_assert(varray.type().is<T>());
    this->data_ = (const T *)varray_gspan_.data();
    this->size_ = varray_gspan_.size();
  }
};

template<typename T> class GVArray_For_OwnedVArray : public GVArray_For_VArray<T> {
 private:
  VArrayPtr<T> owned_varray_;

 public:
  /* Takes ownership of varray and passes a reference to the base class. */
  GVArray_For_OwnedVArray(VArrayPtr<T> varray)
      : GVArray_For_VArray<T>(*varray), owned_varray_(std::move(varray))
  {
  }
};

template<typename T> class VArray_For_OwnedGVArray : public VArray_For_GVArray<T> {
 private:
  GVArrayPtr owned_varray_;

 public:
  /* Takes ownership of varray and passes a reference to the base class. */
  VArray_For_OwnedGVArray(GVArrayPtr varray)
      : VArray_For_GVArray<T>(*varray), owned_varray_(std::move(varray))
  {
  }
};

template<typename T>
class GVMutableArray_For_OwnedVMutableArray : public GVMutableArray_For_VMutableArray<T> {
 private:
  VMutableArrayPtr<T> owned_varray_;

 public:
  /* Takes ownership of varray and passes a reference to the base class. */
  GVMutableArray_For_OwnedVMutableArray(VMutableArrayPtr<T> varray)
      : GVMutableArray_For_VMutableArray<T>(*varray), owned_varray_(std::move(varray))
  {
  }
};

template<typename T>
class VMutableArray_For_OwnedGVMutableArray : public VMutableArray_For_GVMutableArray<T> {
 private:
  GVMutableArrayPtr owned_varray_;

 public:
  /* Takes ownership of varray and passes a reference to the base class. */
  VMutableArray_For_OwnedGVMutableArray(GVMutableArrayPtr varray)
      : VMutableArray_For_GVMutableArray<T>(*varray), owned_varray_(std::move(varray))
  {
  }
};

/* Utility to embed a typed virtual array into a generic one. This avoids one allocation and give
 * the compiler more opportunity to optimize the generic virtual array. */
template<typename T, typename VArrayT>
class GVArray_For_EmbeddedVArray : public GVArray_For_VArray<T> {
 private:
  VArrayT embedded_varray_;

 public:
  template<typename... Args>
  GVArray_For_EmbeddedVArray(const int64_t size, Args &&... args)
      : GVArray_For_VArray<T>(size), embedded_varray_(std::forward<Args>(args)...)
  {
    this->varray_ = &embedded_varray_;
  }
};

/* Same as GVArray_For_EmbeddedVArray, but for mutable virtual arrays. */
template<typename T, typename VMutableArrayT>
class GVMutableArray_For_EmbeddedVMutableArray : public GVMutableArray_For_VMutableArray<T> {
 private:
  VMutableArrayT embedded_varray_;

 public:
  template<typename... Args>
  GVMutableArray_For_EmbeddedVMutableArray(const int64_t size, Args &&... args)
      : GVMutableArray_For_VMutableArray<T>(size), embedded_varray_(std::forward<Args>(args)...)
  {
    this->varray_ = &embedded_varray_;
  }
};

/* Same as VArray_For_ArrayContainer, but for a generic virtual array. */
template<typename Container, typename T = typename Container::value_type>
class GVArray_For_ArrayContainer
    : public GVArray_For_EmbeddedVArray<T, VArray_For_ArrayContainer<Container, T>> {
 public:
  GVArray_For_ArrayContainer(Container container)
      : GVArray_For_EmbeddedVArray<T, VArray_For_ArrayContainer<Container, T>>(
            container.size(), std::move(container))
  {
  }
};

/* Same as VArray_For_DerivedSpan, but for a generic virtual array. */
template<typename StructT, typename ElemT, ElemT (*GetFunc)(const StructT &)>
class GVArray_For_DerivedSpan
    : public GVArray_For_EmbeddedVArray<ElemT, VArray_For_DerivedSpan<StructT, ElemT, GetFunc>> {
 public:
  GVArray_For_DerivedSpan(const Span<StructT> data)
      : GVArray_For_EmbeddedVArray<ElemT, VArray_For_DerivedSpan<StructT, ElemT, GetFunc>>(
            data.size(), data)
  {
  }
};

/* Same as VMutableArray_For_DerivedSpan, but for a generic virtual array. */
template<typename StructT,
         typename ElemT,
         ElemT (*GetFunc)(const StructT &),
         void (*SetFunc)(StructT &, ElemT)>
class GVMutableArray_For_DerivedSpan
    : public GVMutableArray_For_EmbeddedVMutableArray<
          ElemT,
          VMutableArray_For_DerivedSpan<StructT, ElemT, GetFunc, SetFunc>> {
 public:
  GVMutableArray_For_DerivedSpan(const MutableSpan<StructT> data)
      : GVMutableArray_For_EmbeddedVMutableArray<
            ElemT,
            VMutableArray_For_DerivedSpan<StructT, ElemT, GetFunc, SetFunc>>(data.size(), data)
  {
  }
};

/* Same as VArray_For_Span, but for a generic virtual array. */
template<typename T>
class GVArray_For_Span : public GVArray_For_EmbeddedVArray<T, VArray_For_Span<T>> {
 public:
  GVArray_For_Span(const Span<T> data)
      : GVArray_For_EmbeddedVArray<T, VArray_For_Span<T>>(data.size(), data)
  {
  }
};

/* Same as VMutableArray_For_MutableSpan, but for a generic virtual array. */
template<typename T>
class GVMutableArray_For_MutableSpan
    : public GVMutableArray_For_EmbeddedVMutableArray<T, VMutableArray_For_MutableSpan<T>> {
 public:
  GVMutableArray_For_MutableSpan(const MutableSpan<T> data)
      : GVMutableArray_For_EmbeddedVMutableArray<T, VMutableArray_For_MutableSpan<T>>(data.size(),
                                                                                      data)
  {
  }
};

/**
 * Utility class to create the "best" typed virtual array for a given generic virtual array.
 * In most cases we don't just want to use VArray_For_GVArray, because it adds an additional
 * indirection on element-access that can be avoided in many cases (e.g. when the virtual array is
 * just a span or single value).
 *
 * This is not a virtual array itself, but is used to get a virtual array.
 */
template<typename T> class GVArray_Typed {
 private:
  const VArray<T> *varray_;
  /* Of these optional virtual arrays, at most one is constructed at any time. */
  std::optional<VArray_For_Span<T>> varray_span_;
  std::optional<VArray_For_Single<T>> varray_single_;
  std::optional<VArray_For_GVArray<T>> varray_any_;
  GVArrayPtr owned_gvarray_;

 public:
  explicit GVArray_Typed(const GVArray &gvarray)
  {
    BLI_assert(gvarray.type().is<T>());
    if (gvarray.is_span()) {
      const GSpan span = gvarray.get_internal_span();
      varray_span_.emplace(span.typed<T>());
      varray_ = &*varray_span_;
    }
    else if (gvarray.is_single()) {
      T single_value;
      gvarray.get_internal_single(&single_value);
      varray_single_.emplace(single_value, gvarray.size());
      varray_ = &*varray_single_;
    }
    else if (const VArray<T> *internal_varray = gvarray.try_get_internal_varray<T>()) {
      varray_ = internal_varray;
    }
    else {
      varray_any_.emplace(gvarray);
      varray_ = &*varray_any_;
    }
  }

  /* Same as the constructor above, but also takes ownership of the passed in virtual array. */
  explicit GVArray_Typed(GVArrayPtr gvarray) : GVArray_Typed(*gvarray)
  {
    owned_gvarray_ = std::move(gvarray);
  }

  const VArray<T> &operator*() const
  {
    return *varray_;
  }

  const VArray<T> *operator->() const
  {
    return varray_;
  }

  /* Support implicit cast to the typed virtual array for convenience when `varray->typed<T>()` is
   * used within an expression.  */
  operator const VArray<T> &() const
  {
    return *varray_;
  }

  T operator[](const int64_t index) const
  {
    return varray_->get(index);
  }

  int64_t size() const
  {
    return varray_->size();
  }

  IndexRange index_range() const
  {
    return IndexRange(this->size());
  }
};

/* Same as GVArray_Typed, but for mutable virtual arrays. */
template<typename T> class GVMutableArray_Typed {
 private:
  VMutableArray<T> *varray_;
  std::optional<VMutableArray_For_MutableSpan<T>> varray_span_;
  std::optional<VMutableArray_For_GVMutableArray<T>> varray_any_;
  GVMutableArrayPtr owned_gvarray_;

 public:
  explicit GVMutableArray_Typed(GVMutableArray &gvarray)
  {
    BLI_assert(gvarray.type().is<T>());
    if (gvarray.is_span()) {
      const GMutableSpan span = gvarray.get_internal_span();
      varray_span_.emplace(span.typed<T>());
      varray_ = &*varray_span_;
    }
    else if (VMutableArray<T> *internal_varray = gvarray.try_get_internal_mutable_varray<T>()) {
      varray_ = internal_varray;
    }
    else {
      varray_any_.emplace(gvarray);
      varray_ = &*varray_any_;
    }
  }

  explicit GVMutableArray_Typed(GVMutableArrayPtr gvarray) : GVMutableArray_Typed(*gvarray)
  {
    owned_gvarray_ = std::move(gvarray);
  }

  VMutableArray<T> &operator*()
  {
    return *varray_;
  }

  VMutableArray<T> *operator->()
  {
    return varray_;
  }

  operator VMutableArray<T> &()
  {
    return *varray_;
  }

  T operator[](const int64_t index) const
  {
    return varray_->get(index);
  }

  int64_t size() const
  {
    return varray_->size();
  }
};

}  // namespace blender::fn
