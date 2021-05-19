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

#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_pointcloud.h"
#include "BKE_spline.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_join_geometry_in[] = {
    {SOCK_GEOMETRY,
     N_("Geometry"),
     0.0f,
     0.0f,
     0.0f,
     1.0f,
     -1.0f,
     1.0f,
     PROP_NONE,
     SOCK_MULTI_INPUT},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_join_geometry_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static Mesh *join_mesh_topology_and_builtin_attributes(Span<const MeshComponent *> src_components)
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

  for (const MeshComponent *mesh_component : src_components) {
    const Mesh *mesh = mesh_component->get_for_read();
    totverts += mesh->totvert;
    totloops += mesh->totloop;
    totedges += mesh->totedge;
    totpolys += mesh->totpoly;
    cd_dirty_vert |= mesh->runtime.cd_dirty_vert;
    cd_dirty_poly |= mesh->runtime.cd_dirty_poly;
    cd_dirty_edge |= mesh->runtime.cd_dirty_edge;
    cd_dirty_loop |= mesh->runtime.cd_dirty_loop;

    for (const int slot_index : IndexRange(mesh->totcol)) {
      Material *material = mesh->mat[slot_index];
      materials.add(material);
    }
  }

  const Mesh *first_input_mesh = src_components[0]->get_for_read();
  Mesh *new_mesh = BKE_mesh_new_nomain(totverts, totedges, 0, totloops, totpolys);
  BKE_mesh_copy_settings(new_mesh, first_input_mesh);

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
  for (const MeshComponent *mesh_component : src_components) {
    const Mesh *mesh = mesh_component->get_for_read();
    if (mesh == nullptr) {
      continue;
    }

    Array<int> material_index_map(mesh->totcol);
    for (const int i : IndexRange(mesh->totcol)) {
      Material *material = mesh->mat[i];
      const int new_material_index = materials.index_of(material);
      material_index_map[i] = new_material_index;
    }

    for (const int i : IndexRange(mesh->totvert)) {
      const MVert &old_vert = mesh->mvert[i];
      MVert &new_vert = new_mesh->mvert[vert_offset + i];
      new_vert = old_vert;
    }

    for (const int i : IndexRange(mesh->totedge)) {
      const MEdge &old_edge = mesh->medge[i];
      MEdge &new_edge = new_mesh->medge[edge_offset + i];
      new_edge = old_edge;
      new_edge.v1 += vert_offset;
      new_edge.v2 += vert_offset;
    }
    for (const int i : IndexRange(mesh->totloop)) {
      const MLoop &old_loop = mesh->mloop[i];
      MLoop &new_loop = new_mesh->mloop[loop_offset + i];
      new_loop = old_loop;
      new_loop.v += vert_offset;
      new_loop.e += edge_offset;
    }
    for (const int i : IndexRange(mesh->totpoly)) {
      const MPoly &old_poly = mesh->mpoly[i];
      MPoly &new_poly = new_mesh->mpoly[poly_offset + i];
      new_poly = old_poly;
      new_poly.loopstart += loop_offset;
      if (old_poly.mat_nr >= 0 && old_poly.mat_nr < mesh->totcol) {
        new_poly.mat_nr = material_index_map[new_poly.mat_nr];
      }
      else {
        /* The material index was invalid before. */
        new_poly.mat_nr = 0;
      }
    }

    vert_offset += mesh->totvert;
    loop_offset += mesh->totloop;
    edge_offset += mesh->totedge;
    poly_offset += mesh->totpoly;
  }

  return new_mesh;
}

template<typename Component>
static Array<const GeometryComponent *> to_base_components(Span<const Component *> components)
{
  return components;
}

static Set<std::string> find_all_attribute_names(Span<const GeometryComponent *> components)
{
  Set<std::string> attribute_names;
  for (const GeometryComponent *component : components) {
    Set<std::string> names = component->attribute_names();
    for (const std::string &name : names) {
      attribute_names.add(name);
    }
  }
  return attribute_names;
}

