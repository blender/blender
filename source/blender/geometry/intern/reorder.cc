/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_multi_value_map.hh"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "GEO_reorder.hh"

namespace blender::geometry {

const MultiValueMap<bke::GeometryComponent::Type, bke::AttrDomain> &
components_supported_reordering()
{
  using namespace bke;
  const static MultiValueMap<GeometryComponent::Type, AttrDomain> supported_types_and_domains =
      []() {
        MultiValueMap<GeometryComponent::Type, AttrDomain> supported_types_and_domains;
        supported_types_and_domains.add_multiple(
            GeometryComponent::Type::Mesh,
            {AttrDomain::Point, AttrDomain::Edge, AttrDomain::Face});
        supported_types_and_domains.add(GeometryComponent::Type::Curve, AttrDomain::Curve);
        supported_types_and_domains.add(GeometryComponent::Type::PointCloud, AttrDomain::Point);
        supported_types_and_domains.add(GeometryComponent::Type::Instance, AttrDomain::Instance);
        return supported_types_and_domains;
      }();
  return supported_types_and_domains;
}

static void reorder_attributes_group_to_group(const bke::AttributeAccessor src_attributes,
                                              const bke::AttrDomain domain,
                                              const OffsetIndices<int> src_offsets,
                                              const OffsetIndices<int> dst_offsets,
                                              const Span<int> old_by_new_map,
                                              bke::MutableAttributeAccessor dst_attributes)
{
  src_attributes.for_all(
      [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
        if (meta_data.domain != domain) {
          return true;
        }
        if (meta_data.data_type == CD_PROP_STRING) {
          return true;
        }
        const GVArray src = *src_attributes.lookup(id, domain);
        bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
            id, domain, meta_data.data_type);
        if (!dst) {
          return true;
        }

        threading::parallel_for(old_by_new_map.index_range(), 1024, [&](const IndexRange range) {
          for (const int new_i : range) {
            const int old_i = old_by_new_map[new_i];
            array_utils::copy(src.slice(src_offsets[old_i]), dst.span.slice(dst_offsets[new_i]));
          }
        });

        dst.finish();
        return true;
      });
}

static Array<int> invert_permutation(const Span<int> permutation)
{
  Array<int> data(permutation.size());
  threading::parallel_for(permutation.index_range(), 2048, [&](const IndexRange range) {
    for (const int64_t i : range) {
      data[permutation[i]] = i;
    }
  });
  return data;
}

static void reorder_mesh_verts_exec(const Mesh &src_mesh,
                                    const Span<int> old_by_new_map,
                                    Mesh &dst_mesh)
{
  bke::gather_attributes(src_mesh.attributes(),
                         bke::AttrDomain::Point,
                         {},
                         {},
                         old_by_new_map,
                         dst_mesh.attributes_for_write());
  const Array<int> new_by_old_map = invert_permutation(old_by_new_map);
  array_utils::gather(new_by_old_map.as_span(),
                      dst_mesh.edges().cast<int>(),
                      dst_mesh.edges_for_write().cast<int>());
  array_utils::gather(
      new_by_old_map.as_span(), dst_mesh.corner_verts(), dst_mesh.corner_verts_for_write());
}

static void reorder_mesh_edges_exec(const Mesh &src_mesh,
                                    const Span<int> old_by_new_map,
                                    Mesh &dst_mesh)
{
  bke::gather_attributes(src_mesh.attributes(),
                         bke::AttrDomain::Edge,
                         {},
                         {},
                         old_by_new_map,
                         dst_mesh.attributes_for_write());
  const Array<int> new_by_old_map = invert_permutation(old_by_new_map);
  array_utils::gather(
      new_by_old_map.as_span(), dst_mesh.corner_edges(), dst_mesh.corner_edges_for_write());
}

