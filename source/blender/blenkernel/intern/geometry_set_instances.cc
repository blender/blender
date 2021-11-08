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

/**
 * \note This doesn't extract instances from the "dupli" system for non-geometry-nodes instances.
 */
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
  GeometrySet geometry_set;
  if (object.type == OB_MESH) {
    add_final_mesh_as_geometry_component(object, geometry_set);
  }

  /* TODO: Cover the case of point-clouds without modifiers-- they may not be covered by the
   * #geometry_set_eval case above. */

  /* TODO: Add volume support. */

  /* Return by value since there is not always an existing geometry set owned elsewhere to use. */
  return geometry_set;
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
  GeometrySet instance_geometry_set = object_get_evaluated_geometry_set(object);
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
void geometry_set_gather_instances(const GeometrySet &geometry_set,
                                   Vector<GeometryInstanceGroup> &r_instance_groups)
{
  float4x4 unit_transform;
  unit_m4(unit_transform.values);

  geometry_set_collect_recursive(geometry_set, unit_transform, r_instance_groups);
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

static Mesh *join_mesh_topology_and_builtin_attributes(Span<GeometryInstanceGroup> set_groups)
{
  int totverts = 0;
  int totloops = 0;
  int totedges = 0;
  int totpolys = 0;
  int64_t cd_dirty_vert = 0;
  int64_t cd_dirty_poly = 0;
  int64_t cd_dirty_edge = 0;
  int64_t cd_dirty_loop = 0;
  VectorSet<Material *> materials;

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
      for (const int slot_index : IndexRange(mesh.totcol)) {
        Material *material = mesh.mat[slot_index];
        materials.add(material);
      }
    }
  }

  /* Don't create an empty mesh. */
  if ((totverts + totloops + totedges + totpolys) == 0) {
    return nullptr;
  }

  Mesh *new_mesh = BKE_mesh_new_nomain(totverts, totedges, 0, totloops, totpolys);
  /* Copy settings from the first input geometry set with a mesh. */
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    if (set.has_mesh()) {
      const Mesh &mesh = *set.get_mesh_for_read();
      BKE_mesh_copy_parameters_for_eval(new_mesh, &mesh);
      break;
    }
  }
  for (const int i : IndexRange(materials.size())) {
    Material *material = materials[i];
    BKE_id_material_eval_assign(&new_mesh->id, i + 1, material);
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

      Array<int> material_index_map(mesh.totcol);
      for (const int i : IndexRange(mesh.totcol)) {
        Material *material = mesh.mat[i];
        const int new_material_index = materials.index_of(material);
        material_index_map[i] = new_material_index;
      }

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
          if (old_poly.mat_nr >= 0 && old_poly.mat_nr < mesh.totcol) {
            new_poly.mat_nr = material_index_map[new_poly.mat_nr];
          }
          else {
            /* The material index was invalid before. */
            new_poly.mat_nr = 0;
          }
        }

        vert_offset += mesh.totvert;
        loop_offset += mesh.totloop;
        edge_offset += mesh.totedge;
        poly_offset += mesh.totpoly;
      }
    }
  }

  /* A possible optimization is to only tag the normals dirty when there are transforms that change
   * normals. */
  BKE_mesh_normals_tag_dirty(new_mesh);

  return new_mesh;
}

