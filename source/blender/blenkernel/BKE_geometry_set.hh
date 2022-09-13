/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <atomic>
#include <iostream>
#include <mutex>

#include "BLI_float4x4.hh"
#include "BLI_function_ref.hh"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_set.hh"
#include "BLI_user_counter.hh"
#include "BLI_vector_set.hh"

#include "BKE_anonymous_attribute.hh"
#include "BKE_attribute.hh"
#include "BKE_geometry_set.h"

struct Curves;
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
class CurvesEditHints;
}  // namespace blender::bke

class GeometryComponent;

/**
 * This is the base class for specialized geometry component types. A geometry component handles
 * a user count to allow avoiding duplication when it is wrapped with #UserCounter. It also handles
 * the attribute API, which generalizes storing and modifying generic information on a geometry.
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

  int attribute_domain_size(eAttrDomain domain) const;

  /**
   * Get access to the attributes in this geometry component. May return none if the geometry does
   * not support the attribute system.
   */
  virtual std::optional<blender::bke::AttributeAccessor> attributes() const;
  virtual std::optional<blender::bke::MutableAttributeAccessor> attributes_for_write();

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

  virtual bool is_empty() const;
};

template<typename T>
inline constexpr bool is_geometry_component_v = std::is_base_of_v<GeometryComponent, T>;

/**
 * A geometry set is a container for multiple kinds of geometry. It does not own geometry directly
 * itself, instead geometry is owned by multiple #GeometryComponents, and the geometry set
 * increases the user count of each component, so they avoid losing the data. This means
 * individual components might be shared between multiple geometries and other code. Shared
 * components are copied automatically when write access is requested.
 *
 * The components usually do not store data directly, but keep a reference to a data
 * structure defined elsewhere. There is at most one component of each type:
 *  - #MeshComponent
 *  - #CurveComponent
 *  - #PointCloudComponent
 *  - #InstancesComponent
 *  - #VolumeComponent
 *
 * Copying a geometry set is a relatively cheap operation, because it does not copy the referenced
 * geometry components, so #GeometrySet can often be passed or moved by value.
 */
struct GeometrySet {
 private:
  using GeometryComponentPtr = blender::UserCounter<class GeometryComponent>;
  /* Indexed by #GeometryComponentType. */
  std::array<GeometryComponentPtr, GEO_COMPONENT_TYPE_ENUM_SIZE> components_;

 public:
  /**
   * The methods are defaulted here so that they are not instantiated in every translation unit.
   */
  GeometrySet();
  GeometrySet(const GeometrySet &other);
  GeometrySet(GeometrySet &&other);
  ~GeometrySet();
  GeometrySet &operator=(const GeometrySet &other);
  GeometrySet &operator=(GeometrySet &&other);

