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

/** \file
 * \ingroup bke
 */

#include <atomic>
#include <iostream>

#include "BLI_float3.hh"
#include "BLI_float4x4.hh"
#include "BLI_function_ref.hh"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_user_counter.hh"
#include "BLI_vector_set.hh"

#include "BKE_anonymous_attribute.hh"
#include "BKE_attribute_access.hh"
#include "BKE_geometry_set.h"

#include "FN_field.hh"

struct Collection;
struct Curve;
struct CurveEval;
struct Mesh;
struct Object;
struct PointCloud;
struct Volume;

enum class GeometryOwnershipType {
  /* The geometry is owned. This implies that it can be changed. */
  Owned = 0,
  /* The geometry can be changed, but someone else is responsible for freeing it. */
  Editable = 1,
  /* The geometry cannot be changed and someone else is responsible for freeing it. */
  ReadOnly = 2,
};

namespace blender::bke {
class ComponentAttributeProviders;
}

class GeometryComponent;

/**
 * This is the base class for specialized geometry component types.
 */
class GeometryComponent {
 private:
  /* The reference count has two purposes. When it becomes zero, the component is freed. When it is
   * larger than one, the component becomes immutable. */
  mutable std::atomic<int> users_ = 1;
  GeometryComponentType type_;

 public:
  GeometryComponent(GeometryComponentType type);
  virtual ~GeometryComponent() = default;
  static GeometryComponent *create(GeometryComponentType component_type);

  /* The returned component should be of the same type as the type this is called on. */
  virtual GeometryComponent *copy() const = 0;

  /* Direct data is everything except for instances of objects/collections.
   * If this returns true, the geometry set can be cached and is still valid after e.g. modifier
   * evaluation ends. Instances can only be valid as long as the data they instance is valid. */
  virtual bool owns_direct_data() const = 0;
  virtual void ensure_owns_direct_data() = 0;

  void user_add() const;
  void user_remove() const;
  bool is_mutable() const;

  GeometryComponentType type() const;

  /* Return true when any attribute with this name exists, including built in attributes. */
  bool attribute_exists(const blender::bke::AttributeIDRef &attribute_id) const;

  /* Return the data type and domain of an attribute with the given name if it exists. */
  std::optional<AttributeMetaData> attribute_get_meta_data(
      const blender::bke::AttributeIDRef &attribute_id) const;

  /* Returns true when the geometry component supports this attribute domain. */
  bool attribute_domain_supported(const AttributeDomain domain) const;
  /* Can only be used with supported domain types. */
  virtual int attribute_domain_size(const AttributeDomain domain) const;

  bool attribute_is_builtin(const blender::StringRef attribute_name) const;
  bool attribute_is_builtin(const blender::bke::AttributeIDRef &attribute_id) const;

  /* Get read-only access to the highest priority attribute with the given name.
   * Returns null if the attribute does not exist. */
  blender::bke::ReadAttributeLookup attribute_try_get_for_read(
      const blender::bke::AttributeIDRef &attribute_id) const;

  /* Get read and write access to the highest priority attribute with the given name.
   * Returns null if the attribute does not exist. */
  blender::bke::WriteAttributeLookup attribute_try_get_for_write(
      const blender::bke::AttributeIDRef &attribute_id);

  /* Get a read-only attribute for the domain based on the given attribute. This can be used to
   * interpolate from one domain to another.
   * Returns null if the interpolation is not implemented. */
  virtual std::unique_ptr<blender::fn::GVArray> attribute_try_adapt_domain(
      std::unique_ptr<blender::fn::GVArray> varray,
      const AttributeDomain from_domain,
      const AttributeDomain to_domain) const;

  /* Returns true when the attribute has been deleted. */
  bool attribute_try_delete(const blender::bke::AttributeIDRef &attribute_id);

