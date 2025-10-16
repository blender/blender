/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <functional>
#include <optional>

#include "BLI_function_ref.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_implicit_sharing.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"
#include "BLI_struct_equality_utils.hh"

#include "BKE_attribute_filters.hh"

struct ID;
struct Mesh;
struct PointCloud;
namespace blender::fn {
namespace multi_function {
class MultiFunction;
}
class GField;
}  // namespace blender::fn

namespace blender::bke {

class AttributeAccessor;
class MutableAttributeAccessor;

/** Some storage types are only relevant for certain attribute types. */
enum class AttrStorageType : int8_t {
  /** #AttributeDataArray. */
  Array,
  /** A single value for the whole attribute. */
  Single,
};

enum class AttrType : int16_t {
  Bool,
  Int8,
  Int16_2D,
  Int32,
  Int32_2D,
  Float,
  Float2,
  Float3,
  Float4x4,
  ColorByte,
  ColorFloat,
  Quaternion,
  String,
};

const CPPType &attribute_type_to_cpp_type(AttrType type);
AttrType cpp_type_to_attribute_type(const CPPType &type);

enum class AttrDomain : int8_t {
  /* Used to choose automatically based on other data. */
  Auto = -1,
  /* Mesh, Curve or Point Cloud Point. */
  Point = 0,
  /* Mesh Edge. */
  Edge = 1,
  /* Mesh Face. */
  Face = 2,
  /* Mesh Corner. */
  Corner = 3,
  /* A single curve in a larger curve data-block. */
  Curve = 4,
  /* Instance. */
  Instance = 5,
  /* A layer in a grease pencil data-block. */
  Layer = 6,
};
#define ATTR_DOMAIN_NUM 7

/**
 * Contains information about an attribute in a geometry component.
 * More information can be added in the future. E.g. whether the attribute is builtin and how it is
 * stored (uv map, vertex group, ...).
 */
struct AttributeMetaData {
  AttrDomain domain;
  AttrType data_type;

  BLI_STRUCT_EQUALITY_OPERATORS_2(AttributeMetaData, domain, data_type)
};

struct AttributeDomainAndType {
  AttrDomain domain;
  AttrType data_type;
  BLI_STRUCT_EQUALITY_OPERATORS_2(AttributeDomainAndType, domain, data_type)
};

/**
 * Base class for the attribute initializer types described below.
 */
struct AttributeInit {
  enum class Type {
    /** #AttributeInitConstruct. */
    Construct,
    /** #AttributeInitDefaultValue. */
    DefaultValue,
    /** #AttributeInitVArray. */
    VArray,
    /** #AttributeInitMoveArray. */
    MoveArray,
    /** #AttributeInitShared. */
    Shared,
  };
  Type type;
  AttributeInit(const Type type) : type(type) {}
};

/**
 * Default construct new attribute values. Does nothing for trivial types. This should be used
 * if all attribute element values will be set by the caller after creating the attribute.
 */
struct AttributeInitConstruct : public AttributeInit {
  AttributeInitConstruct() : AttributeInit(Type::Construct) {}
};

/**
 * Create an attribute using the default value for the data type (almost always "zero").
 */
struct AttributeInitDefaultValue : public AttributeInit {
  AttributeInitDefaultValue() : AttributeInit(Type::DefaultValue) {}
};

/**
 * Create an attribute by copying data from an existing virtual array. The virtual array
 * must have the same type as the newly created attribute.
 */
struct AttributeInitVArray : public AttributeInit {
  GVArray varray;

  AttributeInitVArray(GVArray varray) : AttributeInit(Type::VArray), varray(std::move(varray)) {}
};

/**
 * Create an attribute with a by passing ownership of a pre-allocated contiguous array of data.
 * Sometimes data is created before a geometry component is available. In that case, it's
 * preferable to move data directly to the created attribute to avoid a new allocation and a copy.
 *
 * The array must be allocated with MEM_*, since `attribute_try_create` will free the array if it
 * can't be used directly, and that is generally how Blender expects custom data to be allocated.
 */
struct AttributeInitMoveArray : public AttributeInit {
  void *data = nullptr;

