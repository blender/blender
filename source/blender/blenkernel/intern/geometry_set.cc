/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_map.hh"
#include "BLI_task.hh"

#include "BLT_translation.h"

#include "BKE_attribute.h"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_pointcloud.h"
#include "BKE_volume.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include "BLI_rand.hh"

#include "MEM_guardedalloc.h"

using blender::float3;
using blender::float4x4;
using blender::Map;
using blender::MutableSpan;
using blender::Span;
using blender::StringRef;
using blender::Vector;
using blender::bke::InstanceReference;
using blender::bke::Instances;

/* -------------------------------------------------------------------- */
/** \name Geometry Component
 * \{ */

GeometryComponent::GeometryComponent(GeometryComponentType type) : type_(type) {}

GeometryComponentPtr GeometryComponent::create(GeometryComponentType component_type)
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
    case GEO_COMPONENT_TYPE_EDIT:
      return new GeometryComponentEditData();
  }
  BLI_assert_unreachable();
  return {};
}

int GeometryComponent::attribute_domain_size(const eAttrDomain domain) const
{
  if (this->is_empty()) {
    return 0;
  }
  const std::optional<blender::bke::AttributeAccessor> attributes = this->attributes();
  if (attributes.has_value()) {
    return attributes->domain_size(domain);
  }
  return 0;
}

std::optional<blender::bke::AttributeAccessor> GeometryComponent::attributes() const
{
  return std::nullopt;
};
std::optional<blender::bke::MutableAttributeAccessor> GeometryComponent::attributes_for_write()
{
  return std::nullopt;
}

GeometryComponentType GeometryComponent::type() const
{
  return type_;
}

bool GeometryComponent::is_empty() const
{
  return false;
}

void GeometryComponent::delete_self()
{
  delete this;
}

