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
#include "FN_generic_virtual_array.hh"

#include "BKE_anonymous_attribute.hh"
#include "BKE_attribute.h"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_function_ref.hh"

namespace blender::bke {

/**
 * Identifies an attribute that is either named or anonymous.
 * It does not own the identifier, so it is just a reference.
 */
class AttributeIDRef {
 private:
  StringRef name_;
  const AnonymousAttributeID *anonymous_id_ = nullptr;

 public:
  AttributeIDRef();
  AttributeIDRef(StringRef name);
  AttributeIDRef(StringRefNull name);
  AttributeIDRef(const char *name);
  AttributeIDRef(const std::string &name);
  AttributeIDRef(const AnonymousAttributeID *anonymous_id);

  operator bool() const;
  uint64_t hash() const;
  bool is_named() const;
  bool is_anonymous() const;
  StringRef name() const;
  const AnonymousAttributeID &anonymous_id() const;
  bool should_be_kept() const;

  friend bool operator==(const AttributeIDRef &a, const AttributeIDRef &b);
  friend std::ostream &operator<<(std::ostream &stream, const AttributeIDRef &attribute_id);
};

}  // namespace blender::bke

/**
 * Contains information about an attribute in a geometry component.
 * More information can be added in the future. E.g. whether the attribute is builtin and how it is
 * stored (uv map, vertex group, ...).
 */
struct AttributeMetaData {
  AttributeDomain domain;
  CustomDataType data_type;

  constexpr friend bool operator==(AttributeMetaData a, AttributeMetaData b)
  {
    return (a.domain == b.domain) && (a.data_type == b.data_type);
  }
};

struct AttributeKind {
  AttributeDomain domain;
  CustomDataType data_type;
};

/**
 * Base class for the attribute initializer types described below.
 */
struct AttributeInit {
  enum class Type {
    Default,
    VArray,
    MoveArray,
  };
  Type type;
  AttributeInit(const Type type) : type(type)
  {
  }
};

/**
 * Create an attribute using the default value for the data type.
 * The default values may depend on the attribute provider implementation.
 */
struct AttributeInitDefault : public AttributeInit {
  AttributeInitDefault() : AttributeInit(Type::Default)
  {
  }
};

/**
 * Create an attribute by copying data from an existing virtual array. The virtual array
 * must have the same type as the newly created attribute.
 *
 * Note that this can be used to fill the new attribute with the default
 */
struct AttributeInitVArray : public AttributeInit {
  const blender::fn::GVArray *varray;

  AttributeInitVArray(const blender::fn::GVArray *varray)
      : AttributeInit(Type::VArray), varray(varray)
  {
  }
};

/**
 * Create an attribute with a by passing ownership of a pre-allocated contiguous array of data.
 * Sometimes data is created before a geometry component is available. In that case, it's
 * preferable to move data directly to the created attribute to avoid a new allocation and a copy.
 *
 * Note that this will only have a benefit for attributes that are stored directly as contiguous
 * arrays, so not for some built-in attributes.
 *
 * The array must be allocated with MEM_*, since `attribute_try_create` will free the array if it
 * can't be used directly, and that is generally how Blender expects custom data to be allocated.
 */
struct AttributeInitMove : public AttributeInit {
  void *data = nullptr;

  AttributeInitMove(void *data) : AttributeInit(Type::MoveArray), data(data)
  {
  }
};

/* Returns false when the iteration should be stopped. */
using AttributeForeachCallback = blender::FunctionRef<bool(
    const blender::bke::AttributeIDRef &attribute_id, const AttributeMetaData &meta_data)>;

namespace blender::bke {

using fn::CPPType;
using fn::GVArray;
using fn::GVArrayPtr;
using fn::GVMutableArray;
using fn::GVMutableArrayPtr;

const CPPType *custom_data_type_to_cpp_type(const CustomDataType type);
CustomDataType cpp_type_to_custom_data_type(const CPPType &type);
CustomDataType attribute_data_type_highest_complexity(Span<CustomDataType> data_types);
AttributeDomain attribute_domain_highest_priority(Span<AttributeDomain> domains);

/**
 * Used when looking up a "plain attribute" based on a name for reading from it.
 */
struct ReadAttributeLookup {
  /* The virtual array that is used to read from this attribute. */
  GVArrayPtr varray;
  /* Domain the attribute lives on in the geometry. */
  AttributeDomain domain;

  /* Convenience function to check if the attribute has been found. */
  operator bool() const
  {
    return this->varray.get() != nullptr;
  }
};

/**
 * Used when looking up a "plain attribute" based on a name for reading from it and writing to it.
 */
struct WriteAttributeLookup {
  /* The virtual array that is used to read from and write to the attribute. */
  GVMutableArrayPtr varray;
  /* Domain the attributes lives on in the geometry. */
  AttributeDomain domain;
  /* Call this after changing the attribute to invalidate caches that depend on this attribute. */
  std::function<void()> tag_modified_fn;