  AttributeInitMoveArray(void *data) : AttributeInit(Type::MoveArray), data(data) {}
};

/**
 * Create a shared attribute by adding a user to a shared data array.
 * The sharing info has ownership of the provided contiguous array.
 */
struct AttributeInitShared : public AttributeInit {
  const void *data = nullptr;
  const ImplicitSharingInfo *sharing_info = nullptr;

  AttributeInitShared(const void *data, const ImplicitSharingInfo &sharing_info)
      : AttributeInit(Type::Shared), data(data), sharing_info(&sharing_info)
  {
  }
};

/* Returns false when the iteration should be stopped. */
using AttributeForeachCallback =
    FunctionRef<bool(StringRefNull attribute_id, const AttributeMetaData &meta_data)>;

/**
 * Result when looking up an attribute from some geometry with the intention of only reading from
 * it.
 */
template<typename T> struct AttributeReader {
  /**
   * Virtual array that provides access to the attribute data. This may be empty.
   */
  VArray<T> varray;
  /**
   * Domain where the attribute is stored. This also determines the size of the virtual array.
   */
  AttrDomain domain;

  /**
   * Information about shared ownership of the attribute array. This will only be provided
   * if the virtual array directly references the contiguous original attribute array.
   */
  const ImplicitSharingInfo *sharing_info;

  const VArray<T> &operator*() const &
  {
    return this->varray;
  }

  VArray<T> &operator*() &
  {
    return this->varray;
  }

  VArray<T> operator*() &&
  {
    return std::move(this->varray);
  }

  operator bool() const
  {
    return this->varray;
  }
};

/**
 * A utility to make sure attribute values are valid, for attributes like "material_index" which
 * can only be positive, or attributes that represent enum options. This is usually only necessary
 * when writing attributes from an untrusted/arbitrary user input.
 */
struct AttributeValidator {
  /**
   * Single input, single output function that corrects attribute values if necessary.
   */
  const fn::multi_function::MultiFunction *function;

  operator bool() const
  {
    return this->function != nullptr;
  }
  /**
   * Return a field that creates corrected attribute values.
   */
  fn::GField validate_field_if_necessary(const fn::GField &field) const;
};

/**
 * Result when looking up an attribute from some geometry with read and write access. After writing
 * to the attribute, the #finish method has to be called. This may invalidate caches based on this
 * attribute.
 */
template<typename T> struct AttributeWriter {
  /**
   * Virtual array giving read and write access to the attribute. This may be empty.
   * Consider using #SpanAttributeWriter when you want to access the virtual array as a span.
   */
  VMutableArray<T> varray;
  /**
   * Domain where the attribute is stored on the geometry. Also determines the size of the virtual
   * array.
   */
  AttrDomain domain;
  /**
   * A function that has to be called after the attribute has been edited. This may be empty.
   */
  std::function<void()> tag_modified_fn;

  operator bool() const
  {
    return this->varray;
  }

  /**
   * Has to be called after the attribute has been modified.
   */
  void finish()
  {
    if (this->tag_modified_fn) {
      this->tag_modified_fn();
    }
  }
};

/**
 * A version of #AttributeWriter for the common case when the user of the attribute wants to write
 * to a span instead of a virtual array. Since most attributes are spans internally, this can
 * result in better performance and also simplifies code.
 */
template<typename T> struct SpanAttributeWriter {
  /**
   * A span based on the virtual array that contains the attribute data. This may be empty.
   */
  MutableVArraySpan<T> span;
  /**
   * Domain of the attribute. Also determines the size of the span.
   */
  AttrDomain domain;
  /**
   * Has to be called after writing to the span.
   */
  std::function<void()> tag_modified_fn;

  SpanAttributeWriter() = default;

  SpanAttributeWriter(AttributeWriter<T> &&other, const bool copy_values_to_span)
      : span(std::move(other.varray), copy_values_to_span),
        domain(other.domain),
        tag_modified_fn(std::move(other.tag_modified_fn))
  {
  }

  operator bool() const
  {
    return span.varray();
  }

  /**
   * Has to be called when done writing to the attribute. This makes sure that the data is copied
   * to the underlying attribute if it was not stored as an array. Furthermore, this may invalidate
   * other data depending on the modified attribute.
   */
  void finish()
  {
    if (this->span.varray()) {
      this->span.save();
    }
    if (this->tag_modified_fn) {
      this->tag_modified_fn();
    }
  }
};

/**
 * A generic version of #AttributeReader.
 */
struct GAttributeReader {
  GVArray varray;
  AttrDomain domain;
  const ImplicitSharingInfo *sharing_info;

