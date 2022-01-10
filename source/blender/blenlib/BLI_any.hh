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
 * A #blender::Any is a type-safe container for single values of any copy constructible type.
 * It is similar to #std::any but provides the following two additional features:
 * - Adjustable inline buffer capacity and alignment. #std::any has a small inline buffer in most
 *   implementations as well, but its size is not guaranteed.
 * - Can store additional user-defined type information without increasing the stack size of #Any.
 */

#include <algorithm>
#include <utility>

#include "BLI_memory_utils.hh"

namespace blender {

namespace detail {

/**
 * Contains function pointers that manage the memory in an #Any.
 * Additional type specific #ExtraInfo can be embedded here as well.
 */
template<typename ExtraInfo> struct AnyTypeInfo {
  void (*copy_construct)(void *dst, const void *src);
  void (*move_construct)(void *dst, void *src);
  void (*destruct)(void *src);
  const void *(*get)(const void *src);
  ExtraInfo extra_info;

  /**
   * Used when #T is stored directly in the inline buffer of the #Any.
   */
  template<typename T> static const AnyTypeInfo &get_for_inline()
  {
    static AnyTypeInfo funcs = {[](void *dst, const void *src) { new (dst) T(*(const T *)src); },
                                [](void *dst, void *src) { new (dst) T(std::move(*(T *)src)); },
                                [](void *src) { ((T *)src)->~T(); },
                                [](const void *src) { return src; },
                                ExtraInfo::template get<T>()};
    return funcs;
  }

  /**
   * Used when #T can't be stored directly in the inline buffer and is stored in a #std::unique_ptr
   * instead. In this scenario, the #std::unique_ptr is stored in the inline buffer.
   */
  template<typename T> static const AnyTypeInfo &get_for_unique_ptr()
  {
    using Ptr = std::unique_ptr<T>;
    static AnyTypeInfo funcs = {
        [](void *dst, const void *src) { new (dst) Ptr(new T(**(const Ptr *)src)); },
        [](void *dst, void *src) { new (dst) Ptr(new T(std::move(**(Ptr *)src))); },
        [](void *src) { ((Ptr *)src)->~Ptr(); },
        [](const void *src) -> const void * { return &**(const Ptr *)src; },
        ExtraInfo::template get<T>()};
    return funcs;
  }

  /**
   * Used when the #Any does not contain any type currently.
   */
  static const AnyTypeInfo &get_for_empty()
  {
    static AnyTypeInfo funcs = {[](void *UNUSED(dst), const void *UNUSED(src)) {},
                                [](void *UNUSED(dst), void *UNUSED(src)) {},
                                [](void *UNUSED(src)) {},
                                [](const void *UNUSED(src)) -> const void * { return nullptr; },
                                ExtraInfo{}};
    return funcs;
  }
};

/**
 * Dummy extra info that is used when no additional type information should be stored in the #Any.
 */
struct NoExtraInfo {
  template<typename T> static NoExtraInfo get()
  {
    return {};
  }
};

}  // namespace detail

template<
    /**
     * Either void or a struct that contains data members for additional type information.
     * The struct has to have a static `ExtraInfo get<T>()` method that initializes the struct
     * based on a type.
     */
    typename ExtraInfo = void,
    /**
     * Size of the inline buffer. This allows types that are small enough to be stored directly
     * inside the #Any without an additional allocation.
     */
    size_t InlineBufferCapacity = 8,
    /**
     * Required minimum alignment of the inline buffer. If this is smaller than the alignment
     * requirement of a used type, a separate allocation is necessary.
     */
    size_t Alignment = 8>
class Any {
 private:
  /* Makes it possible to use void in the template parameters. */
  using RealExtraInfo =
      std::conditional_t<std::is_void_v<ExtraInfo>, detail::NoExtraInfo, ExtraInfo>;
  using Info = detail::AnyTypeInfo<RealExtraInfo>;

  /**
   * Inline buffer that either contains nothing, the stored value directly, or a #std::unique_ptr
   * to the value.
   */
  AlignedBuffer<std::max(InlineBufferCapacity, sizeof(std::unique_ptr<int>)), Alignment> buffer_{};

  /**
   * Information about the type that is currently stored.
   */
  const Info *info_ = &Info::get_for_empty();

 public:
  /** Only copy constructible types can be stored in #Any. */
  template<typename T> static constexpr inline bool is_allowed_v = std::is_copy_constructible_v<T>;

