/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  /* The pointers are allowed to be null, which means that the implementation is trivial. */
  void (*copy_construct)(void *dst, const void *src);
  void (*move_construct)(void *dst, void *src);
  void (*destruct)(void *src);
  const void *(*get)(const void *src);
  ExtraInfo extra_info;
};

/**
 * Used when #T is stored directly in the inline buffer of the #Any.
 */
template<typename ExtraInfo, typename T>
inline constexpr AnyTypeInfo<ExtraInfo> info_for_inline = {
    is_trivially_copy_constructible_extended_v<T> ?
        nullptr :
        +[](void *dst, const void *src) { new (dst) T(*static_cast<const T *>(src)); },
    is_trivially_move_constructible_extended_v<T> ?
        nullptr :
        +[](void *dst, void *src) { new (dst) T(std::move(*static_cast<T *>(src))); },
    is_trivially_destructible_extended_v<T> ?
        nullptr :
        +[](void *src) { std::destroy_at((static_cast<T *>(src))); },
    nullptr,
    ExtraInfo::template get<T>()};

/**
 * Used when #T can't be stored directly in the inline buffer and is stored in a #std::unique_ptr
 * instead. In this scenario, the #std::unique_ptr is stored in the inline buffer.
 */
template<typename T> using Ptr = std::unique_ptr<T>;
template<typename ExtraInfo, typename T>
inline constexpr AnyTypeInfo<ExtraInfo> info_for_unique_ptr = {
    [](void *dst, const void *src) { new (dst) Ptr<T>(new T(**(const Ptr<T> *)src)); },
    [](void *dst, void *src) { new (dst) Ptr<T>(new T(std::move(**(Ptr<T> *)src))); },
    [](void *src) { std::destroy_at((Ptr<T> *)src); },
    [](const void *src) -> const void * { return &**(const Ptr<T> *)src; },
    ExtraInfo::template get<T>()};

/**
 * Dummy extra info that is used when no additional type information should be stored in the #Any.
 */
struct NoExtraInfo {
  template<typename T> static constexpr NoExtraInfo get()
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
      std::conditional_t<std::is_void_v<ExtraInfo>, blender::detail::NoExtraInfo, ExtraInfo>;
  using Info = blender::detail::AnyTypeInfo<RealExtraInfo>;
  static constexpr size_t RealInlineBufferCapacity = std::max(InlineBufferCapacity,
                                                              sizeof(std::unique_ptr<int>));

  /**
   * Inline buffer that either contains nothing, the stored value directly, or a #std::unique_ptr
   * to the value.
   */
  AlignedBuffer<RealInlineBufferCapacity, Alignment> buffer_{};

