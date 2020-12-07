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

#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_pointcloud.h"

#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

using blender::float3;
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
    case GeometryComponentType::Mesh:
      return new MeshComponent();
    case GeometryComponentType::PointCloud:
      return new PointCloudComponent();
    case GeometryComponentType::Instances:
      return new InstancesComponent();
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Component
 * \{ */

MeshComponent::MeshComponent() : GeometryComponent(GeometryComponentType::Mesh)
{
}

MeshComponent::~MeshComponent()
{
  this->clear();
}

GeometryComponent *MeshComponent::copy() const
{
  MeshComponent *new_component = new MeshComponent();
  if (mesh_ != nullptr) {
    new_component->mesh_ = BKE_mesh_copy_for_eval(mesh_, false);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void MeshComponent::clear()
{
  BLI_assert(this->is_mutable());
  if (mesh_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, mesh_);
    }
    mesh_ = nullptr;
  }
  vertex_group_names_.clear();
}

bool MeshComponent::has_mesh() const
{
  return mesh_ != nullptr;
}

/* Clear the component and replace it with the new mesh. */
void MeshComponent::replace(Mesh *mesh, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  mesh_ = mesh;
  ownership_ = ownership;
}

/* Return the mesh and clear the component. The caller takes over responsibility for freeing the
 * mesh (if the component was responsible before). */
Mesh *MeshComponent::release()
{
  BLI_assert(this->is_mutable());
  Mesh *mesh = mesh_;
  mesh_ = nullptr;
  return mesh;
}

void MeshComponent::copy_vertex_group_names_from_object(const Object &object)
{
  BLI_assert(this->is_mutable());
  vertex_group_names_.clear();
  int index = 0;
  LISTBASE_FOREACH (const bDeformGroup *, group, &object.defbase) {
    vertex_group_names_.add(group->name, index);
    index++;
  }
}

/* Get the mesh from this component. This method can be used by multiple threads at the same
 * time. Therefore, the returned mesh should not be modified. No ownership is transferred. */
const Mesh *MeshComponent::get_for_read() const
{
  return mesh_;
}

/* Get the mesh from this component. This method can only be used when the component is mutable,
 * i.e. it is not shared. The returned mesh can be modified. No ownership is transferred. */
Mesh *MeshComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    mesh_ = BKE_mesh_copy_for_eval(mesh_, false);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return mesh_;
}

bool MeshComponent::is_empty() const
{
  return mesh_ == nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pointcloud Component
 * \{ */

PointCloudComponent::PointCloudComponent() : GeometryComponent(GeometryComponentType::PointCloud)
{
}

PointCloudComponent::~PointCloudComponent()
{
  this->clear();
}

GeometryComponent *PointCloudComponent::copy() const
{
  PointCloudComponent *new_component = new PointCloudComponent();
  if (pointcloud_ != nullptr) {
    new_component->pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_, false);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void PointCloudComponent::clear()
{
  BLI_assert(this->is_mutable());
  if (pointcloud_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, pointcloud_);
    }
    pointcloud_ = nullptr;
  }
}

bool PointCloudComponent::has_pointcloud() const
{
  return pointcloud_ != nullptr;
}

/* Clear the component and replace it with the new point cloud. */
void PointCloudComponent::replace(PointCloud *pointcloud, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  pointcloud_ = pointcloud;
  ownership_ = ownership;
}

/* Return the point cloud and clear the component. The caller takes over responsibility for freeing
 * the point cloud (if the component was responsible before). */
PointCloud *PointCloudComponent::release()
{
  BLI_assert(this->is_mutable());
  PointCloud *pointcloud = pointcloud_;
  pointcloud_ = nullptr;
  return pointcloud;
}

/* Get the point cloud from this component. This method can be used by multiple threads at the same
 * time. Therefore, the returned point cloud should not be modified. No ownership is transferred.
 */
const PointCloud *PointCloudComponent::get_for_read() const
{
  return pointcloud_;
}

/* Get the point cloud from this component. This method can only be used when the component is
 * mutable, i.e. it is not shared. The returned point cloud can be modified. No ownership is
 * transferred. */
PointCloud *PointCloudComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    pointcloud_ = BKE_pointcloud_copy_for_eval(pointcloud_, false);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return pointcloud_;
}

bool PointCloudComponent::is_empty() const
{
  return pointcloud_ == nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Instances Component
 * \{ */

InstancesComponent::InstancesComponent() : GeometryComponent(GeometryComponentType::Instances)
{
}

GeometryComponent *InstancesComponent::copy() const
{
  InstancesComponent *new_component = new InstancesComponent();
  new_component->positions_ = positions_;
  new_component->rotations_ = rotations_;
  new_component->scales_ = scales_;
  new_component->objects_ = objects_;
  return new_component;
}

void InstancesComponent::clear()
{
  objects_.clear();
  positions_.clear();
  rotations_.clear();
  scales_.clear();
}
void InstancesComponent::add_instance(const Object *object,
                                      blender::float3 position,
                                      blender::float3 rotation,
                                      blender::float3 scale)
{
  objects_.append(object);
  positions_.append(position);
  rotations_.append(rotation);
  scales_.append(scale);
}

Span<const Object *> InstancesComponent::objects() const
{
  return objects_;
}

Span<float3> InstancesComponent::positions() const
{
  return positions_;
}

blender::Span<blender::float3> InstancesComponent::rotations() const
{
  return rotations_;
}

blender::Span<blender::float3> InstancesComponent::scales() const
{
  return scales_;
}

MutableSpan<float3> InstancesComponent::positions()
{
  return positions_;
}

int InstancesComponent::instances_amount() const
{
  BLI_assert(positions_.size() == objects_.size());
  return objects_.size();
}

bool InstancesComponent::is_empty() const
{
  return positions_.size() == 0;
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
                               float (**r_positions)[3],
                               float (**r_rotations)[3],
                               float (**r_scales)[3],
                               Object ***r_objects)
{
  const InstancesComponent *component = geometry_set->get_component_for_read<InstancesComponent>();
  if (component == nullptr) {
    return 0;
  }
  *r_positions = (float(*)[3])component->positions().data();
  *r_rotations = (float(*)[3])component->rotations().data();
  *r_scales = (float(*)[3])component->scales().data();
  *r_objects = (Object **)component->objects().data();
  return component->instances_amount();
}

/** \} */