static void determine_final_data_type_and_domain(Span<const GeometryComponent *> components,
                                                 StringRef attribute_name,
                                                 CustomDataType *r_type,
                                                 AttributeDomain *r_domain)
{
  Vector<CustomDataType> data_types;
  Vector<AttributeDomain> domains;
  for (const GeometryComponent *component : components) {
    ReadAttributeLookup attribute = component->attribute_try_get_for_read(attribute_name);
    if (attribute) {
      data_types.append(bke::cpp_type_to_custom_data_type(attribute.varray->type()));
      domains.append(attribute.domain);
    }
  }

  *r_type = bke::attribute_data_type_highest_complexity(data_types);
  *r_domain = bke::attribute_domain_highest_priority(domains);
}

static void fill_new_attribute(Span<const GeometryComponent *> src_components,
                               StringRef attribute_name,
                               const CustomDataType data_type,
                               const AttributeDomain domain,
                               GMutableSpan dst_span)
{
  const CPPType *cpp_type = bke::custom_data_type_to_cpp_type(data_type);
  BLI_assert(cpp_type != nullptr);

  int offset = 0;
  for (const GeometryComponent *component : src_components) {
    const int domain_size = component->attribute_domain_size(domain);
    if (domain_size == 0) {
      continue;
    }
    GVArrayPtr read_attribute = component->attribute_get_for_read(
        attribute_name, domain, data_type, nullptr);

    GVArray_GSpan src_span{*read_attribute};
    const void *src_buffer = src_span.data();
    void *dst_buffer = dst_span[offset];
    cpp_type->copy_to_initialized_n(src_buffer, dst_buffer, domain_size);

    offset += domain_size;
  }
}

static void join_attributes(Span<const GeometryComponent *> src_components,
                            GeometryComponent &result,
                            Span<StringRef> ignored_attributes = {})
{
  Set<std::string> attribute_names = find_all_attribute_names(src_components);
  for (StringRef name : ignored_attributes) {
    attribute_names.remove(name);
  }

  for (const std::string &attribute_name : attribute_names) {
    CustomDataType data_type;
    AttributeDomain domain;
    determine_final_data_type_and_domain(src_components, attribute_name, &data_type, &domain);

    OutputAttribute write_attribute = result.attribute_try_get_for_output_only(
        attribute_name, domain, data_type);
    if (!write_attribute) {
      continue;
    }
    GMutableSpan dst_span = write_attribute.as_span();
    fill_new_attribute(src_components, attribute_name, data_type, domain, dst_span);
    write_attribute.save();
  }
}

static void join_components(Span<const MeshComponent *> src_components, GeometrySet &result)
{
  Mesh *new_mesh = join_mesh_topology_and_builtin_attributes(src_components);

  MeshComponent &dst_component = result.get_component_for_write<MeshComponent>();
  dst_component.replace(new_mesh);

  /* Don't copy attributes that are stored directly in the mesh data structs. */
  join_attributes(to_base_components(src_components),
                  dst_component,
                  {"position", "material_index", "normal", "shade_smooth", "crease"});
}

static void join_components(Span<const PointCloudComponent *> src_components, GeometrySet &result)
{
  int totpoints = 0;
  for (const PointCloudComponent *pointcloud_component : src_components) {
    totpoints += pointcloud_component->attribute_domain_size(ATTR_DOMAIN_POINT);
  }

  PointCloudComponent &dst_component = result.get_component_for_write<PointCloudComponent>();
  PointCloud *pointcloud = BKE_pointcloud_new_nomain(totpoints);
  dst_component.replace(pointcloud);

  join_attributes(to_base_components(src_components), dst_component);
}