static void reorder_mesh_faces_exec(const Mesh &src_mesh,
                                    const Span<int> old_by_new_map,
                                    Mesh &dst_mesh)
{
  bke::gather_attributes(src_mesh.attributes(),
                         bke::AttrDomain::Face,
                         {},
                         {},
                         old_by_new_map,
                         dst_mesh.attributes_for_write());
  const Span<int> old_offsets = src_mesh.face_offsets();
  MutableSpan<int> new_offsets = dst_mesh.face_offsets_for_write();
  offset_indices::gather_group_sizes(old_offsets, old_by_new_map, new_offsets);
  offset_indices::accumulate_counts_to_offsets(new_offsets);
  reorder_attributes_group_to_group(src_mesh.attributes(),
                                    bke::AttrDomain::Corner,
                                    old_offsets,
                                    new_offsets.as_span(),
                                    old_by_new_map,
                                    dst_mesh.attributes_for_write());
}

static void reorder_mesh_exec(const Mesh &src_mesh,
                              const Span<int> old_by_new_map,
                              const bke::AttrDomain domain,
                              Mesh &dst_mesh)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      reorder_mesh_verts_exec(src_mesh, old_by_new_map, dst_mesh);
      break;
    case bke::AttrDomain::Edge:
      reorder_mesh_edges_exec(src_mesh, old_by_new_map, dst_mesh);
      break;
    case bke::AttrDomain::Face:
      reorder_mesh_faces_exec(src_mesh, old_by_new_map, dst_mesh);
      break;
    default:
      break;
  }
  dst_mesh.tag_positions_changed();
  dst_mesh.tag_topology_changed();
}

static void reorder_points_exec(const PointCloud &src_pointcloud,
                                const Span<int> old_by_new_map,
                                PointCloud &dst_pointcloud)
{
  bke::gather_attributes(src_pointcloud.attributes(),
                         bke::AttrDomain::Point,
                         {},
                         {},
                         old_by_new_map,
                         dst_pointcloud.attributes_for_write());
  dst_pointcloud.tag_positions_changed();
  dst_pointcloud.tag_radii_changed();
}

static void reorder_curves_exec(const bke::CurvesGeometry &src_curves,
                                const Span<int> old_by_new_map,
                                bke::CurvesGeometry &dst_curves)
{
  bke::gather_attributes(src_curves.attributes(),
                         bke::AttrDomain::Curve,
                         {},
                         {},
                         old_by_new_map,
                         dst_curves.attributes_for_write());

  const Span<int> old_offsets = src_curves.offsets();
  MutableSpan<int> new_offsets = dst_curves.offsets_for_write();
  offset_indices::gather_group_sizes(old_offsets, old_by_new_map, new_offsets);
  offset_indices::accumulate_counts_to_offsets(new_offsets);

  reorder_attributes_group_to_group(src_curves.attributes(),
                                    bke::AttrDomain::Point,
                                    old_offsets,
                                    new_offsets.as_span(),
                                    old_by_new_map,
                                    dst_curves.attributes_for_write());
  dst_curves.tag_topology_changed();
}

static void reorder_instaces_exec(const bke::Instances &src_instances,
                                  const Span<int> old_by_new_map,
                                  bke::Instances &dst_instances)
{
  bke::gather_attributes(src_instances.attributes(),
                         bke::AttrDomain::Instance,
                         {},
                         {},
                         old_by_new_map,
                         dst_instances.attributes_for_write());
}

static void clean_unused_attributes(const bke::AnonymousAttributePropagationInfo &propagation_info,
                                    bke::MutableAttributeAccessor attributes)
{
  Vector<std::string> unused_ids;
  attributes.for_all([&](const bke::AttributeIDRef &id, const bke::AttributeMetaData meta_data) {
    if (!id.is_anonymous()) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }
    if (propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    unused_ids.append(id.name());
    return true;
  });

  for (const std::string &unused_id : unused_ids) {
    attributes.remove(unused_id);
  }
}

