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

#include "BLI_array.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "BKE_pointcloud.h"
#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_curve_to_points_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve");
  b.add_input<decl::Int>("Count").default_value(10).min(2).max(100000);
  b.add_input<decl::Float>("Length").default_value(0.1f).min(0.001f).subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>("Points");
  b.add_output<decl::Vector>("Tangent").field_source();
  b.add_output<decl::Vector>("Normal").field_source();
  b.add_output<decl::Vector>("Rotation").field_source();
}

static void geo_node_curve_to_points_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void geo_node_curve_to_points_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveToPoints *data = (NodeGeometryCurveToPoints *)MEM_callocN(
      sizeof(NodeGeometryCurveToPoints), __func__);

  data->mode = GEO_NODE_CURVE_RESAMPLE_COUNT;
  node->storage = data;
}

static void geo_node_curve_to_points_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryCurveToPoints &node_storage = *(NodeGeometryCurveToPoints *)node->storage;
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)node_storage.mode;

  bNodeSocket *count_socket = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *length_socket = count_socket->next;

  nodeSetSocketAvailability(count_socket, mode == GEO_NODE_CURVE_RESAMPLE_COUNT);
  nodeSetSocketAvailability(length_socket, mode == GEO_NODE_CURVE_RESAMPLE_LENGTH);
}

static Array<int> calculate_spline_point_offsets(GeoNodeExecParams &params,
                                                 const GeometryNodeCurveResampleMode mode,
                                                 const CurveEval &curve,
                                                 const Span<SplinePtr> splines)
{
  const int size = curve.splines().size();
  switch (mode) {
    case GEO_NODE_CURVE_RESAMPLE_COUNT: {
      const int count = params.get_input<int>("Count");
      if (count < 1) {
        return {0};
      }
      Array<int> offsets(size + 1);
      for (const int i : offsets.index_range()) {
        offsets[i] = count * i;
      }
      return offsets;
    }
    case GEO_NODE_CURVE_RESAMPLE_LENGTH: {
      /* Don't allow asymptotic count increase for low resolution values. */
      const float resolution = std::max(params.get_input<float>("Length"), 0.0001f);
      Array<int> offsets(size + 1);
      int offset = 0;
      for (const int i : IndexRange(size)) {
        offsets[i] = offset;
        offset += splines[i]->length() / resolution + 1;
      }
      offsets.last() = offset;
      return offsets;
    }
    case GEO_NODE_CURVE_RESAMPLE_EVALUATED: {
      return curve.evaluated_point_offsets();
    }
  }
  BLI_assert_unreachable();
  return {0};
}

/**
 * \note: Relies on the fact that all attributes on point clouds are stored contiguously.
 */
static GMutableSpan ensure_point_attribute(PointCloudComponent &points,
                                           const AttributeIDRef &attribute_id,
                                           const CustomDataType data_type)
{
  points.attribute_try_create(attribute_id, ATTR_DOMAIN_POINT, data_type, AttributeInitDefault());
  WriteAttributeLookup attribute = points.attribute_try_get_for_write(attribute_id);
  BLI_assert(attribute);
  return attribute.varray->get_internal_span();
}

template<typename T>
static MutableSpan<T> ensure_point_attribute(PointCloudComponent &points,
                                             const AttributeIDRef &attribute_id)
{
  GMutableSpan attribute = ensure_point_attribute(
      points, attribute_id, bke::cpp_type_to_custom_data_type(CPPType::get<T>()));
  return attribute.typed<T>();
}

namespace {
struct AnonymousAttributeIDs {
  StrongAnonymousAttributeID tangent_id;
  StrongAnonymousAttributeID normal_id;
  StrongAnonymousAttributeID rotation_id;
};

struct ResultAttributes {
  MutableSpan<float3> positions;
  MutableSpan<float> radii;

  Map<AttributeIDRef, GMutableSpan> point_attributes;

  MutableSpan<float3> tangents;
  MutableSpan<float3> normals;
  MutableSpan<float3> rotations;
};
}  // namespace

