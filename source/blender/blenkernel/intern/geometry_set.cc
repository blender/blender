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

#include "BLI_map.hh"
#include "BLI_task.hh"

#include "BKE_attribute.h"
#include "BKE_attribute_access.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_pointcloud.h"
#include "BKE_spline.hh"
#include "BKE_volume.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"

#include "BLI_rand.hh"

#include "MEM_guardedalloc.h"

using blender::float3;
using blender::float4x4;
using blender::Map;
using blender::MutableSpan;
using blender::Span;
using blender::StringRef;
using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Geometry Component
 * \{ */

GeometryComponent::GeometryComponent(GeometryComponentType type) : type_(type)
{
}

GeometryComponent *GeometryComponent::create(GeometryComponentType component_type)
{
  switch (component_type) {
    case GEO_COMPONENT_TYPE_MESH:
      return new MeshComponent();
    case GEO_COMPONENT_TYPE_POINT_CLOUD:
      return new PointCloudComponent();
    case GEO_COMPONENT_TYPE_INSTANCES:
      return new InstancesComponent();
    case GEO_COMPONENT_TYPE_VOLUME:
      return new VolumeComponent();
    case GEO_COMPONENT_TYPE_CURVE:
      return new CurveComponent();
  }
  BLI_assert_unreachable();
  return nullptr;
}

void GeometryComponent::user_add() const
{
  users_.fetch_add(1);
}

void GeometryComponent::user_remove() const
{
  const int new_users = users_.fetch_sub(1) - 1;
  if (new_users == 0) {
    delete this;
  }
}

bool GeometryComponent::is_mutable() const
{
  /* If the item is shared, it is read-only. */
  /* The user count can be 0, when this is called from the destructor. */
  return users_ <= 1;
}

GeometryComponentType GeometryComponent::type() const
{
  return type_;
}

bool GeometryComponent::is_empty() const
{
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Set
 * \{ */

/* The methods are defaulted here so that they are not instantiated in every translation unit. */
GeometrySet::GeometrySet() = default;
GeometrySet::GeometrySet(const GeometrySet &other) = default;
GeometrySet::GeometrySet(GeometrySet &&other) = default;
GeometrySet::~GeometrySet() = default;
GeometrySet &GeometrySet::operator=(const GeometrySet &other) = default;
GeometrySet &GeometrySet::operator=(GeometrySet &&other) = default;

/* This method can only be used when the geometry set is mutable. It returns a mutable geometry
 * component of the given type.
 */
GeometryComponent &GeometrySet::get_component_for_write(GeometryComponentType component_type)
{
  return components_.add_or_modify(
      component_type,
      [&](GeometryComponentPtr *value_ptr) -> GeometryComponent & {
        /* If the component did not exist before, create a new one. */
        new (value_ptr) GeometryComponentPtr(GeometryComponent::create(component_type));
        return **value_ptr;
      },
      [&](GeometryComponentPtr *value_ptr) -> GeometryComponent & {
        GeometryComponentPtr &value = *value_ptr;
        if (value->is_mutable()) {
          /* If the referenced component is already mutable, return it directly. */
          return *value;
        }
        /* If the referenced component is shared, make a copy. The copy is not shared and is
         * therefore mutable. */
        GeometryComponent *copied_component = value->copy();
        value = GeometryComponentPtr{copied_component};
        return *copied_component;
      });
}

/**
 * Retrieve the pointer to a component without creating it if it does not exist,
 * unlike #get_component_for_write.
 */
GeometryComponent *GeometrySet::get_component_ptr(GeometryComponentType type)
{
  if (this->has(type)) {
    return &this->get_component_for_write(type);
  }
  return nullptr;
}

/* Get the component of the given type. Might return null if the component does not exist yet. */
const GeometryComponent *GeometrySet::get_component_for_read(
    GeometryComponentType component_type) const
{
  const GeometryComponentPtr *component = components_.lookup_ptr(component_type);
  if (component != nullptr) {
    return component->get();
  }
  return nullptr;
}

bool GeometrySet::has(const GeometryComponentType component_type) const
{
  return components_.contains(component_type);
}

void GeometrySet::remove(const GeometryComponentType component_type)
{
  components_.remove(component_type);
}

/**
 * Remove all geometry components with types that are not in the provided list.
 */
void GeometrySet::keep_only(const blender::Span<GeometryComponentType> component_types)
{
  for (auto it = components_.keys().begin(); it != components_.keys().end(); ++it) {
    const GeometryComponentType type = *it;
    if (!component_types.contains(type)) {
      components_.remove(it);
    }
  }
}

void GeometrySet::add(const GeometryComponent &component)
{
  BLI_assert(!components_.contains(component.type()));
  component.user_add();
  GeometryComponentPtr component_ptr{const_cast<GeometryComponent *>(&component)};
  components_.add_new(component.type(), std::move(component_ptr));
}

/**
 * Get all geometry components in this geometry set for read-only access.
 */
Vector<const GeometryComponent *> GeometrySet::get_components_for_read() const
{
  Vector<const GeometryComponent *> components;
  for (const GeometryComponentPtr &ptr : components_.values()) {
    components.append(ptr.get());
  }
  return components;
}

void GeometrySet::compute_boundbox_without_instances(float3 *r_min, float3 *r_max) const
{
  const PointCloud *pointcloud = this->get_pointcloud_for_read();
  if (pointcloud != nullptr) {
    BKE_pointcloud_minmax(pointcloud, *r_min, *r_max);
  }
  const Mesh *mesh = this->get_mesh_for_read();
  if (mesh != nullptr) {
    BKE_mesh_wrapper_minmax(mesh, *r_min, *r_max);
  }
  const Volume *volume = this->get_volume_for_read();
  if (volume != nullptr) {
    BKE_volume_min_max(volume, *r_min, *r_max);
  }
  const CurveEval *curve = this->get_curve_for_read();
  if (curve != nullptr) {
    /* Using the evaluated positions is somewhat arbitrary, but it is probably expected. */
    curve->bounds_min_max(*r_min, *r_max, true);
  }
}

std::ostream &operator<<(std::ostream &stream, const GeometrySet &geometry_set)
{
  stream << "<GeometrySet at " << &geometry_set << ", " << geometry_set.components_.size()
         << " components>";
  return stream;
}

/* Remove all geometry components from the geometry set. */
void GeometrySet::clear()
{
  components_.clear();
}

/* Make sure that the geometry can be cached. This does not ensure ownership of object/collection
 * instances. */
void GeometrySet::ensure_owns_direct_data()
{
  for (GeometryComponentType type : components_.keys()) {
    const GeometryComponent *component = this->get_component_for_read(type);
    if (!component->owns_direct_data()) {
      GeometryComponent &component_for_write = this->get_component_for_write(type);
      component_for_write.ensure_owns_direct_data();
    }
  }
}

bool GeometrySet::owns_direct_data() const
{
  for (const GeometryComponentPtr &component : components_.values()) {
    if (!component->owns_direct_data()) {
      return false;
    }
  }
  return true;
}

/* Returns a read-only mesh or null. */
const Mesh *GeometrySet::get_mesh_for_read() const
{
  const MeshComponent *component = this->get_component_for_read<MeshComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

/* Returns true when the geometry set has a mesh component that has a mesh. */
bool GeometrySet::has_mesh() const
{
  const MeshComponent *component = this->get_component_for_read<MeshComponent>();
  return component != nullptr && component->has_mesh();
}

/* Returns a read-only point cloud of null. */
const PointCloud *GeometrySet::get_pointcloud_for_read() const
{
  const PointCloudComponent *component = this->get_component_for_read<PointCloudComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

/* Returns a read-only volume or null. */
const Volume *GeometrySet::get_volume_for_read() const
{
  const VolumeComponent *component = this->get_component_for_read<VolumeComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

/* Returns a read-only curve or null. */
const CurveEval *GeometrySet::get_curve_for_read() const
{
  const CurveComponent *component = this->get_component_for_read<CurveComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

/* Returns true when the geometry set has a point cloud component that has a point cloud. */
bool GeometrySet::has_pointcloud() const
{
  const PointCloudComponent *component = this->get_component_for_read<PointCloudComponent>();
  return component != nullptr && component->has_pointcloud();
}

/* Returns true when the geometry set has an instances component that has at least one instance. */
bool GeometrySet::has_instances() const
{
  const InstancesComponent *component = this->get_component_for_read<InstancesComponent>();
  return component != nullptr && component->instances_amount() >= 1;
}

/* Returns true when the geometry set has a volume component that has a volume. */
bool GeometrySet::has_volume() const
{
  const VolumeComponent *component = this->get_component_for_read<VolumeComponent>();
  return component != nullptr && component->has_volume();
}

/* Returns true when the geometry set has a curve component that has a curve. */
bool GeometrySet::has_curve() const
{
  const CurveComponent *component = this->get_component_for_read<CurveComponent>();
  return component != nullptr && component->has_curve();
}

/* Returns true when the geometry set has any data that is not an instance. */
bool GeometrySet::has_realized_data() const
{
  if (components_.is_empty()) {
    return false;
  }
  if (components_.size() > 1) {
    return true;
  }
  /* Check if the only component is an #InstancesComponent. */
  return this->get_component_for_read<InstancesComponent>() == nullptr;
}

/* Return true if the geometry set has any component that isn't empty. */
bool GeometrySet::is_empty() const
{
  if (components_.is_empty()) {
    return true;
  }
  return !(this->has_mesh() || this->has_curve() || this->has_pointcloud() ||
           this->has_instances());
}

/* Create a new geometry set that only contains the given mesh. */
GeometrySet GeometrySet::create_with_mesh(Mesh *mesh, GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  if (mesh != nullptr) {
    MeshComponent &component = geometry_set.get_component_for_write<MeshComponent>();
    component.replace(mesh, ownership);
  }
  return geometry_set;
}

/* Create a new geometry set that only contains the given point cloud. */
GeometrySet GeometrySet::create_with_pointcloud(PointCloud *pointcloud,
                                                GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  if (pointcloud != nullptr) {
    PointCloudComponent &component = geometry_set.get_component_for_write<PointCloudComponent>();
    component.replace(pointcloud, ownership);
  }
  return geometry_set;
}

/* Create a new geometry set that only contains the given curve. */
GeometrySet GeometrySet::create_with_curve(CurveEval *curve, GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  if (curve != nullptr) {
    CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
    component.replace(curve, ownership);
  }
  return geometry_set;
}

/* Clear the existing mesh and replace it with the given one. */
void GeometrySet::replace_mesh(Mesh *mesh, GeometryOwnershipType ownership)
{
  if (mesh == nullptr) {
    this->remove<MeshComponent>();
  }
  else {
    MeshComponent &component = this->get_component_for_write<MeshComponent>();
    component.replace(mesh, ownership);
  }
}

/* Clear the existing curve and replace it with the given one. */
void GeometrySet::replace_curve(CurveEval *curve, GeometryOwnershipType ownership)
{
  if (curve == nullptr) {
    this->remove<CurveComponent>();
  }
  else {
    CurveComponent &component = this->get_component_for_write<CurveComponent>();
    component.replace(curve, ownership);
  }
}

/* Clear the existing point cloud and replace with the given one. */
void GeometrySet::replace_pointcloud(PointCloud *pointcloud, GeometryOwnershipType ownership)
{
  if (pointcloud == nullptr) {
    this->remove<PointCloudComponent>();
  }
  else {
    PointCloudComponent &component = this->get_component_for_write<PointCloudComponent>();
    component.replace(pointcloud, ownership);
  }
}

/* Clear the existing volume and replace with the given one. */
void GeometrySet::replace_volume(Volume *volume, GeometryOwnershipType ownership)
{
  if (volume == nullptr) {
    this->remove<VolumeComponent>();
  }
  else {
    VolumeComponent &component = this->get_component_for_write<VolumeComponent>();
    component.replace(volume, ownership);
  }
}

/* Returns a mutable mesh or null. No ownership is transferred. */
Mesh *GeometrySet::get_mesh_for_write()
{
  MeshComponent *component = this->get_component_ptr<MeshComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

/* Returns a mutable point cloud or null. No ownership is transferred. */
PointCloud *GeometrySet::get_pointcloud_for_write()
{
  PointCloudComponent *component = this->get_component_ptr<PointCloudComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

/* Returns a mutable volume or null. No ownership is transferred. */
Volume *GeometrySet::get_volume_for_write()
{
  VolumeComponent *component = this->get_component_ptr<VolumeComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

/* Returns a mutable curve or null. No ownership is transferred. */
CurveEval *GeometrySet::get_curve_for_write()
{
  CurveComponent *component = this->get_component_ptr<CurveComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

void GeometrySet::attribute_foreach(const Span<GeometryComponentType> component_types,
                                    const bool include_instances,
                                    const AttributeForeachCallback callback) const
{
  using namespace blender;
  using namespace blender::bke;
  for (const GeometryComponentType component_type : component_types) {
    if (!this->has(component_type)) {
      continue;
    }
    const GeometryComponent &component = *this->get_component_for_read(component_type);
    component.attribute_foreach(
        [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
          callback(attribute_id, meta_data, component);
          return true;
        });
  }
  if (include_instances && this->has_instances()) {
    const InstancesComponent &instances = *this->get_component_for_read<InstancesComponent>();
    instances.foreach_referenced_geometry([&](const GeometrySet &instance_geometry_set) {
      instance_geometry_set.attribute_foreach(component_types, include_instances, callback);
    });
  }
}

void GeometrySet::gather_attributes_for_propagation(
    const Span<GeometryComponentType> component_types,
    const GeometryComponentType dst_component_type,
    bool include_instances,
    blender::Map<blender::bke::AttributeIDRef, AttributeKind> &r_attributes) const
{
  using namespace blender;
  using namespace blender::bke;
  /* Only needed right now to check if an attribute is built-in on this component type.
   * TODO: Get rid of the dummy component. */
  const GeometryComponent *dummy_component = GeometryComponent::create(dst_component_type);
  this->attribute_foreach(
      component_types,
      include_instances,
      [&](const AttributeIDRef &attribute_id,
          const AttributeMetaData &meta_data,
          const GeometryComponent &component) {
        if (component.attribute_is_builtin(attribute_id)) {
          if (!dummy_component->attribute_is_builtin(attribute_id)) {
            /* Don't propagate built-in attributes that are not built-in on the destination
             * component. */
            return;
          }
        }

        if (!attribute_id.should_be_kept()) {
          return;
        }

        auto add_info = [&](AttributeKind *attribute_kind) {
          attribute_kind->domain = meta_data.domain;
          attribute_kind->data_type = meta_data.data_type;
        };
        auto modify_info = [&](AttributeKind *attribute_kind) {
          attribute_kind->domain = bke::attribute_domain_highest_priority(
              {attribute_kind->domain, meta_data.domain});
          attribute_kind->data_type = bke::attribute_data_type_highest_complexity(
              {attribute_kind->data_type, meta_data.data_type});
        };
        r_attributes.add_or_modify(attribute_id, add_info, modify_info);
      });
  delete dummy_component;
}

static void gather_mutable_geometry_sets(GeometrySet &geometry_set,
                                         Vector<GeometrySet *> &r_geometry_sets)
{
  r_geometry_sets.append(&geometry_set);
  if (!geometry_set.has_instances()) {
    return;
  }
  /* In the future this can be improved by deduplicating instance references across different
   * instances. */
  InstancesComponent &instances_component =
      geometry_set.get_component_for_write<InstancesComponent>();
  instances_component.ensure_geometry_instances();
  for (const int handle : instances_component.references().index_range()) {
    if (instances_component.references()[handle].type() == InstanceReference::Type::GeometrySet) {
      GeometrySet &instance_geometry = instances_component.geometry_set_from_reference(handle);
      gather_mutable_geometry_sets(instance_geometry, r_geometry_sets);
    }
  }
}

/**
 * Modify every (recursive) instance separately. This is often more efficient than realizing all
 * instances just to change the same thing on all of them.
 */
void GeometrySet::modify_geometry_sets(ForeachSubGeometryCallback callback)
{
  Vector<GeometrySet *> geometry_sets;
  gather_mutable_geometry_sets(*this, geometry_sets);
  blender::threading::parallel_for_each(
      geometry_sets, [&](GeometrySet *geometry_set) { callback(*geometry_set); });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name C API
 * \{ */

void BKE_geometry_set_free(GeometrySet *geometry_set)
{
  delete geometry_set;
}

bool BKE_object_has_geometry_set_instances(const Object *ob)
{
  const GeometrySet *geometry_set = ob->runtime.geometry_set_eval;
  if (geometry_set == nullptr) {
    return false;
  }
  if (geometry_set->has_instances()) {
    return true;
  }
  const bool has_mesh = geometry_set->has_mesh();
  const bool has_pointcloud = geometry_set->has_pointcloud();
  const bool has_volume = geometry_set->has_volume();
  const bool has_curve = geometry_set->has_curve();
  if (ob->type == OB_MESH) {
    return has_pointcloud || has_volume || has_curve;
  }
  if (ob->type == OB_POINTCLOUD) {
    return has_mesh || has_volume || has_curve;
  }
  if (ob->type == OB_VOLUME) {
    return has_mesh || has_pointcloud || has_curve;
  }
  if (ELEM(ob->type, OB_CURVE, OB_FONT)) {
    return has_mesh || has_pointcloud || has_volume;
  }
  return false;
}

/** \} */
