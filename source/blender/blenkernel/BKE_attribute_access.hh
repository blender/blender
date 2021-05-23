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

#include "BKE_attribute.h"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_function_ref.hh"

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
using AttributeForeachCallback = blender::FunctionRef<bool(blender::StringRefNull attribute_name,
                                                           const AttributeMetaData &meta_data)>;

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
  AttributeDomain domain_;
  SaveFn save_;
  std::optional<fn::GVMutableArray_GSpan> optional_span_varray_;
  bool ignore_old_values_ = false;
  bool save_has_been_called_ = false;

 public:
  OutputAttribute() = default;

  OutputAttribute(GVMutableArrayPtr varray,
                  AttributeDomain domain,
                  SaveFn save,
                  const bool ignore_old_values)
      : varray_(std::move(varray)),
        domain_(domain),
        save_(std::move(save)),
        ignore_old_values_(ignore_old_values)
  {
  }

  OutputAttribute(OutputAttribute &&other) = default;

  ~OutputAttribute();

  operator bool() const
  {
    return varray_.get() != nullptr;
  }

  GVMutableArray &operator*()
  {
    return *varray_;
  }

  GVMutableArray *operator->()
  {
    return varray_.get();
  }

  GVMutableArray &varray()
  {
    return *varray_;
  }

  AttributeDomain domain() const
  {
    return domain_;
  }

  const CPPType &cpp_type() const
  {
    return varray_->type();
  }

  CustomDataType custom_data_type() const
  {
    return cpp_type_to_custom_data_type(this->cpp_type());
  }

  fn::GMutableSpan as_span()
  {
    if (!optional_span_varray_.has_value()) {
      const bool materialize_old_values = !ignore_old_values_;
      optional_span_varray_.emplace(*varray_, materialize_old_values);
    }
    fn::GVMutableArray_GSpan &span_varray = *optional_span_varray_;
    return span_varray;
  }

  template<typename T> MutableSpan<T> as_span()
  {
    return this->as_span().typed<T>();
  }

  void save();
};

/**
 * Same as OutputAttribute, but should be used when the data type is known at compile time.
 */
template<typename T> class OutputAttribute_Typed {
 private:
  OutputAttribute attribute_;
  std::optional<fn::GVMutableArray_Typed<T>> optional_varray_;
  VMutableArray<T> *varray_ = nullptr;

 public:
  OutputAttribute_Typed(OutputAttribute attribute) : attribute_(std::move(attribute))
  {
    if (attribute_) {
      optional_varray_.emplace(attribute_.varray());
      varray_ = &**optional_varray_;
    }
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

  void reallocate(const int size);

  std::optional<blender::fn::GSpan> get_for_read(const blender::StringRef name) const;
  std::optional<blender::fn::GMutableSpan> get_for_write(const blender::StringRef name);
  bool create(const blender::StringRef name, const CustomDataType data_type);
  bool create_by_move(const blender::StringRef name, const CustomDataType data_type, void *buffer);
  bool remove(const blender::StringRef name);

  bool foreach_attribute(const AttributeForeachCallback callback,
                         const AttributeDomain domain) const;
};

}  // namespace blender::bke
