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

#include "NOD_type_conversions.hh"

#include "node_geometry_util.hh"

using blender::fn::GVArray_For_GSpan;

namespace blender::nodes {

static void geo_node_join_geometry_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry")).multi_input();
  b.add_output<decl::Geometry>(N_("Geometry"));
}

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
  BKE_mesh_copy_parameters_for_eval(new_mesh, first_input_mesh);

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

static Map<AttributeIDRef, AttributeMetaData> get_final_attribute_info(
    Span<const GeometryComponent *> components, Span<StringRef> ignored_attributes)
{
  Map<AttributeIDRef, AttributeMetaData> info;

  for (const GeometryComponent *component : components) {
    component->attribute_foreach(
        [&](const bke::AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
          if (attribute_id.is_named() && ignored_attributes.contains(attribute_id.name())) {
            return true;
          }
          info.add_or_modify(
              attribute_id,
              [&](AttributeMetaData *meta_data_final) { *meta_data_final = meta_data; },
              [&](AttributeMetaData *meta_data_final) {
                meta_data_final->data_type = blender::bke::attribute_data_type_highest_complexity(
                    {meta_data_final->data_type, meta_data.data_type});
                meta_data_final->domain = blender::bke::attribute_domain_highest_priority(
                    {meta_data_final->domain, meta_data.domain});
              });
          return true;
        });
  }

  return info;
}

static void fill_new_attribute(Span<const GeometryComponent *> src_components,
                               const AttributeIDRef &attribute_id,
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
        attribute_id, domain, data_type, nullptr);

    GVArray_GSpan src_span{*read_attribute};
    const void *src_buffer = src_span.data();
    void *dst_buffer = dst_span[offset];
    cpp_type->copy_assign_n(src_buffer, dst_buffer, domain_size);

    offset += domain_size;
  }
}

static void join_attributes(Span<const GeometryComponent *> src_components,
                            GeometryComponent &result,
                            Span<StringRef> ignored_attributes = {})
{
  const Map<AttributeIDRef, AttributeMetaData> info = get_final_attribute_info(src_components,
                                                                               ignored_attributes);

  for (const Map<AttributeIDRef, AttributeMetaData>::Item item : info.items()) {
    const AttributeIDRef attribute_id = item.key;
    const AttributeMetaData &meta_data = item.value;

    OutputAttribute write_attribute = result.attribute_try_get_for_output_only(
        attribute_id, meta_data.domain, meta_data.data_type);
    if (!write_attribute) {
      continue;
    }
    GMutableSpan dst_span = write_attribute.as_span();
    fill_new_attribute(
        src_components, attribute_id, meta_data.data_type, meta_data.domain, dst_span);
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
    Span<int> src_reference_handles = src_component->instance_reference_handles();

    for (const int i : src_transforms.index_range()) {
      const int src_handle = src_reference_handles[i];
      const int dst_handle = handle_map[src_handle];
      const float4x4 &transform = src_transforms[i];
      dst_component.add_instance(dst_handle, transform);
    }
  }
  join_attributes(to_base_components(src_components), dst_component, {"position"});
}

static void join_components(Span<const VolumeComponent *> src_components, GeometrySet &result)
{
  /* Not yet supported. Joining volume grids with the same name requires resampling of at least one
   * of the grids. The cell size of the resulting volume has to be determined somehow. */
  VolumeComponent &dst_component = result.get_component_for_write<VolumeComponent>();
  UNUSED_VARS(src_components, dst_component);
}

/**
 * \note This takes advantage of the fact that creating attributes on joined curves never
 * changes a point attribute into a spline attribute; it is always the other way around.
 */
static void ensure_control_point_attribute(const AttributeIDRef &attribute_id,
                                           const CustomDataType data_type,
                                           Span<CurveComponent *> src_components,
                                           CurveEval &result)
{
  MutableSpan<SplinePtr> splines = result.splines();
  const CPPType &type = *bke::custom_data_type_to_cpp_type(data_type);

  /* In order to fill point attributes with spline domain attribute values where necessary, keep
   * track of the curve each spline came from while iterating over the splines in the result. */
  int src_component_index = 0;
  int spline_index_in_component = 0;
  const CurveEval *current_curve = src_components[src_component_index]->get_for_read();

  for (SplinePtr &spline : splines) {
    std::optional<GSpan> attribute = spline->attributes.get_for_read(attribute_id);

    if (attribute) {
      if (attribute->type() != type) {
        /* In this case, the attribute exists, but it has the wrong type. So create a buffer
         * for the converted values, do the conversion, and then replace the attribute. */
        void *converted_buffer = MEM_mallocN_aligned(
            spline->size() * type.size(), type.alignment(), __func__);

        const DataTypeConversions &conversions = blender::nodes::get_implicit_type_conversions();
        conversions.try_convert(std::make_unique<GVArray_For_GSpan>(*attribute), type)
            ->materialize(converted_buffer);

        spline->attributes.remove(attribute_id);
        spline->attributes.create_by_move(attribute_id, data_type, converted_buffer);
      }
    }
    else {
      spline->attributes.create(attribute_id, data_type);

      if (current_curve->attributes.get_for_read(attribute_id)) {
        /* In this case the attribute did not exist, but there is a spline domain attribute
         * we can retrieve a value from, as a spline to point domain conversion. So fill the
         * new attribute with the value for this spline. */
        GVArrayPtr current_curve_attribute = current_curve->attributes.get_for_read(
            attribute_id, data_type, nullptr);

        BLI_assert(spline->attributes.get_for_read(attribute_id));
        std::optional<GMutableSpan> new_attribute = spline->attributes.get_for_write(attribute_id);

        BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
        current_curve_attribute->get(spline_index_in_component, buffer);
        type.fill_assign_n(buffer, new_attribute->data(), new_attribute->size());
      }
    }

    /* Move to the next spline and maybe the next input component. */
    spline_index_in_component++;
    if (spline != splines.last() && spline_index_in_component >= current_curve->splines().size()) {
      src_component_index++;
      spline_index_in_component = 0;

      current_curve = src_components[src_component_index]->get_for_read();
    }
  }
}

