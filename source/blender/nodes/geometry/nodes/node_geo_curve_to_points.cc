/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_math_matrix.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "DNA_pointcloud_types.h"

#include "BKE_pointcloud.h"

#include "GEO_resample_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_to_points_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveToPoints)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Int>("Count")
      .default_value(10)
      .min(2)
      .field_on_all()
      .max(100000)
      .make_available(
          [](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_RESAMPLE_COUNT; });
  b.add_input<decl::Float>("Length")
      .default_value(0.1f)
      .min(0.001f)
      .subtype(PROP_DISTANCE)
      .make_available(
          [](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_RESAMPLE_LENGTH; });
  b.add_output<decl::Geometry>("Points").propagate_all();
  b.add_output<decl::Vector>("Tangent").field_on_all();
  b.add_output<decl::Vector>("Normal").field_on_all();
  b.add_output<decl::Vector>("Rotation").field_on_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveToPoints *data = MEM_cnew<NodeGeometryCurveToPoints>(__func__);

  data->mode = GEO_NODE_CURVE_RESAMPLE_COUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveToPoints &storage = node_storage(*node);
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  bNodeSocket *count_socket = static_cast<bNodeSocket *>(node->inputs.first)->next;
  bNodeSocket *length_socket = count_socket->next;

  bke::nodeSetSocketAvailability(ntree, count_socket, mode == GEO_NODE_CURVE_RESAMPLE_COUNT);
  bke::nodeSetSocketAvailability(ntree, length_socket, mode == GEO_NODE_CURVE_RESAMPLE_LENGTH);
}

static void fill_rotation_attribute(const Span<float3> tangents,
                                    const Span<float3> normals,
                                    MutableSpan<float3> rotations)
{
  threading::parallel_for(IndexRange(rotations.size()), 512, [&](IndexRange range) {
    for (const int i : range) {
      rotations[i] = float3(
          math::to_euler(math::from_orthonormal_axes<float4x4>(normals[i], tangents[i])));
    }
  });
}

static void copy_curve_domain_attributes(const AttributeAccessor curve_attributes,
                                         MutableAttributeAccessor point_attributes)
{
  curve_attributes.for_all(
      [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData &meta_data) {
        if (curve_attributes.is_builtin(id)) {
          return true;
        }
        if (meta_data.domain != ATTR_DOMAIN_CURVE) {
          return true;
        }
        point_attributes.add(
            id,
            ATTR_DOMAIN_POINT,
            meta_data.data_type,
            bke::AttributeInitVArray(*curve_attributes.lookup(id, ATTR_DOMAIN_POINT)));
        return true;
      });
}

static PointCloud *pointcloud_from_curves(bke::CurvesGeometry curves,
                                          const AttributeIDRef &tangent_id,
                                          const AttributeIDRef &normal_id,
                                          const AttributeIDRef &rotation_id)
{
  PointCloud *pointcloud = BKE_pointcloud_new_nomain(0);
  pointcloud->totpoint = curves.points_num();

  if (rotation_id) {
    MutableAttributeAccessor attributes = curves.attributes_for_write();
    const VArraySpan tangents = *attributes.lookup<float3>(tangent_id, ATTR_DOMAIN_POINT);
    const VArraySpan normals = *attributes.lookup<float3>(normal_id, ATTR_DOMAIN_POINT);
    SpanAttributeWriter<float3> rotations = attributes.lookup_or_add_for_write_only_span<float3>(
        rotation_id, ATTR_DOMAIN_POINT);
    fill_rotation_attribute(tangents, normals, rotations.span);
    rotations.finish();
  }

  /* Move the curve point custom data to the pointcloud, to avoid any copying. */
  CustomData_free(&pointcloud->pdata, pointcloud->totpoint);
  pointcloud->pdata = curves.point_data;
  CustomData_reset(&curves.point_data);

  copy_curve_domain_attributes(curves.attributes(), pointcloud->attributes_for_write());

  return pointcloud;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveToPoints &storage = node_storage(params.node());
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(geometry_set);

  AnonymousAttributeIDPtr rotation_anonymous_id =
      params.get_output_anonymous_attribute_id_if_needed("Rotation");
  const bool need_tangent_and_normal = bool(rotation_anonymous_id);
  AnonymousAttributeIDPtr tangent_anonymous_id =
      params.get_output_anonymous_attribute_id_if_needed("Tangent", need_tangent_and_normal);
  AnonymousAttributeIDPtr normal_anonymous_id = params.get_output_anonymous_attribute_id_if_needed(
      "Normal", need_tangent_and_normal);

  geometry::ResampleCurvesOutputAttributeIDs resample_attributes;
  resample_attributes.tangent_id = tangent_anonymous_id.get();
  resample_attributes.normal_id = normal_anonymous_id.get();

  switch (mode) {
    case GEO_NODE_CURVE_RESAMPLE_COUNT: {
      Field<int> count = params.extract_input<Field<int>>("Count");
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves_for_read()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          bke::CurvesGeometry dst_curves = geometry::resample_to_count(
              src_curves, fn::make_constant_field<bool>(true), count, resample_attributes);
          PointCloud *pointcloud = pointcloud_from_curves(std::move(dst_curves),
                                                          resample_attributes.tangent_id,
                                                          resample_attributes.normal_id,
                                                          rotation_anonymous_id.get());
          geometry.remove_geometry_during_modify();
          geometry.replace_pointcloud(pointcloud);
        }
      });
      break;
    }
    case GEO_NODE_CURVE_RESAMPLE_LENGTH: {
      Field<float> length = params.extract_input<Field<float>>("Length");
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves_for_read()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          bke::CurvesGeometry dst_curves = geometry::resample_to_length(
              src_curves, fn::make_constant_field<bool>(true), length, resample_attributes);
          PointCloud *pointcloud = pointcloud_from_curves(std::move(dst_curves),
                                                          resample_attributes.tangent_id,
                                                          resample_attributes.normal_id,
                                                          rotation_anonymous_id.get());
          geometry.remove_geometry_during_modify();
          geometry.replace_pointcloud(pointcloud);
        }
      });
      break;
    }
    case GEO_NODE_CURVE_RESAMPLE_EVALUATED:
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves_for_read()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          bke::CurvesGeometry dst_curves = geometry::resample_to_evaluated(
              src_curves, fn::make_constant_field<bool>(true), resample_attributes);
          PointCloud *pointcloud = pointcloud_from_curves(std::move(dst_curves),
                                                          resample_attributes.tangent_id,
                                                          resample_attributes.normal_id,
                                                          rotation_anonymous_id.get());
          geometry.remove_geometry_during_modify();
          geometry.replace_pointcloud(pointcloud);
        }
      });
      break;
  }

  params.set_output("Points", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_to_points_cc

void register_node_type_geo_curve_to_points()
{
  namespace file_ns = blender::nodes::node_geo_curve_to_points_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_TO_POINTS, "Curve to Points", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveToPoints", node_free_standard_storage, node_copy_standard_storage);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  nodeRegisterType(&ntype);
}