  operator bool() const
  {
    return this->varray;
  }

  const GVArray &operator*() const &
  {
    return this->varray;
  }

  GVArray &operator*() &
  {
    return this->varray;
  }

  GVArray operator*() &&
  {
    return std::move(this->varray);
  }

  template<typename T> AttributeReader<T> typed() const
  {
    return {varray.typed<T>(), domain, sharing_info};
  }
};

/**
 * A generic version of #AttributeWriter.
 */
struct GAttributeWriter {
  GVMutableArray varray;
  AttrDomain domain;
  std::function<void()> tag_modified_fn;

  operator bool() const
  {
    return this->varray;
  }

  void finish()
  {
    if (this->tag_modified_fn) {
      this->tag_modified_fn();
    }
  }

  template<typename T> AttributeWriter<T> typed() const
  {
    return {varray.typed<T>(), domain, tag_modified_fn};
  }
};

/**
 * A generic version of #SpanAttributeWriter.
 */
struct GSpanAttributeWriter {
  GMutableVArraySpan span;
  AttrDomain domain;
  std::function<void()> tag_modified_fn;

  GSpanAttributeWriter() = default;

  GSpanAttributeWriter(GAttributeWriter &&other, const bool copy_values_to_span)
      : span(std::move(other.varray), copy_values_to_span),
        domain(other.domain),
        tag_modified_fn(std::move(other.tag_modified_fn))
  {
  }

  operator bool() const
  {
    return span.varray();
  }

  void finish()
  {
    if (this->span.varray()) {
      this->span.save();
    }
    if (this->tag_modified_fn) {
      this->tag_modified_fn();
    }
  }
};

/**
 * This is used when iterating over attributes, e.g. with #foreach_attribute. It contains meta-data
 * for the current attribute and provides easy access to the actual attribute data.
 */
class AttributeIter {
 public:
  StringRefNull name;
  AttrDomain domain;
  AttrType data_type;
  bool is_builtin = false;
  mutable const AttributeAccessor *accessor = nullptr;

 private:
  FunctionRef<GAttributeReader()> get_fn_;
  mutable bool stop_iteration_ = false;

 public:
  AttributeIter(const StringRefNull name,
                const AttrDomain domain,
                const AttrType data_type,
                const FunctionRef<GAttributeReader()> get_fn)
      : name(name), domain(domain), data_type(data_type), get_fn_(get_fn)
  {
  }

  /** Stops the iteration. Remaining attributes will be skipped. */
  void stop() const
  {
    stop_iteration_ = true;
  }

  bool is_stopped() const
  {
    return stop_iteration_;
  }

  /** Get read-only access to the current attribute. This method always succeeds. */
  GAttributeReader get() const
  {
    return get_fn_();
  }

  /** Same as above, but may perform type and domain interpolation. This may return none. */
  GAttributeReader get(std::optional<AttrDomain> domain, std::optional<AttrType> data_type) const;

  GAttributeReader get(const AttrDomain domain) const
  {
    return this->get(domain, std::nullopt);
  }

  GAttributeReader get(const AttrType data_type) const
  {
    return this->get(std::nullopt, data_type);
  }