/**
 * Curve point domain attributes must be in the same order on every spline. The order might have
 * been different on separate instances, so ensure that all splines have the same order. Note that
 * because #Map is used, the order is not necessarily consistent every time, but it is the same for
 * every spline, and that's what matters.
 */
static void sort_curve_point_attributes(const Map<AttributeIDRef, AttributeMetaData> &info,
                                        MutableSpan<SplinePtr> splines)
{
  Vector<AttributeIDRef> new_order;
  for (Map<AttributeIDRef, AttributeMetaData>::Item item : info.items()) {
    if (item.value.domain == ATTR_DOMAIN_POINT) {
      /* Only sort attributes stored on splines. */
      new_order.append(item.key);
    }
  }
  for (SplinePtr &spline : splines) {
    spline->attributes.reorder(new_order);
  }
}

/**
 * Fill data for an attribute on the new curve based on all source curves.
 */
static void ensure_spline_attribute(const AttributeIDRef &attribute_id,
                                    const CustomDataType data_type,
                                    Span<CurveComponent *> src_components,
                                    CurveEval &result)
{
  const CPPType &type = *bke::custom_data_type_to_cpp_type(data_type);

  result.attributes.create(attribute_id, data_type);
  GMutableSpan result_attribute = *result.attributes.get_for_write(attribute_id);

  int offset = 0;
  for (const CurveComponent *component : src_components) {
    const CurveEval &curve = *component->get_for_read();
    const int size = curve.splines().size();
    if (size == 0) {
      continue;
    }
    GVArrayPtr read_attribute = curve.attributes.get_for_read(attribute_id, data_type, nullptr);
    GVArray_GSpan src_span{*read_attribute};

    const void *src_buffer = src_span.data();
    type.copy_assign_n(src_buffer, result_attribute[offset], size);

    offset += size;
  }
}

/**
 * Special handling for copying spline attributes. This is necessary because we move the splines
 * out of the source components instead of copying them, meaning we can no longer access point
 * domain attributes on the source components.
 *
 * \warning Splines have been moved out of the source components at this point, so it
 * is important to only read curve-level data (spline domain attributes) from them.
 */
static void join_curve_attributes(const Map<AttributeIDRef, AttributeMetaData> &info,
                                  Span<CurveComponent *> src_components,
                                  CurveEval &result)
{
  for (const Map<AttributeIDRef, AttributeMetaData>::Item item : info.items()) {
    const AttributeIDRef attribute_id = item.key;
    const AttributeMetaData meta_data = item.value;

    if (meta_data.domain == ATTR_DOMAIN_CURVE) {
      ensure_spline_attribute(attribute_id, meta_data.data_type, src_components, result);
    }
    else {
      ensure_control_point_attribute(attribute_id, meta_data.data_type, src_components, result);
    }
  }

  sort_curve_point_attributes(info, result.splines());
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

  /* Retrieve attribute info before moving the splines out of the input components. */
  const Map<AttributeIDRef, AttributeMetaData> info = get_final_attribute_info(
      {(const GeometryComponent **)src_components.data(), src_components.size()},
      {"position", "radius", "tilt", "handle_left", "handle_right", "cyclic", "resolution"});

  CurveComponent &dst_component = result.get_component_for_write<CurveComponent>();
  CurveEval *dst_curve = new CurveEval();
  for (CurveComponent *component : src_components) {
    CurveEval *src_curve = component->get_for_write();
    for (SplinePtr &spline : src_curve->splines()) {
      dst_curve->add_spline(std::move(spline));
    }
  }
  dst_curve->attributes.reallocate(dst_curve->splines().size());

  join_curve_attributes(info, src_components, *dst_curve);
  dst_curve->assert_valid_point_attributes();

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
  ntype.geometry_node_execute = blender::nodes::geo_node_join_geometry_exec;
  ntype.declare = blender::nodes::geo_node_join_geometry_declare;
  nodeRegisterType(&ntype);
}