Mesh *reorder_mesh(const Mesh &src_mesh,
                   Span<int> old_by_new_map,
                   bke::AttrDomain domain,
                   const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  Mesh *dst_mesh = BKE_mesh_copy_for_eval(src_mesh);
  clean_unused_attributes(propagation_info, dst_mesh->attributes_for_write());
  reorder_mesh_exec(src_mesh, old_by_new_map, domain, *dst_mesh);
  return dst_mesh;
}

PointCloud *reorder_points(const PointCloud &src_pointcloud,
                           Span<int> old_by_new_map,
                           const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  PointCloud *dst_pointcloud = BKE_pointcloud_copy_for_eval(&src_pointcloud);
  clean_unused_attributes(propagation_info, dst_pointcloud->attributes_for_write());
  reorder_points_exec(src_pointcloud, old_by_new_map, *dst_pointcloud);
  return dst_pointcloud;
}

bke::CurvesGeometry reorder_curves_geometry(
    const bke::CurvesGeometry &src_curves,
    Span<int> old_by_new_map,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  bke::CurvesGeometry dst_curves = bke::CurvesGeometry(src_curves);
  clean_unused_attributes(propagation_info, dst_curves.attributes_for_write());
  reorder_curves_exec(src_curves, old_by_new_map, dst_curves);
  return dst_curves;
}

Curves *reorder_curves(const Curves &src_curves,
                       Span<int> old_by_new_map,
                       const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const bke::CurvesGeometry src_curve_geometry = src_curves.geometry.wrap();
  Curves *dst_curves = BKE_curves_copy_for_eval(&src_curves);
  dst_curves->geometry.wrap() = reorder_curves_geometry(
      src_curve_geometry, old_by_new_map, propagation_info);
  return dst_curves;
}

bke::Instances *reorder_instaces(const bke::Instances &src_instances,
                                 Span<int> old_by_new_map,
                                 const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  bke::Instances *dst_instances = new bke::Instances(src_instances);
  clean_unused_attributes(propagation_info, dst_instances->attributes_for_write());
  reorder_instaces_exec(src_instances, old_by_new_map, *dst_instances);
  return dst_instances;
}

bke::GeometryComponentPtr reordered_component(
    const bke::GeometryComponent &src_component,
    const Span<int> old_by_new_map,
    const bke::AttrDomain domain,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  BLI_assert(!src_component.is_empty());

  if (const bke::MeshComponent *src_mesh_component = dynamic_cast<const bke::MeshComponent *>(
          &src_component))
  {
    Mesh *result_mesh = reorder_mesh(
        *src_mesh_component->get(), old_by_new_map, domain, propagation_info);
    return bke::GeometryComponentPtr(new bke::MeshComponent(result_mesh));
  }
  else if (const bke::PointCloudComponent *src_points_component =
               dynamic_cast<const bke::PointCloudComponent *>(&src_component))
  {
    PointCloud *result_point_cloud = reorder_points(
        *src_points_component->get(), old_by_new_map, propagation_info);
    return bke::GeometryComponentPtr(new bke::PointCloudComponent(result_point_cloud));
  }
  else if (const bke::CurveComponent *src_curves_component =
               dynamic_cast<const bke::CurveComponent *>(&src_component))
  {
    Curves *result_curves = reorder_curves(
        *src_curves_component->get(), old_by_new_map, propagation_info);
    return bke::GeometryComponentPtr(new bke::CurveComponent(result_curves));
  }
  else if (const bke::InstancesComponent *src_instances_component =
               dynamic_cast<const bke::InstancesComponent *>(&src_component))
  {
    bke::Instances *result_instances = reorder_instaces(
        *src_instances_component->get(), old_by_new_map, propagation_info);
    return bke::GeometryComponentPtr(new bke::InstancesComponent(result_instances));
  }

  BLI_assert_unreachable();
  return {};
}

}  // namespace blender::geometry
