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
#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_pointcloud.h"

#include "DNA_collection_types.h"
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

void gather_attribute_info(Map<std::string, AttributeKind> &attributes,
                           Span<GeometryComponentType> component_types,
                           Span<GeometryInstanceGroup> set_groups,
                           const Set<std::string> &ignored_attributes)
{
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    for (const GeometryComponentType component_type : component_types) {
      if (!set.has(component_type)) {
        continue;
      }
      const GeometryComponent &component = *set.get_component_for_read(component_type);

      component.attribute_foreach([&](StringRefNull name, const AttributeMetaData &meta_data) {
        if (ignored_attributes.contains(name)) {
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

        attributes.add_or_modify(name, add_info, modify_info);
        return true;
      });
    }
  }
}

static Mesh *join_mesh_topology_and_builtin_attributes(Span<GeometryInstanceGroup> set_groups,
                                                       const bool convert_points_to_vertices)
{
  int totverts = 0;
  int totloops = 0;
  int totedges = 0;
  int totpolys = 0;
  int64_t cd_dirty_vert = 0;
  int64_t cd_dirty_poly = 0;
  int64_t cd_dirty_edge = 0;
  int64_t cd_dirty_loop = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    const int tot_transforms = set_group.transforms.size();
    if (set.has_mesh()) {
      const Mesh &mesh = *set.get_mesh_for_read();
      totverts += mesh.totvert * tot_transforms;
      totloops += mesh.totloop * tot_transforms;
      totedges += mesh.totedge * tot_transforms;
      totpolys += mesh.totpoly * tot_transforms;
      cd_dirty_vert |= mesh.runtime.cd_dirty_vert;
      cd_dirty_poly |= mesh.runtime.cd_dirty_poly;
      cd_dirty_edge |= mesh.runtime.cd_dirty_edge;
      cd_dirty_loop |= mesh.runtime.cd_dirty_loop;
    }
    if (convert_points_to_vertices && set.has_pointcloud()) {
      const PointCloud &pointcloud = *set.get_pointcloud_for_read();
      totverts += pointcloud.totpoint * tot_transforms;
    }
  }

  Mesh *new_mesh = BKE_mesh_new_nomain(totverts, totedges, 0, totloops, totpolys);
  /* Copy settings from the first input geometry set with a mesh. */
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    if (set.has_mesh()) {
      const Mesh &mesh = *set.get_mesh_for_read();
      BKE_mesh_copy_settings(new_mesh, &mesh);
      break;
    }
  }
  new_mesh->runtime.cd_dirty_vert = cd_dirty_vert;
  new_mesh->runtime.cd_dirty_poly = cd_dirty_poly;
  new_mesh->runtime.cd_dirty_edge = cd_dirty_edge;
  new_mesh->runtime.cd_dirty_loop = cd_dirty_loop;

  int vert_offset = 0;
  int loop_offset = 0;
  int edge_offset = 0;
  int poly_offset = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    if (set.has_mesh()) {
      const Mesh &mesh = *set.get_mesh_for_read();
      for (const float4x4 &transform : set_group.transforms) {
        for (const int i : IndexRange(mesh.totvert)) {
          const MVert &old_vert = mesh.mvert[i];
          MVert &new_vert = new_mesh->mvert[vert_offset + i];

          new_vert = old_vert;

          const float3 new_position = transform * float3(old_vert.co);
          copy_v3_v3(new_vert.co, new_position);
        }
        for (const int i : IndexRange(mesh.totedge)) {
          const MEdge &old_edge = mesh.medge[i];
          MEdge &new_edge = new_mesh->medge[edge_offset + i];
          new_edge = old_edge;
          new_edge.v1 += vert_offset;
          new_edge.v2 += vert_offset;
        }
        for (const int i : IndexRange(mesh.totloop)) {
          const MLoop &old_loop = mesh.mloop[i];
          MLoop &new_loop = new_mesh->mloop[loop_offset + i];
          new_loop = old_loop;
          new_loop.v += vert_offset;
          new_loop.e += edge_offset;
        }
        for (const int i : IndexRange(mesh.totpoly)) {
          const MPoly &old_poly = mesh.mpoly[i];
          MPoly &new_poly = new_mesh->mpoly[poly_offset + i];
          new_poly = old_poly;
          new_poly.loopstart += loop_offset;
        }

        vert_offset += mesh.totvert;
        loop_offset += mesh.totloop;
        edge_offset += mesh.totedge;
        poly_offset += mesh.totpoly;
      }
    }
    if (convert_points_to_vertices && set.has_pointcloud()) {
      const PointCloud &pointcloud = *set.get_pointcloud_for_read();
      for (const float4x4 &transform : set_group.transforms) {
        for (const int i : IndexRange(pointcloud.totpoint)) {
          MVert &new_vert = new_mesh->mvert[vert_offset + i];
          const float3 old_position = pointcloud.co[i];
          const float3 new_position = transform * old_position;
          copy_v3_v3(new_vert.co, new_position);
        }
        vert_offset += pointcloud.totpoint;
      }
    }
  }

  return new_mesh;
}