  /* Returns true when the attribute has been created. */
  bool attribute_try_create(const blender::bke::AttributeIDRef &attribute_id,
                            const AttributeDomain domain,
                            const CustomDataType data_type,
                            const AttributeInit &initializer);

  /* Try to create the builtin attribute with the given name. No data type or domain has to be
   * provided, because those are fixed for builtin attributes. */
  bool attribute_try_create_builtin(const blender::StringRef attribute_name,
                                    const AttributeInit &initializer);

  blender::Set<blender::bke::AttributeIDRef> attribute_ids() const;
  bool attribute_foreach(const AttributeForeachCallback callback) const;

  virtual bool is_empty() const;

  /* Get a virtual array to read the data of an attribute on the given domain and data type.
   * Returns null when the attribute does not exist or cannot be converted to the requested domain
   * and data type. */
  std::unique_ptr<blender::fn::GVArray> attribute_try_get_for_read(
      const blender::bke::AttributeIDRef &attribute_id,
      const AttributeDomain domain,
      const CustomDataType data_type) const;

  /* Get a virtual array to read the data of an attribute on the given domain. The data type is
   * left unchanged. Returns null when the attribute does not exist or cannot be adapted to the
   * requested domain. */
  std::unique_ptr<blender::fn::GVArray> attribute_try_get_for_read(
      const blender::bke::AttributeIDRef &attribute_id, const AttributeDomain domain) const;

  /* Get a virtual array to read data of an attribute with the given data type. The domain is
   * left unchanged. Returns null when the attribute does not exist or cannot be converted to the
   * requested data type. */
  blender::bke::ReadAttributeLookup attribute_try_get_for_read(
      const blender::bke::AttributeIDRef &attribute_id, const CustomDataType data_type) const;

  /* Get a virtual array to read the data of an attribute. If that is not possible, the returned
   * virtual array will contain a default value. This never returns null. */
  std::unique_ptr<blender::fn::GVArray> attribute_get_for_read(
      const blender::bke::AttributeIDRef &attribute_id,
      const AttributeDomain domain,
      const CustomDataType data_type,
      const void *default_value = nullptr) const;

  /* Should be used instead of the method above when the requested data type is known at compile
   * time for better type safety. */
  template<typename T>
  blender::fn::GVArray_Typed<T> attribute_get_for_read(
      const blender::bke::AttributeIDRef &attribute_id,
      const AttributeDomain domain,
      const T &default_value) const
  {
    const blender::fn::CPPType &cpp_type = blender::fn::CPPType::get<T>();
    const CustomDataType type = blender::bke::cpp_type_to_custom_data_type(cpp_type);
    std::unique_ptr varray = this->attribute_get_for_read(
        attribute_id, domain, type, &default_value);
    return blender::fn::GVArray_Typed<T>(std::move(varray));
  }

  /**
   * Returns an "output attribute", which is essentially a mutable virtual array with some commonly
   * used convince features. The returned output attribute might be empty if requested attribute
   * cannot exist on the geometry.
   *
   * The included convenience features are:
   * - Implicit type conversion when writing to builtin attributes.
   * - If the attribute name exists already, but has a different type/domain, a temporary attribute
   *   is created that will overwrite the existing attribute in the end.
   */
  blender::bke::OutputAttribute attribute_try_get_for_output(
      const blender::bke::AttributeIDRef &attribute_id,
      const AttributeDomain domain,
      const CustomDataType data_type,
      const void *default_value = nullptr);

  /* Same as attribute_try_get_for_output, but should be used when the original values in the
   * attributes are not read, i.e. the attribute is used only for output. Since values are not read
   * from this attribute, no default value is necessary. */
  blender::bke::OutputAttribute attribute_try_get_for_output_only(
      const blender::bke::AttributeIDRef &attribute_id,
      const AttributeDomain domain,
      const CustomDataType data_type);