  template<typename T>
  AttributeReader<T> get(const std::optional<AttrDomain> domain = std::nullopt) const
  {
    const CPPType &cpp_type = CPPType::get<T>();
    const AttrType data_type = cpp_type_to_attribute_type(cpp_type);
    return this->get(domain, data_type).typed<T>();
  }
};

/**
 * Core functions which make up the attribute API. They should not be called directly, but through
 * #AttributesAccessor or #MutableAttributesAccessor.
 *
 * This is similar to a virtual function table. A struct of function pointers is used instead,
 * because this way the attribute accessors can be trivial and can be passed around by value. This
 * makes it easy to return the attribute accessor for a geometry from a function.
 */
struct AttributeAccessorFunctions {
  bool (*domain_supported)(const void *owner, AttrDomain domain);
  int (*domain_size)(const void *owner, AttrDomain domain);
  std::optional<AttributeDomainAndType> (*builtin_domain_and_type)(const void *owner,
                                                                   StringRef attribute_id);
  GPointer (*get_builtin_default)(const void *owner, StringRef attribute_id);
  GAttributeReader (*lookup)(const void *owner, StringRef attribute_id);
  GVArray (*adapt_domain)(const void *owner,
                          const GVArray &varray,
                          AttrDomain from_domain,
                          AttrDomain to_domain);
  void (*foreach_attribute)(const void *owner,
                            FunctionRef<void(const AttributeIter &iter)> fn,
                            const AttributeAccessor &accessor);
  AttributeValidator (*lookup_validator)(const void *owner, StringRef attribute_id);
  GAttributeWriter (*lookup_for_write)(void *owner, StringRef attribute_id);
  bool (*remove)(void *owner, StringRef attribute_id);
  bool (*add)(void *owner,
              StringRef attribute_id,
              AttrDomain domain,
              AttrType data_type,
              const AttributeInit &initializer);
};

/**
 * Provides read-only access to the set of attributes on some geometry.
 *
 * Note, this does not own the attributes. When the owner is freed, it is invalid to access its
 * attributes.
 */
class AttributeAccessor {
 protected:
  /**
   * The data that actually owns the attributes, for example, a pointer to a #Mesh or #PointCloud
   * Most commonly this is a pointer to a #Mesh or #PointCloud.
   * Under some circumstances this can be null. In that case most methods can't be used. Allowed
   * methods are #domain_size, #foreach_attribute and #is_builtin. We could potentially make these
   * methods accessible without #AttributeAccessor and then #owner_ could always be non-null.
   *
   * \note This class cannot modify the owner's attributes, but the pointer is still non-const, so
   * this class can be a base class for the mutable version.
   */
  void *owner_;
  /**
   * Functions that know how to access the attributes stored in the owner above.
   */
  const AttributeAccessorFunctions *fn_;

 public:
  AttributeAccessor(const void *owner, const AttributeAccessorFunctions &fn)
      : owner_(const_cast<void *>(owner)), fn_(&fn)
  {
  }

  /**
   * Construct an #AttributeAccessor from an ID.
   */
  static std::optional<AttributeAccessor> from_id(const ID &id);

  /**
   * \return True, when the attribute is available.
   */
  bool contains(StringRef attribute_id) const;

  /**
   * \return Information about the attribute if it exists.
   */
  std::optional<AttributeMetaData> lookup_meta_data(StringRef attribute_id) const;

  /**
   * \return True, when attributes can exist on that domain.
   */
  bool domain_supported(const AttrDomain domain) const
  {
    return fn_->domain_supported(owner_, domain);
  }

  /**
   * \return Number of elements in the given domain.
   */
  int domain_size(const AttrDomain domain) const
  {
    return fn_->domain_size(owner_, domain);
  }

  /**
   * \return True, when the attribute has a special meaning for Blender and can't be used for
   * arbitrary things.
   */
  bool is_builtin(const StringRef attribute_id) const
  {
    return fn_->builtin_domain_and_type(owner_, attribute_id).has_value();
  }

  /**
   * \return The required domain and type for the attribute, if it is builtin.
   */
  std::optional<AttributeDomainAndType> get_builtin_domain_and_type(const StringRef name) const
  {
    return fn_->builtin_domain_and_type(owner_, name);
  }

  /**
   * \return The default value defined by the `#BuiltinAttributeProvider`. The provided
   * attribute_id must refer to a builtin attribute.
   */
  GPointer get_builtin_default(const StringRef attribute_id) const
  {
    BLI_assert(this->is_builtin(attribute_id));
    return fn_->get_builtin_default(owner_, attribute_id);
  }

  /**
   * Get read-only access to the attribute. If the attribute does not exist, the return value is
   * empty.
   */
  GAttributeReader lookup(const StringRef attribute_id) const
  {
    return fn_->lookup(owner_, attribute_id);
  }

  /**
   * Get read-only access to the attribute. If necessary, the attribute is interpolated to the
   * given domain, and converted to the given type, in that order.  The result may be empty.
   */
  GAttributeReader lookup(StringRef attribute_id,
                          std::optional<AttrDomain> domain,
                          std::optional<AttrType> data_type) const;

