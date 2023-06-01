/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_collection.h"
#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"

#include "DNA_collection_types.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"

namespace blender::bke {

static void add_final_mesh_as_geometry_component(const Object &object, GeometrySet &geometry_set)
{
  Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(
      &const_cast<Object &>(object));

  if (mesh != nullptr) {
    BKE_mesh_wrapper_ensure_mdata(mesh);
    geometry_set.replace_mesh(mesh, GeometryOwnershipType::ReadOnly);
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
    GeometrySet geometry_set = *object.runtime.geometry_set_eval;
    /* Ensure that subdivision is performed on the CPU. */
    if (geometry_set.has_mesh()) {
      add_final_mesh_as_geometry_component(object, geometry_set);
    }
    return geometry_set;
  }

  /* Otherwise, construct a new geometry set with the component based on the object type. */
  if (object.type == OB_MESH) {
    GeometrySet geometry_set;
    add_final_mesh_as_geometry_component(object, geometry_set);
    return geometry_set;
  }
  if (object.type == OB_EMPTY && object.instance_collection != nullptr) {
    Collection &collection = *object.instance_collection;
    std::unique_ptr<Instances> instances = std::make_unique<Instances>();
    const int handle = instances->add_reference(collection);
    instances->add_instance(handle, float4x4::identity());
    return GeometrySet::create_with_instances(instances.release());
  }

  /* Return by value since there is not always an existing geometry set owned elsewhere to use. */
  return {};
}

void Instances::foreach_referenced_geometry(
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

void Instances::ensure_geometry_instances()
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
          object_geometry_set.get_instances_for_write()->ensure_geometry_instances();
        }
        new_references.add_new(std::move(object_geometry_set));
        break;
      }
      case InstanceReference::Type::Collection: {
        /* Create a new reference that contains a geometry set that contains all objects from the
         * collection as instances. */
        std::unique_ptr<Instances> instances = std::make_unique<Instances>();
        Collection &collection = reference.collection();
        FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (&collection, object) {
          const int handle = instances->add_reference(*object);
          instances->add_instance(handle, float4x4(object->object_to_world));
          float4x4 &transform = instances->transforms().last();
          transform.location() -= collection.instance_offset;
        }
        FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
        instances->ensure_geometry_instances();
        new_references.add_new(GeometrySet::create_with_instances(instances.release()));
        break;
      }
    }
  }
  references_ = std::move(new_references);
}

}  // namespace blender::bke
