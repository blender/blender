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
#include "BLI_memory_counter_fwd.hh"
#include "BLI_shared_cache.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array_fwd.hh"

#include "BKE_attribute_filter.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_geometry_set.hh"

struct Object;
struct Collection;
namespace blender::bke {
class AttributeAccessor;
class MutableAttributeAccessor;
}  // namespace blender::bke

namespace blender::bke {

struct GeometrySet;
struct AttributeAccessorFunctions;

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

  /**
   * Converts the instance reference to a geometry set, even if it was an object or collection
   * before.
   *
   * \note Uses out-parameter to be able to use #GeometrySet forward declaration.
   */
  void to_geometry_set(GeometrySet &r_geometry_set) const;

  StringRefNull name() const;

  bool owns_direct_data() const;
  void ensure_owns_direct_data();

  void count_memory(MemoryCounter &memory) const;

  friend bool operator==(const InstanceReference &a, const InstanceReference &b);

  uint64_t hash() const;
};

class Instances {
 private:
  /**
   * Contains the data that is used by the individual instances.
   * Actual instances store an index ("handle") into this vector.
   */
  Vector<InstanceReference> references_;

  int instances_num_ = 0;

  bke::AttributeStorage attributes_;

  /**
   * Caches how often each reference is used.
   */
  mutable SharedCache<Array<int>> reference_user_counts_;

  /* These unique ids are generated based on the `id` attribute, which might not contain
   * unique ids at all. */
  mutable SharedCache<Array<int>> unique_ids_cache_;

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
  /**
   * Same as above, but does not deduplicate with existing references.
   */
  int add_new_reference(const InstanceReference &reference);
  std::optional<int> find_reference_handle(const InstanceReference &query);
  /**
   * Add a reference to the instance reference with an index specified by the #instance_handle
   * argument. For adding many instances, using #resize and accessing the transform array
   * directly is preferred.
   */
  void add_instance(int instance_handle, const float4x4 &transform);

  Span<InstanceReference> references() const;
  MutableSpan<InstanceReference> references_for_write();

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
  void remove(const IndexMask &mask, const AttributeFilter &attribute_filter);
  /**
   * Get an id for every instance. These can be used e.g. motion blur. This is based on the "id"
   * attribute but makes sure that the ids are actually unique.
   */
  Span<int> unique_ids() const;

  /**
   * Get cached user counts for every reference.
   */
  Span<int> reference_user_counts() const;

  bke::AttributeAccessor attributes() const;
  bke::MutableAttributeAccessor attributes_for_write();

  bke::AttributeStorage &attribute_storage();
  const bke::AttributeStorage &attribute_storage() const;

  void foreach_referenced_geometry(
      FunctionRef<void(const GeometrySet &geometry_set)> callback) const;

  bool owns_direct_data() const;
  void ensure_owns_direct_data();

  void count_memory(MemoryCounter &memory) const;

  void tag_reference_handles_changed()
  {
    reference_user_counts_.tag_dirty();
    unique_ids_cache_.tag_dirty();
  }
};

VArray<float3> instance_position_varray(const Instances &instances);
VMutableArray<float3> instance_position_varray_for_write(Instances &instances);
const AttributeAccessorFunctions &instance_attribute_accessor_functions();

/* -------------------------------------------------------------------- */
/** \name #InstanceReference Inline Methods
 * \{ */

inline InstanceReference::InstanceReference(std::unique_ptr<GeometrySet> geometry_set)
    : type_(Type::GeometrySet), geometry_set_(std::move(geometry_set))
{
}

inline InstanceReference::InstanceReference(Object &object) : type_(Type::Object), data_(&object)
{
}

inline InstanceReference::InstanceReference(Collection &collection)
    : type_(Type::Collection), data_(&collection)
{
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

inline AttributeStorage &Instances::attribute_storage()
{
  return attributes_;
}

inline const AttributeStorage &Instances::attribute_storage() const
{
  return attributes_;
}

/** \} */

}  // namespace blender::bke