static void join_attributes(Span<GeometryInstanceGroup> set_groups,
                            Span<GeometryComponentType> component_types,
                            const Map<AttributeIDRef, AttributeKind> &attribute_info,
                            GeometryComponent &result)
{
  for (Map<AttributeIDRef, AttributeKind>::Item entry : attribute_info.items()) {
    const AttributeIDRef attribute_id = entry.key;
    const AttributeDomain domain_output = entry.value.domain;
    const CustomDataType data_type_output = entry.value.data_type;
    const CPPType *cpp_type = bke::custom_data_type_to_cpp_type(data_type_output);
    BLI_assert(cpp_type != nullptr);

    result.attribute_try_create(
        entry.key, domain_output, data_type_output, AttributeInitDefault());
    WriteAttributeLookup write_attribute = result.attribute_try_get_for_write(attribute_id);
    if (!write_attribute || &write_attribute.varray->type() != cpp_type ||
        write_attribute.domain != domain_output) {
      continue;
    }

    fn::GVMutableArray_GSpan dst_span{*write_attribute.varray};

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
          GVArrayPtr source_attribute = component.attribute_try_get_for_read(
              attribute_id, domain_output, data_type_output);

          if (source_attribute) {
            fn::GVArray_GSpan src_span{*source_attribute};
            const void *src_buffer = src_span.data();
            for (const int UNUSED(i) : set_group.transforms.index_range()) {
              void *dst_buffer = dst_span[offset];
              cpp_type->copy_assign_n(src_buffer, dst_buffer, domain_size);
              offset += domain_size;
            }
          }
          else {
            offset += domain_size * set_group.transforms.size();
          }
        }
      }
    }

    dst_span.save();
  }
}

static PointCloud *join_pointcloud_position_attribute(Span<GeometryInstanceGroup> set_groups)
{
  /* Count the total number of points. */
  int totpoint = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    if (set.has<PointCloudComponent>()) {
      const PointCloudComponent &component = *set.get_component_for_read<PointCloudComponent>();
      totpoint += component.attribute_domain_size(ATTR_DOMAIN_POINT);
    }
  }
  if (totpoint == 0) {
    return nullptr;
  }

  PointCloud *new_pointcloud = BKE_pointcloud_new_nomain(totpoint);
  MutableSpan new_positions{(float3 *)new_pointcloud->co, new_pointcloud->totpoint};

  /* Transform each instance's point locations into the new point cloud. */
  int offset = 0;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    const PointCloud *pointcloud = set.get_pointcloud_for_read();
    if (pointcloud == nullptr) {
      continue;
    }
    for (const float4x4 &transform : set_group.transforms) {
      for (const int i : IndexRange(pointcloud->totpoint)) {
        new_positions[offset + i] = transform * float3(pointcloud->co[i]);
      }
      offset += pointcloud->totpoint;
    }
  }

  return new_pointcloud;
}

static CurveEval *join_curve_splines_and_builtin_attributes(Span<GeometryInstanceGroup> set_groups)
{
  Vector<SplinePtr> new_splines;
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    if (!set.has_curve()) {
      continue;
    }

    const CurveEval &source_curve = *set.get_curve_for_read();
    for (const SplinePtr &source_spline : source_curve.splines()) {
      for (const float4x4 &transform : set_group.transforms) {
        SplinePtr new_spline = source_spline->copy_without_attributes();
        new_spline->transform(transform);
        new_splines.append(std::move(new_spline));
      }
    }
  }
  if (new_splines.is_empty()) {
    return nullptr;
  }

  CurveEval *new_curve = new CurveEval();
  for (SplinePtr &new_spline : new_splines) {
    new_curve->add_spline(std::move(new_spline));
  }

  new_curve->attributes.reallocate(new_curve->splines().size());
  return new_curve;
}

static void join_instance_groups_mesh(Span<GeometryInstanceGroup> set_groups, GeometrySet &result)
{
  Mesh *new_mesh = join_mesh_topology_and_builtin_attributes(set_groups);
  if (new_mesh == nullptr) {
    return;
  }

  MeshComponent &dst_component = result.get_component_for_write<MeshComponent>();
  dst_component.replace(new_mesh);

  /* Don't copy attributes that are stored directly in the mesh data structs. */
  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set_gather_instances_attribute_info(
      set_groups,
      {GEO_COMPONENT_TYPE_MESH},
      {"position", "material_index", "normal", "shade_smooth", "crease"},
      attributes);
  join_attributes(set_groups,
                  {GEO_COMPONENT_TYPE_MESH},
                  attributes,
                  static_cast<GeometryComponent &>(dst_component));
}

