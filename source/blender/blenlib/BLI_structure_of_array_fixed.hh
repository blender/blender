/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_structure_of_array_pointer.hh"
#include "BLI_utildefines.hh"

namespace blender {

template<int64_t Size, typename First, typename... Rest>
inline auto to_fixed_soa_array_tuple_like(TypeSequence<First, Rest...> /*types*/)
{
  if constexpr ((std::is_same_v<First, Rest> && ...)) {
    return std::array<std::array<First, Size>, 1 + sizeof...(Rest)>();
  }
  else {
    return std::tuple<std::array<First, Size>, std::array<Rest, Size>...>();
  }
}

/**
 * #FixedSoaArray is the counterpart to std::array for structure-of-array types (similar to how
 * SoaPtr is the counterpart to a raw pointer).
 *
 * For example, while std::array<float3, 4> stores XYZXYZXYZXYZ, FixedSoaArray<float3, 4> stores
 * XXXXYYYYZZZZ while still providing easy access to the members as float3.
 */
template<SoaType T, int64_t Capacity>
class FixedSoaArray : public SoaReadMixin<FixedSoaArray<T, Capacity>, T>,
                      public SoaWriteMixin<FixedSoaArray<T, Capacity>, T> {
 private:
  using SoaTypeSequence = soa_type_sequence_t<T>;
  using SoaArrayTupleLike = decltype(to_fixed_soa_array_tuple_like<Capacity>(SoaTypeSequence()));

  SoaArrayTupleLike arrays_;

 public:
  FixedSoaArray() = default;

  FixedSoaArray(const SoaPtr<T> &values, const int64_t size)
  {
    this->set_multiple(values, size);
  }

  template<int64_t I> const soa_component_t<T, I> *data() const
  {
    return std::get<I>(arrays_).data();
  }

  template<int64_t I> soa_component_t<T, I> *data()
  {
    return std::get<I>(arrays_).data();
  }

  const soa_component_t<T, 0> *data(const int64_t i) const
    requires soa_is_homogeneous_v<T>
  {
    BLI_assert(i >= 0);
    BLI_assert(i < int64_t(SoaTypeSequence::size()));
    return arrays_[i].data();
  }

  soa_component_t<T, 0> *data(const int64_t i)
  {
    BLI_assert(i >= 0);
    BLI_assert(i < int64_t(SoaTypeSequence::size()));
    return arrays_[i].data();
  }

  SoaPtr<T> data() const
  {
    return [&]<size_t... I>(std::index_sequence<I...> /*indices*/) {
      return SoaPtr<T>(std::get<I>(arrays_).data()...);
    }(std::make_index_sequence<SoaTypeSequence::size()>());
  }

  MutableSoaPtr<T> data()
  {
    return [&]<size_t... I>(std::index_sequence<I...> /*indices*/) {
      return MutableSoaPtr<T>(std::get<I>(arrays_).data()...);
    }(std::make_index_sequence<SoaTypeSequence::size()>());
  }

  operator SoaPtr<T>() const
  {
    return this->data();
  }

  operator MutableSoaPtr<T>()
  {
    return this->data();
  }

  void assert_valid_index(const int64_t i) const
  {
    UNUSED_VARS_NDEBUG(i);
    BLI_assert(i >= 0);
    BLI_assert(i < Capacity);
  }

  void assert_valid_range(const IndexRange range) const
  {
    UNUSED_VARS_NDEBUG(range);
    BLI_assert(range.one_after_last() <= Capacity);
  }
};

template<typename T, int64_t Capacity> SoaPtr(const FixedSoaArray<T, Capacity> &) -> SoaPtr<T>;
template<typename T, int64_t Capacity>
MutableSoaPtr(FixedSoaArray<T, Capacity> &) -> MutableSoaPtr<T>;

}  // namespace blender
