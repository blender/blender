/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_index_mask_ops.hh"
#include "BLI_length_parameterize.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_resample_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveResample)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).supports_field().hide_value();
  b.add_input<decl::Int>(N_("Count")).default_value(10).min(1).max(100000).supports_field();
  b.add_input<decl::Float>(N_("Length"))
      .default_value(0.1f)
      .min(0.01f)
      .supports_field()
      .subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveResample *data = MEM_cnew<NodeGeometryCurveResample>(__func__);

  data->mode = GEO_NODE_CURVE_RESAMPLE_COUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveResample &storage = node_storage(*node);
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  bNodeSocket *count_socket = ((bNodeSocket *)node->inputs.first)->next->next;
  bNodeSocket *length_socket = count_socket->next;

  nodeSetSocketAvailability(ntree, count_socket, mode == GEO_NODE_CURVE_RESAMPLE_COUNT);
  nodeSetSocketAvailability(ntree, length_socket, mode == GEO_NODE_CURVE_RESAMPLE_LENGTH);
}

/** Returns the number of evaluated points in each curve. Used to deselect curves with none. */
class EvaluatedCountFieldInput final : public GeometryFieldInput {
 public:
  EvaluatedCountFieldInput() : GeometryFieldInput(CPPType::get<int>(), "Evaluated Point Count")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_CURVE && domain == ATTR_DOMAIN_CURVE &&
        !component.is_empty()) {
      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      const Curves &curves_id = *curve_component.get_for_read();
      const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);
      curves.ensure_evaluated_offsets();
      return VArray<int>::ForFunc(curves.curves_num(), [&](const int64_t index) -> int {
        return curves.evaluated_points_for_curve(index).size();
      });
    }
    return {};
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 234905872379865;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const EvaluatedCountFieldInput *>(&other) != nullptr;
  }
};

/**
 * Return true if the attribute should be copied/interpolated to the result curves.
 * Don't output attributes that correspond to curve types that have no curves in the result.
 */
static bool interpolate_attribute_to_curves(const AttributeIDRef &attribute_id,
                                            const std::array<int, CURVE_TYPES_NUM> &type_counts)
{
  if (!attribute_id.is_named()) {
    return true;
  }
  if (ELEM(attribute_id.name(),
           "handle_type_left",
           "handle_type_right",
           "handle_left",
           "handle_right")) {
    return type_counts[CURVE_TYPE_BEZIER] != 0;
  }
  if (ELEM(attribute_id.name(), "nurbs_weight")) {
    return type_counts[CURVE_TYPE_NURBS] != 0;
  }
  return true;
}

/**
 * Return true if the attribute should be copied to poly curves.
 */
static bool interpolate_attribute_to_poly_curve(const AttributeIDRef &attribute_id)
{
  static const Set<StringRef> no_interpolation{{
      "handle_type_left",
      "handle_type_right",
      "handle_position_right",
      "handle_position_left",
      "nurbs_weight",
  }};
  return !(attribute_id.is_named() && no_interpolation.contains(attribute_id.name()));
}

/**
 * Retrieve spans from source and result attributes.
 */
static void retrieve_attribute_spans(const Span<AttributeIDRef> ids,
                                     const CurveComponent &src_component,
                                     CurveComponent &dst_component,
                                     Vector<GSpan> &src,
                                     Vector<GMutableSpan> &dst,
                                     Vector<OutputAttribute> &dst_attributes)
{
  for (const int i : ids.index_range()) {
    GVArray src_attribute = src_component.attribute_try_get_for_read(ids[i], ATTR_DOMAIN_POINT);
    BLI_assert(src_attribute);
    src.append(src_attribute.get_internal_span());

    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(src_attribute.type());
    OutputAttribute dst_attribute = dst_component.attribute_try_get_for_output_only(
        ids[i], ATTR_DOMAIN_POINT, data_type);
    dst.append(dst_attribute.as_span());
    dst_attributes.append(std::move(dst_attribute));
  }
}