static void join_components(Span<const InstancesComponent *> src_components, GeometrySet &result)
{
  InstancesComponent &dst_component = result.get_component_for_write<InstancesComponent>();

  int tot_instances = 0;
  for (const InstancesComponent *src_component : src_components) {
    tot_instances += src_component->instances_amount();
  }
  dst_component.reserve(tot_instances);

  for (const InstancesComponent *src_component : src_components) {
    Span<InstanceReference> src_references = src_component->references();
    Array<int> handle_map(src_references.size());
    for (const int src_handle : src_references.index_range()) {
      handle_map[src_handle] = dst_component.add_reference(src_references[src_handle]);
    }

    Span<float4x4> src_transforms = src_component->instance_transforms();
    Span<int> src_ids = src_component->instance_ids();
    Span<int> src_reference_handles = src_component->instance_reference_handles();

    for (const int i : src_transforms.index_range()) {
      const int src_handle = src_reference_handles[i];
      const int dst_handle = handle_map[src_handle];
      const float4x4 &transform = src_transforms[i];
      const int id = src_ids[i];
      dst_component.add_instance(dst_handle, transform, id);
    }
  }
}

static void join_components(Span<const VolumeComponent *> src_components, GeometrySet &result)
{
  /* Not yet supported. Joining volume grids with the same name requires resampling of at least one
   * of the grids. The cell size of the resulting volume has to be determined somehow. */
  VolumeComponent &dst_component = result.get_component_for_write<VolumeComponent>();
  UNUSED_VARS(src_components, dst_component);
}

static void join_curve_components(MutableSpan<GeometrySet> src_geometry_sets, GeometrySet &result)
{
  Vector<CurveComponent *> src_components;
  for (GeometrySet &geometry_set : src_geometry_sets) {
    if (geometry_set.has_curve()) {
      /* Retrieving with write access seems counterintuitive, but it can allow avoiding a copy
       * in the case where the input spline has no other users, because the splines can be
       * moved from the source curve rather than copied from a read-only source. Retrieving
       * the curve for write will make a copy only when it has a user elsewhere. */
      CurveComponent &component = geometry_set.get_component_for_write<CurveComponent>();
      src_components.append(&component);
    }
  }

  if (src_components.size() == 0) {
    return;
  }
  if (src_components.size() == 1) {
    result.add(*src_components[0]);
    return;
  }

  CurveComponent &dst_component = result.get_component_for_write<CurveComponent>();
  CurveEval *dst_curve = new CurveEval();
  for (CurveComponent *component : src_components) {
    CurveEval *src_curve = component->get_for_write();
    for (SplinePtr &spline : src_curve->splines()) {
      dst_curve->add_spline(std::move(spline));
    }
  }

  /* For now, remove all custom attributes, since they might have different types,
   * or an attribute might not exist on all splines. */
  dst_curve->attributes.reallocate(dst_curve->splines().size());
  CustomData_reset(&dst_curve->attributes.data);
  for (SplinePtr &spline : dst_curve->splines()) {
    CustomData_reset(&spline->attributes.data);
  }

  dst_component.replace(dst_curve);
}

template<typename Component>
static void join_component_type(Span<GeometrySet> src_geometry_sets, GeometrySet &result)
{
  Vector<const Component *> components;
  for (const GeometrySet &geometry_set : src_geometry_sets) {
    const Component *component = geometry_set.get_component_for_read<Component>();
    if (component != nullptr && !component->is_empty()) {
      components.append(component);
    }
  }

  if (components.size() == 0) {
    return;
  }
  if (components.size() == 1) {
    result.add(*components[0]);
    return;
  }
  join_components(components, result);
}

static void geo_node_join_geometry_exec(GeoNodeExecParams params)
{
  Vector<GeometrySet> geometry_sets = params.extract_multi_input<GeometrySet>("Geometry");

  GeometrySet geometry_set_result;
  join_component_type<MeshComponent>(geometry_sets, geometry_set_result);
  join_component_type<PointCloudComponent>(geometry_sets, geometry_set_result);
  join_component_type<InstancesComponent>(geometry_sets, geometry_set_result);
  join_component_type<VolumeComponent>(geometry_sets, geometry_set_result);
  join_curve_components(geometry_sets, geometry_set_result);

  params.set_output("Geometry", std::move(geometry_set_result));
}
}  // namespace blender::nodes

void register_node_type_geo_join_geometry()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_JOIN_GEOMETRY, "Join Geometry", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_join_geometry_in, geo_node_join_geometry_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_join_geometry_exec;
  nodeRegisterType(&ntype);
}
