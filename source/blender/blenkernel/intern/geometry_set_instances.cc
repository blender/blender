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

#include "BKE_geometry_set_instances.hh"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"

namespace blender::bke {

static void geometry_set_collect_recursive(const GeometrySet &geometry_set,
                                           const float4x4 &transform,
                                           Vector<GeometryInstanceGroup> &r_sets);

static void geometry_set_collect_recursive_collection(const Collection &collection,
                                                      const float4x4 &transform,
                                                      Vector<GeometryInstanceGroup> &r_sets);

/**
 * \note This doesn't extract instances from the "dupli" system for non-geometry-nodes instances.
 */
static GeometrySet object_get_geometry_set_for_read(const Object &object)
{
  /* Objects evaluated with a nodes modifier will have a geometry set already. */
  if (object.runtime.geometry_set_eval != nullptr) {
    return *object.runtime.geometry_set_eval;
  }

  /* Otherwise, construct a new geometry set with the component based on the object type. */
  GeometrySet new_geometry_set;

  if (object.type == OB_MESH) {
    Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(
        &const_cast<Object &>(object), false);

    if (mesh != nullptr) {
      BKE_mesh_wrapper_ensure_mdata(mesh);

      MeshComponent &mesh_component = new_geometry_set.get_component_for_write<MeshComponent>();
      mesh_component.replace(mesh, GeometryOwnershipType::ReadOnly);
      mesh_component.copy_vertex_group_names_from_object(object);
    }
  }

  /* TODO: Cover the case of point-clouds without modifiers-- they may not be covered by the
   * #geometry_set_eval case above. */

  /* TODO: Add volume support. */

  /* Return by value since there is not always an existing geometry set owned elsewhere to use. */
  return new_geometry_set;
}

static void geometry_set_collect_recursive_collection_instance(
    const Collection &collection, const float4x4 &transform, Vector<GeometryInstanceGroup> &r_sets)
{
  float4x4 offset_matrix;
  unit_m4(offset_matrix.values);
  sub_v3_v3(offset_matrix.values[3], collection.instance_offset);
  const float4x4 instance_transform = transform * offset_matrix;
  geometry_set_collect_recursive_collection(collection, instance_transform, r_sets);
}

static void geometry_set_collect_recursive_object(const Object &object,
                                                  const float4x4 &transform,
                                                  Vector<GeometryInstanceGroup> &r_sets)
{
  GeometrySet instance_geometry_set = object_get_geometry_set_for_read(object);
  geometry_set_collect_recursive(instance_geometry_set, transform, r_sets);

  if (object.type == OB_EMPTY) {
    const Collection *collection_instance = object.instance_collection;
    if (collection_instance != nullptr) {
      geometry_set_collect_recursive_collection_instance(*collection_instance, transform, r_sets);
    }
  }
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

    Span<float4x4> transforms = instances_component.transforms();
    Span<InstancedData> instances = instances_component.instanced_data();
    for (const int i : instances.index_range()) {
      const InstancedData &data = instances[i];
      const float4x4 instance_transform = transform * transforms[i];

      if (data.type == INSTANCE_DATA_TYPE_OBJECT) {
        BLI_assert(data.data.object != nullptr);
        const Object &object = *data.data.object;
        geometry_set_collect_recursive_object(object, instance_transform, r_sets);
      }
      else if (data.type == INSTANCE_DATA_TYPE_COLLECTION) {
        BLI_assert(data.data.collection != nullptr);
        const Collection &collection = *data.data.collection;
        geometry_set_collect_recursive_collection_instance(collection, instance_transform, r_sets);
      }
    }
  }
}

/**
 * Return flattened vector of the geometry component's recursive instances. I.e. all collection
 * instances and object instances will be expanded into the instances of their geometry components.
 * Even the instances in those geometry components' will be included.
 *
 * \note For convenience (to avoid duplication in the caller), the returned vector also contains
 * the argument geometry set.
 *
 * \note This doesn't extract instances from the "dupli" system for non-geometry-nodes instances.
 */
Vector<GeometryInstanceGroup> geometry_set_gather_instances(const GeometrySet &geometry_set)
{
  Vector<GeometryInstanceGroup> result_vector;

  float4x4 unit_transform;
  unit_m4(unit_transform.values);

  geometry_set_collect_recursive(geometry_set, unit_transform, result_vector);

  return result_vector;
}

}  // namespace blender::bke
