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
#include "BKE_type_conversions.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "GEO_realize_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_join_geometry_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry")).multi_input();
  b.add_output<decl::Geometry>(N_("Geometry"));
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
    GVArray read_attribute = component->attribute_get_for_read(
        attribute_id, domain, data_type, nullptr);

    GVArray_GSpan src_span{read_attribute};
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

  GeometrySet instances_geometry_set;
  InstancesComponent &instances =
      instances_geometry_set.get_component_for_write<InstancesComponent>();

  if constexpr (is_same_any_v<Component, InstancesComponent, VolumeComponent>) {
    join_components(components, result);
  }
  else {
    for (const Component *component : components) {
      GeometrySet tmp_geo;
      tmp_geo.add(*component);
      const int handle = instances.add_reference(InstanceReference{tmp_geo});
      instances.add_instance(handle, float4x4::identity());
    }

    geometry::RealizeInstancesOptions options;
    options.keep_original_ids = true;
    options.realize_instance_attributes = false;
    GeometrySet joined_components = geometry::realize_instances(instances_geometry_set, options);
    result.add(joined_components.get_component_for_write<Component>());
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Vector<GeometrySet> geometry_sets = params.extract_multi_input<GeometrySet>("Geometry");

  GeometrySet geometry_set_result;
  join_component_type<MeshComponent>(geometry_sets, geometry_set_result);
  join_component_type<PointCloudComponent>(geometry_sets, geometry_set_result);
  join_component_type<InstancesComponent>(geometry_sets, geometry_set_result);
  join_component_type<VolumeComponent>(geometry_sets, geometry_set_result);
  join_component_type<CurveComponent>(geometry_sets, geometry_set_result);

  params.set_output("Geometry", std::move(geometry_set_result));
}
}  // namespace blender::nodes::node_geo_join_geometry_cc

void register_node_type_geo_join_geometry()
{
  namespace file_ns = blender::nodes::node_geo_join_geometry_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_JOIN_GEOMETRY, "Join Geometry", NODE_CLASS_GEOMETRY, 0);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