static ResultAttributes create_attributes_for_transfer(PointCloudComponent &points,
                                                       const CurveEval &curve,
                                                       const AnonymousAttributeIDs &attributes)
{
  ResultAttributes outputs;

  outputs.positions = ensure_point_attribute<float3>(points, "position");
  outputs.radii = ensure_point_attribute<float>(points, "radius");

  if (attributes.tangent_id) {
    outputs.tangents = ensure_point_attribute<float3>(points, attributes.tangent_id.get());
  }
  if (attributes.normal_id) {
    outputs.normals = ensure_point_attribute<float3>(points, attributes.normal_id.get());
  }
  if (attributes.rotation_id) {
    outputs.rotations = ensure_point_attribute<float3>(points, attributes.rotation_id.get());
  }

  /* Because of the invariants of the curve component, we use the attributes of the first spline
   * as a representative for the attribute meta data all splines. Attributes from the spline domain
   * are handled separately. */
  curve.splines().first()->attributes.foreach_attribute(
      [&](const AttributeIDRef &id, const AttributeMetaData &meta_data) {
        if (id.should_be_kept()) {
          outputs.point_attributes.add_new(
              id, ensure_point_attribute(points, id, meta_data.data_type));
        }
        return true;
      },
      ATTR_DOMAIN_POINT);

  return outputs;
}

/**
 * TODO: For non-poly splines, this has double copies that could be avoided as part
 * of a general look at optimizing uses of #Spline::interpolate_to_evaluated.
 */
static void copy_evaluated_point_attributes(const Span<SplinePtr> splines,
                                            const Span<int> offsets,
                                            ResultAttributes &data)
{
  threading::parallel_for(splines.index_range(), 64, [&](IndexRange range) {
    for (const int i : range) {
      const Spline &spline = *splines[i];
      const int offset = offsets[i];
      const int size = offsets[i + 1] - offsets[i];

      data.positions.slice(offset, size).copy_from(spline.evaluated_positions());
      spline.interpolate_to_evaluated(spline.radii())->materialize(data.radii.slice(offset, size));

      for (const Map<AttributeIDRef, GMutableSpan>::Item item : data.point_attributes.items()) {
        const AttributeIDRef attribute_id = item.key;
        const GMutableSpan dst = item.value;

        BLI_assert(spline.attributes.get_for_read(attribute_id));
        GSpan spline_span = *spline.attributes.get_for_read(attribute_id);

        spline.interpolate_to_evaluated(spline_span)->materialize(dst.slice(offset, size).data());
      }

      if (!data.tangents.is_empty()) {
        data.tangents.slice(offset, size).copy_from(spline.evaluated_tangents());
      }
      if (!data.normals.is_empty()) {
        data.normals.slice(offset, size).copy_from(spline.evaluated_normals());
      }
    }
  });
}

static void copy_uniform_sample_point_attributes(const Span<SplinePtr> splines,
                                                 const Span<int> offsets,
                                                 ResultAttributes &data)
{
  threading::parallel_for(splines.index_range(), 64, [&](IndexRange range) {
    for (const int i : range) {
      const Spline &spline = *splines[i];
      const int offset = offsets[i];
      const int size = offsets[i + 1] - offsets[i];
      if (size == 0) {
        continue;
      }

      const Array<float> uniform_samples = spline.sample_uniform_index_factors(size);

      spline.sample_with_index_factors<float3>(
          spline.evaluated_positions(), uniform_samples, data.positions.slice(offset, size));
      spline.sample_with_index_factors<float>(*spline.interpolate_to_evaluated(spline.radii()),
                                              uniform_samples,
                                              data.radii.slice(offset, size));

      for (const Map<AttributeIDRef, GMutableSpan>::Item item : data.point_attributes.items()) {
        const AttributeIDRef attribute_id = item.key;
        const GMutableSpan dst = item.value;

        BLI_assert(spline.attributes.get_for_read(attribute_id));
        GSpan spline_span = *spline.attributes.get_for_read(attribute_id);

        spline.sample_with_index_factors(*spline.interpolate_to_evaluated(spline_span),
                                         uniform_samples,
                                         dst.slice(offset, size));
      }

      if (!data.tangents.is_empty()) {
        spline.sample_with_index_factors<float3>(
            spline.evaluated_tangents(), uniform_samples, data.tangents.slice(offset, size));
        for (float3 &tangent : data.tangents) {
          tangent.normalize();
        }
      }

      if (!data.normals.is_empty()) {
        spline.sample_with_index_factors<float3>(
            spline.evaluated_normals(), uniform_samples, data.normals.slice(offset, size));
        for (float3 &normals : data.normals) {
          normals.normalize();
        }
      }
    }
  });
}

