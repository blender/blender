/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_color.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_math_vector.hh"

#include "BLI_generic_array.hh"
#include "BLI_length_parameterize.hh"

#include "BKE_curves.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_sample_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveSample)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curves")
      .only_realized_data()
      .supported_type(GeometryComponent::Type::Curve)
      .description("Curves to sample positions on");

  if (const bNode *node = b.node_or_null()) {
    const NodeGeometryCurveSample &storage = node_storage(*node);
    b.add_input(eCustomDataType(storage.data_type), "Value").hide_value().field_on_all();
  }

  auto &factor = b.add_input<decl::Float>("Factor")
                     .min(0.0f)
                     .max(1.0f)
                     .subtype(PROP_FACTOR)
                     .supports_field()
                     .structure_type(StructureType::Dynamic)
                     .make_available([](bNode &node) {
                       node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR;
                     });
  auto &length = b.add_input<decl::Float>("Length")
                     .min(0.0f)
                     .subtype(PROP_DISTANCE)
                     .supports_field()
                     .structure_type(StructureType::Dynamic)
                     .make_available([](bNode &node) {
                       node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH;
                     });
  auto &index = b.add_input<decl::Int>("Curve Index")
                    .supports_field()
                    .structure_type(StructureType::Dynamic)
                    .make_available(
                        [](bNode &node) { node_storage(node).use_all_curves = false; });

  if (const bNode *node = b.node_or_null()) {
    const NodeGeometryCurveSample &storage = node_storage(*node);
    const GeometryNodeCurveSampleMode mode = GeometryNodeCurveSampleMode(storage.mode);
    b.add_output(eCustomDataType(storage.data_type), "Value").dependent_field({2, 3, 4});

    factor.available(mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
    length.available(mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
    index.available(!storage.use_all_curves);
  }

  b.add_output<decl::Vector>("Position").dependent_field({2, 3, 4});
  b.add_output<decl::Vector>("Tangent").dependent_field({2, 3, 4});
  b.add_output<decl::Vector>("Normal").dependent_field({2, 3, 4});
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr, "mode", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->prop(ptr, "use_all_curves", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveSample *data = MEM_callocN<NodeGeometryCurveSample>(__func__);
  data->mode = GEO_NODE_CURVE_SAMPLE_FACTOR;
  data->use_all_curves = false;
  data->data_type = CD_PROP_FLOAT;
  node->storage = data;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_front(1));
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_back(3));
  search_link_ops_for_declarations(params, declaration.outputs.as_span().take_back(3));

  const std::optional<eCustomDataType> type = bke::socket_type_to_custom_data_type(
      eNodeSocketDatatype(params.other_socket().type));
  if (type && *type != CD_PROP_STRING) {
    /* The input and output sockets have the same name. */
    params.add_item(IFACE_("Value"), [type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleCurve");
      node_storage(node).data_type = *type;
      params.update_and_connect_available_socket(node, "Value");
    });
  }
}

static void sample_indices_and_lengths(const Span<float> accumulated_lengths,
                                       const Span<float> sample_lengths,
                                       const GeometryNodeCurveSampleMode length_mode,
                                       const IndexMask &mask,
                                       MutableSpan<int> r_segment_indices,
                                       MutableSpan<float> r_length_in_segment)
{
  const float total_length = accumulated_lengths.last();
  length_parameterize::SampleSegmentHint hint;

  mask.foreach_index_optimized<int>([&](const int i) {
    const float sample_length = length_mode == GEO_NODE_CURVE_SAMPLE_FACTOR ?
                                    sample_lengths[i] * total_length :
                                    sample_lengths[i];
    int segment_i;
    float factor_in_segment;
    length_parameterize::sample_at_length(accumulated_lengths,
                                          std::clamp(sample_length, 0.0f, total_length),
                                          segment_i,
                                          factor_in_segment,
                                          &hint);
    const float segment_start = segment_i == 0 ? 0.0f : accumulated_lengths[segment_i - 1];
    const float segment_end = accumulated_lengths[segment_i];
    const float segment_length = segment_end - segment_start;

    r_segment_indices[i] = segment_i;
    r_length_in_segment[i] = factor_in_segment * segment_length;
  });
}