  /**
   * Information about the type that is currently stored.
   * This is null when the #Any does not contain a value.
   */
  const Info *info_ = nullptr;

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
      return detail::template info_for_inline<RealExtraInfo, DecayT>;
    }
    else {
      return detail::template info_for_unique_ptr<RealExtraInfo, DecayT>;
    }
  }

 public:
  Any() = default;

  Any(const Any &other) : info_(other.info_)
  {
    if (info_ != nullptr) {
      if (info_->copy_construct != nullptr) {
        info_->copy_construct(&buffer_, &other.buffer_);
      }
      else {
        std::copy_n(static_cast<const std::byte *>(other.buffer_.ptr()),
                    RealInlineBufferCapacity,
                    static_cast<std::byte *>(buffer_.ptr()));
      }
    }
  }

  /**
   * \note The #other #Any will not be empty afterwards if it was not before. Just its value is in
   * a moved-from state.
   */
  Any(Any &&other) noexcept : info_(other.info_)
  {
    if (info_ != nullptr) {
      if (info_->move_construct != nullptr) {
        info_->move_construct(&buffer_, &other.buffer_);
      }
      else {
        std::copy_n(static_cast<const std::byte *>(other.buffer_.ptr()),
                    RealInlineBufferCapacity,
                    static_cast<std::byte *>(buffer_.ptr()));
      }
    }
  }

  /**
   * Constructs a new #Any that contains the given type #T from #args. The #std::in_place_type_t is
   * used to disambiguate this and the copy/move constructors.
   */
  template<typename T, typename... Args> explicit Any(std::in_place_type_t<T>, Args &&...args)
  {
    this->emplace_on_empty<T>(std::forward<Args>(args)...);
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
    if (info_ != nullptr) {
      if (info_->destruct != nullptr) {
        info_->destruct(&buffer_);
      }
    }
  }

  /**
   * \note Only needed because the template below does not count as copy assignment operator.
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
    if (info_ != nullptr) {
      if (info_->destruct != nullptr) {
        info_->destruct(&buffer_);
      }
    }
    info_ = nullptr;
  }

  operator bool() const
  {
    return this->has_value();
  }

  bool has_value() const
  {
    return info_ != nullptr;
  }

  template<typename T, typename... Args> std::decay_t<T> &emplace(Args &&...args)
  {
    this->~Any();
    new (this) Any(std::in_place_type<T>, std::forward<Args>(args)...);
    return this->get<T>();
  }

  template<typename T, typename... Args> std::decay_t<T> &emplace_on_empty(Args &&...args)
  {
    BLI_assert(!this->has_value());
    using DecayT = std::decay_t<T>;
    static_assert(is_allowed_v<DecayT>);
    info_ = &this->template get_info<DecayT>();
    if constexpr (is_inline_v<DecayT>) {
      /* Construct the value directly in the inline buffer. */
      DecayT *stored_value = new (&buffer_) DecayT(std::forward<Args>(args)...);
      return *stored_value;
    }
    else {
      /* Construct the value in a new allocation and store a #std::unique_ptr to it in the inline
       * buffer. */
      std::unique_ptr<DecayT> *stored_value = new (&buffer_)
          std::unique_ptr<DecayT>(new DecayT(std::forward<Args>(args)...));
      return **stored_value;
    }
  }

  /**
   * Like #emplace but does *not* actually construct the value. The caller is responsible for
   * calling the constructor before the value is used.
   */
  template<typename T> void *allocate()
  {
    this->reset();
    return this->allocate_on_empty<T>();
  }

  /**
   * Like #emplace_on_empty but does *not* actually construct the value. The caller is responsible
   * for calling the constructor before the value is used.
   */
  template<typename T> void *allocate_on_empty()
  {
    BLI_assert(!this->has_value());
    static_assert(is_allowed_v<T>);
    info_ = &this->template get_info<T>();
    if constexpr (is_inline_v<T>) {
      return buffer_.ptr();
    }
    else {
      /* Using raw allocation here. The caller is responsible for constructing the value. */
      T *value = static_cast<T *>(::operator new(sizeof(T)));
      new (&buffer_) std::unique_ptr<T>(value);
      return value;
    }
  }

  /** Return true when the value that is currently stored is a #T. */
  template<typename T> bool is() const
  {
    return info_ == &this->template get_info<T>();
  }

  /** Get a pointer to the stored value. */
  void *get()
  {
    BLI_assert(info_ != nullptr);
    if (info_->get != nullptr) {
      return const_cast<void *>(info_->get(&buffer_));
    }
    return &buffer_;
  }

  /** Get a pointer to the stored value. */
  const void *get() const
  {
    BLI_assert(info_ != nullptr);
    if (info_->get != nullptr) {
      return info_->get(&buffer_);
    }
    return &buffer_;
  }

  /**
   * Get a reference to the stored value. This invokes undefined behavior when #T does not have the
   * correct type.
   */
  template<typename T> T &get()
  {
    /* Use const-cast to be able to reuse the const method above. */
    return const_cast<T &>(const_cast<const Any *>(this)->get<T>());
  }

  /**
   * Get a reference to the stored value. This invokes undefined behavior when #T does not have the
   * correct type.
   */
  template<typename T> const T &get() const
  {
    BLI_assert(this->is<T>());
    const void *buffer;
    /* Can avoid the `info_->get == nullptr` check because the result is known statically. */
    if constexpr (is_inline_v<T>) {
      buffer = &buffer_;
    }
    else {
      BLI_assert(info_->get != nullptr);
      buffer = info_->get(&buffer_);
    }
    return *static_cast<const T *>(buffer);
  }

  /**
   * Get extra information that has been stored for the contained type.
   */
  const RealExtraInfo &extra_info() const
  {
    BLI_assert(info_ != nullptr);
    return info_->extra_info;
  }
};

}  // namespace blender