  /* Statically typed method corresponding to the equally named generic one. */
  template<typename T>
  blender::bke::OutputAttribute_Typed<T> attribute_try_get_for_output(
      const blender::bke::AttributeIDRef &attribute_id,
      const AttributeDomain domain,
      const T default_value)
  {
    const blender::fn::CPPType &cpp_type = blender::fn::CPPType::get<T>();
    const CustomDataType data_type = blender::bke::cpp_type_to_custom_data_type(cpp_type);
    return this->attribute_try_get_for_output(attribute_id, domain, data_type, &default_value);
  }

  /* Statically typed method corresponding to the equally named generic one. */
  template<typename T>
  blender::bke::OutputAttribute_Typed<T> attribute_try_get_for_output_only(
      const blender::bke::AttributeIDRef &attribute_id, const AttributeDomain domain)
  {
    const blender::fn::CPPType &cpp_type = blender::fn::CPPType::get<T>();
    const CustomDataType data_type = blender::bke::cpp_type_to_custom_data_type(cpp_type);
    return this->attribute_try_get_for_output_only(attribute_id, domain, data_type);
  }

 private:
  virtual const blender::bke::ComponentAttributeProviders *get_attribute_providers() const;
};

template<typename T>
inline constexpr bool is_geometry_component_v = std::is_base_of_v<GeometryComponent, T>;

/**
 * A geometry set contains zero or more geometry components. There is at most one component of each
 * type. Individual components might be shared between multiple geometries. Shared components are
 * copied automatically when write access is requested.
 *
 * Copying a geometry set is a relatively cheap operation, because it does not copy the referenced
 * geometry components.
 */
struct GeometrySet {
 private:
  using GeometryComponentPtr = blender::UserCounter<class GeometryComponent>;
  blender::Map<GeometryComponentType, GeometryComponentPtr> components_;

 public:
  GeometrySet();
  GeometrySet(const GeometrySet &other);
  GeometrySet(GeometrySet &&other);
  ~GeometrySet();
  GeometrySet &operator=(const GeometrySet &other);
  GeometrySet &operator=(GeometrySet &&other);