static void sample_indices_and_factors_to_compressed(const Span<float> accumulated_lengths,
                                                     const Span<float> sample_lengths,
                                                     const GeometryNodeCurveSampleMode length_mode,
                                                     const IndexMask &mask,
                                                     MutableSpan<int> r_segment_indices,
                                                     MutableSpan<float> r_factor_in_segment)
{
  const float total_length = accumulated_lengths.last();
  length_parameterize::SampleSegmentHint hint;

  switch (length_mode) {
    case GEO_NODE_CURVE_SAMPLE_FACTOR:
      mask.foreach_index_optimized<int>([&](const int i, const int pos) {
        const float length = sample_lengths[i] * total_length;
        length_parameterize::sample_at_length(accumulated_lengths,
                                              std::clamp(length, 0.0f, total_length),
                                              r_segment_indices[pos],
                                              r_factor_in_segment[pos],
                                              &hint);
      });
      break;
    case GEO_NODE_CURVE_SAMPLE_LENGTH:
      mask.foreach_index_optimized<int>([&](const int i, const int pos) {
        const float length = sample_lengths[i];
        length_parameterize::sample_at_length(accumulated_lengths,
                                              std::clamp(length, 0.0f, total_length),
                                              r_segment_indices[pos],
                                              r_factor_in_segment[pos],
                                              &hint);
      });
      break;
  }
}

/**
 * Given an array of accumulated lengths, find the segment indices that
 * sample lengths lie on, and how far along the segment they are.
 */
class SampleFloatSegmentsFunction : public mf::MultiFunction {
 private:
  Array<float> accumulated_lengths_;
  GeometryNodeCurveSampleMode length_mode_;

 public:
  SampleFloatSegmentsFunction(Array<float> accumulated_lengths,
                              const GeometryNodeCurveSampleMode length_mode)
      : accumulated_lengths_(std::move(accumulated_lengths)), length_mode_(length_mode)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Sample Curve Index", signature};
      builder.single_input<float>("Length");

      builder.single_output<int>("Curve Index");
      builder.single_output<float>("Length in Curve");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<float> lengths = params.readonly_single_input<float>(0, "Length");
    MutableSpan<int> indices = params.uninitialized_single_output<int>(1, "Curve Index");
    MutableSpan<float> lengths_in_segments = params.uninitialized_single_output<float>(
        2, "Length in Curve");

    sample_indices_and_lengths(
        accumulated_lengths_, lengths, length_mode_, mask, indices, lengths_in_segments);
  }
};

class SampleCurveFunction : public mf::MultiFunction {
 private:
  /**
   * The function holds a geometry set instead of curves or a curve component reference in order
   * to maintain a ownership of the geometry while the field tree is being built and used, so
   * that the curve is not freed before the function can execute.
   */
  GeometrySet geometry_set_;
  GField src_field_;
  GeometryNodeCurveSampleMode length_mode_;

  mf::Signature signature_;

  std::optional<bke::CurvesFieldContext> source_context_;
  std::unique_ptr<FieldEvaluator> source_evaluator_;
  const GVArray *source_data_;