struct AttributesForInterpolation : NonCopyable, NonMovable {
  Vector<GSpan> src;
  Vector<GMutableSpan> dst;

  Vector<OutputAttribute> dst_attributes;

  Vector<GSpan> src_no_interpolation;
  Vector<GMutableSpan> dst_no_interpolation;
};

/**
 * Gather a set of all generic attribute IDs to copy to the result curves.
 */
static void gather_point_attributes_to_interpolate(const CurveComponent &src_component,
                                                   CurveComponent &dst_component,
                                                   AttributesForInterpolation &result)
{
  const Curves &dst_curves_id = *dst_component.get_for_read();
  const bke::CurvesGeometry &dst_curves = bke::CurvesGeometry::wrap(dst_curves_id.geometry);
  const std::array<int, CURVE_TYPES_NUM> type_counts = dst_curves.count_curve_types();

  VectorSet<AttributeIDRef> ids;
  VectorSet<AttributeIDRef> ids_no_interpolation;
  src_component.attribute_foreach(
      [&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
        if (meta_data.domain != ATTR_DOMAIN_POINT) {
          return true;
        }
        if (!interpolate_attribute_to_curves(id, type_counts)) {
          return true;
        }
        if (interpolate_attribute_to_poly_curve(id)) {
          ids.add_new(id);
        }
        else {
          ids_no_interpolation.add_new(id);
        }
        return true;
      });

  /* Position is handled differently since it has non-generic interpolation for Bezier
   * curves and because the evaluated positions are cached for each evaluated point. */
  ids.remove_contained("position");

  retrieve_attribute_spans(
      ids, src_component, dst_component, result.src, result.dst, result.dst_attributes);

  /* Attributes that aren't interpolated like Bezier handles still have to be be copied
   * to the result when there are any unselected curves of the corresponding type. */
  retrieve_attribute_spans(ids_no_interpolation,
                           src_component,
                           dst_component,
                           result.src_no_interpolation,
                           result.dst_no_interpolation,
                           result.dst_attributes);
}

/**
 * Copy the provided point attribute values between all curves in the #curve_ranges index
 * ranges, assuming that all curves are the same size in #src_curves and #dst_curves.
 */
template<typename T>
static void copy_between_curves(const bke::CurvesGeometry &src_curves,
                                const bke::CurvesGeometry &dst_curves,
                                const Span<IndexRange> curve_ranges,
                                const Span<T> src,
                                const MutableSpan<T> dst)
{
  threading::parallel_for(curve_ranges.index_range(), 512, [&](IndexRange range) {
    for (const IndexRange range : curve_ranges.slice(range)) {
      const IndexRange src_points = src_curves.points_for_curves(range);
      const IndexRange dst_points = dst_curves.points_for_curves(range);
      /* The arrays might be large, so a threaded copy might make sense here too. */
      dst.slice(dst_points).copy_from(src.slice(src_points));
    }
  });
}
static void copy_between_curves(const bke::CurvesGeometry &src_curves,
                                const bke::CurvesGeometry &dst_curves,
                                const Span<IndexRange> unselected_ranges,
                                const GSpan src,
                                const GMutableSpan dst)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    copy_between_curves(src_curves, dst_curves, unselected_ranges, src.typed<T>(), dst.typed<T>());
  });
}

/**
 * Copy the size of every curve in #curve_ranges to the corresponding index in #counts.
 */
static void fill_curve_counts(const bke::CurvesGeometry &src_curves,
                              const Span<IndexRange> curve_ranges,
                              MutableSpan<int> counts)
{
  threading::parallel_for(curve_ranges.index_range(), 512, [&](IndexRange ranges_range) {
    for (const IndexRange curves_range : curve_ranges.slice(ranges_range)) {
      for (const int i : curves_range) {
        counts[i] = src_curves.points_for_curve(i).size();
      }
    }
  });
}