  /**
   * Get read-only access to the attribute whereby the attribute is interpolated to the given
   * domain. The result may be empty.
   */
  GAttributeReader lookup(const StringRef attribute_id, const AttrDomain domain) const
  {
    return this->lookup(attribute_id, domain, std::nullopt);
  }

  /**
   * Get read-only access to the attribute whereby the attribute is converted to the given type.
   * The result may be empty.
   */
  GAttributeReader lookup(const StringRef attribute_id, const AttrType data_type) const
  {
    return this->lookup(attribute_id, std::nullopt, data_type);
  }

  /**
   * Get read-only access to the attribute. If necessary, the attribute is interpolated to the
   * given domain and then converted to the given type, in that order. The result may be empty.
   */
  template<typename T>
  AttributeReader<T> lookup(const StringRef attribute_id,
                            const std::optional<AttrDomain> domain = std::nullopt) const
  {
    const CPPType &cpp_type = CPPType::get<T>();
    const AttrType data_type = cpp_type_to_attribute_type(cpp_type);
    return this->lookup(attribute_id, domain, data_type).typed<T>();
  }

  /**
   * Get read-only access to the attribute. If necessary, the attribute is interpolated to the
   * given domain and then converted to the given data type, in that order.
   * If the attribute does not exist, a virtual array with the given default value is returned.
   * If the passed in default value is null, the default value of the type is used (generally 0).
   */
  GAttributeReader lookup_or_default(StringRef attribute_id,
                                     AttrDomain domain,
                                     AttrType data_type,
                                     const void *default_value = nullptr) const;

  /**
   * Same as the generic version above, but should be used when the type is known at compile time.
   */
  template<typename T>
  AttributeReader<T> lookup_or_default(const StringRef attribute_id,
                                       const AttrDomain domain,
                                       const T &default_value) const
  {
    if (AttributeReader<T> varray = this->lookup<T>(attribute_id, domain)) {
      return varray;
    }
    return {VArray<T>::from_single(default_value, this->domain_size(domain)), domain};
  }

  /**
   * Same as the generic version above, but should be used when the type is known at compile time.
   */
  AttributeValidator lookup_validator(const StringRef attribute_id) const
  {
    return fn_->lookup_validator(owner_, attribute_id);
  }

  /**
   * Interpolate data from one domain to another.
   */
  GVArray adapt_domain(const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) const
  {
    return fn_->adapt_domain(owner_, varray, from_domain, to_domain);
  }

  /**
   * Interpolate data from one domain to another.
   */
  template<typename T>
  VArray<T> adapt_domain(const VArray<T> &varray,
                         const AttrDomain from_domain,
                         const AttrDomain to_domain) const
  {
    return this->adapt_domain(GVArray(varray), from_domain, to_domain).typed<T>();
  }

  /**
   * Run the provided function for every attribute.
   * Attributes should not be removed or added during iteration.
   */
  void foreach_attribute(const FunctionRef<void(const AttributeIter &)> fn) const
  {
    if (owner_ != nullptr) {
      fn_->foreach_attribute(owner_, fn, *this);
    }
  }

  /**
   * Get a set of all attributes.
   */
  Set<StringRefNull> all_ids() const;
};

/**
 * Extends #AttributeAccessor with methods that allow modifying individual attributes as well as
 * the set of attributes.
 */
class MutableAttributeAccessor : public AttributeAccessor {
 public:
  MutableAttributeAccessor(void *owner, const AttributeAccessorFunctions &fn)
      : AttributeAccessor(owner, fn)
  {
  }

  /**
   * Get a writable attribute or none if it does not exist.
   * Make sure to call #finish after changes are done.
   */
  GAttributeWriter lookup_for_write(StringRef attribute_id);

  /**
   * Same as above, but returns a type that makes it easier to work with the attribute as a span.
   */
  GSpanAttributeWriter lookup_for_write_span(StringRef attribute_id);

  /**
   * Get a writable attribute or non if it does not exist.
   * Make sure to call #finish after changes are done.
   */
  template<typename T> AttributeWriter<T> lookup_for_write(const StringRef attribute_id)
  {
    GAttributeWriter attribute = this->lookup_for_write(attribute_id);
    if (!attribute) {
      return {};
    }
    if (!attribute.varray.type().is<T>()) {
      return {};
    }
    return attribute.typed<T>();
  }

