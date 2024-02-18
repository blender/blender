/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <iterator>

namespace blender::iterator {

/**
 * Simplifies implementing a random-access-iterator.
 *
 * The actual iterator should derive from this class publicly. Additionally, it has to provide a
 * const `iter_prop` method which returns a reference to the internal property that corresponds to
 * the current position. This is typically a pointer or an index.
 *
 * Implementing some random-access-iterator is generally quite simple but requires a lot of
 * boilerplate code because algorithms expect many operators to work on the iterator type.
 * They are expected to behave similarly to pointers and thus have to implement many of the same
 * operators.
 */
template<typename Derived> class RandomAccessIteratorMixin {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using difference_type = std::ptrdiff_t;

  constexpr friend Derived &operator++(Derived &a)
  {
    ++a.iter_prop_mutable();
    return a;
  }

  constexpr friend Derived operator++(Derived &a, int)
  {
    Derived copy = a;
    ++a;
    return copy;
  }

  constexpr friend Derived &operator--(Derived &a)
  {
    --a.iter_prop_mutable();
    return a;
  }

  constexpr friend Derived operator--(Derived &a, int)
  {
    Derived copy = a;
    --a;
    return copy;
  }

  constexpr friend Derived &operator+=(Derived &a, const std::ptrdiff_t n)
  {
    a.iter_prop_mutable() += n;
    return a;
  }

  constexpr friend Derived &operator-=(Derived &a, const std::ptrdiff_t n)
  {
    a.iter_prop_mutable() -= n;
    return a;
  }

  constexpr friend Derived operator+(const Derived &a, const std::ptrdiff_t n)
  {
    Derived copy = a;
    copy.iter_prop_mutable() += n;
    return copy;
  }

  constexpr friend Derived operator-(const Derived &a, const std::ptrdiff_t n)
  {
    Derived copy = a;
    copy.iter_prop_mutable() -= n;
    return copy;
  }

  constexpr friend auto operator-(const Derived &a, const Derived &b)
  {
    return a.iter_prop() - b.iter_prop();
  }

  constexpr friend bool operator!=(const Derived &a, const Derived &b)
  {
    return a.iter_prop() != b.iter_prop();
  }

  constexpr friend bool operator==(const Derived &a, const Derived &b)
  {
    return a.iter_prop() == b.iter_prop();
  }

  constexpr friend bool operator<(const Derived &a, const Derived &b)
  {
    return a.iter_prop() < b.iter_prop();
  }

  constexpr friend bool operator>(const Derived &a, const Derived &b)
  {
    return a.iter_prop() > b.iter_prop();
  }

  constexpr friend bool operator<=(const Derived &a, const Derived &b)
  {
    return a.iter_prop() <= b.iter_prop();
  }

  constexpr friend bool operator>=(const Derived &a, const Derived &b)
  {
    return a.iter_prop() >= b.iter_prop();
  }

  constexpr decltype(auto) operator[](const std::ptrdiff_t i)
  {
    return *(*static_cast<Derived *>(this) + i);
  }

  constexpr decltype(auto) operator[](const std::ptrdiff_t i) const
  {
    return *(*static_cast<const Derived *>(this) + i);
  }

  auto &iter_prop_mutable()
  {
    const auto &const_iter_prop = static_cast<const Derived *>(this)->iter_prop();
    return const_cast<std::remove_const_t<std::remove_reference_t<decltype(const_iter_prop)>> &>(
        const_iter_prop);
  }
};

}  // namespace blender::iterator