/**
 * Turn an array of sizes into the offset at each index including all previous sizes.
 */
static void accumulate_counts_to_offsets(MutableSpan<int> counts_to_offsets)
{
  int total = 0;
  for (const int i : counts_to_offsets.index_range().drop_back(1)) {
    const int count = counts_to_offsets[i];
    BLI_assert(count > 0);
    counts_to_offsets[i] = total;
    total += count;
  }
  counts_to_offsets.last() = total;
}

/**
 * Create new curves where the selected curves have been resampled with a number of uniform-length
 * samples defined by the count field. Interpolate attributes to the result, with an accuracy that
 * depends on the curve's resolution parameter.
 *
 * \warning The values provided by the #count_field must be 1 or greater.
 * \warning Curves with no evaluated points must not be selected.
 */
static Curves *resample_to_uniform_count(const CurveComponent &src_component,
                                         const Field<bool> &selection_field,
                                         const Field<int> &count_field)
{
  const Curves &src_curves_id = *src_component.get_for_read();
  const bke::CurvesGeometry &src_curves = bke::CurvesGeometry::wrap(src_curves_id.geometry);

  /* Create the new curves without any points and evaluate the final count directly
   * into the offsets array, in order to be accumulated into offsets later. */
  Curves *dst_curves_id = bke::curves_new_nomain(0, src_curves.curves_num());
  bke::CurvesGeometry &dst_curves = bke::CurvesGeometry::wrap(dst_curves_id->geometry);
  CurveComponent dst_component;
  dst_component.replace(dst_curves_id, GeometryOwnershipType::Editable);
  /* Directly copy curve attributes, since they stay the same (except for curve types). */
  CustomData_copy(&src_curves.curve_data,
                  &dst_curves.curve_data,
                  CD_MASK_ALL,
                  CD_DUPLICATE,
                  src_curves.curves_num());
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();

  GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(count_field, dst_offsets);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const Vector<IndexRange> unselected_ranges = selection.extract_ranges_invert(
      src_curves.curves_range(), nullptr);

  /* Fill the counts for the curves that aren't selected and accumulate the counts into offsets. */
  fill_curve_counts(src_curves, unselected_ranges, dst_offsets);
  accumulate_counts_to_offsets(dst_offsets);
  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());

  /* All resampled curves are poly curves. */
  dst_curves.curve_types_for_write().fill_indices(selection, CURVE_TYPE_POLY);

  VArray<bool> curves_cyclic = src_curves.cyclic();
  VArray<int8_t> curve_types = src_curves.curve_types();
  Span<float3> evaluated_positions = src_curves.evaluated_positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  AttributesForInterpolation attributes;
  gather_point_attributes_to_interpolate(src_component, dst_component, attributes);

  src_curves.ensure_evaluated_lengths();

  /* Sampling arbitrary attributes works by first interpolating them to the curve's standard
   * "evaluated points" and then interpolating that result with the uniform samples. This is
   * potentially wasteful when down-sampling a curve to many fewer points. There are two possible
   * solutions: only sample the necessary points for interpolation, or first sample curve
   * parameter/segment indices and evaluate the curve directly. */
  Array<int> sample_indices(dst_curves.points_num());
  Array<float> sample_factors(dst_curves.points_num());

  /* Use a "for each group of curves: for each attribute: for each curve" pattern to work on
   * smaller sections of data that ideally fit into CPU cache better than simply one attribute at a
   * time or one curve at a time. */
  threading::parallel_for(selection.index_range(), 512, [&](IndexRange selection_range) {
    const IndexMask sliced_selection = selection.slice(selection_range);

    Vector<std::byte> evaluated_buffer;

    /* Gather uniform samples based on the accumulated lengths of the original curve. */
    for (const int i_curve : sliced_selection) {
      const bool cyclic = curves_cyclic[i_curve];
      const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
      length_parameterize::create_uniform_samples(
          src_curves.evaluated_lengths_for_curve(i_curve, cyclic),
          curves_cyclic[i_curve],
          sample_indices.as_mutable_span().slice(dst_points),
          sample_factors.as_mutable_span().slice(dst_points));
    }

    /* For every attribute, evaluate attributes from every curve in the range in the original
     * curve's "evaluated points", then use linear interpolation to sample to the result. */
    for (const int i_attribute : attributes.dst.index_range()) {
      attribute_math::convert_to_static_type(attributes.src[i_attribute].type(), [&](auto dummy) {
        using T = decltype(dummy);
        Span<T> src = attributes.src[i_attribute].typed<T>();
        MutableSpan<T> dst = attributes.dst[i_attribute].typed<T>();

        for (const int i_curve : sliced_selection) {
          const IndexRange src_points = src_curves.points_for_curve(i_curve);
          const IndexRange dst_points = dst_curves.points_for_curve(i_curve);

          if (curve_types[i_curve] == CURVE_TYPE_POLY) {
            length_parameterize::linear_interpolation(src.slice(src_points),
                                                      sample_indices.as_span().slice(dst_points),
                                                      sample_factors.as_span().slice(dst_points),
                                                      dst.slice(dst_points));
          }
          else {
            const int evaluated_size = src_curves.evaluated_points_for_curve(i_curve).size();
            evaluated_buffer.clear();
            evaluated_buffer.resize(sizeof(T) * evaluated_size);
            MutableSpan<T> evaluated = evaluated_buffer.as_mutable_span().cast<T>();
            src_curves.interpolate_to_evaluated(i_curve, src.slice(src_points), evaluated);

            length_parameterize::linear_interpolation(evaluated.as_span(),
                                                      sample_indices.as_span().slice(dst_points),
                                                      sample_factors.as_span().slice(dst_points),
                                                      dst.slice(dst_points));
          }
        }
      });
    }

    /* Interpolate the evaluated positions to the resampled curves. */
    for (const int i_curve : sliced_selection) {
      const IndexRange src_points = src_curves.evaluated_points_for_curve(i_curve);
      const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
      length_parameterize::linear_interpolation(evaluated_positions.slice(src_points),
                                                sample_indices.as_span().slice(dst_points),
                                                sample_factors.as_span().slice(dst_points),
                                                dst_positions.slice(dst_points));
    }

    /* Fill the default value for non-interpolating attributes that still must be copied. */
    for (GMutableSpan dst : attributes.dst_no_interpolation) {
      for (const int i_curve : sliced_selection) {
        const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
        dst.type().value_initialize_n(dst.slice(dst_points).data(), dst_points.size());
      }
    }
  });

  /* Any attribute data from unselected curve points can be directly copied. */
  for (const int i : attributes.src.index_range()) {
    copy_between_curves(
        src_curves, dst_curves, unselected_ranges, attributes.src[i], attributes.dst[i]);
  }
  for (const int i : attributes.src_no_interpolation.index_range()) {
    copy_between_curves(src_curves,
                        dst_curves,
                        unselected_ranges,
                        attributes.src_no_interpolation[i],
                        attributes.dst_no_interpolation[i]);
  }

  /* Copy positions for unselected curves. */
  Span<float3> src_positions = src_curves.positions();
  copy_between_curves(src_curves, dst_curves, unselected_ranges, src_positions, dst_positions);

  for (OutputAttribute &attribute : attributes.dst_attributes) {
    attribute.save();
  }

  return dst_curves_id;
}