void GeometryComponent::delete_data_only()
{
  this->clear();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Set
 * \{ */

GeometrySet::GeometrySet() = default;
GeometrySet::GeometrySet(const GeometrySet &other) = default;
GeometrySet::GeometrySet(GeometrySet &&other) = default;
GeometrySet::~GeometrySet() = default;
GeometrySet &GeometrySet::operator=(const GeometrySet &other) = default;
GeometrySet &GeometrySet::operator=(GeometrySet &&other) = default;

GeometryComponent &GeometrySet::get_component_for_write(GeometryComponentType component_type)
{
  GeometryComponentPtr &component_ptr = components_[component_type];
  if (!component_ptr) {
    /* If the component did not exist before, create a new one. */
    component_ptr = GeometryComponent::create(component_type);
    return *component_ptr;
  }
  if (component_ptr->is_mutable()) {
    /* If the referenced component is already mutable, return it directly. */
    component_ptr->tag_ensured_mutable();
    return *component_ptr;
  }
  /* If the referenced component is shared, make a copy. The copy is not shared and is
   * therefore mutable. */
  component_ptr = component_ptr->copy();
  return *component_ptr;
}

GeometryComponent *GeometrySet::get_component_ptr(GeometryComponentType type)
{
  if (this->has(type)) {
    return &this->get_component_for_write(type);
  }
  return nullptr;
}

const GeometryComponent *GeometrySet::get_component_for_read(
    GeometryComponentType component_type) const
{
  return components_[component_type].get();
}

bool GeometrySet::has(const GeometryComponentType component_type) const
{
  const GeometryComponentPtr &component = components_[component_type];
  return component.has_value() && !component->is_empty();
}

void GeometrySet::remove(const GeometryComponentType component_type)
{
  components_[component_type].reset();
}

void GeometrySet::keep_only(const blender::Span<GeometryComponentType> component_types)
{
  for (GeometryComponentPtr &component_ptr : components_) {
    if (component_ptr) {
      if (!component_types.contains(component_ptr->type())) {
        component_ptr.reset();
      }
    }
  }
}

void GeometrySet::keep_only_during_modify(
    const blender::Span<GeometryComponentType> component_types)
{
  Vector<GeometryComponentType> extended_types = component_types;
  extended_types.append_non_duplicates(GEO_COMPONENT_TYPE_INSTANCES);
  extended_types.append_non_duplicates(GEO_COMPONENT_TYPE_EDIT);
  this->keep_only(extended_types);
}

void GeometrySet::remove_geometry_during_modify()
{
  this->keep_only_during_modify({});
}

void GeometrySet::add(const GeometryComponent &component)
{
  BLI_assert(!components_[component.type()]);
  component.add_user();
  components_[component.type()] = const_cast<GeometryComponent *>(&component);
}

Vector<const GeometryComponent *> GeometrySet::get_components_for_read() const
{
  Vector<const GeometryComponent *> components;
  for (const GeometryComponentPtr &component_ptr : components_) {
    if (component_ptr) {
      components.append(component_ptr.get());
    }
  }
  return components;
}

bool GeometrySet::compute_boundbox_without_instances(float3 *r_min, float3 *r_max) const
{
  using namespace blender;
  bool have_minmax = false;
  if (const PointCloud *pointcloud = this->get_pointcloud_for_read()) {
    have_minmax |= pointcloud->bounds_min_max(*r_min, *r_max);
  }
  if (const Mesh *mesh = this->get_mesh_for_read()) {
    have_minmax |= BKE_mesh_wrapper_minmax(mesh, *r_min, *r_max);
  }
  if (const Volume *volume = this->get_volume_for_read()) {
    have_minmax |= BKE_volume_min_max(volume, *r_min, *r_max);
  }
  if (const Curves *curves_id = this->get_curves_for_read()) {
    const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    have_minmax |= curves.bounds_min_max(*r_min, *r_max);
  }
  return have_minmax;
}

std::ostream &operator<<(std::ostream &stream, const GeometrySet &geometry_set)
{
  Vector<std::string> parts;
  if (const Mesh *mesh = geometry_set.get_mesh_for_read()) {
    parts.append(std::to_string(mesh->totvert) + " verts");
    parts.append(std::to_string(mesh->totedge) + " edges");
    parts.append(std::to_string(mesh->totpoly) + " polys");
    parts.append(std::to_string(mesh->totloop) + " corners");
  }
  if (const Curves *curves = geometry_set.get_curves_for_read()) {
    parts.append(std::to_string(curves->geometry.point_num) + " control points");
    parts.append(std::to_string(curves->geometry.curve_num) + " curves");
  }
  if (const PointCloud *point_cloud = geometry_set.get_pointcloud_for_read()) {
    parts.append(std::to_string(point_cloud->totpoint) + " points");
  }
  if (const Volume *volume = geometry_set.get_volume_for_read()) {
    parts.append(std::to_string(BKE_volume_num_grids(volume)) + " volume grids");
  }
  if (geometry_set.has_instances()) {
    parts.append(std::to_string(geometry_set.get_instances_for_read()->instances_num()) +
                 " instances");
  }
  if (geometry_set.get_curve_edit_hints_for_read()) {
    parts.append("curve edit hints");
  }

  stream << "<GeometrySet: ";
  for (const int i : parts.index_range()) {
    stream << parts[i];
    if (i < parts.size() - 1) {
      stream << ", ";
    }
  }
  stream << ">";
  return stream;
}

void GeometrySet::clear()
{
  for (GeometryComponentPtr &component_ptr : components_) {
    component_ptr.reset();
  }
}

void GeometrySet::ensure_owns_direct_data()
{
  for (GeometryComponentPtr &component_ptr : components_) {
    if (!component_ptr) {
      continue;
    }
    if (component_ptr->owns_direct_data()) {
      continue;
    }
    GeometryComponent &component_for_write = this->get_component_for_write(component_ptr->type());
    component_for_write.ensure_owns_direct_data();
  }
}

void GeometrySet::ensure_owns_all_data()
{
  if (Instances *instances = this->get_instances_for_write()) {
    instances->ensure_geometry_instances();
  }
  this->ensure_owns_direct_data();
}

bool GeometrySet::owns_direct_data() const
{
  for (const GeometryComponentPtr &component_ptr : components_) {
    if (component_ptr) {
      if (!component_ptr->owns_direct_data()) {
        return false;
      }
    }
  }
  return true;
}

const Mesh *GeometrySet::get_mesh_for_read() const
{
  const MeshComponent *component = this->get_component_for_read<MeshComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

bool GeometrySet::has_mesh() const
{
  const MeshComponent *component = this->get_component_for_read<MeshComponent>();
  return component != nullptr && component->has_mesh();
}

const PointCloud *GeometrySet::get_pointcloud_for_read() const
{
  const PointCloudComponent *component = this->get_component_for_read<PointCloudComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

const Volume *GeometrySet::get_volume_for_read() const
{
  const VolumeComponent *component = this->get_component_for_read<VolumeComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

const Curves *GeometrySet::get_curves_for_read() const
{
  const CurveComponent *component = this->get_component_for_read<CurveComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

const Instances *GeometrySet::get_instances_for_read() const
{
  const InstancesComponent *component = this->get_component_for_read<InstancesComponent>();
  return (component == nullptr) ? nullptr : component->get_for_read();
}

const blender::bke::CurvesEditHints *GeometrySet::get_curve_edit_hints_for_read() const
{
  const GeometryComponentEditData *component =
      this->get_component_for_read<GeometryComponentEditData>();
  return (component == nullptr) ? nullptr : component->curves_edit_hints_.get();
}

bool GeometrySet::has_pointcloud() const
{
  const PointCloudComponent *component = this->get_component_for_read<PointCloudComponent>();
  return component != nullptr && component->has_pointcloud();
}

bool GeometrySet::has_instances() const
{
  const InstancesComponent *component = this->get_component_for_read<InstancesComponent>();
  return component != nullptr && component->get_for_read() != nullptr &&
         component->get_for_read()->instances_num() >= 1;
}

bool GeometrySet::has_volume() const
{
  const VolumeComponent *component = this->get_component_for_read<VolumeComponent>();
  return component != nullptr && component->has_volume();
}

bool GeometrySet::has_curves() const
{
  const CurveComponent *component = this->get_component_for_read<CurveComponent>();
  return component != nullptr && component->has_curves();
}

bool GeometrySet::has_realized_data() const
{
  for (const GeometryComponentPtr &component_ptr : components_) {
    if (component_ptr) {
      if (component_ptr->type() != GEO_COMPONENT_TYPE_INSTANCES) {
        return true;
      }
    }
  }
  return false;
}

bool GeometrySet::is_empty() const
{
  return !(this->has_mesh() || this->has_curves() || this->has_pointcloud() ||
           this->has_volume() || this->has_instances());
}

GeometrySet GeometrySet::create_with_mesh(Mesh *mesh, GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  if (mesh != nullptr) {
    MeshComponent &component = geometry_set.get_component_for_write<MeshComponent>();
    component.replace(mesh, ownership);
  }
  return geometry_set;
}

GeometrySet GeometrySet::create_with_volume(Volume *volume, GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  if (volume != nullptr) {
    VolumeComponent &component = geometry_set.get_component_for_write<VolumeComponent>();
    component.replace(volume, ownership);
  }
  return geometry_set;
}

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

GeometrySet GeometrySet::create_with_curves(Curves *curves, GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  if (curves != nullptr) {
    CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
    component.replace(curves, ownership);
  }
  return geometry_set;
}

GeometrySet GeometrySet::create_with_instances(Instances *instances,
                                               GeometryOwnershipType ownership)
{
  GeometrySet geometry_set;
  geometry_set.replace_instances(instances, ownership);
  return geometry_set;
}

void GeometrySet::replace_mesh(Mesh *mesh, GeometryOwnershipType ownership)
{
  if (mesh == nullptr) {
    this->remove<MeshComponent>();
    return;
  }
  if (mesh == this->get_mesh_for_read()) {
    return;
  }
  this->remove<MeshComponent>();
  MeshComponent &component = this->get_component_for_write<MeshComponent>();
  component.replace(mesh, ownership);
}

void GeometrySet::replace_curves(Curves *curves, GeometryOwnershipType ownership)
{
  if (curves == nullptr) {
    this->remove<CurveComponent>();
    return;
  }
  if (curves == this->get_curves_for_read()) {
    return;
  }
  this->remove<CurveComponent>();
  CurveComponent &component = this->get_component_for_write<CurveComponent>();
  component.replace(curves, ownership);
}

void GeometrySet::replace_instances(Instances *instances, GeometryOwnershipType ownership)
{
  if (instances == nullptr) {
    this->remove<InstancesComponent>();
    return;
  }
  if (instances == this->get_instances_for_read()) {
    return;
  }
  this->remove<InstancesComponent>();
  InstancesComponent &component = this->get_component_for_write<InstancesComponent>();
  component.replace(instances, ownership);
}

void GeometrySet::replace_pointcloud(PointCloud *pointcloud, GeometryOwnershipType ownership)
{
  if (pointcloud == nullptr) {
    this->remove<PointCloudComponent>();
    return;
  }
  if (pointcloud == this->get_pointcloud_for_read()) {
    return;
  }
  this->remove<PointCloudComponent>();
  PointCloudComponent &component = this->get_component_for_write<PointCloudComponent>();
  component.replace(pointcloud, ownership);
}

void GeometrySet::replace_volume(Volume *volume, GeometryOwnershipType ownership)
{
  if (volume == nullptr) {
    this->remove<VolumeComponent>();
    return;
  }
  if (volume == this->get_volume_for_read()) {
    return;
  }
  this->remove<VolumeComponent>();
  VolumeComponent &component = this->get_component_for_write<VolumeComponent>();
  component.replace(volume, ownership);
}

Mesh *GeometrySet::get_mesh_for_write()
{
  MeshComponent *component = this->get_component_ptr<MeshComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

PointCloud *GeometrySet::get_pointcloud_for_write()
{
  PointCloudComponent *component = this->get_component_ptr<PointCloudComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

Volume *GeometrySet::get_volume_for_write()
{
  VolumeComponent *component = this->get_component_ptr<VolumeComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

Curves *GeometrySet::get_curves_for_write()
{
  CurveComponent *component = this->get_component_ptr<CurveComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

Instances *GeometrySet::get_instances_for_write()
{
  InstancesComponent *component = this->get_component_ptr<InstancesComponent>();
  return component == nullptr ? nullptr : component->get_for_write();
}

blender::bke::CurvesEditHints *GeometrySet::get_curve_edit_hints_for_write()
{
  if (!this->has<GeometryComponentEditData>()) {
    return nullptr;
  }
  GeometryComponentEditData &component =
      this->get_component_for_write<GeometryComponentEditData>();
  return component.curves_edit_hints_.get();
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
    const std::optional<AttributeAccessor> attributes = component.attributes();
    if (attributes.has_value()) {
      attributes->for_all(
          [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
            callback(attribute_id, meta_data, component);
            return true;
          });
    }
  }
  if (include_instances && this->has_instances()) {
    const Instances &instances = *this->get_instances_for_read();
    instances.foreach_referenced_geometry([&](const GeometrySet &instance_geometry_set) {
      instance_geometry_set.attribute_foreach(component_types, include_instances, callback);
    });
  }
}

void GeometrySet::gather_attributes_for_propagation(
    const Span<GeometryComponentType> component_types,
    const GeometryComponentType dst_component_type,
    bool include_instances,
    const blender::bke::AnonymousAttributePropagationInfo &propagation_info,
    blender::Map<blender::bke::AttributeIDRef, blender::bke::AttributeKind> &r_attributes) const
{
  using namespace blender;
  using namespace blender::bke;
  /* Only needed right now to check if an attribute is built-in on this component type.
   * TODO: Get rid of the dummy component. */
  const GeometryComponentPtr dummy_component = GeometryComponent::create(dst_component_type);
  this->attribute_foreach(
      component_types,
      include_instances,
      [&](const AttributeIDRef &attribute_id,
          const AttributeMetaData &meta_data,
          const GeometryComponent &component) {
        if (component.attributes()->is_builtin(attribute_id)) {
          if (!dummy_component->attributes()->is_builtin(attribute_id)) {
            /* Don't propagate built-in attributes that are not built-in on the destination
             * component. */
            return;
          }
        }
        if (meta_data.data_type == CD_PROP_STRING) {
          /* Propagating string attributes is not supported yet. */
          return;
        }
        if (attribute_id.is_anonymous() &&
            !propagation_info.propagate(attribute_id.anonymous_id())) {
          return;
        }

        eAttrDomain domain = meta_data.domain;
        if (dst_component_type != GEO_COMPONENT_TYPE_INSTANCES && domain == ATTR_DOMAIN_INSTANCE) {
          domain = ATTR_DOMAIN_POINT;
        }

        auto add_info = [&](AttributeKind *attribute_kind) {
          attribute_kind->domain = domain;
          attribute_kind->data_type = meta_data.data_type;
        };
        auto modify_info = [&](AttributeKind *attribute_kind) {
          attribute_kind->domain = bke::attribute_domain_highest_priority(
              {attribute_kind->domain, domain});
          attribute_kind->data_type = bke::attribute_data_type_highest_complexity(
              {attribute_kind->data_type, meta_data.data_type});
        };
        r_attributes.add_or_modify(attribute_id, add_info, modify_info);
      });
}

static void gather_component_types_recursive(const GeometrySet &geometry_set,
                                             const bool include_instances,
                                             const bool ignore_empty,
                                             Vector<GeometryComponentType> &r_types)
{
  for (const GeometryComponent *component : geometry_set.get_components_for_read()) {
    if (ignore_empty) {
      if (component->is_empty()) {
        continue;
      }
    }
    r_types.append_non_duplicates(component->type());
  }
  if (!include_instances) {
    return;
  }
  const blender::bke::Instances *instances = geometry_set.get_instances_for_read();
  if (instances == nullptr) {
    return;
  }
  instances->foreach_referenced_geometry([&](const GeometrySet &instance_geometry_set) {
    gather_component_types_recursive(
        instance_geometry_set, include_instances, ignore_empty, r_types);
  });
}

blender::Vector<GeometryComponentType> GeometrySet::gather_component_types(
    const bool include_instances, bool ignore_empty) const
{
  Vector<GeometryComponentType> types;
  gather_component_types_recursive(*this, include_instances, ignore_empty, types);
  return types;
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
  Instances &instances = *geometry_set.get_instances_for_write();
  instances.ensure_geometry_instances();
  for (const int handle : instances.references().index_range()) {
    if (instances.references()[handle].type() == InstanceReference::Type::GeometrySet) {
      GeometrySet &instance_geometry = instances.geometry_set_from_reference(handle);
      gather_mutable_geometry_sets(instance_geometry, r_geometry_sets);
    }
  }
}

void GeometrySet::modify_geometry_sets(ForeachSubGeometryCallback callback)
{
  Vector<GeometrySet *> geometry_sets;
  gather_mutable_geometry_sets(*this, geometry_sets);
  if (geometry_sets.size() == 1) {
    /* Avoid possible overhead and a large call stack when multithreading is pointless. */
    callback(*geometry_sets.first());
  }
  else {
    blender::threading::parallel_for_each(
        geometry_sets, [&](GeometrySet *geometry_set) { callback(*geometry_set); });
  }
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
  for (const GeometryComponent *component : geometry_set->get_components_for_read()) {
    if (component->is_empty()) {
      continue;
    }
    const GeometryComponentType type = component->type();
    bool is_instance = false;
    switch (type) {
      case GEO_COMPONENT_TYPE_MESH:
        is_instance = ob->type != OB_MESH;
        break;
      case GEO_COMPONENT_TYPE_POINT_CLOUD:
        is_instance = ob->type != OB_POINTCLOUD;
        break;
      case GEO_COMPONENT_TYPE_INSTANCES:
        is_instance = true;
        break;
      case GEO_COMPONENT_TYPE_VOLUME:
        is_instance = ob->type != OB_VOLUME;
        break;
      case GEO_COMPONENT_TYPE_CURVE:
        is_instance = !ELEM(ob->type, OB_CURVES_LEGACY, OB_FONT);
        break;
      case GEO_COMPONENT_TYPE_EDIT:
        break;
    }
    if (is_instance) {
      return true;
    }
  }
  return false;
}

/** \} */