  /**
   * This method can only be used when the geometry set is mutable. It returns a mutable geometry
   * component of the given type.
   */
  GeometryComponent &get_component_for_write(GeometryComponentType component_type);
  template<typename Component> Component &get_component_for_write()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<Component &>(this->get_component_for_write(Component::static_type));
  }

  /**
   * Get the component of the given type. Might return null if the component does not exist yet.
   */
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

  /**
   * Remove all geometry components with types that are not in the provided list.
   */
  void keep_only(const blender::Span<GeometryComponentType> component_types);
  /**
   * Keeps the provided geometry types, but also instances and edit data.
   * Instances must not be removed while using #modify_geometry_sets.
   */
  void keep_only_during_modify(const blender::Span<GeometryComponentType> component_types);
  void remove_geometry_during_modify();

  void add(const GeometryComponent &component);

  /**
   * Get all geometry components in this geometry set for read-only access.
   */
  blender::Vector<const GeometryComponent *> get_components_for_read() const;

  bool compute_boundbox_without_instances(blender::float3 *r_min, blender::float3 *r_max) const;

  friend std::ostream &operator<<(std::ostream &stream, const GeometrySet &geometry_set);

  /**
   * Remove all geometry components from the geometry set.
   */
  void clear();

  bool owns_direct_data() const;
  /**
   * Make sure that the geometry can be cached. This does not ensure ownership of object/collection
   * instances. This is necessary because sometimes components only have read-only or editing
   * access to their data, which might be freed later if this geometry set outlasts the data.
   */
  void ensure_owns_direct_data();

  using AttributeForeachCallback =
      blender::FunctionRef<void(const blender::bke::AttributeIDRef &attribute_id,
                                const blender::bke::AttributeMetaData &meta_data,
                                const GeometryComponent &component)>;

  void attribute_foreach(blender::Span<GeometryComponentType> component_types,
                         bool include_instances,
                         AttributeForeachCallback callback) const;

  void gather_attributes_for_propagation(
      blender::Span<GeometryComponentType> component_types,
      GeometryComponentType dst_component_type,
      bool include_instances,
      blender::Map<blender::bke::AttributeIDRef, blender::bke::AttributeKind> &r_attributes) const;

  blender::Vector<GeometryComponentType> gather_component_types(bool include_instances,
                                                                bool ignore_empty) const;

  using ForeachSubGeometryCallback = blender::FunctionRef<void(GeometrySet &geometry_set)>;

  /**
   * Modify every (recursive) instance separately. This is often more efficient than realizing all
   * instances just to change the same thing on all of them.
   */
  void modify_geometry_sets(ForeachSubGeometryCallback callback);

  /* Utility methods for creation. */
  /**
   * Create a new geometry set that only contains the given mesh.
   */
  static GeometrySet create_with_mesh(
      Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Create a new geometry set that only contains the given volume.
   */
  static GeometrySet create_with_volume(
      Volume *volume, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Create a new geometry set that only contains the given point cloud.
   */
  static GeometrySet create_with_pointcloud(
      PointCloud *pointcloud, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Create a new geometry set that only contains the given curves.
   */
  static GeometrySet create_with_curves(
      Curves *curves, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

  /* Utility methods for access. */
  /**
   * Returns true when the geometry set has a mesh component that has a mesh.
   */
  bool has_mesh() const;
  /**
   * Returns true when the geometry set has a point cloud component that has a point cloud.
   */
  bool has_pointcloud() const;
  /**
   * Returns true when the geometry set has an instances component that has at least one instance.
   */
  bool has_instances() const;
  /**
   * Returns true when the geometry set has a volume component that has a volume.
   */
  bool has_volume() const;
  /**
   * Returns true when the geometry set has a curves component that has a curves data-block.
   */
  bool has_curves() const;
  /**
   * Returns true when the geometry set has any data that is not an instance.
   */
  bool has_realized_data() const;
  /**
   * Return true if the geometry set has any component that isn't empty.
   */
  bool is_empty() const;

  /**
   * Returns a read-only mesh or null.
   */
  const Mesh *get_mesh_for_read() const;
  /**
   * Returns a read-only point cloud of null.
   */
  const PointCloud *get_pointcloud_for_read() const;
  /**
   * Returns a read-only volume or null.
   */
  const Volume *get_volume_for_read() const;
  /**
   * Returns a read-only curves data-block or null.
   */
  const Curves *get_curves_for_read() const;
  /**
   * Returns read-only curve edit hints or null.
   */
  const blender::bke::CurvesEditHints *get_curve_edit_hints_for_read() const;

  /**
   * Returns a mutable mesh or null. No ownership is transferred.
   */
  Mesh *get_mesh_for_write();
  /**
   * Returns a mutable point cloud or null. No ownership is transferred.
   */
  PointCloud *get_pointcloud_for_write();
  /**
   * Returns a mutable volume or null. No ownership is transferred.
   */
  Volume *get_volume_for_write();
  /**
   * Returns a mutable curves data-block or null. No ownership is transferred.
   */
  Curves *get_curves_for_write();
  /**
   * Returns mutable curve edit hints or null.
   */
  blender::bke::CurvesEditHints *get_curve_edit_hints_for_write();

  /* Utility methods for replacement. */
  /**
   * Clear the existing mesh and replace it with the given one.
   */
  void replace_mesh(Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Clear the existing point cloud and replace with the given one.
   */
  void replace_pointcloud(PointCloud *pointcloud,
                          GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Clear the existing volume and replace with the given one.
   */
  void replace_volume(Volume *volume,
                      GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Clear the existing curves data-block and replace it with the given one.
   */
  void replace_curves(Curves *curves,
                      GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

 private:
  /**
   * Retrieve the pointer to a component without creating it if it does not exist,
   * unlike #get_component_for_write.
   */
  GeometryComponent *get_component_ptr(GeometryComponentType type);
  template<typename Component> Component *get_component_ptr()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<Component *>(get_component_ptr(Component::static_type));
  }
};

/**
 * A geometry component that can store a mesh, using the #Mesh data-block.
 *
 * Attributes are stored, on any of the four attribute domains. Generic attributes are stored in
 * contiguous arrays, but often built-in attributes are stored in an array of structs fashion for
 * historical reasons, requiring more complex attribute access.
 */
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
  /**
   * Clear the component and replace it with the new mesh.
   */
  void replace(Mesh *mesh, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Return the mesh and clear the component. The caller takes over responsibility for freeing the
   * mesh (if the component was responsible before).
   */
  Mesh *release();

  /**
   * Get the mesh from this component. This method can be used by multiple threads at the same
   * time. Therefore, the returned mesh should not be modified. No ownership is transferred.
   */
  const Mesh *get_for_read() const;
  /**
   * Get the mesh from this component. This method can only be used when the component is mutable,
   * i.e. it is not shared. The returned mesh can be modified. No ownership is transferred.
   */
  Mesh *get_for_write();

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_MESH;

  std::optional<blender::bke::AttributeAccessor> attributes() const final;
  std::optional<blender::bke::MutableAttributeAccessor> attributes_for_write() final;
};

/**
 * A geometry component that stores a point cloud, corresponding to the #PointCloud data structure.
 * While a point cloud is technically a subset of a mesh in some respects, it is useful because of
 * its simplicity, partly on a conceptual level for the user, but also in the code, though partly
 * for historical reasons. Point clouds can also be rendered in special ways, based on the built-in
 * `radius` attribute.
 *
 * Attributes on point clouds are all stored in contiguous arrays in its #CustomData,
 * which makes them efficient to process, relative to some legacy built-in mesh attributes.
 */
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
  /**
   * Clear the component and replace it with the new point cloud.
   */
  void replace(PointCloud *pointcloud,
               GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Return the point cloud and clear the component. The caller takes over responsibility for
   * freeing the point cloud (if the component was responsible before).
   */
  PointCloud *release();

  /**
   * Get the point cloud from this component. This method can be used by multiple threads at the
   * same time. Therefore, the returned point cloud should not be modified. No ownership is
   * transferred.
   */
  const PointCloud *get_for_read() const;
  /**
   * Get the point cloud from this component. This method can only be used when the component is
   * mutable, i.e. it is not shared. The returned point cloud can be modified. No ownership is
   * transferred.
   */
  PointCloud *get_for_write();

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  std::optional<blender::bke::AttributeAccessor> attributes() const final;
  std::optional<blender::bke::MutableAttributeAccessor> attributes_for_write() final;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_POINT_CLOUD;

 private:
};

/**
 * Legacy runtime-only curves type.
 * These curves are stored differently than other geometry components, because the data structure
 * used here does not correspond exactly to the #Curve DNA data structure. A #CurveEval is stored
 * here instead, though the component does give access to a #Curve for interfacing with render
 * engines and other areas of Blender that expect to use a data-block with an #ID.
 */
class CurveComponentLegacy : public GeometryComponent {
 private:
  CurveEval *curve_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  CurveComponentLegacy();
  ~CurveComponentLegacy();
  GeometryComponent *copy() const override;

  void clear();
  bool has_curve() const;
  /**
   * Clear the component and replace it with the new curve.
   */
  void replace(CurveEval *curve, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  CurveEval *release();

  const CurveEval *get_for_read() const;
  CurveEval *get_for_write();

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  std::optional<blender::bke::AttributeAccessor> attributes() const final;
  std::optional<blender::bke::MutableAttributeAccessor> attributes_for_write() final;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_CURVE;
};

/**
 * A geometry component that stores a group of curves, corresponding the #Curves data-block type
 * and the #CurvesGeometry type. Attributes are stored on the control point domain and the
 * curve domain.
 */
class CurveComponent : public GeometryComponent {
 private:
  Curves *curves_ = nullptr;
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
  bool has_curves() const;
  /**
   * Clear the component and replace it with the new curve.
   */
  void replace(Curves *curve, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  Curves *release();

  const Curves *get_for_read() const;
  Curves *get_for_write();

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  /**
   * Create empty curve data used for rendering the spline's wire edges.
   * \note See comment on #curve_for_render_ for further explanation.
   */
  const Curve *get_curve_for_render() const;

  std::optional<blender::bke::AttributeAccessor> attributes() const final;
  std::optional<blender::bke::MutableAttributeAccessor> attributes_for_write() final;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_CURVE;
};

/**
 * Holds a reference to conceptually unique geometry or a pointer to object/collection data
 * that is instanced with a transform in #InstancesComponent.
 */
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

/**
 * A geometry component that stores instances. The instance data can be any type described by
 * #InstanceReference. Geometry instances can even contain instances themselves, for nested
 * instancing. Each instance has an index into an array of unique instance data, and a transform.
 * The component can also store generic attributes for each instance.
 *
 * The component works differently from other geometry components in that it stores
 * data about instancing directly, rather than owning a pointer to a separate data structure.
 *
 * This component is not responsible for handling the interface to a render engine, or other
 * areas that work with all visible geometry, that is handled by the dependency graph iterator
 * (see `DEG_depsgraph_query.h`).
 */
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

  /* These almost unique ids are generated based on the `id` attribute, which might not contain
   * unique ids at all. They are *almost* unique, because under certain very unlikely
   * circumstances, they are not unique. Code using these ids should not crash when they are not
   * unique but can generally expect them to be unique. */
  mutable std::mutex almost_unique_ids_mutex_;
  mutable blender::Array<int> almost_unique_ids_;

  blender::bke::CustomDataAttributes attributes_;

 public:
  InstancesComponent();
  ~InstancesComponent() = default;
  GeometryComponent *copy() const override;

  void clear();

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
   * argument. For adding many instances, using #resize and accessing the transform array directly
   * is preferred.
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

  blender::Span<int> instance_reference_handles() const;
  blender::MutableSpan<int> instance_reference_handles();
  blender::MutableSpan<blender::float4x4> instance_transforms();
  blender::Span<blender::float4x4> instance_transforms() const;

  int instances_num() const;
  int references_num() const;

  /**
   * Remove the indices that are not contained in the mask input, and remove unused instance
   * references afterwards.
   */
  void remove_instances(const blender::IndexMask mask);

  blender::Span<int> almost_unique_ids() const;

  blender::bke::CustomDataAttributes &instance_attributes();
  const blender::bke::CustomDataAttributes &instance_attributes() const;

  std::optional<blender::bke::AttributeAccessor> attributes() const final;
  std::optional<blender::bke::MutableAttributeAccessor> attributes_for_write() final;

  void foreach_referenced_geometry(
      blender::FunctionRef<void(const GeometrySet &geometry_set)> callback) const;

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_INSTANCES;

 private:
};

/**
 * A geometry component that stores volume grids, corresponding to the #Volume data structure.
 * This component does not implement an attribute API, partly because storage of sparse volume
 * information in grids is much more complicated than it is for other types
 */
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
  /**
   * Clear the component and replace it with the new volume.
   */
  void replace(Volume *volume, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Return the volume and clear the component. The caller takes over responsibility for freeing
   * the volume (if the component was responsible before).
   */
  Volume *release();

  /**
   * Get the volume from this component. This method can be used by multiple threads at the same
   * time. Therefore, the returned volume should not be modified. No ownership is transferred.
   */
  const Volume *get_for_read() const;
  /**
   * Get the volume from this component. This method can only be used when the component is
   * mutable, i.e. it is not shared. The returned volume can be modified. No ownership is
   * transferred.
   */
  Volume *get_for_write();

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_VOLUME;
};

/**
 * When the original data is in some edit mode, we want to propagate some additional information
 * through object evaluation. This information can be used by edit modes to support working on
 * evaluated data.
 *
 * This component is added at the beginning of modifier evaluation.
 */
class GeometryComponentEditData final : public GeometryComponent {
 public:
  /**
   * Information about how original curves are manipulated during evaluation. This data is used so
   * that curve sculpt tools can work on evaluated data. It is not stored in #CurveComponent
   * because the data remains valid even when there is no actual curves geometry anymore, for
   * example, when the curves have been converted to a mesh.
   */
  std::unique_ptr<blender::bke::CurvesEditHints> curves_edit_hints_;

  GeometryComponentEditData();

  GeometryComponent *copy() const final;
  bool owns_direct_data() const final;
  void ensure_owns_direct_data() final;

  /**
   * The first node that does topology changing operations on curves should store the curve point
   * positions it retrieved as input. Without this, information about the deformed positions is
   * lost, which would make curves sculpt mode fall back to using original curve positions instead
   * of deformed ones.
   */
  static void remember_deformed_curve_positions_if_necessary(GeometrySet &geometry);

  static constexpr inline GeometryComponentType static_type = GEO_COMPONENT_TYPE_EDIT;
};