/**
 * Evaluate each selected curve to its implicit evaluated points.
 *
 * \warning Curves with no evaluated points must not be selected.
 */
static Curves *resample_to_evaluated(const CurveComponent &src_component,
                                     const Field<bool> &selection_field)
{
  const Curves &src_curves_id = *src_component.get_for_read();
  const bke::CurvesGeometry &src_curves = bke::CurvesGeometry::wrap(src_curves_id.geometry);

  GeometryComponentFieldContext field_context{src_component, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const Vector<IndexRange> unselected_ranges = selection.extract_ranges_invert(
      src_curves.curves_range(), nullptr);

  Curves *dst_curves_id = bke::curves_new_nomain(0, src_curves.curves_num());
  bke::CurvesGeometry &dst_curves = bke::CurvesGeometry::wrap(dst_curves_id->geometry);
  CurveComponent dst_component;
  dst_component.replace(dst_curves_id, GeometryOwnershipType::Editable);
  /* Directly copy curve attributes, since they stay the same (except for curve types). */
  CustomData_copy(&src_curves.curve_data,
                  &dst_curves.curve_data,
                  CD_MASK_ALL,
                  CD_DUPLICATE,
                  src_curves.curves_num());
  /* All resampled curves are poly curves. */
  dst_curves.curve_types_for_write().fill_indices(selection, CURVE_TYPE_POLY);
  MutableSpan<int> dst_offsets = dst_curves.offsets_for_write();

  src_curves.ensure_evaluated_offsets();
  threading::parallel_for(selection.index_range(), 4096, [&](IndexRange range) {
    for (const int i : selection.slice(range)) {
      dst_offsets[i] = src_curves.evaluated_points_for_curve(i).size();
    }
  });
  fill_curve_counts(src_curves, unselected_ranges, dst_offsets);
  accumulate_counts_to_offsets(dst_offsets);

  dst_curves.resize(dst_offsets.last(), dst_curves.curves_num());

  /* Create the correct number of uniform-length samples for every selected curve. */
  Span<float3> evaluated_positions = src_curves.evaluated_positions();
  MutableSpan<float3> dst_positions = dst_curves.positions_for_write();

  AttributesForInterpolation attributes;
  gather_point_attributes_to_interpolate(src_component, dst_component, attributes);

  threading::parallel_for(selection.index_range(), 512, [&](IndexRange selection_range) {
    const IndexMask sliced_selection = selection.slice(selection_range);

    /* Evaluate generic point attributes directly to the result attributes. */
    for (const int i_attribute : attributes.dst.index_range()) {
      attribute_math::convert_to_static_type(attributes.src[i_attribute].type(), [&](auto dummy) {
        using T = decltype(dummy);
        Span<T> src = attributes.src[i_attribute].typed<T>();
        MutableSpan<T> dst = attributes.dst[i_attribute].typed<T>();

        for (const int i_curve : sliced_selection) {
          const IndexRange src_points = src_curves.points_for_curve(i_curve);
          const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
          src_curves.interpolate_to_evaluated(
              i_curve, src.slice(src_points), dst.slice(dst_points));
        }
      });
    }

    /* Copy the evaluated positions to the selected curves. */
    for (const int i_curve : sliced_selection) {
      const IndexRange src_points = src_curves.evaluated_points_for_curve(i_curve);
      const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
      dst_positions.slice(dst_points).copy_from(evaluated_positions.slice(src_points));
    }

    /* Fill the default value for non-interpolating attributes that still must be copied. */
    for (GMutableSpan dst : attributes.dst_no_interpolation) {
      for (const int i_curve : sliced_selection) {
        const IndexRange dst_points = dst_curves.points_for_curve(i_curve);
        dst.type().value_initialize_n(dst.slice(dst_points).data(), dst_points.size());
      }
    }
  });

  /* Any attribute data from unselected curve points can be directly copied. */
  for (const int i : attributes.src.index_range()) {
    copy_between_curves(
        src_curves, dst_curves, unselected_ranges, attributes.src[i], attributes.dst[i]);
  }
  for (const int i : attributes.src_no_interpolation.index_range()) {
    copy_between_curves(src_curves,
                        dst_curves,
                        unselected_ranges,
                        attributes.src_no_interpolation[i],
                        attributes.dst_no_interpolation[i]);
  }

  /* Copy positions for unselected curves. */
  Span<float3> src_positions = src_curves.positions();
  copy_between_curves(src_curves, dst_curves, unselected_ranges, src_positions, dst_positions);

  for (OutputAttribute &attribute : attributes.dst_attributes) {
    attribute.save();
  }

  return dst_curves_id;
}

/**
 * Create a resampled curve point count field for both "uniform" options.
 * The complexity is handled here in order to make the actual resampling functions simpler.
 */
static Field<int> get_curve_count_field(GeoNodeExecParams params,
                                        const GeometryNodeCurveResampleMode mode)
{
  if (mode == GEO_NODE_CURVE_RESAMPLE_COUNT) {
    static fn::CustomMF_SI_SO<int, int> max_one_fn("Clamp Above One",
                                                   [](int value) { return std::max(1, value); });
    auto clamp_op = std::make_shared<FieldOperation>(
        FieldOperation(max_one_fn, {Field<int>(params.extract_input<Field<int>>("Count"))}));

    return Field<int>(std::move(clamp_op));
  }

  if (mode == GEO_NODE_CURVE_RESAMPLE_LENGTH) {
    static fn::CustomMF_SI_SI_SO<float, float, int> get_count_fn(
        "Length Input to Count", [](const float curve_length, const float sample_length) {
          /* Find the number of sampled segments by dividing the total length by
           * the sample length. Then there is one more sampled point than segment. */
          const int count = int(curve_length / sample_length) + 1;
          return std::max(1, count);
        });

    auto get_count_op = std::make_shared<FieldOperation>(
        FieldOperation(get_count_fn,
                       {Field<float>(std::make_shared<SplineLengthFieldInput>()),
                        params.extract_input<Field<float>>("Length")}));

    return Field<int>(std::move(get_count_op));
  }

  BLI_assert_unreachable();
  return {};
}

/**
 * Create a selection field that removes curves without any evaluated points (invalid NURBS curves)
 * from the original selection provided to the node. This is here to simplify the sampling actual
 * resampling code.
 */
static Field<bool> get_selection_field(GeoNodeExecParams params)
{
  static fn::CustomMF_SI_SI_SO<bool, int, bool> get_selection_fn(
      "Create Curve Selection", [](const bool orig_selection, const int evaluated_points_num) {
        return orig_selection && evaluated_points_num > 1;
      });

  auto selection_op = std::make_shared<FieldOperation>(
      FieldOperation(get_selection_fn,
                     {params.extract_input<Field<bool>>("Selection"),
                      Field<int>(std::make_shared<EvaluatedCountFieldInput>())}));

  return Field<bool>(std::move(selection_op));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  const NodeGeometryCurveResample &storage = node_storage(params.node());
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  const Field<bool> selection = get_selection_field(params);

  switch (mode) {
    case GEO_NODE_CURVE_RESAMPLE_COUNT:
    case GEO_NODE_CURVE_RESAMPLE_LENGTH: {
      Field<int> count = get_curve_count_field(params, mode);

      geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
        if (!geometry_set.has_curves()) {
          return;
        }

        Curves *result = resample_to_uniform_count(
            *geometry_set.get_component_for_read<CurveComponent>(), selection, count);

        geometry_set.replace_curves(result);
      });
      break;
    }
    case GEO_NODE_CURVE_RESAMPLE_EVALUATED:
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
        if (!geometry_set.has_curves()) {
          return;
        }

        Curves *result = resample_to_evaluated(
            *geometry_set.get_component_for_read<CurveComponent>(), selection);

        geometry_set.replace_curves(result);
      });
      break;
  }

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_resample_cc

void register_node_type_geo_curve_resample()
{
  namespace file_ns = blender::nodes::node_geo_curve_resample_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_RESAMPLE_CURVE, "Resample Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveResample", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