  /**
   * Checks if the type will be stored in the inline buffer or if it requires a separate
   * allocation.
   */
  template<typename T>
  static constexpr inline bool is_inline_v = std::is_nothrow_move_constructible_v<T> &&
                                             sizeof(T) <= InlineBufferCapacity &&
                                             alignof(T) <= Alignment;

  /**
   * Checks if #T is the same type as this #Any, because in this case the behavior of e.g. the
   * assignment operator is different.
   */
  template<typename T>
  static constexpr inline bool is_same_any_v = std::is_same_v<std::decay_t<T>, Any>;

 private:
  template<typename T> const Info &get_info() const
  {
    using DecayT = std::decay_t<T>;
    static_assert(is_allowed_v<DecayT>);
    if constexpr (is_inline_v<DecayT>) {
      return Info::template get_for_inline<DecayT>();
    }
    else {
      return Info::template get_for_unique_ptr<DecayT>();
    }
  }

 public:
  Any() = default;

  Any(const Any &other) : info_(other.info_)
  {
    info_->copy_construct(&buffer_, &other.buffer_);
  }

  /**
   * \note The #other #Any will not be empty afterwards if it was not before. Just its value is in
   * a moved-from state.
   */
  Any(Any &&other) noexcept : info_(other.info_)
  {
    info_->move_construct(&buffer_, &other.buffer_);
  }

  /**
   * Constructs a new #Any that contains the given type #T from #args. The #std::in_place_type_t is
   * used to disambiguate this and the copy/move constructors.
   */
  template<typename T, typename... Args> explicit Any(std::in_place_type_t<T>, Args &&...args)
  {
    using DecayT = std::decay_t<T>;
    static_assert(is_allowed_v<DecayT>);
    info_ = &this->template get_info<DecayT>();
    if constexpr (is_inline_v<DecayT>) {
      /* Construct the value directly in the inline buffer. */
      new (&buffer_) DecayT(std::forward<Args>(args)...);
    }
    else {
      /* Construct the value in a new allocation and store a #std::unique_ptr to it in the inline
       * buffer. */
      new (&buffer_) std::unique_ptr<DecayT>(new DecayT(std::forward<Args>(args)...));
    }
  }

  /**
   * Constructs a new #Any that contains the given value.
   */
  template<typename T, BLI_ENABLE_IF((!is_same_any_v<T>))>
  Any(T &&value) : Any(std::in_place_type<T>, std::forward<T>(value))
  {
  }

  ~Any()
  {
    info_->destruct(&buffer_);
  }

  /**
   * \note: Only needed because the template below does not count as copy assignment operator.
   */
  Any &operator=(const Any &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~Any();
    new (this) Any(other);
    return *this;
  }

  /** Assign any value to the #Any. */
  template<typename T> Any &operator=(T &&other)
  {
    if constexpr (is_same_any_v<T>) {
      if (this == &other) {
        return *this;
      }
    }
    this->~Any();
    new (this) Any(std::forward<T>(other));
    return *this;
  }

  /** Destruct any existing value to make it empty. */
  void reset()
  {
    info_->destruct(&buffer_);
    info_ = &Info::get_for_empty();
  }

  operator bool() const
  {
    return this->has_value();
  }

  bool has_value() const
  {
    return info_ != &Info::get_for_empty();
  }

  template<typename T, typename... Args> std::decay_t<T> &emplace(Args &&...args)
  {
    this->~Any();
    new (this) Any(std::in_place_type<T>, std::forward<Args>(args)...);
    return this->get<T>();
  }

  /** Return true when the value that is currently stored is a #T. */
  template<typename T> bool is() const
  {
    return info_ == &this->template get_info<T>();
  }

  /** Get a pointer to the stored value. */
  void *get()
  {
    return const_cast<void *>(info_->get(&buffer_));
  }

  /** Get a pointer to the stored value. */
  const void *get() const
  {
    return info_->get(&buffer_);
  }

  /**
   * Get a reference to the stored value. This invokes undefined behavior when #T does not have the
   * correct type.
   */
  template<typename T> std::decay_t<T> &get()
  {
    BLI_assert(this->is<T>());
    return *static_cast<std::decay_t<T> *>(this->get());
  }

  /**
   * Get a reference to the stored value. This invokes undefined behavior when #T does not have the
   * correct type.
   */
  template<typename T> const std::decay_t<T> &get() const
  {
    BLI_assert(this->is<T>());
    return *static_cast<const std::decay_t<T> *>(this->get());
  }

  /**
   * Get extra information that has been stored for the contained type.
   */
  const RealExtraInfo &extra_info() const
  {
    return info_->extra_info;
  }
};

}  // namespace blender