  /**
   * Same as above, but returns a type that makes it easier to work with the attribute as a span.
   */
  template<typename T> SpanAttributeWriter<T> lookup_for_write_span(const StringRef attribute_id)
  {
    AttributeWriter<T> attribute = this->lookup_for_write<T>(attribute_id);
    if (attribute) {
      return SpanAttributeWriter<T>{std::move(attribute), true};
    }
    return {};
  }

  /**
   * Replace the existing attribute with a new one with a different name.
   */
  bool rename(StringRef old_attribute_id, StringRef new_attribute_id);

  /**
   * Create a new attribute.
   * \return True, when a new attribute has been created. False, when it's not possible to create
   * this attribute or there is already an attribute with that id.
   */
  bool add(const StringRef attribute_id,
           const AttrDomain domain,
           const AttrType data_type,
           const AttributeInit &initializer)
  {
    if (!this->domain_supported(domain)) {
      return false;
    }
    if (this->contains(attribute_id)) {
      return false;
    }
    return fn_->add(owner_, attribute_id, domain, data_type, initializer);
  }
  template<typename T>
  bool add(const StringRef attribute_id, const AttrDomain domain, const AttributeInit &initializer)
  {
    const CPPType &cpp_type = CPPType::get<T>();
    const AttrType data_type = cpp_type_to_attribute_type(cpp_type);
    return this->add(attribute_id, domain, data_type, initializer);
  }

  /**
   * Find an attribute with the given id, domain and data type. If it does not exist, create a new
   * attribute. If the attribute does not exist and can't be created (e.g. because it already
   * exists on a different domain or with a different type), none is returned.
   */
  GAttributeWriter lookup_or_add_for_write(
      StringRef attribute_id,
      AttrDomain domain,
      AttrType data_type,
      const AttributeInit &initializer = AttributeInitDefaultValue());

  /**
   * Same as above, but returns a type that makes it easier to work with the attribute as a span.
   * If the caller newly initializes the attribute, it's better to use
   * #lookup_or_add_for_write_only_span.
   */
  GSpanAttributeWriter lookup_or_add_for_write_span(
      StringRef attribute_id,
      AttrDomain domain,
      AttrType data_type,
      const AttributeInit &initializer = AttributeInitDefaultValue());

  /**
   * Same as above, but should be used when the type is known at compile time.
   */
  template<typename T>
  AttributeWriter<T> lookup_or_add_for_write(
      const StringRef attribute_id,
      const AttrDomain domain,
      const AttributeInit &initializer = AttributeInitDefaultValue())
  {
    const CPPType &cpp_type = CPPType::get<T>();
    const AttrType data_type = cpp_type_to_attribute_type(cpp_type);
    return this->lookup_or_add_for_write(attribute_id, domain, data_type, initializer).typed<T>();
  }

  /**
   * Same as above, but should be used when the type is known at compile time.
   */
  template<typename T>
  SpanAttributeWriter<T> lookup_or_add_for_write_span(
      const StringRef attribute_id,
      const AttrDomain domain,
      const AttributeInit &initializer = AttributeInitDefaultValue())
  {
    AttributeWriter<T> attribute = this->lookup_or_add_for_write<T>(
        attribute_id, domain, initializer);
    if (attribute) {
      return SpanAttributeWriter<T>{std::move(attribute), true};
    }
    return {};
  }

  /**
   * Find an attribute with the given id, domain and data type. If it does not exist, create a new
   * attribute. If the attribute does not exist and can't be created, none is returned.
   *
   * The "only" in the name indicates that the caller should not read existing values from the
   * span. If the attribute is not stored as span internally, the existing values won't be copied
   * over to the span.
   *
   * For trivial types, the values in a newly created attribute will not be initialized.
   */
  GSpanAttributeWriter lookup_or_add_for_write_only_span(StringRef attribute_id,
                                                         AttrDomain domain,
                                                         AttrType data_type);

