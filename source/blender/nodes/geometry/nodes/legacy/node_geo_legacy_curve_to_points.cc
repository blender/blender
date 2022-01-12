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

static GMutableSpan create_attribute_and_retrieve_span(PointCloudComponent &points,
                                                       const AttributeIDRef &attribute_id,
                                                       const CustomDataType data_type)
{
  points.attribute_try_create(attribute_id, ATTR_DOMAIN_POINT, data_type, AttributeInitDefault());
  WriteAttributeLookup attribute = points.attribute_try_get_for_write(attribute_id);
  BLI_assert(attribute);
  return attribute.varray.get_internal_span();
}

template<typename T>
static MutableSpan<T> create_attribute_and_retrieve_span(PointCloudComponent &points,
                                                         const AttributeIDRef &attribute_id)
{
  GMutableSpan attribute = create_attribute_and_retrieve_span(
      points, attribute_id, bke::cpp_type_to_custom_data_type(CPPType::get<T>()));
  return attribute.typed<T>();
}

CurveToPointsResults curve_to_points_create_result_attributes(PointCloudComponent &points,
                                                              const CurveEval &curve)
{
  CurveToPointsResults attributes;

  attributes.result_size = points.attribute_domain_size(ATTR_DOMAIN_POINT);

  attributes.positions = create_attribute_and_retrieve_span<float3>(points, "position");
  attributes.radii = create_attribute_and_retrieve_span<float>(points, "radius");
  attributes.tilts = create_attribute_and_retrieve_span<float>(points, "tilt");

  /* Because of the invariants of the curve component, we use the attributes of the
   * first spline as a representative for the attribute meta data all splines. */
  curve.splines().first()->attributes.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        attributes.point_attributes.add_new(
            attribute_id,
            create_attribute_and_retrieve_span(points, attribute_id, meta_data.data_type));
        return true;
      },
      ATTR_DOMAIN_POINT);

  attributes.tangents = create_attribute_and_retrieve_span<float3>(points, "tangent");
  attributes.normals = create_attribute_and_retrieve_span<float3>(points, "normal");
  attributes.rotations = create_attribute_and_retrieve_span<float3>(points, "rotation");

  return attributes;
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_legacy_curve_to_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Int>(N_("Count")).default_value(10).min(2).max(100000);
  b.add_input<decl::Float>(N_("Length")).default_value(0.1f).min(0.001f).subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveToPoints *data = MEM_cnew<NodeGeometryCurveToPoints>(__func__);

  data->mode = GEO_NODE_CURVE_RESAMPLE_COUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  NodeGeometryCurveToPoints &node_storage = *(NodeGeometryCurveToPoints *)node->storage;
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)node_storage.mode;

  bNodeSocket *count_socket = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *length_socket = count_socket->next;

  nodeSetSocketAvailability(ntree, count_socket, mode == GEO_NODE_CURVE_RESAMPLE_COUNT);
  nodeSetSocketAvailability(ntree, length_socket, mode == GEO_NODE_CURVE_RESAMPLE_LENGTH);
}

/**
 * Evaluate splines in parallel to speed up the rest of the node's execution.
 */
static void evaluate_splines(Span<SplinePtr> splines)
{
  threading::parallel_for_each(splines, [](const SplinePtr &spline) {
    /* These functions fill the corresponding caches on each spline. */
    spline->evaluated_positions();
    spline->evaluated_tangents();
    spline->evaluated_normals();
    spline->evaluated_lengths();
  });
}

