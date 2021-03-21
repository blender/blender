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

#include <mutex>

#include "FN_cpp_type.hh"
#include "FN_generic_span.hh"

#include "BKE_attribute.h"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"

namespace blender::bke {

using fn::CPPType;

const CPPType *custom_data_type_to_cpp_type(const CustomDataType type);
CustomDataType cpp_type_to_custom_data_type(const CPPType &type);
CustomDataType attribute_data_type_highest_complexity(Span<CustomDataType> data_types);
AttributeDomain attribute_domain_highest_priority(Span<AttributeDomain> domains);

/**
 * This class offers an indirection for reading an attribute.
 * This is useful for the following reasons:
 * - Blender does not store all attributes the same way.
 *   The simplest case are custom data layers with primitive types.
 *   A bit more complex are mesh attributes like the position of vertices,
 *   which are embedded into the MVert struct.
 *   Even more complex to access are vertex weights.
 * - Sometimes attributes are stored on one domain, but we want to access
 *   the attribute on a different domain. Therefore, we have to interpolate
 *   between the domains.
 */
class ReadAttribute {
 protected:
  const AttributeDomain domain_;
  const CPPType &cpp_type_;
  const CustomDataType custom_data_type_;
  const int64_t size_;

  /* Protects the span below, so that no two threads initialize it at the same time. */
  mutable std::mutex span_mutex_;
  /* When it is not null, it points to the attribute array or a temporary array that contains all
   * the attribute values. */
  mutable void *array_buffer_ = nullptr;
  /* Is true when the buffer above is owned by the attribute accessor. */
  mutable bool array_is_temporary_ = false;

 public:
  ReadAttribute(AttributeDomain domain, const CPPType &cpp_type, const int64_t size)
      : domain_(domain),
        cpp_type_(cpp_type),
        custom_data_type_(cpp_type_to_custom_data_type(cpp_type)),
        size_(size)
  {
  }

  virtual ~ReadAttribute();

  AttributeDomain domain() const
  {
    return domain_;
  }

  const CPPType &cpp_type() const
  {
    return cpp_type_;
  }

  CustomDataType custom_data_type() const
  {
    return custom_data_type_;
  }

  int64_t size() const
  {
    return size_;
  }

  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index < size_);
    this->get_internal(index, r_value);
  }

  /* Get a span that contains all attribute values. */
  fn::GSpan get_span() const;

  template<typename T> Span<T> get_span() const
  {
    return this->get_span().typed<T>();
  }

 protected:
  /* r_value is expected to be uninitialized. */
  virtual void get_internal(const int64_t index, void *r_value) const = 0;

  virtual void initialize_span() const;
};

/**
 * This exists for similar reasons as the ReadAttribute class, except that
 * it does not deal with interpolation between domains.
 */
class WriteAttribute {
 protected:
  const AttributeDomain domain_;
  const CPPType &cpp_type_;
  const CustomDataType custom_data_type_;
  const int64_t size_;

  /* When not null, this points either to the attribute array or to a temporary array. */
  void *array_buffer_ = nullptr;
  /* True, when the buffer points to a temporary array. */
  bool array_is_temporary_ = false;
  /* This helps to protect against forgetting to apply changes done to the array. */
  bool array_should_be_applied_ = false;

 public:
  WriteAttribute(AttributeDomain domain, const CPPType &cpp_type, const int64_t size)
      : domain_(domain),
        cpp_type_(cpp_type),
        custom_data_type_(cpp_type_to_custom_data_type(cpp_type)),
        size_(size)
  {
  }

  virtual ~WriteAttribute();

  AttributeDomain domain() const
  {
    return domain_;
  }

  const CPPType &cpp_type() const
  {
    return cpp_type_;
  }

  CustomDataType custom_data_type() const
  {
    return custom_data_type_;
  }

  int64_t size() const
  {
    return size_;
  }

  void get(const int64_t index, void *r_value) const
  {
    BLI_assert(index < size_);
    this->get_internal(index, r_value);
  }

  void set(const int64_t index, const void *value)
  {
    BLI_assert(index < size_);
    this->set_internal(index, value);
  }