static void copy_spline_domain_attributes(const CurveEval &curve,
                                          const Span<int> offsets,
                                          PointCloudComponent &points)
{
  curve.attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        const GSpan curve_attribute = *curve.attributes.get_for_read(attribute_id);
        const CPPType &type = curve_attribute.type();
        const GMutableSpan dst = ensure_point_attribute(points, attribute_id, meta_data.data_type);

        for (const int i : curve.splines().index_range()) {
          const int offset = offsets[i];
          const int size = offsets[i + 1] - offsets[i];
          type.fill_assign_n(curve_attribute[i], dst[offset], size);
        }

        return true;
      },
      ATTR_DOMAIN_CURVE);
}

void curve_create_default_rotation_attribute(Span<float3> tangents,
                                             Span<float3> normals,
                                             MutableSpan<float3> rotations)
{
  threading::parallel_for(IndexRange(rotations.size()), 512, [&](IndexRange range) {
    for (const int i : range) {
      rotations[i] =
          float4x4::from_normalized_axis_data({0, 0, 0}, normals[i], tangents[i]).to_euler();
    }
  });
}

static void geo_node_curve_to_points_exec(GeoNodeExecParams params)
{
  NodeGeometryCurveToPoints &node_storage = *(NodeGeometryCurveToPoints *)params.node().storage;
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)node_storage.mode;
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  AnonymousAttributeIDs attribute_outputs;
  attribute_outputs.tangent_id = StrongAnonymousAttributeID("Tangent");
  attribute_outputs.normal_id = StrongAnonymousAttributeID("Normal");
  attribute_outputs.rotation_id = StrongAnonymousAttributeID("Rotation");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curve()) {
      geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
      return;
    }
    const CurveEval &curve = *geometry_set.get_curve_for_read();
    const Span<SplinePtr> splines = curve.splines();
    curve.assert_valid_point_attributes();

    const Array<int> offsets = calculate_spline_point_offsets(params, mode, curve, splines);
    const int total_size = offsets.last();
    if (total_size == 0) {
      geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
      return;
    }

    geometry_set.replace_pointcloud(BKE_pointcloud_new_nomain(total_size));
    PointCloudComponent &points = geometry_set.get_component_for_write<PointCloudComponent>();
    ResultAttributes point_attributes = create_attributes_for_transfer(
        points, curve, attribute_outputs);

    switch (mode) {
      case GEO_NODE_CURVE_RESAMPLE_COUNT:
      case GEO_NODE_CURVE_RESAMPLE_LENGTH:
        copy_uniform_sample_point_attributes(splines, offsets, point_attributes);
        break;
      case GEO_NODE_CURVE_RESAMPLE_EVALUATED:
        copy_evaluated_point_attributes(splines, offsets, point_attributes);
        break;
    }

    copy_spline_domain_attributes(curve, offsets, points);

    if (!point_attributes.rotations.is_empty()) {
      curve_create_default_rotation_attribute(
          point_attributes.tangents, point_attributes.normals, point_attributes.rotations);
    }

    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES, GEO_COMPONENT_TYPE_POINT_CLOUD});
  });

  params.set_output("Points", std::move(geometry_set));
  if (attribute_outputs.tangent_id) {
    params.set_output(
        "Tangent",
        AnonymousAttributeFieldInput::Create<float3>(std::move(attribute_outputs.tangent_id)));
  }
  if (attribute_outputs.normal_id) {
    params.set_output(
        "Normal",
        AnonymousAttributeFieldInput::Create<float3>(std::move(attribute_outputs.normal_id)));
  }
  if (attribute_outputs.rotation_id) {
    params.set_output(
        "Rotation",
        AnonymousAttributeFieldInput::Create<float3>(std::move(attribute_outputs.rotation_id)));
  }
}

}  // namespace blender::nodes

void register_node_type_geo_curve_to_points()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_TO_POINTS, "Curve to Points", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_curve_to_points_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_to_points_exec;
  ntype.draw_buttons = blender::nodes::geo_node_curve_to_points_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveToPoints", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, blender::nodes::geo_node_curve_to_points_init);
  node_type_update(&ntype, blender::nodes::geo_node_curve_to_points_update);

  nodeRegisterType(&ntype);
}
