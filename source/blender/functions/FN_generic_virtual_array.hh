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

#include "BLI_timeit.hh"
#include "BLI_virtual_array.hh"

#include "FN_generic_array.hh"
#include "FN_generic_span.hh"

namespace blender::fn {

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl and #GVMutableArrayImpl.
 * \{ */

class GVArray;
class GVArrayImpl;
class GVMutableArray;
class GVMutableArrayImpl;

/* A generically typed version of #VArrayImpl. */
class GVArrayImpl {
 protected:
  const CPPType *type_;
  int64_t size_;

 public:
  GVArrayImpl(const CPPType &type, const int64_t size);
  virtual ~GVArrayImpl() = default;

  const CPPType &type() const;

  int64_t size() const;

  virtual void get(const int64_t index, void *r_value) const;
  virtual void get_to_uninitialized(const int64_t index, void *r_value) const = 0;

  virtual bool is_span() const;
  virtual GSpan get_internal_span() const;

  virtual bool is_single() const;
  virtual void get_internal_single(void *UNUSED(r_value)) const;

  virtual void materialize(const IndexMask mask, void *dst) const;
  virtual void materialize_to_uninitialized(const IndexMask mask, void *dst) const;

  virtual bool try_assign_VArray(void *varray) const;
  virtual bool may_have_ownership() const;
};

/* A generic version of #VMutableArrayImpl. */
class GVMutableArrayImpl : public GVArrayImpl {
 public:
  GVMutableArrayImpl(const CPPType &type, const int64_t size);

  virtual void set_by_copy(const int64_t index, const void *value);
  virtual void set_by_relocate(const int64_t index, void *value);
  virtual void set_by_move(const int64_t index, void *value) = 0;

  virtual void set_all(const void *src);

  virtual bool try_assign_VMutableArray(void *varray) const;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArray and #GVMutableArray
 * \{ */

namespace detail {
struct GVArrayAnyExtraInfo {
  const GVArrayImpl *(*get_varray)(const void *buffer) =
      [](const void *UNUSED(buffer)) -> const GVArrayImpl * { return nullptr; };

  template<typename StorageT> static GVArrayAnyExtraInfo get();
};
}  // namespace detail

class GVMutableArray;

/**
 * Utility class to reduce code duplication between #GVArray and #GVMutableArray.
 * It pretty much follows #VArrayCommon. Don't use this class outside of this header.
 */
class GVArrayCommon {
 protected:
  /**
   * See #VArrayCommon for more information. The inline buffer is a bit larger here, because
   * generic virtual array implementations often require a bit more space than typed ones.
   */
  using Storage = Any<detail::GVArrayAnyExtraInfo, 40, 8>;

  const GVArrayImpl *impl_ = nullptr;
  Storage storage_;

 protected:
  GVArrayCommon();
  GVArrayCommon(const GVArrayCommon &other);
  GVArrayCommon(GVArrayCommon &&other) noexcept;
  GVArrayCommon(const GVArrayImpl *impl);
  GVArrayCommon(std::shared_ptr<const GVArrayImpl> impl);
  ~GVArrayCommon();

  template<typename ImplT, typename... Args> void emplace(Args &&...args);

  void copy_from(const GVArrayCommon &other);
  void move_from(GVArrayCommon &&other) noexcept;

  const GVArrayImpl *impl_from_storage() const;

 public:
  const CPPType &type() const;
  operator bool() const;

  int64_t size() const;
  bool is_empty() const;
  IndexRange index_range() const;

  template<typename T> bool try_assign_VArray(VArray<T> &varray) const;
  bool may_have_ownership() const;

  void materialize(void *dst) const;
  void materialize(const IndexMask mask, void *dst) const;

  void materialize_to_uninitialized(void *dst) const;
  void materialize_to_uninitialized(const IndexMask mask, void *dst) const;

  /**
   * Returns true when the virtual array is stored as a span internally.
   */
  bool is_span() const;
  /**
   * Returns the internally used span of the virtual array. This invokes undefined behavior is the
   * virtual array is not stored as a span internally.
   */
  GSpan get_internal_span() const;

  /**
   * Returns true when the virtual array returns the same value for every index.
   */
  bool is_single() const;
  /**
   * Copies the value that is used for every element into `r_value`, which is expected to point to
   * initialized memory. This invokes undefined behavior if the virtual array would not return the
   * same value for every index.
   */
  void get_internal_single(void *r_value) const;
  /**
   * Same as `get_internal_single`, but `r_value` points to initialized memory.
   */
  void get_internal_single_to_uninitialized(void *r_value) const;

  void get(const int64_t index, void *r_value) const;
  /**
   * Returns a copy of the value at the given index. Usually a typed virtual array should
   * be used instead, but sometimes this is simpler when only a few indices are needed.
   */
  template<typename T> T get(const int64_t index) const;
  void get_to_uninitialized(const int64_t index, void *r_value) const;
};

/** Generic version of #VArray. */
class GVArray : public GVArrayCommon {
 private:
  friend GVMutableArray;

 public:
  GVArray() = default;

  GVArray(const GVArray &other);
  GVArray(GVArray &&other) noexcept;
  GVArray(const GVArrayImpl *impl);
  GVArray(std::shared_ptr<const GVArrayImpl> impl);

  template<typename T> GVArray(const VArray<T> &varray);
  template<typename T> VArray<T> typed() const;

  template<typename ImplT, typename... Args> static GVArray For(Args &&...args);

  static GVArray ForSingle(const CPPType &type, const int64_t size, const void *value);
  static GVArray ForSingleRef(const CPPType &type, const int64_t size, const void *value);
  static GVArray ForSingleDefault(const CPPType &type, const int64_t size);
  static GVArray ForSpan(GSpan span);
  static GVArray ForGArray(GArray<> array);
  static GVArray ForEmpty(const CPPType &type);

  GVArray slice(IndexRange slice) const;

  GVArray &operator=(const GVArray &other);
  GVArray &operator=(GVArray &&other) noexcept;

  const GVArrayImpl *get_implementation() const
  {
    return impl_;
  }
};

/** Generic version of #VMutableArray. */
class GVMutableArray : public GVArrayCommon {
 public:
  GVMutableArray() = default;
  GVMutableArray(const GVMutableArray &other);
  GVMutableArray(GVMutableArray &&other) noexcept;
  GVMutableArray(GVMutableArrayImpl *impl);
  GVMutableArray(std::shared_ptr<GVMutableArrayImpl> impl);

  template<typename T> GVMutableArray(const VMutableArray<T> &varray);
  template<typename T> VMutableArray<T> typed() const;

  template<typename ImplT, typename... Args> static GVMutableArray For(Args &&...args);

  static GVMutableArray ForSpan(GMutableSpan span);

  operator GVArray() const &;
  operator GVArray() &&noexcept;

  GVMutableArray &operator=(const GVMutableArray &other);
  GVMutableArray &operator=(GVMutableArray &&other) noexcept;

  GMutableSpan get_internal_span() const;

  template<typename T> bool try_assign_VMutableArray(VMutableArray<T> &varray) const;

  void set_by_copy(const int64_t index, const void *value);
  void set_by_move(const int64_t index, void *value);
  void set_by_relocate(const int64_t index, void *value);

  void fill(const void *value);
  /**
   * Copy the values from the source buffer to all elements in the virtual array.
   */
  void set_all(const void *src);

  GVMutableArrayImpl *get_implementation() const;

 private:
  GVMutableArrayImpl *get_impl() const;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArray_GSpan and #GVMutableArray_GSpan.
 * \{ */

/* A generic version of VArray_Span. */
class GVArray_GSpan : public GSpan {
 private:
  GVArray varray_;
  void *owned_data_ = nullptr;

 public:
  GVArray_GSpan(GVArray varray);
  ~GVArray_GSpan();
};

/* A generic version of VMutableArray_Span. */
class GVMutableArray_GSpan : public GMutableSpan {
 private:
  GVMutableArray varray_;
  void *owned_data_ = nullptr;
  bool save_has_been_called_ = false;
  bool show_not_saved_warning_ = true;

 public:
  GVMutableArray_GSpan(GVMutableArray varray, bool copy_values_to_span = true);
  ~GVMutableArray_GSpan();

  void save();
  void disable_not_applied_warning();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversions between generic and typed virtual arrays.
 * \{ */

/* Used to convert a typed virtual array into a generic one. */
template<typename T> class GVArrayImpl_For_VArray : public GVArrayImpl {
 protected:
  VArray<T> varray_;

 public:
  GVArrayImpl_For_VArray(VArray<T> varray)
      : GVArrayImpl(CPPType::get<T>(), varray.size()), varray_(std::move(varray))
  {
  }

 protected:
  void get(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = varray_[index];
  }

  void get_to_uninitialized(const int64_t index, void *r_value) const override
  {
    new (r_value) T(varray_[index]);
  }

  bool is_span() const override
  {
    return varray_.is_span();
  }

  GSpan get_internal_span() const override
  {
    return GSpan(varray_.get_internal_span());
  }

  bool is_single() const override
  {
    return varray_.is_single();
  }

  void get_internal_single(void *r_value) const override
  {
    *(T *)r_value = varray_.get_internal_single();
  }

  void materialize(const IndexMask mask, void *dst) const override
  {
    varray_.materialize(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  void materialize_to_uninitialized(const IndexMask mask, void *dst) const override
  {
    varray_.materialize_to_uninitialized(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  bool try_assign_VArray(void *varray) const override
  {
    *(VArray<T> *)varray = varray_;
    return true;
  }

  bool may_have_ownership() const override
  {
    return varray_.may_have_ownership();
  }
};

/* Used to convert any generic virtual array into a typed one. */
template<typename T> class VArrayImpl_For_GVArray : public VArrayImpl<T> {
 protected:
  GVArray varray_;

 public:
  VArrayImpl_For_GVArray(GVArray varray) : VArrayImpl<T>(varray.size()), varray_(std::move(varray))
  {
    BLI_assert(varray_);
    BLI_assert(varray_.type().template is<T>());
  }

 protected:
  T get(const int64_t index) const override
  {
    T value;
    varray_.get(index, &value);
    return value;
  }

  bool is_span() const override
  {
    return varray_.is_span();
  }

  Span<T> get_internal_span() const override
  {
    return varray_.get_internal_span().template typed<T>();
  }

  bool is_single() const override
  {
    return varray_.is_single();
  }

  T get_internal_single() const override
  {
    T value;
    varray_.get_internal_single(&value);
    return value;
  }

  bool try_assign_GVArray(GVArray &varray) const override
  {
    varray = varray_;
    return true;
  }

  bool may_have_ownership() const override
  {
    return varray_.may_have_ownership();
  }
};

/* Used to convert any typed virtual mutable array into a generic one. */
template<typename T> class GVMutableArrayImpl_For_VMutableArray : public GVMutableArrayImpl {
 protected:
  VMutableArray<T> varray_;

 public:
  GVMutableArrayImpl_For_VMutableArray(VMutableArray<T> varray)
      : GVMutableArrayImpl(CPPType::get<T>(), varray.size()), varray_(std::move(varray))
  {
  }

 protected:
  void get(const int64_t index, void *r_value) const override
  {
    *(T *)r_value = varray_[index];
  }

  void get_to_uninitialized(const int64_t index, void *r_value) const override
  {
    new (r_value) T(varray_[index]);
  }

  bool is_span() const override
  {
    return varray_.is_span();
  }

  GSpan get_internal_span() const override
  {
    Span<T> span = varray_.get_internal_span();
    return span;
  }

  bool is_single() const override
  {
    return varray_.is_single();
  }

  void get_internal_single(void *r_value) const override
  {
    *(T *)r_value = varray_.get_internal_single();
  }

  void set_by_copy(const int64_t index, const void *value) override
  {
    const T &value_ = *(const T *)value;
    varray_.set(index, value_);
  }

  void set_by_relocate(const int64_t index, void *value) override
  {
    T &value_ = *(T *)value;
    varray_.set(index, std::move(value_));
    value_.~T();
  }

  void set_by_move(const int64_t index, void *value) override
  {
    T &value_ = *(T *)value;
    varray_.set(index, std::move(value_));
  }

  void set_all(const void *src) override
  {
    varray_.set_all(Span((T *)src, size_));
  }

  void materialize(const IndexMask mask, void *dst) const override
  {
    varray_.materialize(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  void materialize_to_uninitialized(const IndexMask mask, void *dst) const override
  {
    varray_.materialize_to_uninitialized(mask, MutableSpan((T *)dst, mask.min_array_size()));
  }

  bool try_assign_VArray(void *varray) const override
  {
    *(VArray<T> *)varray = varray_;
    return true;
  }

  bool try_assign_VMutableArray(void *varray) const override
  {
    *(VMutableArray<T> *)varray = varray_;
    return true;
  }

  bool may_have_ownership() const override
  {
    return varray_.may_have_ownership();
  }
};

/* Used to convert an generic mutable virtual array into a typed one. */
template<typename T> class VMutableArrayImpl_For_GVMutableArray : public VMutableArrayImpl<T> {
 protected:
  GVMutableArray varray_;

 public:
  VMutableArrayImpl_For_GVMutableArray(GVMutableArray varray)
      : VMutableArrayImpl<T>(varray.size()), varray_(varray)
  {
    BLI_assert(varray_);
    BLI_assert(varray_.type().template is<T>());
  }

 private:
  T get(const int64_t index) const override
  {
    T value;
    varray_.get(index, &value);
    return value;
  }

  void set(const int64_t index, T value) override
  {
    varray_.set_by_relocate(index, &value);
  }

  bool is_span() const override
  {
    return varray_.is_span();
  }

  Span<T> get_internal_span() const override
  {
    return varray_.get_internal_span().template typed<T>();
  }

  bool is_single() const override
  {
    return varray_.is_single();
  }

  T get_internal_single() const override
  {
    T value;
    varray_.get_internal_single(&value);
    return value;
  }

  bool try_assign_GVArray(GVArray &varray) const override
  {
    varray = varray_;
    return true;
  }

  bool try_assign_GVMutableArray(GVMutableArray &varray) const override
  {
    varray = varray_;
    return true;
  }

  bool may_have_ownership() const override
  {
    return varray_.may_have_ownership();
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #GVArrayImpl_For_GSpan.
 * \{ */

class GVArrayImpl_For_GSpan : public GVMutableArrayImpl {
 protected:
  void *data_ = nullptr;
  const int64_t element_size_;

 public:
  GVArrayImpl_For_GSpan(const GMutableSpan span);

 protected:
  GVArrayImpl_For_GSpan(const CPPType &type, const int64_t size);

 public:
  void get(const int64_t index, void *r_value) const override;
  void get_to_uninitialized(const int64_t index, void *r_value) const override;

  void set_by_copy(const int64_t index, const void *value) override;
  void set_by_move(const int64_t index, void *value) override;
  void set_by_relocate(const int64_t index, void *value) override;

  bool is_span() const override;
  GSpan get_internal_span() const override;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline methods for #GVArrayImpl.
 * \{ */

inline GVArrayImpl::GVArrayImpl(const CPPType &type, const int64_t size)
    : type_(&type), size_(size)
{
  BLI_assert(size_ >= 0);
}

inline const CPPType &GVArrayImpl::type() const
{
  return *type_;
}

inline int64_t GVArrayImpl::size() const
{
  return size_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline methods for #GVMutableArrayImpl.
 * \{ */

inline void GVMutableArray::set_by_copy(const int64_t index, const void *value)
{
  BLI_assert(index >= 0);
  BLI_assert(index < this->size());
  this->get_impl()->set_by_copy(index, value);
}

inline void GVMutableArray::set_by_move(const int64_t index, void *value)
{
  BLI_assert(index >= 0);
  BLI_assert(index < this->size());
  this->get_impl()->set_by_move(index, value);
}

inline void GVMutableArray::set_by_relocate(const int64_t index, void *value)
{
  BLI_assert(index >= 0);
  BLI_assert(index < this->size());
  this->get_impl()->set_by_relocate(index, value);
}

template<typename T>
inline bool GVMutableArray::try_assign_VMutableArray(VMutableArray<T> &varray) const
{
  BLI_assert(impl_->type().is<T>());
  return this->get_impl()->try_assign_VMutableArray(&varray);
}

inline GVMutableArrayImpl *GVMutableArray::get_impl() const
{
  return (GVMutableArrayImpl *)impl_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline methods for #GVArrayCommon.
 * \{ */

template<typename ImplT, typename... Args> inline void GVArrayCommon::emplace(Args &&...args)
{
  static_assert(std::is_base_of_v<GVArrayImpl, ImplT>);
  if constexpr (std::is_copy_constructible_v<ImplT> && Storage::template is_inline_v<ImplT>) {
    impl_ = &storage_.template emplace<ImplT>(std::forward<Args>(args)...);
  }
  else {
    std::shared_ptr<const GVArrayImpl> ptr = std::make_shared<ImplT>(std::forward<Args>(args)...);
    impl_ = &*ptr;
    storage_ = std::move(ptr);
  }
}

/* Copies the value at the given index into the provided storage. The `r_value` pointer is
 * expected to point to initialized memory. */
inline void GVArrayCommon::get(const int64_t index, void *r_value) const
{
  BLI_assert(index >= 0);
  BLI_assert(index < this->size());
  impl_->get(index, r_value);
}

template<typename T> inline T GVArrayCommon::get(const int64_t index) const
{
  BLI_assert(index >= 0);
  BLI_assert(index < this->size());
  BLI_assert(this->type().is<T>());
  T value{};
  impl_->get(index, &value);
  return value;
}

/* Same as `get`, but `r_value` is expected to point to uninitialized memory. */
inline void GVArrayCommon::get_to_uninitialized(const int64_t index, void *r_value) const
{
  BLI_assert(index >= 0);
  BLI_assert(index < this->size());
  impl_->get_to_uninitialized(index, r_value);
}

template<typename T> inline bool GVArrayCommon::try_assign_VArray(VArray<T> &varray) const
{
  BLI_assert(impl_->type().is<T>());
  return impl_->try_assign_VArray(&varray);
}

inline const CPPType &GVArrayCommon::type() const
{
  return impl_->type();
}

inline GVArrayCommon::operator bool() const
{
  return impl_ != nullptr;
}

inline int64_t GVArrayCommon::size() const
{
  if (impl_ == nullptr) {
    return 0;
  }
  return impl_->size();
}

inline bool GVArrayCommon::is_empty() const
{
  return this->size() == 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline methods for #GVArray.
 * \{ */

namespace detail {
template<typename StorageT> inline GVArrayAnyExtraInfo GVArrayAnyExtraInfo::get()
{
  static_assert(std::is_base_of_v<GVArrayImpl, StorageT> ||
                is_same_any_v<StorageT, const GVArrayImpl *, std::shared_ptr<const GVArrayImpl>>);

  if constexpr (std::is_base_of_v<GVArrayImpl, StorageT>) {
    return {[](const void *buffer) {
      return static_cast<const GVArrayImpl *>((const StorageT *)buffer);
    }};
  }
  else if constexpr (std::is_same_v<StorageT, const GVArrayImpl *>) {
    return {[](const void *buffer) { return *(const StorageT *)buffer; }};
  }
  else if constexpr (std::is_same_v<StorageT, std::shared_ptr<const GVArrayImpl>>) {
    return {[](const void *buffer) { return ((const StorageT *)buffer)->get(); }};
  }
  else {
    BLI_assert_unreachable();
    return {};
  }
}
}  // namespace detail

template<typename ImplT, typename... Args> inline GVArray GVArray::For(Args &&...args)
{
  static_assert(std::is_base_of_v<GVArrayImpl, ImplT>);
  GVArray varray;
  varray.template emplace<ImplT>(std::forward<Args>(args)...);
  return varray;
}

template<typename T> inline GVArray::GVArray(const VArray<T> &varray)
{
  if (!varray) {
    return;
  }
  if (varray.try_assign_GVArray(*this)) {
    return;
  }
  /* Need to check this before the span/single special cases, because otherwise we might loose
   * ownership to the referenced data when #varray goes out of scope. */
  if (varray.may_have_ownership()) {
    *this = GVArray::For<GVArrayImpl_For_VArray<T>>(varray);
  }
  else if (varray.is_span()) {
    Span<T> data = varray.get_internal_span();
    *this = GVArray::ForSpan(data);
  }
  else if (varray.is_single()) {
    T value = varray.get_internal_single();
    *this = GVArray::ForSingle(CPPType::get<T>(), varray.size(), &value);
  }
  else {
    *this = GVArray::For<GVArrayImpl_For_VArray<T>>(varray);
  }
}

template<typename T> inline VArray<T> GVArray::typed() const
{
  if (!*this) {
    return {};
  }
  BLI_assert(impl_->type().is<T>());
  VArray<T> varray;
  if (this->try_assign_VArray(varray)) {
    return varray;
  }
  if (this->may_have_ownership()) {
    return VArray<T>::template For<VArrayImpl_For_GVArray<T>>(*this);
  }
  if (this->is_span()) {
    const Span<T> span = this->get_internal_span().typed<T>();
    return VArray<T>::ForSpan(span);
  }
  if (this->is_single()) {
    T value;
    this->get_internal_single(&value);
    return VArray<T>::ForSingle(value, this->size());
  }
  return VArray<T>::template For<VArrayImpl_For_GVArray<T>>(*this);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline methods for #GVMutableArray.
 * \{ */

template<typename ImplT, typename... Args>
inline GVMutableArray GVMutableArray::For(Args &&...args)
{
  static_assert(std::is_base_of_v<GVMutableArrayImpl, ImplT>);
  GVMutableArray varray;
  varray.emplace<ImplT>(std::forward<Args>(args)...);
  return varray;
}

template<typename T> inline GVMutableArray::GVMutableArray(const VMutableArray<T> &varray)
{
  if (!varray) {
    return;
  }
  if (varray.try_assign_GVMutableArray(*this)) {
    return;
  }
  if (varray.may_have_ownership()) {
    *this = GVMutableArray::For<GVMutableArrayImpl_For_VMutableArray<T>>(varray);
  }
  else if (varray.is_span()) {
    MutableSpan<T> data = varray.get_internal_span();
    *this = GVMutableArray::ForSpan(data);
  }
  else {
    *this = GVMutableArray::For<GVMutableArrayImpl_For_VMutableArray<T>>(varray);
  }
}

template<typename T> inline VMutableArray<T> GVMutableArray::typed() const
{
  if (!*this) {
    return {};
  }
  BLI_assert(this->type().is<T>());
  VMutableArray<T> varray;
  if (this->try_assign_VMutableArray(varray)) {
    return varray;
  }
  if (this->may_have_ownership()) {
    return VMutableArray<T>::template For<VMutableArrayImpl_For_GVMutableArray<T>>(*this);
  }
  if (this->is_span()) {
    const MutableSpan<T> span = this->get_internal_span().typed<T>();
    return VMutableArray<T>::ForSpan(span);
  }
  return VMutableArray<T>::template For<VMutableArrayImpl_For_GVMutableArray<T>>(*this);
}

/** \} */

}  // namespace blender::fn
