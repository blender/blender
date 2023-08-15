/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <iostream>
#include <mutex>

#include "BLI_bounds_types.hh"
#include "BLI_function_ref.hh"
#include "BLI_map.hh"

#include "BKE_attribute.hh"

struct Curves;
struct Curve;
struct Mesh;
struct PointCloud;
struct Volume;
struct GreasePencil;

namespace blender::bke {

#define GEO_COMPONENT_TYPE_ENUM_SIZE 7

enum class GeometryOwnershipType {
  /* The geometry is owned. This implies that it can be changed. */
  Owned = 0,
  /* The geometry can be changed, but someone else is responsible for freeing it. */
  Editable = 1,
  /* The geometry cannot be changed and someone else is responsible for freeing it. */
  ReadOnly = 2,
};

class ComponentAttributeProviders;
class CurvesEditHints;
class Instances;

class GeometryComponent;
using GeometryComponentPtr = ImplicitSharingPtr<GeometryComponent>;

/**
 * This is the base class for specialized geometry component types. A geometry component uses
 * implicit sharing to avoid read-only copies. It also integrates with attribute API, which
 * generalizes storing and modifying generic information on a geometry.
 */
class GeometryComponent : public ImplicitSharingMixin {
 public:
  /**
   * Each geometry component has a specific type. The type determines what kind of data the
   * component stores. Functions modifying a geometry will usually just modify a subset of the
   * component types.
   * \note These values are stored in files, so they should not be reordered.
   */
  enum class Type {
    Mesh = 0,
    PointCloud = 1,
    Instance = 2,
    Volume = 3,
    Curve = 4,
    Edit = 5,
    GreasePencil = 6,
  };

 private:
  Type type_;

 public:
  GeometryComponent(Type type);
  virtual ~GeometryComponent() = default;
  static GeometryComponentPtr create(Type component_type);

  int attribute_domain_size(eAttrDomain domain) const;

  /**
   * Get access to the attributes in this geometry component. May return none if the geometry does
   * not support the attribute system.
   */
  virtual std::optional<AttributeAccessor> attributes() const;
  virtual std::optional<MutableAttributeAccessor> attributes_for_write();

  /* The returned component should be of the same type as the type this is called on. */
  virtual GeometryComponent *copy() const = 0;

  /** Remove referenced data from the geometry component. */
  virtual void clear() = 0;

  /* Direct data is everything except for instances of objects/collections.
   * If this returns true, the geometry set can be cached and is still valid after e.g. modifier
   * evaluation ends. Instances can only be valid as long as the data they instance is valid. */
  virtual bool owns_direct_data() const = 0;
  virtual void ensure_owns_direct_data() = 0;

  Type type() const;

  virtual bool is_empty() const;

 private:
  void delete_self() override;
  void delete_data_only() override;
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
 *  - #GreasePencilComponent
 *
 * Copying a geometry set is a relatively cheap operation, because it does not copy the referenced
 * geometry components, so #GeometrySet can often be passed or moved by value.
 */
struct GeometrySet {
 private:
  /* Indexed by #GeometryComponent::Type. */
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
  GeometryComponent &get_component_for_write(GeometryComponent::Type component_type);
  template<typename Component> Component &get_component_for_write()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<Component &>(this->get_component_for_write(Component::static_type));
  }

  /**
   * Get the component of the given type. Might return null if the component does not exist yet.
   */
  const GeometryComponent *get_component(GeometryComponent::Type component_type) const;
  template<typename Component> const Component *get_component() const
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<const Component *>(get_component(Component::static_type));
  }

