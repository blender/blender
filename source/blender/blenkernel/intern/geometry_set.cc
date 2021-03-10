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

#include "BKE_attribute.h"
#include "BKE_attribute_access.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_pointcloud.h"
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

GeometryComponent ::~GeometryComponent()
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
  }
  BLI_assert(false);
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
}

std::ostream &operator<<(std::ostream &stream, const GeometrySet &geometry_set)
{
  stream << "<GeometrySet at " << &geometry_set << ", " << geometry_set.components_.size()
         << " components>";
  return stream;
}

/* This generally should not be used. It is necessary currently, so that GeometrySet can by used by
 * the CPPType system. */
bool operator==(const GeometrySet &UNUSED(a), const GeometrySet &UNUSED(b))
{
  return false;
}

/* This generally should not be used. It is necessary currently, so that GeometrySet can by used by
 * the CPPType system. */
uint64_t GeometrySet::hash() const
{
  return reinterpret_cast<uint64_t>(this);
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

/* Create a new geometry set that only contains the given mesh. */
GeometrySet GeometrySet::create_with_mesh(Mesh *mesh, GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  MeshComponent &component = geometry_set.get_component_for_write<MeshComponent>();
  component.replace(mesh, ownership);
  return geometry_set;
}

/* Create a new geometry set that only contains the given point cloud. */
GeometrySet GeometrySet::create_with_pointcloud(PointCloud *pointcloud,
                                                GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  PointCloudComponent &component = geometry_set.get_component_for_write<PointCloudComponent>();
  component.replace(pointcloud, ownership);
  return geometry_set;
}

/* Clear the existing mesh and replace it with the given one. */
void GeometrySet::replace_mesh(Mesh *mesh, GeometryOwnershipType ownership)
{
  MeshComponent &component = this->get_component_for_write<MeshComponent>();
  component.replace(mesh, ownership);
}

/* Clear the existing point cloud and replace with the given one. */
void GeometrySet::replace_pointcloud(PointCloud *pointcloud, GeometryOwnershipType ownership)
{
  PointCloudComponent &pointcloud_component = this->get_component_for_write<PointCloudComponent>();
  pointcloud_component.replace(pointcloud, ownership);
}

/* Returns a mutable mesh or null. No ownership is transferred. */
Mesh *GeometrySet::get_mesh_for_write()
{
  MeshComponent &component = this->get_component_for_write<MeshComponent>();
  return component.get_for_write();
}

/* Returns a mutable point cloud or null. No ownership is transferred. */
PointCloud *GeometrySet::get_pointcloud_for_write()
{
  PointCloudComponent &component = this->get_component_for_write<PointCloudComponent>();
  return component.get_for_write();
}

/* Returns a mutable volume or null. No ownership is transferred. */
Volume *GeometrySet::get_volume_for_write()
{
  VolumeComponent &component = this->get_component_for_write<VolumeComponent>();
  return component.get_for_write();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name C API
 * \{ */

void BKE_geometry_set_free(GeometrySet *geometry_set)
{
  delete geometry_set;
}

bool BKE_geometry_set_has_instances(const GeometrySet *geometry_set)
{
  return geometry_set->get_component_for_read<InstancesComponent>() != nullptr;
}

int BKE_geometry_set_instances(const GeometrySet *geometry_set,
                               float (**r_transforms)[4][4],
                               const int **r_almost_unique_ids,
                               InstancedData **r_instanced_data)
{
  const InstancesComponent *component = geometry_set->get_component_for_read<InstancesComponent>();
  if (component == nullptr) {
    return 0;
  }
  *r_transforms = (float(*)[4][4])component->transforms().data();
  *r_instanced_data = (InstancedData *)component->instanced_data().data();
  *r_almost_unique_ids = (const int *)component->almost_unique_ids().data();
  return component->instances_amount();
}

/** \} */
