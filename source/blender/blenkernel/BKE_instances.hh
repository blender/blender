/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * #Instances is a container for geometry instances. It fulfills some key requirements:
 * - Support nested instances.
 * - Support instance attributes.
 * - Support referencing different kinds of instances (objects, collections, geometry sets).
 * - Support efficiently iterating over the instanced geometries, i.e. without have to iterate over
 *   all instances.
 *
 * #Instances has an ordered set of #InstanceReference. An #InstanceReference contains information
 * about a particular instanced geometry. Each #InstanceReference has a handle (integer index)
 * which is then stored per instance. Many instances can use the same #InstanceReference.
 */

#include <optional>

#include "BLI_array.hh"
#include "BLI_function_ref.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_shared_cache.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array_fwd.hh"

#include "DNA_customdata_types.h"

struct Object;
struct Collection;
namespace blender::bke {
class AnonymousAttributePropagationInfo;
class AttributeAccessor;
class MutableAttributeAccessor;
}  // namespace blender::bke

namespace blender::bke {

struct GeometrySet;

/**
 * Holds a reference to conceptually unique geometry or a pointer to object/collection data
 * that is instanced with a transform in #Instances.
 */
class InstanceReference {
 public:
  enum class Type {
    /**
     * An empty instance. This allows an `InstanceReference` to be default constructed without
     * being in an invalid state. There might also be other use cases that we haven't explored
     * much yet (such as changing the instance later on, and "disabling" some instances).
     */
    None,
    Object,
    Collection,
    GeometrySet,
  };

 private:
  Type type_ = Type::None;
  /** Depending on the type this is either null, an Object or Collection pointer. */
  void *data_ = nullptr;
  std::unique_ptr<GeometrySet> geometry_set_;

 public:
  InstanceReference() = default;
  InstanceReference(Object &object);
  InstanceReference(Collection &collection);
  InstanceReference(GeometrySet geometry_set);
  InstanceReference(std::unique_ptr<GeometrySet> geometry_set);

  InstanceReference(const InstanceReference &other);
  InstanceReference(InstanceReference &&other);

  InstanceReference &operator=(const InstanceReference &other);
  InstanceReference &operator=(InstanceReference &&other);

  Type type() const;
  Object &object() const;
  Collection &collection() const;
  GeometrySet &geometry_set();
  const GeometrySet &geometry_set() const;

  bool owns_direct_data() const;
  void ensure_owns_direct_data();

  friend bool operator==(const InstanceReference &a, const InstanceReference &b);
};

class Instances {
 private:
  /**
   * Contains the data that is used by the individual instances.
   * Actual instances store an index ("handle") into this vector.
   */
  Vector<InstanceReference> references_;

  int instances_num_ = 0;

  CustomData attributes_;

  /* These almost unique ids are generated based on the `id` attribute, which might not contain
   * unique ids at all. They are *almost* unique, because under certain very unlikely
   * circumstances, they are not unique. Code using these ids should not crash when they are not
   * unique but can generally expect them to be unique. */
  mutable SharedCache<Array<int>> almost_unique_ids_cache_;

 public:
  Instances();
  Instances(Instances &&other);
  Instances(const Instances &other);
  ~Instances();

  Instances &operator=(const Instances &other);
  Instances &operator=(Instances &&other);

  /**
   * Resize the transform, handles, and attributes to the specified capacity.
   *
   * \note This function should be used carefully, only when it's guaranteed
   * that the data will be filled.
   */
  void resize(int capacity);

  /**
   * Returns a handle for the given reference.
   * If the reference exists already, the handle of the existing reference is returned.
   * Otherwise a new handle is added.
   */
  int add_reference(const InstanceReference &reference);
  std::optional<int> find_reference_handle(const InstanceReference &query);
  /**
   * Add a reference to the instance reference with an index specified by the #instance_handle
   * argument. For adding many instances, using #resize and accessing the transform array
   * directly is preferred.
   */
  void add_instance(int instance_handle, const float4x4 &transform);

  Span<InstanceReference> references() const;
  void remove_unused_references();