  /* Convenience function to check if the attribute has been found. */
  operator bool() const
  {
    return this->varray.get() != nullptr;
  }
};

/**
 * An output attribute allows writing to an attribute (and optionally reading as well). It adds
 * some convenience features on top of `GVMutableArray` that are very commonly used.
 *
 * Supported convenience features:
 * - Implicit type conversion when writing to builtin attributes.
 * - Supports simple access to a span containing the attribute values (that avoids the use of
 *   VMutableArray_Span in many cases).
 * - An output attribute can live side by side with an existing attribute with a different domain
 *   or data type. The old attribute will only be overwritten when the #save function is called.
 */
class OutputAttribute {
 public:
  using SaveFn = std::function<void(OutputAttribute &)>;

 private:
  GVMutableArrayPtr varray_;
  AttributeDomain domain_ = ATTR_DOMAIN_AUTO;
  SaveFn save_;
  std::unique_ptr<fn::GVMutableArray_GSpan> optional_span_varray_;
  bool ignore_old_values_ = false;
  bool save_has_been_called_ = false;

 public:
  OutputAttribute();
  OutputAttribute(OutputAttribute &&other);
  OutputAttribute(GVMutableArrayPtr varray,
                  AttributeDomain domain,
                  SaveFn save,
                  const bool ignore_old_values);

  ~OutputAttribute();

  operator bool() const;

  GVMutableArray &operator*();
  GVMutableArray *operator->();
  GVMutableArray &varray();
  AttributeDomain domain() const;
  const CPPType &cpp_type() const;
  CustomDataType custom_data_type() const;

  fn::GMutableSpan as_span();
  template<typename T> MutableSpan<T> as_span();

  void save();
};

/**
 * Same as OutputAttribute, but should be used when the data type is known at compile time.
 */
template<typename T> class OutputAttribute_Typed {
 private:
  OutputAttribute attribute_;
  std::unique_ptr<fn::GVMutableArray_Typed<T>> optional_varray_;
  VMutableArray<T> *varray_ = nullptr;

 public:
  OutputAttribute_Typed();
  OutputAttribute_Typed(OutputAttribute attribute) : attribute_(std::move(attribute))
  {
    if (attribute_) {
      optional_varray_ = std::make_unique<fn::GVMutableArray_Typed<T>>(attribute_.varray());
      varray_ = &**optional_varray_;
    }
  }

  OutputAttribute_Typed(OutputAttribute_Typed &&other);
  ~OutputAttribute_Typed();

  OutputAttribute_Typed &operator=(OutputAttribute_Typed &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~OutputAttribute_Typed();
    new (this) OutputAttribute_Typed(std::move(other));
    return *this;
  }

  operator bool() const
  {
    return varray_ != nullptr;
  }

  VMutableArray<T> &operator*()
  {
    return *varray_;
  }

  VMutableArray<T> *operator->()
  {
    return varray_;
  }

  VMutableArray<T> &varray()
  {
    return *varray_;
  }

  AttributeDomain domain() const
  {
    return attribute_.domain();
  }

  const CPPType &cpp_type() const
  {
    return CPPType::get<T>();
  }

  CustomDataType custom_data_type() const
  {
    return cpp_type_to_custom_data_type(this->cpp_type());
  }

  MutableSpan<T> as_span()
  {
    return attribute_.as_span<T>();
  }

  void save()
  {
    attribute_.save();
  }
};

/* These are not defined in the class directly, because when defining them there, the external
 * template instantiation does not work, resulting in longer compile times. */
template<typename T> inline OutputAttribute_Typed<T>::OutputAttribute_Typed() = default;
template<typename T>
inline OutputAttribute_Typed<T>::OutputAttribute_Typed(OutputAttribute_Typed &&other) = default;
template<typename T> inline OutputAttribute_Typed<T>::~OutputAttribute_Typed() = default;

/**
 * A basic container around DNA CustomData so that its users
 * don't have to implement special copy and move constructors.
 */
class CustomDataAttributes {
  /**
   * #CustomData needs a size to be freed, and unfortunately it isn't stored in the struct
   * itself, so keep track of the size here so this class can implement its own destructor.
   * If the implementation of the attribute storage changes, this could be removed.
   */
  int size_;

 public:
  CustomData data;

  CustomDataAttributes();
  ~CustomDataAttributes();
  CustomDataAttributes(const CustomDataAttributes &other);
  CustomDataAttributes(CustomDataAttributes &&other);
  CustomDataAttributes &operator=(const CustomDataAttributes &other);

  void reallocate(const int size);

  std::optional<blender::fn::GSpan> get_for_read(const AttributeIDRef &attribute_id) const;

  blender::fn::GVArrayPtr get_for_read(const AttributeIDRef &attribute_id,
                                       const CustomDataType data_type,
                                       const void *default_value) const;