  /* Get a span that new attribute values can be written into. When all values have been changed,
   * #apply_span has to be called. */
  fn::GMutableSpan get_span();
  /* The span returned by this method might not contain the current attribute values. */
  fn::GMutableSpan get_span_for_write_only();
  /* Write the changes to the span into the actual attribute, if they aren't already. */
  void apply_span();

  template<typename T> MutableSpan<T> get_span()
  {
    return this->get_span().typed<T>();
  }

  template<typename T> MutableSpan<T> get_span_for_write_only()
  {
    return this->get_span_for_write_only().typed<T>();
  }

 protected:
  virtual void get_internal(const int64_t index, void *r_value) const = 0;
  virtual void set_internal(const int64_t index, const void *value) = 0;

  virtual void initialize_span(const bool write_only);
  virtual void apply_span_if_necessary();
};

using ReadAttributePtr = std::unique_ptr<ReadAttribute>;
using WriteAttributePtr = std::unique_ptr<WriteAttribute>;

/* This provides type safe access to an attribute.
 * The underlying ReadAttribute is owned optionally. */
template<typename T> class TypedReadAttribute {
 private:
  std::unique_ptr<const ReadAttribute> owned_attribute_;
  const ReadAttribute *attribute_;

 public:
  TypedReadAttribute(ReadAttributePtr attribute) : TypedReadAttribute(*attribute)
  {
    owned_attribute_ = std::move(attribute);
    BLI_assert(owned_attribute_);
  }

  TypedReadAttribute(const ReadAttribute &attribute) : attribute_(&attribute)
  {
    BLI_assert(attribute_->cpp_type().is<T>());
  }

  int64_t size() const
  {
    return attribute_->size();
  }

  T operator[](const int64_t index) const
  {
    BLI_assert(index < attribute_->size());
    T value;
    value.~T();
    attribute_->get(index, &value);
    return value;
  }

  /* Get a span to that contains all attribute values for faster and more convenient access. */
  Span<T> get_span() const
  {
    return attribute_->get_span().template typed<T>();
  }
};

/* This provides type safe access to an attribute.
 * The underlying WriteAttribute is owned optionally. */
template<typename T> class TypedWriteAttribute {
 private:
  std::unique_ptr<WriteAttribute> owned_attribute_;
  WriteAttribute *attribute_;

 public:
  TypedWriteAttribute(WriteAttributePtr attribute) : TypedWriteAttribute(*attribute)
  {
    owned_attribute_ = std::move(attribute);
    BLI_assert(owned_attribute_);
  }

  TypedWriteAttribute(WriteAttribute &attribute) : attribute_(&attribute)
  {
    BLI_assert(attribute_->cpp_type().is<T>());
  }

  int64_t size() const
  {
    return attribute_->size();
  }

  T operator[](const int64_t index) const
  {
    BLI_assert(index < attribute_->size());
    T value;
    value.~T();
    attribute_->get(index, &value);
    return value;
  }

  void set(const int64_t index, const T &value)
  {
    attribute_->set(index, &value);
  }

  /* Get a span that new values can be written into. Once all values have been updated #apply_span
   * has to be called. */
  MutableSpan<T> get_span()
  {
    return attribute_->get_span().typed<T>();
  }
  /* The span returned by this method might not contain the current attribute values. */
  MutableSpan<T> get_span_for_write_only()
  {
    return attribute_->get_span_for_write_only().typed<T>();
  }

  /* Write back all changes to the actual attribute, if necessary. */
  void apply_span()
  {
    attribute_->apply_span();
  }
};

using BooleanReadAttribute = TypedReadAttribute<bool>;
using FloatReadAttribute = TypedReadAttribute<float>;
using Float2ReadAttribute = TypedReadAttribute<float2>;
using Float3ReadAttribute = TypedReadAttribute<float3>;
using Int32ReadAttribute = TypedReadAttribute<int>;
using Color4fReadAttribute = TypedReadAttribute<Color4f>;
using BooleanWriteAttribute = TypedWriteAttribute<bool>;
using FloatWriteAttribute = TypedWriteAttribute<float>;
using Float2WriteAttribute = TypedWriteAttribute<float2>;
using Float3WriteAttribute = TypedWriteAttribute<float3>;
using Int32WriteAttribute = TypedWriteAttribute<int>;
using Color4fWriteAttribute = TypedWriteAttribute<Color4f>;

}  // namespace blender::bke
