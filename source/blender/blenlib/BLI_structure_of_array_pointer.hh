/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <array>

#include "BLI_index_range.hh"
#include "BLI_structure_of_array_conversion.hh"

namespace blender {

template<typename First, typename... Rest>
inline auto to_soa_const_pointer_tuple_like(TypeSequence<First, Rest...> /*types*/)
{
  if constexpr ((std::is_same_v<First, Rest> && ...)) {
    return std::array<const First *, 1 + sizeof...(Rest)>();
  }
  else {
    return std::tuple<const First *, const Rest *...>();
  }
}

template<typename First, typename... Rest>
inline auto to_soa_pointer_tuple_like(TypeSequence<First, Rest...> /*types*/)
{
  if constexpr ((std::is_same_v<First, Rest> && ...)) {
    return std::array<First *, 1 + sizeof...(Rest)>();
  }
  else {
    return std::tuple<First *, Rest *...>();
  }
}

template<SoaType T> class SoaPtr;

struct SoaReadMixinBase {};
struct SoaWriteMixinBase {};

/** Utility class to read from SoA data conveniently. */
template<typename Derived, typename ValueT> class SoaReadMixin : public SoaReadMixinBase {
 private:
  using SoaTypeSequence = soa_type_sequence_t<ValueT>;
  static constexpr auto soa_index_sequence = std::make_index_sequence<SoaTypeSequence::size()>();

  template<size_t... I>
  ValueT get_impl(std::index_sequence<I...> /*indices*/, const int64_t i) const
  {
    const Derived &derived = static_cast<const Derived &>(*this);
    derived.assert_valid_index(i);
    return struct_from_soa_components<ValueT>(derived.template data<I>()[i]...);
  }

  template<size_t... I>
  void get_multiple_impl(std::index_sequence<I...> indices,
                         ValueT *r_values,
                         const int64_t size) const
  {
    const Derived &derived = static_cast<const Derived &>(*this);
    derived.assert_valid_range(IndexRange(size));
    if constexpr (requires(const int64_t size,
                           ValueT *r_dst,
                           const SoaTypeSequence::template at_index<I> *...comps) {
                    soa_to_aos(size, comps..., r_dst);
                  })
    {
      soa_to_aos(size, derived.template data<I>()..., r_values);
    }
    else {
      for (int64_t i = 0; i < size; i++) {
        r_values[i] = this->get_impl(indices, i);
      }
    }
  }

 public:
  /** Access a single item as a struct. */
  ValueT operator[](const int64_t i) const
  {
    return this->get_impl(soa_index_sequence, i);
  }

  /**
   * Read multiple values at once. This may use optimized functions like #soa_to_aos_3x4byte if
   * possible.
   */
  void get_multiple(ValueT *r_values, const int64_t size) const
  {
    this->get_multiple_impl(soa_index_sequence, r_values, size);
  }
};

/** Utility class to write to SoA data conveniently. */
template<typename Derived, typename ValueT> class SoaWriteMixin : public SoaWriteMixinBase {
 private:
  using SoaTypeSequence = soa_type_sequence_t<ValueT>;
  static constexpr auto soa_index_sequence = std::make_index_sequence<SoaTypeSequence::size()>();

  template<size_t... I>
  void set_impl(std::index_sequence<I...> /*indices*/, const int64_t i, const ValueT &value)
  {
    Derived &derived = static_cast<Derived &>(*this);
    derived.assert_valid_index(i);
    auto soa_components = to_soa_components(value);
    ((derived.template data<I>()[i] = std::get<I>(soa_components)), ...);
  }

  template<size_t... I>
  void fill_impl(std::index_sequence<I...> /*indices*/, const ValueT &value, const int64_t size)
  {
    Derived &derived = static_cast<Derived &>(*this);
    derived.assert_valid_range(IndexRange(size));
    auto soa_components = to_soa_components(value);
    (
        [&]() {
          const auto &component = std::get<I>(soa_components);
          std::fill_n(derived.template data<I>(), size, component);
        }(),
        ...);
  }

  template<size_t... I>
  void set_multiple_impl(std::index_sequence<I...> /*indices*/,
                         const SoaPtr<ValueT> &values,
                         const int64_t size)
  {
    Derived &derived = static_cast<Derived &>(*this);
    derived.assert_valid_range(IndexRange(size));
    (std::copy_n(values.template data<I>(), size, derived.template data<I>()), ...);
  }

  template<size_t... I>
  void set_multiple_impl(std::index_sequence<I...> /*indices*/,
                         const ValueT *values,
                         const int64_t size)
  {
    Derived &derived = static_cast<Derived &>(*this);
    derived.assert_valid_range(IndexRange(size));
    if constexpr (requires(const int64_t size,
                           const ValueT *src,
                           SoaTypeSequence::template at_index<I> *...comps) {
                    aos_to_soa(size, src, comps...);
                  })
    {
      aos_to_soa(size, values, derived.template data<I>()...);
    }
    else {
      for (int64_t i = 0; i < size; i++) {
        this->set_impl(soa_index_sequence, i, values[i]);
      }
    }
  }

 public:
  /**
   * Store a value at the specific index. Note that operator[] does not work easily here since
   * it's not possible to return a reference to the data because it's stored in separate arrays. It
   * may be possible to implement something like #BitRef in the future though.
   */
  void set(const int64_t i, const ValueT &value)
  {
    this->set_impl(soa_index_sequence, i, value);
  }

  /**
   * Write multiple values into the storage at once. This may use optimized functions like
   * #aos_to_soa_3x4byte if possible.
   */
  void set_multiple(const ValueT *values, const int64_t size)
  {
    this->set_multiple_impl(soa_index_sequence, values, size);
  }

  void set_multiple(const SoaPtr<ValueT> &values, const int64_t size)
  {
    this->set_multiple_impl(soa_index_sequence, values, size);
  }

  /** Write the same value multiple times. */
  void fill(const ValueT &value, const int64_t size)
  {
    this->fill_impl(soa_index_sequence, value, size);
  }
};

template<typename T> constexpr bool is_SoaReadable_v = std::derived_from<T, SoaReadMixinBase>;
template<typename T> constexpr bool is_SoaWritable_v = std::derived_from<T, SoaWriteMixinBase>;

template<SoaType T> class MutableSoaPtr;

/**
 * Contains all the structure-of-array pointers for a higher level struct. E.g. SoaPtr<float3> is
 * mostly the same as std::array<const float *, 3>. It has some useful utilities like reading the
 * data as structure on top.
 */
template<SoaType T> class SoaPtr : public SoaReadMixin<SoaPtr<T>, T> {
 private:
  using SoaTypeSequence = soa_type_sequence_t<T>;
  using PointerTupleLike = decltype(to_soa_const_pointer_tuple_like(SoaTypeSequence()));

  PointerTupleLike pointers_;

 public:
  SoaPtr() = default;

  template<typename... Pointers>
  SoaPtr(const Pointers *...pointers)
    requires(sizeof...(Pointers) == SoaTypeSequence::size())
      : pointers_{pointers...}
  {
  }

  SoaPtr(const MutableSoaPtr<T> &src);

  /** Get the raw data pointer for one component. */
  template<int64_t I> const soa_component_t<T, I> *data() const
  {
    return std::get<I>(pointers_);
  }

  /**
   * Same as above, but the component index can be specified at run-time. This only works when the
   * pointers all have the same type (e.g. for SoaPtr<float3>).
   */
  const soa_component_t<T, 0> *data(const int64_t i) const
    requires soa_is_homogeneous_v<T>
  {
    BLI_assert(i >= 0);
    BLI_assert(i < int64_t(SoaTypeSequence::size()));
    return pointers_[i];
  }

  /** Get all raw data pointers. */
  const PointerTupleLike &data() const
  {
    return pointers_;
  }

  /** Used by #SoaReadMixin but the index can't be asserted since the size is not known here. */
  void assert_valid_index(const int64_t /*i*/) const {}
  void assert_valid_range(const IndexRange /*range*/) const {}
};

/** Same as SoaPtr but allows modifying the values. */
template<SoaType T>
class MutableSoaPtr : public SoaReadMixin<MutableSoaPtr<T>, T>,
                      public SoaWriteMixin<MutableSoaPtr<T>, T> {
 private:
  using SoaTypeSequence = soa_type_sequence_t<T>;
  using PointerTupleLike = decltype(to_soa_pointer_tuple_like(SoaTypeSequence()));

  PointerTupleLike pointers_{};

 public:
  MutableSoaPtr() = default;

  template<typename... Pointers>
  MutableSoaPtr(Pointers *...pointers)
    requires(sizeof...(Pointers) == SoaTypeSequence::size())
      : pointers_{pointers...}
  {
  }

  /** Get the raw data pointer for one component. */
  template<int64_t I> SoaTypeSequence::template at_index<I> *data() const
  {
    return std::get<I>(pointers_);
  }

  /**
   * Same as above, but the component index can be specified at run-time. This only works when the
   * pointers all have the same type (e.g. for SoaPtr<float3>).
   */
  soa_component_t<T, 0> *data(const int64_t i) const
    requires soa_is_homogeneous_v<T>
  {
    BLI_assert(i >= 0);
    BLI_assert(i < int64_t(SoaTypeSequence::size()));
    return pointers_[i];
  }

  /** Get all raw data pointers. */
  const PointerTupleLike &data() const
  {
    return pointers_;
  }

  /* Used by #SoaReadMixin and #SoaWriteMixin but the index can't be asserted since the size is
   * not known here. */
  void assert_valid_index(const int64_t /*i*/) const {}
  void assert_valid_range(const IndexRange /*range*/) const {}
};

template<SoaType T> inline SoaPtr<T>::SoaPtr(const MutableSoaPtr<T> &src)
{
  [&]<size_t... I>(std::index_sequence<I...> /*indices*/) {
    pointers_ = PointerTupleLike{src.template data<I>()...};
  }(std::make_index_sequence<SoaTypeSequence::size()>());
}

}  // namespace blender