  /**
   * Same as above, but should be used when the type is known at compile time.
   */
  template<typename T>
  SpanAttributeWriter<T> lookup_or_add_for_write_only_span(const StringRef attribute_id,
                                                           const AttrDomain domain)
  {
    AttributeWriter<T> attribute = this->lookup_or_add_for_write<T>(
        attribute_id, domain, AttributeInitConstruct());

    if (attribute) {
      return SpanAttributeWriter<T>{std::move(attribute), false};
    }
    return {};
  }

  /**
   * Remove an attribute.
   * \return True, when the attribute has been deleted. False, when it's not possible to delete
   * this attribute or if there is no attribute with that id.
   */
  bool remove(const StringRef attribute_id)
  {
    return fn_->remove(owner_, attribute_id);
  }

  /**
   * Remove all anonymous attributes.
   */
  void remove_anonymous();
};

struct AttributeTransferData {
  /* Expect that if an attribute exists, it is stored as a contiguous array internally anyway. */
  GVArraySpan src;
  StringRef name;
  AttributeMetaData meta_data;
  GSpanAttributeWriter dst;
};
/**
 * Retrieve attribute arrays and writers for attributes that should be transferred between
 * data-blocks of the same type.
 */
Vector<AttributeTransferData> retrieve_attributes_for_transfer(
    const AttributeAccessor src_attributes,
    MutableAttributeAccessor dst_attributes,
    Span<AttrDomain> domains,
    const AttributeFilter &attribute_filter = {});

bool allow_procedural_attribute_access(StringRef attribute_name);
extern const char *no_procedural_access_message;

AttrType attribute_data_type_highest_complexity(Span<AttrType> data_types);
/**
 * Domains with a higher "information density" have a higher priority,
 * in order to choose a domain that will not lose data through domain conversion.
 */
AttrDomain attribute_domain_highest_priority(Span<AttrDomain> domains);

void gather_attributes(AttributeAccessor src_attributes,
                       AttrDomain src_domain,
                       AttrDomain dst_domain,
                       const AttributeFilter &attribute_filter,
                       const IndexMask &selection,
                       MutableAttributeAccessor dst_attributes);

/**
 * Fill the destination attribute by gathering indexed values from src attributes.
 */
void gather_attributes(AttributeAccessor src_attributes,
                       AttrDomain src_domain,
                       AttrDomain dst_domain,
                       const AttributeFilter &attribute_filter,
                       Span<int> indices,
                       MutableAttributeAccessor dst_attributes);

/**
 * Copy attribute values from groups defined by \a src_offsets to groups defined by \a
 * dst_offsets. The group indices are gathered to the result by \a selection. The size of each
 * source and result group must be the same.
 */
void gather_attributes_group_to_group(AttributeAccessor src_attributes,
                                      AttrDomain src_domain,
                                      AttrDomain dst_domain,
                                      const AttributeFilter &attribute_filter,
                                      OffsetIndices<int> src_offsets,
                                      OffsetIndices<int> dst_offsets,
                                      const IndexMask &selection,
                                      MutableAttributeAccessor dst_attributes);

void gather_attributes_to_groups(AttributeAccessor src_attributes,
                                 AttrDomain src_domain,
                                 AttrDomain dst_domain,
                                 const AttributeFilter &attribute_filter,
                                 OffsetIndices<int> dst_offsets,
                                 const IndexMask &src_selection,
                                 MutableAttributeAccessor dst_attributes);

void copy_attributes(const AttributeAccessor src_attributes,
                     AttrDomain src_domain,
                     AttrDomain dst_domain,
                     const AttributeFilter &attribute_filter,
                     MutableAttributeAccessor dst_attributes);

void copy_attributes_group_to_group(AttributeAccessor src_attributes,
                                    AttrDomain src_domain,
                                    AttrDomain dst_domain,
                                    const AttributeFilter &attribute_filter,
                                    OffsetIndices<int> src_offsets,
                                    OffsetIndices<int> dst_offsets,
                                    const IndexMask &selection,
                                    MutableAttributeAccessor dst_attributes);

void fill_attribute_range_default(MutableAttributeAccessor dst_attributes,
                                  AttrDomain domain,
                                  const AttributeFilter &attribute_filter,
                                  IndexRange range);

/**
 * Apply a transform to the "custom_normal" attribute.
 */
void transform_custom_normal_attribute(const float4x4 &transform,
                                       MutableAttributeAccessor &attributes);

}  // namespace blender::bke