static void join_attributes(Span<GeometryInstanceGroup> set_groups,
                            Span<GeometryComponentType> component_types,
                            const Map<std::string, AttributeKind> &attribute_info,
                            GeometryComponent &result)
{
  for (Map<std::string, AttributeKind>::Item entry : attribute_info.items()) {
    StringRef name = entry.key;
    const AttributeDomain domain_output = entry.value.domain;
    const CustomDataType data_type_output = entry.value.data_type;
    const CPPType *cpp_type = bke::custom_data_type_to_cpp_type(data_type_output);
    BLI_assert(cpp_type != nullptr);

    result.attribute_try_create(entry.key, domain_output, data_type_output);
    WriteAttributePtr write_attribute = result.attribute_try_get_for_write(name);
    if (!write_attribute || &write_attribute->cpp_type() != cpp_type ||
        write_attribute->domain() != domain_output) {
      continue;
    }
    fn::GMutableSpan dst_span = write_attribute->get_span_for_write_only();

    int offset = 0;
    for (const GeometryInstanceGroup &set_group : set_groups) {
      const GeometrySet &set = set_group.geometry_set;
      for (const GeometryComponentType component_type : component_types) {
        if (set.has(component_type)) {
          const GeometryComponent &component = *set.get_component_for_read(component_type);
          const int domain_size = component.attribute_domain_size(domain_output);
          if (domain_size == 0) {
            continue; /* Domain size is 0, so no need to increment the offset. */
          }
          ReadAttributePtr source_attribute = component.attribute_try_get_for_read(
              name, domain_output, data_type_output);

          if (source_attribute) {
            fn::GSpan src_span = source_attribute->get_span();
            const void *src_buffer = src_span.data();
            for (const int UNUSED(i) : set_group.transforms.index_range()) {
              void *dst_buffer = dst_span[offset];
              cpp_type->copy_to_initialized_n(src_buffer, dst_buffer, domain_size);
              offset += domain_size;
            }
          }
          else {
            offset += domain_size * set_group.transforms.size();
          }
        }
      }
    }

    write_attribute->apply_span();
  }
}

static void join_instance_groups_mesh(Span<GeometryInstanceGroup> set_groups,
                                      bool convert_points_to_vertices,
                                      GeometrySet &result)
{
  Mesh *new_mesh = join_mesh_topology_and_builtin_attributes(set_groups,
                                                             convert_points_to_vertices);

  MeshComponent &dst_component = result.get_component_for_write<MeshComponent>();
  dst_component.replace(new_mesh);

  Vector<GeometryComponentType> component_types;
  component_types.append(GEO_COMPONENT_TYPE_MESH);
  if (convert_points_to_vertices) {
    component_types.append(GEO_COMPONENT_TYPE_POINT_CLOUD);
  }

  /* Don't copy attributes that are stored directly in the mesh data structs. */
  Map<std::string, AttributeKind> attributes;
  gather_attribute_info(attributes,
                        component_types,
                        set_groups,
                        {"position", "material_index", "vertex_normal", "shade_smooth"});
  join_attributes(
      set_groups, component_types, attributes, static_cast<GeometryComponent &>(dst_component));
}

static void join_instance_groups_pointcloud(Span<GeometryInstanceGroup> set_groups,
                                            GeometrySet &result)
{
  int totpoint = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    if (set.has<PointCloudComponent>()) {
      const PointCloudComponent &component = *set.get_component_for_read<PointCloudComponent>();
      totpoint += component.attribute_domain_size(ATTR_DOMAIN_POINT);
    }
  }

  PointCloudComponent &dst_component = result.get_component_for_write<PointCloudComponent>();
  PointCloud *pointcloud = BKE_pointcloud_new_nomain(totpoint);
  dst_component.replace(pointcloud);
  Map<std::string, AttributeKind> attributes;
  gather_attribute_info(attributes, {GEO_COMPONENT_TYPE_POINT_CLOUD}, set_groups, {});
  join_attributes(set_groups,
                  {GEO_COMPONENT_TYPE_POINT_CLOUD},
                  attributes,
                  static_cast<GeometryComponent &>(dst_component));
}

static void join_instance_groups_volume(Span<GeometryInstanceGroup> set_groups,
                                        GeometrySet &result)
{
  /* Not yet supported. Joining volume grids with the same name requires resampling of at least
   * one of the grids. The cell size of the resulting volume has to be determined somehow. */
  VolumeComponent &dst_component = result.get_component_for_write<VolumeComponent>();
  UNUSED_VARS(set_groups, dst_component);
}

GeometrySet geometry_set_realize_mesh_for_modifier(const GeometrySet &geometry_set)
{
  if (!geometry_set.has_instances() && !geometry_set.has_pointcloud()) {
    return geometry_set;
  }

  GeometrySet new_geometry_set = geometry_set;
  Vector<GeometryInstanceGroup> set_groups = geometry_set_gather_instances(geometry_set);
  join_instance_groups_mesh(set_groups, true, new_geometry_set);
  /* Remove all instances, even though some might contain other non-mesh data. We can't really
   * keep only non-mesh instances in general. */
  new_geometry_set.remove<InstancesComponent>();
  /* If there was a point cloud, it is now part of the mesh. */
  new_geometry_set.remove<PointCloudComponent>();
  return new_geometry_set;
}

GeometrySet geometry_set_realize_instances(const GeometrySet &geometry_set)
{
  if (!geometry_set.has_instances()) {
    return geometry_set;
  }

  GeometrySet new_geometry_set;

  Vector<GeometryInstanceGroup> set_groups = geometry_set_gather_instances(geometry_set);
  join_instance_groups_mesh(set_groups, false, new_geometry_set);
  join_instance_groups_pointcloud(set_groups, new_geometry_set);
  join_instance_groups_volume(set_groups, new_geometry_set);

  return new_geometry_set;
}

}  // namespace blender::bke
