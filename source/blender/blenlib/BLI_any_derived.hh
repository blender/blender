/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_any.hh"

namespace blender {

namespace detail {

template<typename Base> struct AnyDerivedExtraInfo {

  Base *(*get_impl)(const void *buffer);

  template<typename StorageT> static constexpr AnyDerivedExtraInfo get()
  {
    /* These are the only allowed types in the #Any. */
    static_assert(std::is_base_of_v<Base, StorageT> ||
                  is_same_any_v<StorageT, Base *, std::shared_ptr<Base>>);

    /* Depending on how the implementation is stored in the #Any, a different #get_impl function
     * is required. */
    if constexpr (std::is_base_of_v<Base, StorageT>) {
      return {[](const void *buffer) {
        return static_cast<Base *>(const_cast<StorageT *>(static_cast<const StorageT *>(buffer)));
      }};
    }
    else if constexpr (std::is_same_v<StorageT, Base *>) {
      return {[](const void *buffer) {
        return *const_cast<StorageT *>(static_cast<const StorageT *>(buffer));
      }};
    }
    else if constexpr (std::is_same_v<StorageT, std::shared_ptr<Base>>) {
      return {[](const void *buffer) {
        return (const_cast<StorageT *>(static_cast<const StorageT *>(buffer)))->get();
      }};
    }
    else {
      BLI_assert_unreachable();
      return {};
    }
  }
};

}  // namespace detail

/**
 * This allows storing or passing around derived classes of a common base class. Typically, this
 * always requires allocating the value on the heap and passing it around e.g. as unique_ptr.
 * #AnyDerived has small buffer optimization. So if the type is small, it can be stored directly
 * without an additional allocation.
 *
 * If all derived types are known where the type is used, it can be more efficient to use
 * std::variant<Derived1, Derived2, ...> instead. #AnyDerived uses type erasure through the use of
 * #Any and therefore works even when not all used derived types are known.
 *
 * This is used extensively for virtual arrays. Each type of virtual array is implemented as
 * subclass of #VArrayImpl while #VArray actually stores a specific implementation.
 *
 * Note: If the value is not stored inline, it's currently stored as a shared_ptr which is shared
 * when the AnyDerived is copied. This behavior is fine and efficient for all current uses but it
 * may need to be generalized if #AnyDerived is supposed to be used in more places.
 */
template<typename Base, int64_t InlineBufferCapacity = 24> struct AnyDerived {
 private:
  using Storage = Any<detail::AnyDerivedExtraInfo<Base>, InlineBufferCapacity, alignof(Base)>;

  /** Pointer to the currently contained implementation. This may be null. */
  Base *impl_ = nullptr;

  /**
   * Does the memory management for the implementation. It contains one of the following:
   * - Inline subclass of T.
   * - Non-owning pointer to a T.
   * - Shared pointer to a T.
   */
  Storage storage_;

 public:
  AnyDerived() = default;

  AnyDerived(const AnyDerived &other) : storage_(other.storage_)
  {
    impl_ = this->impl_from_storage();
  }

  AnyDerived(AnyDerived &&other) noexcept : storage_(std::move(other.storage_))
  {
    impl_ = this->impl_from_storage();
    other.storage_.reset();
    other.impl_ = nullptr;
  }

  explicit AnyDerived(Base *impl) : impl_(impl)
  {
    storage_ = impl_;
  }

  explicit AnyDerived(std::shared_ptr<Base> impl) : impl_(impl.get())
  {
    if (impl_) {
      storage_ = std::move(impl);
    }
  }

  AnyDerived &operator=(const AnyDerived &other)
  {
    if (this == &other) {
      return *this;
    }
    storage_ = other.storage_;
    impl_ = this->impl_from_storage();
    return *this;
  }

  AnyDerived &operator=(AnyDerived &&other) noexcept
  {
    if (this == &other) {
      return *this;
    }
    storage_ = std::move(other.storage_);
    impl_ = this->impl_from_storage();
    other.storage_.reset();
    other.impl_ = nullptr;
    return *this;
  }

  template<typename ImplT, typename... Args>
    requires std::is_base_of_v<Base, ImplT>
  void emplace(Args &&...args)
  {
    if constexpr (std::is_copy_constructible_v<ImplT> && Storage::template is_inline_v<ImplT>) {
      /* Only inline the implementation when it is copyable and when it fits into the inline
       * buffer of the storage. */
      impl_ = &storage_.template emplace<ImplT>(std::forward<Args>(args)...);
    }
    else {
      /* If it can't be inlined, create a new #std::shared_ptr instead and store that in the
       * storage. */
      std::shared_ptr<Base> ptr = std::make_shared<ImplT>(std::forward<Args>(args)...);
      impl_ = &*ptr;
      storage_ = std::move(ptr);
    }
  }

  operator bool() const
  {
    return impl_ != nullptr;
  }

  Base &operator*() const
  {
    return *impl_;
  }

  Base *operator->() const
  {
    return impl_;
  }

  Base *get() const
  {
    return impl_;
  }

 protected:
  Base *impl_from_storage() const
  {
    if (!storage_.has_value()) {
      return nullptr;
    }
    return storage_.extra_info().get_impl(storage_.get());
  }
};

}  // namespace blender