  template<typename T>
  blender::fn::GVArray_Typed<T> get_for_read(const AttributeIDRef &attribute_id,
                                             const T &default_value) const
  {
    const blender::fn::CPPType &cpp_type = blender::fn::CPPType::get<T>();
    const CustomDataType type = blender::bke::cpp_type_to_custom_data_type(cpp_type);
    GVArrayPtr varray = this->get_for_read(attribute_id, type, &default_value);
    return blender::fn::GVArray_Typed<T>(std::move(varray));
  }

  std::optional<blender::fn::GMutableSpan> get_for_write(const AttributeIDRef &attribute_id);
  bool create(const AttributeIDRef &attribute_id, const CustomDataType data_type);
  bool create_by_move(const AttributeIDRef &attribute_id,
                      const CustomDataType data_type,
                      void *buffer);
  bool remove(const AttributeIDRef &attribute_id);

  bool foreach_attribute(const AttributeForeachCallback callback,
                         const AttributeDomain domain) const;
};

/* -------------------------------------------------------------------- */
/** \name #AttributeIDRef Inline Methods
 * \{ */

inline AttributeIDRef::AttributeIDRef() = default;

inline AttributeIDRef::AttributeIDRef(StringRef name) : name_(name)
{
}

inline AttributeIDRef::AttributeIDRef(StringRefNull name) : name_(name)
{
}

inline AttributeIDRef::AttributeIDRef(const char *name) : name_(name)
{
}

inline AttributeIDRef::AttributeIDRef(const std::string &name) : name_(name)
{
}

/* The anonymous id is only borrowed, the caller has to keep a reference to it. */
inline AttributeIDRef::AttributeIDRef(const AnonymousAttributeID *anonymous_id)
    : anonymous_id_(anonymous_id)
{
}

inline bool operator==(const AttributeIDRef &a, const AttributeIDRef &b)
{
  return a.anonymous_id_ == b.anonymous_id_ && a.name_ == b.name_;
}

inline AttributeIDRef::operator bool() const
{
  return this->is_named() || this->is_anonymous();
}

inline uint64_t AttributeIDRef::hash() const
{
  return get_default_hash_2(name_, anonymous_id_);
}

inline bool AttributeIDRef::is_named() const
{
  return !name_.is_empty();
}

inline bool AttributeIDRef::is_anonymous() const
{
  return anonymous_id_ != nullptr;
}

inline StringRef AttributeIDRef::name() const
{
  BLI_assert(this->is_named());
  return name_;
}

inline const AnonymousAttributeID &AttributeIDRef::anonymous_id() const
{
  BLI_assert(this->is_anonymous());
  return *anonymous_id_;
}

/**
 * \return True if the attribute should not be removed automatically as an optimization during
 * processing or copying. Anonymous attributes can be removed when they no longer have any
 * references.
 */
inline bool AttributeIDRef::should_be_kept() const
{
  return this->is_named() || BKE_anonymous_attribute_id_has_strong_references(anonymous_id_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #OutputAttribute Inline Methods
 * \{ */

inline OutputAttribute::OutputAttribute() = default;
inline OutputAttribute::OutputAttribute(OutputAttribute &&other) = default;

inline OutputAttribute::OutputAttribute(GVMutableArrayPtr varray,
                                        AttributeDomain domain,
                                        SaveFn save,
                                        const bool ignore_old_values)
    : varray_(std::move(varray)),
      domain_(domain),
      save_(std::move(save)),
      ignore_old_values_(ignore_old_values)
{
}

inline OutputAttribute::operator bool() const
{
  return varray_.get() != nullptr;
}

inline GVMutableArray &OutputAttribute::operator*()
{
  return *varray_;
}

inline GVMutableArray *OutputAttribute::operator->()
{
  return varray_.get();
}

inline GVMutableArray &OutputAttribute::varray()
{
  return *varray_;
}

inline AttributeDomain OutputAttribute::domain() const
{
  return domain_;
}

inline const CPPType &OutputAttribute::cpp_type() const
{
  return varray_->type();
}

inline CustomDataType OutputAttribute::custom_data_type() const
{
  return cpp_type_to_custom_data_type(this->cpp_type());
}

template<typename T> inline MutableSpan<T> OutputAttribute::as_span()
{
  return this->as_span().typed<T>();
}

/** \} */

}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name External Template Instantiations
 *
 * Defined in `intern/extern_implementations.cc`.
 * \{ */

namespace blender::bke {
extern template class OutputAttribute_Typed<float>;
extern template class OutputAttribute_Typed<int>;
extern template class OutputAttribute_Typed<float3>;
extern template class OutputAttribute_Typed<bool>;
extern template class OutputAttribute_Typed<ColorGeometry4f>;
}  // namespace blender::bke

/** \} */