static Array<int> calculate_spline_point_offsets(GeoNodeExecParams &params,
                                                 const GeometryNodeCurveResampleMode mode,
                                                 const CurveEval &curve,
                                                 const Span<SplinePtr> splines)
{
  const int size = curve.splines().size();
  switch (mode) {
    case GEO_NODE_CURVE_RESAMPLE_COUNT: {
      const int count = params.extract_input<int>("Count");
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
      const float resolution = std::max(params.extract_input<float>("Length"), 0.0001f);
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
 * TODO: For non-poly splines, this has double copies that could be avoided as part
 * of a general look at optimizing uses of #Spline::interpolate_to_evaluated.
 */
static void copy_evaluated_point_attributes(Span<SplinePtr> splines,
                                            Span<int> offsets,
                                            CurveToPointsResults &data)
{
  threading::parallel_for(splines.index_range(), 64, [&](IndexRange range) {
    for (const int i : range) {
      const Spline &spline = *splines[i];
      const int offset = offsets[i];
      const int size = offsets[i + 1] - offsets[i];

      data.positions.slice(offset, size).copy_from(spline.evaluated_positions());
      spline.interpolate_to_evaluated(spline.radii()).materialize(data.radii.slice(offset, size));
      spline.interpolate_to_evaluated(spline.tilts()).materialize(data.tilts.slice(offset, size));

      for (const Map<AttributeIDRef, GMutableSpan>::Item item : data.point_attributes.items()) {
        const AttributeIDRef attribute_id = item.key;
        GMutableSpan point_span = item.value;

        BLI_assert(spline.attributes.get_for_read(attribute_id));
        GSpan spline_span = *spline.attributes.get_for_read(attribute_id);

        spline.interpolate_to_evaluated(spline_span)
            .materialize(point_span.slice(offset, size).data());
      }

      data.tangents.slice(offset, size).copy_from(spline.evaluated_tangents());
      data.normals.slice(offset, size).copy_from(spline.evaluated_normals());
    }
  });
}

static void copy_uniform_sample_point_attributes(Span<SplinePtr> splines,
                                                 Span<int> offsets,
                                                 CurveToPointsResults &data)
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

      spline.sample_with_index_factors<float>(spline.interpolate_to_evaluated(spline.radii()),
                                              uniform_samples,
                                              data.radii.slice(offset, size));

      spline.sample_with_index_factors<float>(spline.interpolate_to_evaluated(spline.tilts()),
                                              uniform_samples,
                                              data.tilts.slice(offset, size));

      for (const Map<AttributeIDRef, GMutableSpan>::Item item : data.point_attributes.items()) {
        const AttributeIDRef attribute_id = item.key;
        GMutableSpan point_span = item.value;

        BLI_assert(spline.attributes.get_for_read(attribute_id));
        GSpan spline_span = *spline.attributes.get_for_read(attribute_id);

        spline.sample_with_index_factors(spline.interpolate_to_evaluated(spline_span),
                                         uniform_samples,
                                         point_span.slice(offset, size));
      }

      spline.sample_with_index_factors<float3>(
          spline.evaluated_tangents(), uniform_samples, data.tangents.slice(offset, size));
      for (float3 &tangent : data.tangents) {
        tangent.normalize();
      }

      spline.sample_with_index_factors<float3>(
          spline.evaluated_normals(), uniform_samples, data.normals.slice(offset, size));
      for (float3 &normals : data.normals) {
        normals.normalize();
      }
    }
  });
}

/**
 * \note Use attributes from the curve component rather than the attribute data directly on the
 * attribute storage to allow reading the virtual spline attributes like "cyclic" and "resolution".
 */
static void copy_spline_domain_attributes(const CurveComponent &curve_component,
                                          Span<int> offsets,
                                          PointCloudComponent &points)
{
  curve_component.attribute_foreach(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        if (meta_data.domain != ATTR_DOMAIN_CURVE) {
          return true;
        }
        GVArray spline_attribute = curve_component.attribute_get_for_read(
            attribute_id, ATTR_DOMAIN_CURVE, meta_data.data_type);
        const CPPType &type = spline_attribute.type();

        OutputAttribute result_attribute = points.attribute_try_get_for_output_only(
            attribute_id, ATTR_DOMAIN_POINT, meta_data.data_type);
        GMutableSpan result = result_attribute.as_span();

        for (const int i : spline_attribute.index_range()) {
          const int offset = offsets[i];
          const int size = offsets[i + 1] - offsets[i];
          if (size != 0) {
            BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
            spline_attribute.get(i, buffer);
            type.fill_assign_n(buffer, result[offset], size);
          }
        }

        result_attribute.save();
        return true;
      });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  NodeGeometryCurveToPoints &node_storage = *(NodeGeometryCurveToPoints *)params.node().storage;
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)node_storage.mode;
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  if (!geometry_set.has_curve()) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  const CurveComponent &curve_component = *geometry_set.get_component_for_read<CurveComponent>();
  const CurveEval &curve = *curve_component.get_for_read();
  const Span<SplinePtr> splines = curve.splines();
  curve.assert_valid_point_attributes();

  evaluate_splines(splines);

  const Array<int> offsets = calculate_spline_point_offsets(params, mode, curve, splines);
  const int total_size = offsets.last();
  if (total_size == 0) {
    params.set_output("Geometry", GeometrySet());
    return;
  }

  GeometrySet result = GeometrySet::create_with_pointcloud(BKE_pointcloud_new_nomain(total_size));
  PointCloudComponent &point_component = result.get_component_for_write<PointCloudComponent>();

  CurveToPointsResults new_attributes = curve_to_points_create_result_attributes(point_component,
                                                                                 curve);
  switch (mode) {
    case GEO_NODE_CURVE_RESAMPLE_COUNT:
    case GEO_NODE_CURVE_RESAMPLE_LENGTH:
      copy_uniform_sample_point_attributes(splines, offsets, new_attributes);
      break;
    case GEO_NODE_CURVE_RESAMPLE_EVALUATED:
      copy_evaluated_point_attributes(splines, offsets, new_attributes);
      break;
  }

  copy_spline_domain_attributes(curve_component, offsets, point_component);
  curve_create_default_rotation_attribute(
      new_attributes.tangents, new_attributes.normals, new_attributes.rotations);

  /* The default radius is way too large for points, divide by 10. */
  for (float &radius : new_attributes.radii) {
    radius *= 0.1f;
  }

  params.set_output("Geometry", std::move(result));
}

}  // namespace blender::nodes::node_geo_legacy_curve_to_points_cc

void register_node_type_geo_legacy_curve_to_points()
{
  namespace file_ns = blender::nodes::node_geo_legacy_curve_to_points_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_CURVE_TO_POINTS, "Curve to Points", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveToPoints", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);

  nodeRegisterType(&ntype);
}