  GeometryComponent &get_component_for_write(GeometryComponentType component_type);
  template<typename Component> Component &get_component_for_write()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<Component &>(this->get_component_for_write(Component::static_type));
  }

  const GeometryComponent *get_component_for_read(GeometryComponentType component_type) const;
  template<typename Component> const Component *get_component_for_read() const
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<const Component *>(get_component_for_read(Component::static_type));
  }

  bool has(const GeometryComponentType component_type) const;
  template<typename Component> bool has() const
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return this->has(Component::static_type);
  }

  void remove(const GeometryComponentType component_type);
  template<typename Component> void remove()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return this->remove(Component::static_type);
  }

  void keep_only(const blender::Span<GeometryComponentType> component_types);

  void add(const GeometryComponent &component);

  blender::Vector<const GeometryComponent *> get_components_for_read() const;

  void compute_boundbox_without_instances(blender::float3 *r_min, blender::float3 *r_max) const;

  friend std::ostream &operator<<(std::ostream &stream, const GeometrySet &geometry_set);

  void clear();

  bool owns_direct_data() const;
  void ensure_owns_direct_data();

  using AttributeForeachCallback =
      blender::FunctionRef<void(const blender::bke::AttributeIDRef &attribute_id,
                                const AttributeMetaData &meta_data,
                                const GeometryComponent &component)>;

  void attribute_foreach(blender::Span<GeometryComponentType> component_types,
                         bool include_instances,
                         AttributeForeachCallback callback) const;

  void gather_attributes_for_propagation(
      blender::Span<GeometryComponentType> component_types,
      GeometryComponentType dst_component_type,
      bool include_instances,
      blender::Map<blender::bke::AttributeIDRef, AttributeKind> &r_attributes) const;

  using ForeachSubGeometryCallback = blender::FunctionRef<void(GeometrySet &geometry_set)>;

  void modify_geometry_sets(ForeachSubGeometryCallback callback);

  /* Utility methods for creation. */
  static GeometrySet create_with_mesh(
      Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  static GeometrySet create_with_pointcloud(
      PointCloud *pointcloud, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  static GeometrySet create_with_curve(
      CurveEval *curve, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

  /* Utility methods for access. */
  bool has_mesh() const;
  bool has_pointcloud() const;
  bool has_instances() const;
  bool has_volume() const;
  bool has_curve() const;
  bool has_realized_data() const;
  bool is_empty() const;

  const Mesh *get_mesh_for_read() const;
  const PointCloud *get_pointcloud_for_read() const;
  const Volume *get_volume_for_read() const;
  const CurveEval *get_curve_for_read() const;

  Mesh *get_mesh_for_write();
  PointCloud *get_pointcloud_for_write();
  Volume *get_volume_for_write();
  CurveEval *get_curve_for_write();

  /* Utility methods for replacement. */
  void replace_mesh(Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  void replace_pointcloud(PointCloud *pointcloud,
                          GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  void replace_volume(Volume *volume,
                      GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  void replace_curve(CurveEval *curve,
                     GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

 private:
  /* Utility to retrieve a mutable component without creating it. */
  GeometryComponent *get_component_ptr(GeometryComponentType type);
  template<typename Component> Component *get_component_ptr()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<Component *>(get_component_ptr(Component::static_type));
  }
};

/** A geometry component that can store a mesh. */
class MeshComponent : public GeometryComponent {
 private:
  Mesh *mesh_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  MeshComponent();
  ~MeshComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_mesh() const;
  void replace(Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  Mesh *release();

  const Mesh *get_for_read() const;
  Mesh *get_for_write();

  int attribute_domain_size(const AttributeDomain domain) const final;
  std::unique_ptr<blender::fn::GVArray> attribute_try_adapt_domain(
      std::unique_ptr<blender::fn::GVArray> varray,
      const AttributeDomain from_domain,
      const AttributeDomain to_domain) const final;

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_MESH;

 private:
  const blender::bke::ComponentAttributeProviders *get_attribute_providers() const final;
};

/** A geometry component that stores a point cloud. */
class PointCloudComponent : public GeometryComponent {
 private:
  PointCloud *pointcloud_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  PointCloudComponent();
  ~PointCloudComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_pointcloud() const;
  void replace(PointCloud *pointcloud,
               GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  PointCloud *release();

  const PointCloud *get_for_read() const;
  PointCloud *get_for_write();

  int attribute_domain_size(const AttributeDomain domain) const final;

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_POINT_CLOUD;

 private:
  const blender::bke::ComponentAttributeProviders *get_attribute_providers() const final;
};

/** A geometry component that stores curve data, in other words, a group of splines. */
class CurveComponent : public GeometryComponent {
 private:
  CurveEval *curve_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

  /**
   * Curve data necessary to hold the draw cache for rendering, consistent over multiple redraws.
   * This is necessary because Blender assumes that objects evaluate to an object data type, and
   * we use #CurveEval rather than #Curve here. It also allows us to mostly reuse the same
   * batch cache implementation.
   */
  mutable Curve *curve_for_render_ = nullptr;
  mutable std::mutex curve_for_render_mutex_;

 public:
  CurveComponent();
  ~CurveComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_curve() const;
  void replace(CurveEval *curve, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  CurveEval *release();

  const CurveEval *get_for_read() const;
  CurveEval *get_for_write();

  int attribute_domain_size(const AttributeDomain domain) const final;
  std::unique_ptr<blender::fn::GVArray> attribute_try_adapt_domain(
      std::unique_ptr<blender::fn::GVArray> varray,
      const AttributeDomain from_domain,
      const AttributeDomain to_domain) const final;

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  const Curve *get_curve_for_render() const;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_CURVE;

 private:
  const blender::bke::ComponentAttributeProviders *get_attribute_providers() const final;
};

class InstanceReference {
 public:
  enum class Type {
    /**
     * An empty instance. This allows an `InstanceReference` to be default constructed without
     * being in an invalid state. There might also be other use cases that we haven't explored much
     * yet (such as changing the instance later on, and "disabling" some instances).
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

  InstanceReference(Object &object) : type_(Type::Object), data_(&object)
  {
  }

  InstanceReference(Collection &collection) : type_(Type::Collection), data_(&collection)
  {
  }

  InstanceReference(GeometrySet geometry_set)
      : type_(Type::GeometrySet),
        geometry_set_(std::make_unique<GeometrySet>(std::move(geometry_set)))
  {
  }

  InstanceReference(const InstanceReference &other) : type_(other.type_), data_(other.data_)
  {
    if (other.geometry_set_) {
      geometry_set_ = std::make_unique<GeometrySet>(*other.geometry_set_);
    }
  }

  InstanceReference(InstanceReference &&other)
      : type_(other.type_), data_(other.data_), geometry_set_(std::move(other.geometry_set_))
  {
    other.type_ = Type::None;
    other.data_ = nullptr;
  }

  InstanceReference &operator=(const InstanceReference &other)
  {
    if (this == &other) {
      return *this;
    }
    this->~InstanceReference();
    new (this) InstanceReference(other);
    return *this;
  }

  InstanceReference &operator=(InstanceReference &&other)
  {
    if (this == &other) {
      return *this;
    }
    this->~InstanceReference();
    new (this) InstanceReference(std::move(other));
    return *this;
  }

  Type type() const
  {
    return type_;
  }

  Object &object() const
  {
    BLI_assert(type_ == Type::Object);
    return *(Object *)data_;
  }

  Collection &collection() const
  {
    BLI_assert(type_ == Type::Collection);
    return *(Collection *)data_;
  }

  const GeometrySet &geometry_set() const
  {
    BLI_assert(type_ == Type::GeometrySet);
    return *geometry_set_;
  }

  bool owns_direct_data() const
  {
    if (type_ != Type::GeometrySet) {
      /* The object and collection instances are not direct data. */
      return true;
    }
    return geometry_set_->owns_direct_data();
  }

  void ensure_owns_direct_data()
  {
    if (type_ != Type::GeometrySet) {
      return;
    }
    geometry_set_->ensure_owns_direct_data();
  }

  uint64_t hash() const
  {
    return blender::get_default_hash_2(data_, geometry_set_.get());
  }

  friend bool operator==(const InstanceReference &a, const InstanceReference &b)
  {
    return a.data_ == b.data_ && a.geometry_set_.get() == b.geometry_set_.get();
  }
};

/** A geometry component that stores instances. */
class InstancesComponent : public GeometryComponent {
 private:
  /**
   * Indexed set containing information about the data that is instanced.
   * Actual instances store an index ("handle") into this set.
   */
  blender::VectorSet<InstanceReference> references_;

  /** Index into `references_`. Determines what data is instanced. */
  blender::Vector<int> instance_reference_handles_;
  /** Transformation of the instances. */
  blender::Vector<blender::float4x4> instance_transforms_;
  /**
   * IDs of the instances. They are used for consistency over multiple frames for things like
   * motion blur.
   */
  blender::Vector<int> instance_ids_;

  /* These almost unique ids are generated based on `ids_`, which might not contain unique ids at
   * all. They are *almost* unique, because under certain very unlikely circumstances, they are not
   * unique. Code using these ids should not crash when they are not unique but can generally
   * expect them to be unique. */
  mutable std::mutex almost_unique_ids_mutex_;
  mutable blender::Array<int> almost_unique_ids_;

 public:
  InstancesComponent();
  ~InstancesComponent() = default;
  GeometryComponent *copy() const override;

  void clear();

  void reserve(int min_capacity);
  void resize(int capacity);

  int add_reference(const InstanceReference &reference);
  void add_instance(int instance_handle, const blender::float4x4 &transform, const int id = -1);

  blender::Span<InstanceReference> references() const;
  void remove_unused_references();

  void ensure_geometry_instances();
  GeometrySet &geometry_set_from_reference(const int reference_index);

  blender::Span<int> instance_reference_handles() const;
  blender::MutableSpan<int> instance_reference_handles();
  blender::MutableSpan<blender::float4x4> instance_transforms();
  blender::Span<blender::float4x4> instance_transforms() const;
  blender::MutableSpan<int> instance_ids();
  blender::Span<int> instance_ids() const;

  int instances_amount() const;
  int references_amount() const;

  blender::Span<int> almost_unique_ids() const;

  int attribute_domain_size(const AttributeDomain domain) const final;

  void foreach_referenced_geometry(
      blender::FunctionRef<void(const GeometrySet &geometry_set)> callback) const;

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_INSTANCES;

 private:
  const blender::bke::ComponentAttributeProviders *get_attribute_providers() const final;
};

/** A geometry component that stores volume grids. */
class VolumeComponent : public GeometryComponent {
 private:
  Volume *volume_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  VolumeComponent();
  ~VolumeComponent();
  GeometryComponent *copy() const override;

  void clear();
  bool has_volume() const;
  void replace(Volume *volume, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  Volume *release();

  const Volume *get_for_read() const;
  Volume *get_for_write();

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_VOLUME;
};

namespace blender::bke {

class GeometryComponentFieldContext : public fn::FieldContext {
 private:
  const GeometryComponent &component_;
  const AttributeDomain domain_;

 public:
  GeometryComponentFieldContext(const GeometryComponent &component, const AttributeDomain domain)
      : component_(component), domain_(domain)
  {
  }

  const GeometryComponent &geometry_component() const
  {
    return component_;
  }

  AttributeDomain domain() const
  {
    return domain_;
  }
};

class AttributeFieldInput : public fn::FieldInput {
 private:
  std::string name_;

 public:
  AttributeFieldInput(std::string name, const CPPType &type)
      : fn::FieldInput(type, name), name_(std::move(name))
  {
  }

  template<typename T> static fn::Field<T> Create(std::string name)
  {
    const CPPType &type = CPPType::get<T>();
    auto field_input = std::make_shared<AttributeFieldInput>(std::move(name), type);
    return fn::Field<T>{field_input};
  }

  StringRefNull attribute_name() const
  {
    return name_;
  }

  const GVArray *get_varray_for_context(const fn::FieldContext &context,
                                        IndexMask mask,
                                        ResourceScope &scope) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

class IDAttributeFieldInput : public fn::FieldInput {
 public:
  IDAttributeFieldInput() : fn::FieldInput(CPPType::get<int>())
  {
  }

  static fn::Field<int> Create();

  const GVArray *get_varray_for_context(const fn::FieldContext &context,
                                        IndexMask mask,
                                        ResourceScope &scope) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

class AnonymousAttributeFieldInput : public fn::FieldInput {
 private:
  /**
   * A strong reference is required to make sure that the referenced attribute is not removed
   * automatically.
   */
  StrongAnonymousAttributeID anonymous_id_;

 public:
  AnonymousAttributeFieldInput(StrongAnonymousAttributeID anonymous_id, const CPPType &type)
      : fn::FieldInput(type, anonymous_id.debug_name()), anonymous_id_(std::move(anonymous_id))
  {
  }

  template<typename T> static fn::Field<T> Create(StrongAnonymousAttributeID anonymous_id)
  {
    const CPPType &type = CPPType::get<T>();
    auto field_input = std::make_shared<AnonymousAttributeFieldInput>(std::move(anonymous_id),
                                                                      type);
    return fn::Field<T>{field_input};
  }

  const GVArray *get_varray_for_context(const fn::FieldContext &context,
                                        IndexMask mask,
                                        ResourceScope &scope) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

}  // namespace blender::bke