 public:
  SampleCurveFunction(GeometrySet geometry_set,
                      const GeometryNodeCurveSampleMode length_mode,
                      const GField &src_field)
      : geometry_set_(std::move(geometry_set)), src_field_(src_field), length_mode_(length_mode)
  {
    mf::SignatureBuilder builder{"Sample Curve", signature_};
    builder.single_input<int>("Curve Index");
    builder.single_input<float>("Length");
    builder.single_output<float3>("Position", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<float3>("Tangent", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<float3>("Normal", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output("Value", src_field_.cpp_type(), mf::ParamFlag::SupportsUnusedOutput);
    this->set_signature(&signature_);

    this->evaluate_source();
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    MutableSpan<float3> sampled_positions = params.uninitialized_single_output_if_required<float3>(
        2, "Position");
    MutableSpan<float3> sampled_tangents = params.uninitialized_single_output_if_required<float3>(
        3, "Tangent");
    MutableSpan<float3> sampled_normals = params.uninitialized_single_output_if_required<float3>(
        4, "Normal");
    GMutableSpan sampled_values = params.uninitialized_single_output_if_required(5, "Value");

    auto return_default = [&]() {
      if (!sampled_positions.is_empty()) {
        index_mask::masked_fill(sampled_positions, {0, 0, 0}, mask);
      }
      if (!sampled_tangents.is_empty()) {
        index_mask::masked_fill(sampled_tangents, {0, 0, 0}, mask);
      }
      if (!sampled_normals.is_empty()) {
        index_mask::masked_fill(sampled_normals, {0, 0, 0}, mask);
      }
    };

    if (!geometry_set_.has_curves()) {
      return_default();
      return;
    }

    const Curves &curves_id = *geometry_set_.get_curves();
    const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
    if (curves.is_empty()) {
      return_default();
      return;
    }
    curves.ensure_can_interpolate_to_evaluated();
    Span<float3> evaluated_positions = curves.evaluated_positions();
    Span<float3> evaluated_tangents;
    Span<float3> evaluated_normals;
    if (!sampled_tangents.is_empty()) {
      evaluated_tangents = curves.evaluated_tangents();
    }
    if (!sampled_normals.is_empty()) {
      evaluated_normals = curves.evaluated_normals();
    }

    const OffsetIndices points_by_curve = curves.points_by_curve();
    const OffsetIndices evaluated_points_by_curve = curves.evaluated_points_by_curve();
    const VArray<int> curve_indices = params.readonly_single_input<int>(0, "Curve Index");
    const VArraySpan<float> lengths = params.readonly_single_input<float>(1, "Length");
    const VArray<bool> cyclic = curves.cyclic();

    Array<int> indices;
    Array<float> factors;
    GArray<> src_original_values(source_data_->type());
    GArray<> src_evaluated_values(source_data_->type());

    auto fill_invalid = [&](const IndexMask &mask) {
      if (!sampled_positions.is_empty()) {
        index_mask::masked_fill(sampled_positions, float3(0), mask);
      }
      if (!sampled_tangents.is_empty()) {
        index_mask::masked_fill(sampled_tangents, float3(0), mask);
      }
      if (!sampled_normals.is_empty()) {
        index_mask::masked_fill(sampled_normals, float3(0), mask);
      }
      if (!sampled_values.is_empty()) {
        bke::attribute_math::convert_to_static_type(source_data_->type(), [&](auto dummy) {
          using T = decltype(dummy);
          index_mask::masked_fill<T>(sampled_values.typed<T>(), {}, mask);
        });
      }
    };

    auto sample_curve = [&](const int curve_i, const IndexMask &mask) {
      const IndexRange evaluated_points = evaluated_points_by_curve[curve_i];
      if (evaluated_points.size() == 1) {
        if (!sampled_positions.is_empty()) {
          index_mask::masked_fill(
              sampled_positions, evaluated_positions[evaluated_points.first()], mask);
        }
        if (!sampled_tangents.is_empty()) {
          index_mask::masked_fill(
              sampled_tangents, evaluated_tangents[evaluated_points.first()], mask);
        }
        if (!sampled_normals.is_empty()) {
          index_mask::masked_fill(
              sampled_normals, evaluated_normals[evaluated_points.first()], mask);
        }
        if (!sampled_values.is_empty()) {
          bke::attribute_math::convert_to_static_type(source_data_->type(), [&](auto dummy) {
            using T = decltype(dummy);
            const T &value = source_data_->typed<T>()[points_by_curve[curve_i].first()];
            index_mask::masked_fill<T>(sampled_values.typed<T>(), value, mask);
          });
        }
        return;
      }

      const Span<float> accumulated_lengths = curves.evaluated_lengths_for_curve(curve_i,
                                                                                 cyclic[curve_i]);
      if (accumulated_lengths.is_empty()) {
        /* Sanity check in case of invalid evaluation (for example NURBS with invalid order). */
        fill_invalid(mask);
        return;
      }

      /* Store the sampled indices and factors in arrays the size of the mask.
       * Then, during interpolation, move the results back to the masked indices. */
      indices.reinitialize(mask.size());
      factors.reinitialize(mask.size());
      sample_indices_and_factors_to_compressed(
          accumulated_lengths, lengths, length_mode_, mask, indices, factors);

      if (!sampled_positions.is_empty()) {
        length_parameterize::interpolate_to_masked<float3>(
            evaluated_positions.slice(evaluated_points),
            indices,
            factors,
            mask,
            sampled_positions);
      }
      if (!sampled_tangents.is_empty()) {
        length_parameterize::interpolate_to_masked<float3>(
            evaluated_tangents.slice(evaluated_points), indices, factors, mask, sampled_tangents);
        mask.foreach_index(
            [&](const int i) { sampled_tangents[i] = math::normalize(sampled_tangents[i]); });
      }
      if (!sampled_normals.is_empty()) {
        length_parameterize::interpolate_to_masked<float3>(
            evaluated_normals.slice(evaluated_points), indices, factors, mask, sampled_normals);
        mask.foreach_index(
            [&](const int i) { sampled_normals[i] = math::normalize(sampled_normals[i]); });
      }
      if (!sampled_values.is_empty()) {
        const IndexRange points = points_by_curve[curve_i];
        src_original_values.reinitialize(points.size());
        source_data_->materialize_compressed_to_uninitialized(points, src_original_values.data());
        src_evaluated_values.reinitialize(evaluated_points.size());
        curves.interpolate_to_evaluated(curve_i, src_original_values, src_evaluated_values);
        bke::attribute_math::convert_to_static_type(source_data_->type(), [&](auto dummy) {
          using T = decltype(dummy);
          const Span<T> src_evaluated_values_typed = src_evaluated_values.as_span().typed<T>();
          MutableSpan<T> sampled_values_typed = sampled_values.typed<T>();
          length_parameterize::interpolate_to_masked<T>(
              src_evaluated_values_typed, indices, factors, mask, sampled_values_typed);
        });
      }
    };

    if (const std::optional<int> curve_i = curve_indices.get_if_single()) {
      if (curves.curves_range().contains(*curve_i)) {
        sample_curve(*curve_i, mask);
      }
      else {
        fill_invalid(mask);
      }
    }
    else {
      Vector<int> valid_indices;
      Vector<int> invalid_indices;
      VectorSet<int> used_curves;
      devirtualize_varray(curve_indices, [&](const auto curve_indices) {
        mask.foreach_index([&](const int i) {
          const int curve_i = curve_indices[i];
          if (curves.curves_range().contains(curve_i)) {
            used_curves.add(curve_i);
            valid_indices.append(i);
          }
          else {
            invalid_indices.append(i);
          }
        });
      });

      IndexMaskMemory memory;
      const IndexMask valid_indices_mask = valid_indices.size() == mask.size() ?
                                               mask :
                                               IndexMask::from_indices(valid_indices.as_span(),
                                                                       memory);
      Array<IndexMask> mask_by_curve(used_curves.size());
      IndexMask::from_groups<int>(
          valid_indices_mask,
          memory,
          [&](const int i) { return used_curves.index_of(curve_indices[i]); },
          mask_by_curve);

      for (const int i : mask_by_curve.index_range()) {
        sample_curve(used_curves[i], mask_by_curve[i]);
      }
      fill_invalid(IndexMask::from_indices<int>(invalid_indices, memory));
    }
  }

 private:
  void evaluate_source()
  {
    const Curves &curves_id = *geometry_set_.get_curves();
    const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
    source_context_.emplace(bke::CurvesFieldContext{curves_id, AttrDomain::Point});
    source_evaluator_ = std::make_unique<FieldEvaluator>(*source_context_, curves.points_num());
    source_evaluator_->add(src_field_);
    source_evaluator_->evaluate();
    source_data_ = &source_evaluator_->get_evaluated(0);
  }
};

static Array<float> curve_accumulated_lengths(const bke::CurvesGeometry &curves)
{

  Array<float> curve_lengths(curves.curves_num());
  const VArray<bool> cyclic = curves.cyclic();
  float length = 0.0f;
  for (const int i : curves.curves_range()) {
    length += curves.evaluated_length_total_for_curve(i, cyclic[i]);
    curve_lengths[i] = length;
  }
  return curve_lengths;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curves");
  if (!geometry_set.has_curves()) {
    params.set_default_remaining_outputs();
    return;
  }

  const Curves &curves_id = *geometry_set.get_curves();
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
  if (curves.is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  curves.ensure_evaluated_lengths();

  const NodeGeometryCurveSample &storage = node_storage(params.node());
  const GeometryNodeCurveSampleMode mode = GeometryNodeCurveSampleMode(storage.mode);

  const StringRef length_input_name = mode == GEO_NODE_CURVE_SAMPLE_FACTOR ? "Factor" : "Length";
  auto sample_length = params.extract_input<bke::SocketValueVariant>(length_input_name);

  GField src_values_field = params.extract_input<GField>("Value");

  std::string error_message;

  bke::SocketValueVariant position;
  bke::SocketValueVariant tangent;
  bke::SocketValueVariant normal;
  bke::SocketValueVariant value;
  std::shared_ptr<FieldOperation> sample_op;
  if (curves.curves_num() == 1) {
    auto curve_index = bke::SocketValueVariant::From(fn::make_constant_field<int>(0));
    if (!execute_multi_function_on_value_variant(
            std::make_unique<SampleCurveFunction>(
                std::move(geometry_set), mode, std::move(src_values_field)),
            {&curve_index, &sample_length},
            {&position, &tangent, &normal, &value},
            params.user_data(),
            error_message))
    {
      params.set_default_remaining_outputs();
      params.error_message_add(NodeWarningType::Error, std::move(error_message));
      return;
    }
  }
  else {
    if (storage.use_all_curves) {
      bke::SocketValueVariant curve_index;
      bke::SocketValueVariant length_in_curve;
      if (!execute_multi_function_on_value_variant(std::make_unique<SampleFloatSegmentsFunction>(
                                                       curve_accumulated_lengths(curves), mode),
                                                   {&sample_length},
                                                   {&curve_index, &length_in_curve},
                                                   params.user_data(),
                                                   error_message))
      {
        params.set_default_remaining_outputs();
        params.error_message_add(NodeWarningType::Error, std::move(error_message));
        return;
      }
      if (!execute_multi_function_on_value_variant(
              std::make_shared<SampleCurveFunction>(std::move(geometry_set),
                                                    GEO_NODE_CURVE_SAMPLE_LENGTH,
                                                    std::move(src_values_field)),
              {&curve_index, &length_in_curve},
              {&position, &tangent, &normal, &value},
              params.user_data(),
              error_message))
      {
        params.set_default_remaining_outputs();
        params.error_message_add(NodeWarningType::Error, std::move(error_message));
        return;
      }
    }
    else {
      auto curve_index = params.extract_input<bke::SocketValueVariant>("Curve Index");
      if (!execute_multi_function_on_value_variant(
              std::make_shared<SampleCurveFunction>(
                  std::move(geometry_set), mode, std::move(src_values_field)),
              {&curve_index, &sample_length},
              {&position, &tangent, &normal, &value},
              params.user_data(),
              error_message))
      {
        params.set_default_remaining_outputs();
        params.error_message_add(NodeWarningType::Error, std::move(error_message));
        return;
      }
    }
  }

  params.set_output("Position", std::move(position));
  params.set_output("Tangent", std::move(tangent));
  params.set_output("Normal", std::move(normal));
  params.set_output("Value", std::move(value));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSampleCurve", GEO_NODE_SAMPLE_CURVE);
  ntype.ui_name = "Sample Curve";
  ntype.ui_description =
      "Retrieve data from a point on a curve at a certain distance from its start";
  ntype.enum_name_legacy = "SAMPLE_CURVE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryCurveSample", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_sample_cc