  /**
   * If references have a collection or object type, convert them into geometry instances
   * recursively. After that, the geometry sets can be edited. There may still be instances of
   * other types of they can't be converted to geometry sets.
   */
  void ensure_geometry_instances();
  /**
   * With write access to the instances component, the data in the instanced geometry sets can be
   * changed. This is a function on the component rather than each reference to ensure `const`
   * correctness for that reason.
   */
  GeometrySet &geometry_set_from_reference(int reference_index);

  Span<int> reference_handles() const;
  MutableSpan<int> reference_handles_for_write();
  Span<float4x4> transforms() const;
  MutableSpan<float4x4> transforms_for_write();

  int instances_num() const;
  int references_num() const;

  /**
   * Remove the indices that are not contained in the mask input, and remove unused instance
   * references afterwards.
   */
  void remove(const IndexMask &mask, const AnonymousAttributePropagationInfo &propagation_info);
  /**
   * Get an id for every instance. These can be used for e.g. motion blur.
   */
  Span<int> almost_unique_ids() const;

  bke::AttributeAccessor attributes() const;
  bke::MutableAttributeAccessor attributes_for_write();

  CustomData &custom_data_attributes();
  const CustomData &custom_data_attributes() const;

  void foreach_referenced_geometry(
      FunctionRef<void(const GeometrySet &geometry_set)> callback) const;

  bool owns_direct_data() const;
  void ensure_owns_direct_data();

  void tag_reference_handles_changed()
  {
    almost_unique_ids_cache_.tag_dirty();
  }
};

VArray<float3> instance_position_varray(const Instances &instances);
VMutableArray<float3> instance_position_varray_for_write(Instances &instances);

/* -------------------------------------------------------------------- */
/** \name #InstanceReference Inline Methods
 * \{ */

inline InstanceReference::InstanceReference(std::unique_ptr<GeometrySet> geometry_set)
    : type_(Type::GeometrySet), data_(nullptr), geometry_set_(std::move(geometry_set))
{
}

inline InstanceReference::InstanceReference(Object &object) : type_(Type::Object), data_(&object)
{
}

inline InstanceReference::InstanceReference(Collection &collection)
    : type_(Type::Collection), data_(&collection)
{
}

inline InstanceReference::InstanceReference(const InstanceReference &other)
    : type_(other.type_), data_(other.data_)
{
  if (other.geometry_set_) {
    geometry_set_ = std::make_unique<GeometrySet>(*other.geometry_set_);
  }
}

inline InstanceReference::InstanceReference(InstanceReference &&other)
    : type_(other.type_), data_(other.data_), geometry_set_(std::move(other.geometry_set_))
{
  other.type_ = Type::None;
  other.data_ = nullptr;
}

inline InstanceReference &InstanceReference::operator=(const InstanceReference &other)
{
  if (this == &other) {
    return *this;
  }
  this->~InstanceReference();
  new (this) InstanceReference(other);
  return *this;
}

inline InstanceReference &InstanceReference::operator=(InstanceReference &&other)
{
  if (this == &other) {
    return *this;
  }
  this->~InstanceReference();
  new (this) InstanceReference(std::move(other));
  return *this;
}

inline InstanceReference::Type InstanceReference::type() const
{
  return type_;
}

inline Object &InstanceReference::object() const
{
  BLI_assert(type_ == Type::Object);
  return *(Object *)data_;
}

inline Collection &InstanceReference::collection() const
{
  BLI_assert(type_ == Type::Collection);
  return *(Collection *)data_;
}

inline GeometrySet &InstanceReference::geometry_set()
{
  BLI_assert(type_ == Type::GeometrySet);
  return *geometry_set_;
}

inline const GeometrySet &InstanceReference::geometry_set() const
{
  BLI_assert(type_ == Type::GeometrySet);
  return *geometry_set_;
}

inline CustomData &Instances::custom_data_attributes()
{
  return attributes_;
}

inline const CustomData &Instances::custom_data_attributes() const
{
  return attributes_;
}

/** \} */

}  // namespace blender::bke
