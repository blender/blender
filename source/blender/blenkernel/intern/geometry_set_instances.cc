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

#include "BKE_collection.h"
#include "BKE_geometry_set_instances.hh"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_pointcloud.h"
#include "BKE_spline.hh"

#include "DNA_collection_types.h"
#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

namespace blender::bke {

static void geometry_set_collect_recursive(const GeometrySet &geometry_set,
                                           const float4x4 &transform,
                                           Vector<GeometryInstanceGroup> &r_sets);

static void geometry_set_collect_recursive_collection(const Collection &collection,
                                                      const float4x4 &transform,
                                                      Vector<GeometryInstanceGroup> &r_sets);

static void add_final_mesh_as_geometry_component(const Object &object, GeometrySet &geometry_set)
{
  Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(&const_cast<Object &>(object),
                                                                     false);

  if (mesh != nullptr) {
    BKE_mesh_wrapper_ensure_mdata(mesh);

    MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
    mesh_component.replace(mesh, GeometryOwnershipType::ReadOnly);
  }
}

GeometrySet object_get_evaluated_geometry_set(const Object &object)
{
  if (object.type == OB_MESH && object.mode == OB_MODE_EDIT) {
    GeometrySet geometry_set;
    if (object.runtime.geometry_set_eval != nullptr) {
      /* `geometry_set_eval` only contains non-mesh components, see `editbmesh_build_data`. */
      geometry_set = *object.runtime.geometry_set_eval;
    }
    add_final_mesh_as_geometry_component(object, geometry_set);
    return geometry_set;
  }
  if (object.runtime.geometry_set_eval != nullptr) {
    return *object.runtime.geometry_set_eval;
  }

  /* Otherwise, construct a new geometry set with the component based on the object type. */
  if (object.type == OB_MESH) {
    GeometrySet geometry_set;
    add_final_mesh_as_geometry_component(object, geometry_set);
    return geometry_set;
  }
  if (object.type == OB_EMPTY && object.instance_collection != nullptr) {
    GeometrySet geometry_set;
    Collection &collection = *object.instance_collection;
    InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();
    const int handle = instances.add_reference(collection);
    instances.add_instance(handle, float4x4::identity());
    return geometry_set;
  }

  /* TODO: Cover the case of point clouds without modifiers-- they may not be covered by the
   * #geometry_set_eval case above. */

  /* TODO: Add volume support. */

  /* Return by value since there is not always an existing geometry set owned elsewhere to use. */
  return {};
}

static void geometry_set_collect_recursive_collection_instance(
    const Collection &collection, const float4x4 &transform, Vector<GeometryInstanceGroup> &r_sets)
{
  float4x4 offset_matrix = float4x4::identity();
  sub_v3_v3(offset_matrix.values[3], collection.instance_offset);
  const float4x4 instance_transform = transform * offset_matrix;
  geometry_set_collect_recursive_collection(collection, instance_transform, r_sets);
}

static void geometry_set_collect_recursive_object(const Object &object,
                                                  const float4x4 &transform,
                                                  Vector<GeometryInstanceGroup> &r_sets)
{
  GeometrySet instance_geometry_set = object_get_evaluated_geometry_set(object);
  geometry_set_collect_recursive(instance_geometry_set, transform, r_sets);
}

static void geometry_set_collect_recursive_collection(const Collection &collection,
                                                      const float4x4 &transform,
                                                      Vector<GeometryInstanceGroup> &r_sets)
{
  LISTBASE_FOREACH (const CollectionObject *, collection_object, &collection.gobject) {
    BLI_assert(collection_object->ob != nullptr);
    const Object &object = *collection_object->ob;
    const float4x4 object_transform = transform * object.obmat;
    geometry_set_collect_recursive_object(object, object_transform, r_sets);
  }
  LISTBASE_FOREACH (const CollectionChild *, collection_child, &collection.children) {
    BLI_assert(collection_child->collection != nullptr);
    const Collection &collection = *collection_child->collection;
    geometry_set_collect_recursive_collection(collection, transform, r_sets);
  }
}

static void geometry_set_collect_recursive(const GeometrySet &geometry_set,
                                           const float4x4 &transform,
                                           Vector<GeometryInstanceGroup> &r_sets)
{
  r_sets.append({geometry_set, {transform}});

  if (geometry_set.has_instances()) {
    const InstancesComponent &instances_component =
        *geometry_set.get_component_for_read<InstancesComponent>();

    Span<float4x4> transforms = instances_component.instance_transforms();
    Span<int> handles = instances_component.instance_reference_handles();
    Span<InstanceReference> references = instances_component.references();
    for (const int i : transforms.index_range()) {
      const InstanceReference &reference = references[handles[i]];
      const float4x4 instance_transform = transform * transforms[i];

      switch (reference.type()) {
        case InstanceReference::Type::Object: {
          Object &object = reference.object();
          geometry_set_collect_recursive_object(object, instance_transform, r_sets);
          break;
        }
        case InstanceReference::Type::Collection: {
          Collection &collection = reference.collection();
          geometry_set_collect_recursive_collection_instance(
              collection, instance_transform, r_sets);
          break;
        }
        case InstanceReference::Type::GeometrySet: {
          const GeometrySet &geometry_set = reference.geometry_set();
          geometry_set_collect_recursive(geometry_set, instance_transform, r_sets);
          break;
        }
        case InstanceReference::Type::None: {
          break;
        }
      }
    }
  }
}

void geometry_set_gather_instances(const GeometrySet &geometry_set,
                                   Vector<GeometryInstanceGroup> &r_instance_groups)
{
  geometry_set_collect_recursive(geometry_set, float4x4::identity(), r_instance_groups);
}

void geometry_set_gather_instances_attribute_info(Span<GeometryInstanceGroup> set_groups,
                                                  Span<GeometryComponentType> component_types,
                                                  const Set<std::string> &ignored_attributes,
                                                  Map<AttributeIDRef, AttributeKind> &r_attributes)
{
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    for (const GeometryComponentType component_type : component_types) {
      if (!set.has(component_type)) {
        continue;
      }
      const GeometryComponent &component = *set.get_component_for_read(component_type);

      component.attribute_foreach(
          [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
            if (attribute_id.is_named() && ignored_attributes.contains(attribute_id.name())) {
              return true;
            }
            auto add_info = [&](AttributeKind *attribute_kind) {
              attribute_kind->domain = meta_data.domain;
              attribute_kind->data_type = meta_data.data_type;
            };
            auto modify_info = [&](AttributeKind *attribute_kind) {
              attribute_kind->domain = meta_data.domain; /* TODO: Use highest priority domain. */
              attribute_kind->data_type = bke::attribute_data_type_highest_complexity(
                  {attribute_kind->data_type, meta_data.data_type});
            };

            r_attributes.add_or_modify(attribute_id, add_info, modify_info);
            return true;
          });
    }
  }
}

}  // namespace blender::bke