  bool has(const GeometryComponent::Type component_type) const;
  template<typename Component> bool has() const
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return this->has(Component::static_type);
  }

  void remove(const GeometryComponent::Type component_type);
  template<typename Component> void remove()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return this->remove(Component::static_type);
  }

  /**
   * Remove all geometry components with types that are not in the provided list.
   */
  void keep_only(const Span<GeometryComponent::Type> component_types);
  /**
   * Keeps the provided geometry types, but also instances and edit data.
   * Instances must not be removed while using #modify_geometry_sets.
   */
  void keep_only_during_modify(const Span<GeometryComponent::Type> component_types);
  void remove_geometry_during_modify();

  void add(const GeometryComponent &component);

  /**
   * Get all geometry components in this geometry set for read-only access.
   */
  Vector<const GeometryComponent *> get_components() const;

  std::optional<Bounds<float3>> compute_boundbox_without_instances() const;

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
  /**
   * Same as #ensure_owns_direct_data but also turns object/collection instances into geometry
   * instances so that they can be owned.
   */
  void ensure_owns_all_data();

  using AttributeForeachCallback = FunctionRef<void(const AttributeIDRef &attribute_id,
                                                    const AttributeMetaData &meta_data,
                                                    const GeometryComponent &component)>;

  void attribute_foreach(Span<GeometryComponent::Type> component_types,
                         bool include_instances,
                         AttributeForeachCallback callback) const;

  void gather_attributes_for_propagation(Span<GeometryComponent::Type> component_types,
                                         GeometryComponent::Type dst_component_type,
                                         bool include_instances,
                                         const AnonymousAttributePropagationInfo &propagation_info,
                                         Map<AttributeIDRef, AttributeKind> &r_attributes) const;

  Vector<GeometryComponent::Type> gather_component_types(bool include_instances,
                                                         bool ignore_empty) const;

  using ForeachSubGeometryCallback = FunctionRef<void(GeometrySet &geometry_set)>;

  /**
   * Modify every (recursive) instance separately. This is often more efficient than realizing all
   * instances just to change the same thing on all of them.
   */
  void modify_geometry_sets(ForeachSubGeometryCallback callback);

  /* Utility methods for creation. */
  /**
   * Create a new geometry set that only contains the given mesh.
   */
  static GeometrySet from_mesh(Mesh *mesh,
                               GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Create a new geometry set that only contains the given volume.
   */
  static GeometrySet from_volume(Volume *volume,
                                 GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Create a new geometry set that only contains the given point cloud.
   */
  static GeometrySet from_pointcloud(
      PointCloud *pointcloud, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Create a new geometry set that only contains the given curves.
   */
  static GeometrySet from_curves(Curves *curves,
                                 GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Create a new geometry set that only contains the given instances.
   */
  static GeometrySet from_instances(
      Instances *instances, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Create a new geometry set that only contains the given Grease Pencil data.
   */
  static GeometrySet from_grease_pencil(
      GreasePencil *grease_pencil, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

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
   * Returns true when the geometry set has a Grease Pencil component that has grease pencil data.
   */
  bool has_grease_pencil() const;
  /**
   * Return true if the geometry set has any component that isn't empty.
   */
  bool is_empty() const;

  /**
   * Returns a read-only mesh or null.
   */
  const Mesh *get_mesh() const;
  /**
   * Returns a read-only point cloud of null.
   */
  const PointCloud *get_pointcloud() const;
  /**
   * Returns a read-only volume or null.
   */
  const Volume *get_volume() const;
  /**
   * Returns a read-only curves data-block or null.
   */
  const Curves *get_curves() const;
  /**
   * Returns read-only instances or null.
   */
  const Instances *get_instances() const;
  /**
   * Returns read-only curve edit hints or null.
   */
  const CurvesEditHints *get_curve_edit_hints() const;
  /**
   * Returns a read-only Grease Pencil data-block or null.
   */
  const GreasePencil *get_grease_pencil() const;

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
   * Returns mutable instances or null. No ownership is transferred.
   */
  Instances *get_instances_for_write();
  /**
   * Returns mutable curve edit hints or null.
   */
  CurvesEditHints *get_curve_edit_hints_for_write();
  /**
   * Returns a mutable Grease Pencil data-block or null. No ownership is transferred.
   */
  GreasePencil *get_grease_pencil_for_write();

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
  /**
   * Clear the existing instances and replace them with the given one.
   */
  void replace_instances(Instances *instances,
                         GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Clear the existing Grease Pencil data-block and replace it with the given one.
   */
  void replace_grease_pencil(GreasePencil *grease_pencil,
                             GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

 private:
  /**
   * Retrieve the pointer to a component without creating it if it does not exist,
   * unlike #get_component_for_write.
   */
  GeometryComponent *get_component_ptr(GeometryComponent::Type type);
  template<typename Component> Component *get_component_ptr()
  {
    BLI_STATIC_ASSERT(is_geometry_component_v<Component>, "");
    return static_cast<Component *>(get_component_ptr(Component::static_type));
  }
};

/**
 * A geometry component that can store a mesh, using the #Mesh data-block.
 * Attributes are stored on any of the four attribute domains.
 */
class MeshComponent : public GeometryComponent {
 private:
  Mesh *mesh_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  MeshComponent();
  ~MeshComponent();
  GeometryComponent *copy() const override;

  void clear() override;
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
  const Mesh *get() const;
  /**
   * Get the mesh from this component. This method can only be used when the component is mutable,
   * i.e. it is not shared. The returned mesh can be modified. No ownership is transferred.
   */
  Mesh *get_for_write();

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponent::Type static_type = Type::Mesh;

  std::optional<AttributeAccessor> attributes() const final;
  std::optional<MutableAttributeAccessor> attributes_for_write() final;
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

  void clear() override;
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
  const PointCloud *get() const;
  /**
   * Get the point cloud from this component. This method can only be used when the component is
   * mutable, i.e. it is not shared. The returned point cloud can be modified. No ownership is
   * transferred.
   */
  PointCloud *get_for_write();

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  std::optional<AttributeAccessor> attributes() const final;
  std::optional<MutableAttributeAccessor> attributes_for_write() final;

  static constexpr inline GeometryComponent::Type static_type = Type::PointCloud;
};

/**
 * A geometry component that stores a group of curves, corresponding the #Curves data-block
 * and the #CurvesGeometry type. Attributes are stored on the control point domain and the
 * curve domain.
 */
class CurveComponent : public GeometryComponent {
 private:
  Curves *curves_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

  /**
   * Because rendering #Curves isn't fully working yet, we must provide a #Curve for the render
   * engine and depsgraph object iterator in some cases. This allows using the old curve rendering
   * even when the new curve data structure is used.
   */
  mutable Curve *curve_for_render_ = nullptr;
  mutable std::mutex curve_for_render_mutex_;

 public:
  CurveComponent();
  ~CurveComponent();
  GeometryComponent *copy() const override;

  void clear() override;
  bool has_curves() const;
  /**
   * Clear the component and replace it with the new curve.
   */
  void replace(Curves *curve, GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  Curves *release();

  const Curves *get() const;
  Curves *get_for_write();

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  /**
   * Create empty curve data used for rendering the spline's wire edges.
   * \note See comment on #curve_for_render_ for further explanation.
   */
  const Curve *get_curve_for_render() const;

  std::optional<AttributeAccessor> attributes() const final;
  std::optional<MutableAttributeAccessor> attributes_for_write() final;

  static constexpr inline GeometryComponent::Type static_type = Type::Curve;
};

/**
 * A geometry component that stores #Instances.
 */
class InstancesComponent : public GeometryComponent {
 private:
  Instances *instances_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  InstancesComponent();
  ~InstancesComponent();
  GeometryComponent *copy() const override;

  void clear() override;

  const Instances *get() const;
  Instances *get_for_write();

  void replace(Instances *instances,
               GeometryOwnershipType ownership = GeometryOwnershipType::Owned);

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  std::optional<AttributeAccessor> attributes() const final;
  std::optional<MutableAttributeAccessor> attributes_for_write() final;

  static constexpr inline GeometryComponent::Type static_type = Type::Instance;
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

  void clear() override;
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
  const Volume *get() const;
  /**
   * Get the volume from this component. This method can only be used when the component is
   * mutable, i.e. it is not shared. The returned volume can be modified. No ownership is
   * transferred.
   */
  Volume *get_for_write();

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponent::Type static_type = Type::Volume;
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
  std::unique_ptr<CurvesEditHints> curves_edit_hints_;

  GeometryComponentEditData();

  GeometryComponent *copy() const final;
  bool owns_direct_data() const final;
  void ensure_owns_direct_data() final;

  void clear() override;

  /**
   * The first node that does topology changing operations on curves should store the curve point
   * positions it retrieved as input. Without this, information about the deformed positions is
   * lost, which would make curves sculpt mode fall back to using original curve positions instead
   * of deformed ones.
   */
  static void remember_deformed_curve_positions_if_necessary(GeometrySet &geometry);

  static constexpr inline GeometryComponent::Type static_type = GeometryComponent::Type::Edit;
};

/**
 * A geometry component that stores #GreasePencil data.
 * This component does not implement an attribute API, because the #GreasePencil data itself does
 * not store any attributes, only the individual drawings within it.
 */
class GreasePencilComponent : public GeometryComponent {
 private:
  GreasePencil *grease_pencil_ = nullptr;
  GeometryOwnershipType ownership_ = GeometryOwnershipType::Owned;

 public:
  GreasePencilComponent();
  ~GreasePencilComponent();
  GeometryComponent *copy() const override;

  void clear() override;
  bool has_grease_pencil() const;
  /**
   * Clear the component and replace it with the new \a grease_pencil data.
   */
  void replace(GreasePencil *grease_pencil,
               GeometryOwnershipType ownership = GeometryOwnershipType::Owned);
  /**
   * Return the Grease Pencil data and clear the component. The caller takes over responsibility
   * for freeing the Grease Pencil data (if the component was responsible before).
   */
  GreasePencil *release();

  const GreasePencil *get() const;
  GreasePencil *get_for_write();

  bool is_empty() const final;

  bool owns_direct_data() const override;
  void ensure_owns_direct_data() override;

  static constexpr inline GeometryComponent::Type static_type = Type::GreasePencil;
};

}  // namespace blender::bke