static void join_instance_groups_pointcloud(Span<GeometryInstanceGroup> set_groups,
                                            GeometrySet &result)
{
  PointCloud *new_pointcloud = join_pointcloud_position_attribute(set_groups);
  if (new_pointcloud == nullptr) {
    return;
  }

  PointCloudComponent &dst_component = result.get_component_for_write<PointCloudComponent>();
  dst_component.replace(new_pointcloud);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set_gather_instances_attribute_info(
      set_groups, {GEO_COMPONENT_TYPE_POINT_CLOUD}, {"position"}, attributes);
  join_attributes(set_groups,
                  {GEO_COMPONENT_TYPE_POINT_CLOUD},
                  attributes,
                  static_cast<GeometryComponent &>(dst_component));
}

static void join_instance_groups_volume(Span<GeometryInstanceGroup> set_groups,
                                        GeometrySet &result)
{
  /* Not yet supported; for now only return the first volume. Joining volume grids with the same
   * name requires resampling of at least one of the grids. The cell size of the resulting volume
   * has to be determined somehow. */
  for (const GeometryInstanceGroup &set_group : set_groups) {
    const GeometrySet &set = set_group.geometry_set;
    if (set.has<VolumeComponent>()) {
      result.add(*set.get_component_for_read<VolumeComponent>());
      return;
    }
  }
}

/**
 * Curve point domain attributes must be in the same order on every spline. The order might have
 * been different on separate instances, so ensure that all splines have the same order. Note that
 * because #Map is used, the order is not necessarily consistent every time, but it is the same for
 * every spline, and that's what matters.
 */
static void sort_curve_point_attributes(const Map<AttributeIDRef, AttributeKind> &info,
                                        MutableSpan<SplinePtr> splines)
{
  Vector<AttributeIDRef> new_order;
  for (Map<AttributeIDRef, AttributeKind>::Item item : info.items()) {
    if (item.value.domain == ATTR_DOMAIN_POINT) {
      /* Only sort attributes stored on splines. */
      new_order.append(item.key);
    }
  }
  for (SplinePtr &spline : splines) {
    spline->attributes.reorder(new_order);
  }
}

static void join_instance_groups_curve(Span<GeometryInstanceGroup> set_groups, GeometrySet &result)
{
  CurveEval *curve = join_curve_splines_and_builtin_attributes(set_groups);
  if (curve == nullptr) {
    return;
  }

  CurveComponent &dst_component = result.get_component_for_write<CurveComponent>();
  dst_component.replace(curve);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set_gather_instances_attribute_info(
      set_groups,
      {GEO_COMPONENT_TYPE_CURVE},
      {"position", "radius", "tilt", "handle_left", "handle_right", "cyclic", "resolution"},
      attributes);
  join_attributes(set_groups,
                  {GEO_COMPONENT_TYPE_CURVE},
                  attributes,
                  static_cast<GeometryComponent &>(dst_component));
  sort_curve_point_attributes(attributes, curve->splines());
  curve->assert_valid_point_attributes();
}

GeometrySet geometry_set_realize_instances(const GeometrySet &geometry_set)
{
  if (!geometry_set.has_instances()) {
    return geometry_set;
  }

  GeometrySet new_geometry_set;

  Vector<GeometryInstanceGroup> set_groups;
  geometry_set_gather_instances(geometry_set, set_groups);
  join_instance_groups_mesh(set_groups, new_geometry_set);
  join_instance_groups_pointcloud(set_groups, new_geometry_set);
  join_instance_groups_volume(set_groups, new_geometry_set);
  join_instance_groups_curve(set_groups, new_geometry_set);

  return new_geometry_set;
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

/**
 * If references have a collection or object type, convert them into geometry instances
 * recursively. After that, the geometry sets can be edited. There may still be instances of other
 * types of they can't be converted to geometry sets.
 */
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
