/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>
#include <string>

#include "BLI_hash.hh"
#include "BLI_string_ref.hh"

#include "BKE_anonymous_attribute.h"

namespace blender::bke {

/**
 * Wrapper for #AnonymousAttributeID with RAII semantics.
 * This class should typically not be used directly. Instead use #StrongAnonymousAttributeID or
 * #WeakAnonymousAttributeID.
 */
template<bool IsStrongReference> class OwnedAnonymousAttributeID {
 private:
  const AnonymousAttributeID *data_ = nullptr;

  template<bool OtherIsStrongReference> friend class OwnedAnonymousAttributeID;

 public:
  OwnedAnonymousAttributeID() = default;

  /** Create a new anonymous attribute id. */
  explicit OwnedAnonymousAttributeID(StringRefNull debug_name)
  {
    if constexpr (IsStrongReference) {
      data_ = BKE_anonymous_attribute_id_new_strong(debug_name.c_str());
    }
    else {
      data_ = BKE_anonymous_attribute_id_new_weak(debug_name.c_str());
    }
  }

  /**
   * This transfers ownership, so no incref is necessary.
   * The caller has to make sure that it owned the anonymous id.
   */
  explicit OwnedAnonymousAttributeID(const AnonymousAttributeID *anonymous_id)
      : data_(anonymous_id)
  {
  }

  template<bool OtherIsStrong>
  OwnedAnonymousAttributeID(const OwnedAnonymousAttributeID<OtherIsStrong> &other)
  {
    data_ = other.data_;
    this->incref();
  }

  template<bool OtherIsStrong>
  OwnedAnonymousAttributeID(OwnedAnonymousAttributeID<OtherIsStrong> &&other)
  {
    data_ = other.data_;
    this->incref();
    other.decref();
    other.data_ = nullptr;
  }

  ~OwnedAnonymousAttributeID()
  {
    this->decref();
  }

  template<bool OtherIsStrong>
  OwnedAnonymousAttributeID &operator=(const OwnedAnonymousAttributeID<OtherIsStrong> &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~OwnedAnonymousAttributeID();
    new (this) OwnedAnonymousAttributeID(other);
    return *this;
  }

  template<bool OtherIsStrong>
  OwnedAnonymousAttributeID &operator=(OwnedAnonymousAttributeID<OtherIsStrong> &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~OwnedAnonymousAttributeID();
    new (this) OwnedAnonymousAttributeID(std::move(other));
    return *this;
  }

  operator bool() const
  {
    return data_ != nullptr;
  }

  StringRefNull debug_name() const
  {
    BLI_assert(data_ != nullptr);
    return BKE_anonymous_attribute_id_debug_name(data_);
  }

  bool has_strong_references() const
  {
    BLI_assert(data_ != nullptr);
    return BKE_anonymous_attribute_id_has_strong_references(data_);
  }

  /** Extract the ownership of the currently wrapped anonymous id. */
  const AnonymousAttributeID *extract()
  {
    const AnonymousAttributeID *extracted_data = data_;
    /* Don't decref because the caller becomes the new owner. */
    data_ = nullptr;
    return extracted_data;
  }

  /** Get the wrapped anonymous id, without taking ownership. */
  const AnonymousAttributeID *get() const
  {
    return data_;
  }

 private:
  void incref()
  {
    if (data_ == nullptr) {
      return;
    }
    if constexpr (IsStrongReference) {
      BKE_anonymous_attribute_id_increment_strong(data_);
    }
    else {
      BKE_anonymous_attribute_id_increment_weak(data_);
    }
  }

  void decref()
  {
    if (data_ == nullptr) {
      return;
    }
    if constexpr (IsStrongReference) {
      BKE_anonymous_attribute_id_decrement_strong(data_);
    }
    else {
      BKE_anonymous_attribute_id_decrement_weak(data_);
    }
  }
};

using StrongAnonymousAttributeID = OwnedAnonymousAttributeID<true>;
using WeakAnonymousAttributeID = OwnedAnonymousAttributeID<false>;

}  // namespace blender::bke