void InstancesComponent::foreach_referenced_geometry(
    blender::FunctionRef<void(const GeometrySet &geometry_set)> callback) const
{
  using namespace blender::bke;
  for (const InstanceReference &reference : references_) {
    switch (reference.type()) {
      case InstanceReference::Type::Object: {
        const Object &object = reference.object();
        const GeometrySet object_geometry_set = object_get_evaluated_geometry_set(object);
        callback(object_geometry_set);
        break;
      }
      case InstanceReference::Type::Collection: {
        Collection &collection = reference.collection();
        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (&collection, object) {
          const GeometrySet object_geometry_set = object_get_evaluated_geometry_set(*object);
          callback(object_geometry_set);
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
        break;
      }
      case InstanceReference::Type::GeometrySet: {
        const GeometrySet &instance_geometry_set = reference.geometry_set();
        callback(instance_geometry_set);
        break;
      }
      case InstanceReference::Type::None: {
        break;
      }
    }
  }
}

void InstancesComponent::ensure_geometry_instances()
{
  using namespace blender;
  using namespace blender::bke;
  VectorSet<InstanceReference> new_references;
  new_references.reserve(references_.size());
  for (const InstanceReference &reference : references_) {
    switch (reference.type()) {
      case InstanceReference::Type::None:
      case InstanceReference::Type::GeometrySet: {
        /* Those references can stay as their were. */
        new_references.add_new(reference);
        break;
      }
      case InstanceReference::Type::Object: {
        /* Create a new reference that contains the geometry set of the object. We may want to
         * treat e.g. lamps and similar object types separately here. */
        const Object &object = reference.object();
        GeometrySet object_geometry_set = object_get_evaluated_geometry_set(object);
        if (object_geometry_set.has_instances()) {
          InstancesComponent &component =
              object_geometry_set.get_component_for_write<InstancesComponent>();
          component.ensure_geometry_instances();
        }
        new_references.add_new(std::move(object_geometry_set));
        break;
      }
      case InstanceReference::Type::Collection: {
        /* Create a new reference that contains a geometry set that contains all objects from the
         * collection as instances. */
        GeometrySet collection_geometry_set;
        InstancesComponent &component =
            collection_geometry_set.get_component_for_write<InstancesComponent>();
        Collection &collection = reference.collection();
        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (&collection, object) {
          const int handle = component.add_reference(*object);
          component.add_instance(handle, object->obmat);
          float4x4 &transform = component.instance_transforms().last();
          sub_v3_v3(transform.values[3], collection.instance_offset);
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
        component.ensure_geometry_instances();
        new_references.add_new(std::move(collection_geometry_set));
        break;
      }
    }
  }
  references_ = std::move(new_references);
}
