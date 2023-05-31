/* SPDX-License-Identifier: GPL-2.0-or-later */

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

#include <mutex>

#include "BLI_math_matrix_types.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BKE_attribute.hh"

struct GeometrySet;
struct Object;
struct Collection;

namespace blender::bke {

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

  InstanceReference(const InstanceReference &other);
  InstanceReference(InstanceReference &&other);

  InstanceReference &operator=(const InstanceReference &other);
  InstanceReference &operator=(InstanceReference &&other);

  Type type() const;
  Object &object() const;
  Collection &collection() const;
  const GeometrySet &geometry_set() const;

  bool owns_direct_data() const;
  void ensure_owns_direct_data();

  uint64_t hash() const;
  friend bool operator==(const InstanceReference &a, const InstanceReference &b);
};

class Instances {
 private:
  /**
   * Indexed set containing information about the data that is instanced.
   * Actual instances store an index ("handle") into this set.
   */
  blender::VectorSet<InstanceReference> references_;

  /** Indices into `references_`. Determines what data is instanced. */
  blender::Vector<int> reference_handles_;
  /** Transformation of the instances. */
  blender::Vector<blender::float4x4> transforms_;

  /* These almost unique ids are generated based on the `id` attribute, which might not contain
   * unique ids at all. They are *almost* unique, because under certain very unlikely
   * circumstances, they are not unique. Code using these ids should not crash when they are not
   * unique but can generally expect them to be unique. */
  mutable std::mutex almost_unique_ids_mutex_;
  mutable blender::Array<int> almost_unique_ids_;

  CustomDataAttributes attributes_;

 public:
  Instances() = default;
  Instances(const Instances &other);

  void reserve(int min_capacity);
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
   * Add a reference to the instance reference with an index specified by the #instance_handle
   * argument. For adding many instances, using #resize and accessing the transform array
   * directly is preferred.
   */
  void add_instance(int instance_handle, const blender::float4x4 &transform);

  blender::Span<InstanceReference> references() const;
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

  blender::Span<int> reference_handles() const;
  blender::MutableSpan<int> reference_handles();
  blender::MutableSpan<blender::float4x4> transforms();
  blender::Span<blender::float4x4> transforms() const;

  int instances_num() const;
  int references_num() const;

  /**
   * Remove the indices that are not contained in the mask input, and remove unused instance
   * references afterwards.
   */
  void remove(const blender::IndexMask &mask,
              const blender::bke::AnonymousAttributePropagationInfo &propagation_info);
  /**
   * Get an id for every instance. These can be used for e.g. motion blur.
   */
  blender::Span<int> almost_unique_ids() const;

  blender::bke::AttributeAccessor attributes() const;
  blender::bke::MutableAttributeAccessor attributes_for_write();

  CustomDataAttributes &custom_data_attributes();
  const CustomDataAttributes &custom_data_attributes() const;

  void foreach_referenced_geometry(
      blender::FunctionRef<void(const GeometrySet &geometry_set)> callback) const;

  bool owns_direct_data() const;
  void ensure_owns_direct_data();
};

/* -------------------------------------------------------------------- */
/** \name #InstanceReference Inline Methods
 * \{ */

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

inline const GeometrySet &InstanceReference::geometry_set() const
{
  BLI_assert(type_ == Type::GeometrySet);
  return *geometry_set_;
}

inline CustomDataAttributes &Instances::custom_data_attributes()
{
  return attributes_;
}

inline const CustomDataAttributes &Instances::custom_data_attributes() const
{
  return attributes_;
}

inline uint64_t InstanceReference::hash() const
{
  return blender::get_default_hash_2(data_, geometry_set_.get());
}

inline bool operator==(const InstanceReference &a, const InstanceReference &b)
{
  return a.data_ == b.data_ && a.geometry_set_.get() == b.geometry_set_.get();
}

/** \} */

}  // namespace blender::bke
