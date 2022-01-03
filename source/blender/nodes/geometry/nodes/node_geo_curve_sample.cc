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

#include "BLI_task.hh"

#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_sample_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveSample)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve"))
      .only_realized_data()
      .supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Float>(N_("Factor"))
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .supports_field()
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR; });
  b.add_input<decl::Float>(N_("Length"))
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field()
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH; });
  b.add_output<decl::Vector>(N_("Position")).dependent_field();
  b.add_output<decl::Vector>(N_("Tangent")).dependent_field();
  b.add_output<decl::Vector>(N_("Normal")).dependent_field();
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_type_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSample *data = MEM_cnew<NodeGeometryCurveSample>(__func__);
  data->mode = GEO_NODE_CURVE_SAMPLE_LENGTH;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveSample &storage = node_storage(*node);
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)storage.mode;

  bNodeSocket *factor = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *length = factor->next;

  nodeSetSocketAvailability(ntree, factor, mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
  nodeSetSocketAvailability(ntree, length, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
}

template<typename T> static T sample_with_lookup(const Spline::LookupResult lookup, Span<T> data)
{
  return attribute_math::mix2(
      lookup.factor, data[lookup.evaluated_index], data[lookup.next_evaluated_index]);
}

class SampleCurveFunction : public fn::MultiFunction {
 private:
  /**
   * The function holds a geometry set instead of a curve or a curve component in order to
   * maintain a reference to the geometry while the field tree is being built, so that the
   * curve is not freed before the function can execute.
   */
  GeometrySet geometry_set_;
  /**
   * To support factor inputs, the node adds another field operation before this one to multiply by
   * the curve's total length. Since that must calculate the spline lengths anyway, store them to
   * reuse the calculation.
   */
  Array<float> spline_lengths_;
  /** The last member of #spline_lengths_, extracted for convenience. */
  const float total_length_;

 public:
  SampleCurveFunction(GeometrySet geometry_set, Array<float> spline_lengths)
      : geometry_set_(std::move(geometry_set)),
        spline_lengths_(std::move(spline_lengths)),
        total_length_(spline_lengths_.last())
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Curve Sample"};
    signature.single_input<float>("Length");
    signature.single_output<float3>("Position");
    signature.single_output<float3>("Tangent");
    signature.single_output<float3>("Normal");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    MutableSpan<float3> sampled_positions = params.uninitialized_single_output_if_required<float3>(
        1, "Position");
    MutableSpan<float3> sampled_tangents = params.uninitialized_single_output_if_required<float3>(
        2, "Tangent");
    MutableSpan<float3> sampled_normals = params.uninitialized_single_output_if_required<float3>(
        3, "Normal");

    auto return_default = [&]() {
      if (!sampled_positions.is_empty()) {
        sampled_positions.fill_indices(mask, {0, 0, 0});
      }
      if (!sampled_tangents.is_empty()) {
        sampled_tangents.fill_indices(mask, {0, 0, 0});
      }
      if (!sampled_normals.is_empty()) {
        sampled_normals.fill_indices(mask, {0, 0, 0});
      }
    };

    if (!geometry_set_.has_curve()) {
      return return_default();
    }

    const CurveComponent *curve_component = geometry_set_.get_component_for_read<CurveComponent>();
    const CurveEval *curve = curve_component->get_for_read();
    Span<SplinePtr> splines = curve->splines();
    if (splines.is_empty()) {
      return return_default();
    }

    const VArray<float> &lengths_varray = params.readonly_single_input<float>(0, "Length");
    const VArray_Span lengths{lengths_varray};
#ifdef DEBUG
    for (const float length : lengths) {
      /* Lengths must be in range of the curve's total length. This is ensured in
       * #get_length_input_field by adding another multi-function before this one
       * to clamp the lengths. */
      BLI_assert(length >= 0.0f && length <= total_length_);
    }
#endif

    Array<int> spline_indices(mask.min_array_size());
    for (const int i : mask) {
      const float *offset = std::lower_bound(
          spline_lengths_.begin(), spline_lengths_.end(), lengths[i]);
      const int index = offset - spline_lengths_.data() - 1;
      spline_indices[i] = std::max(index, 0);
    }

    /* Storing lookups in an array is unnecessary but will simplify custom attribute transfer. */
    Array<Spline::LookupResult> lookups(mask.min_array_size());
    for (const int i : mask) {
      const float length_in_spline = lengths[i] - spline_lengths_[spline_indices[i]];
      lookups[i] = splines[spline_indices[i]]->lookup_evaluated_length(length_in_spline);
    }

    if (!sampled_positions.is_empty()) {
      for (const int i : mask) {
        const Spline::LookupResult &lookup = lookups[i];
        const Span<float3> evaluated_positions = splines[spline_indices[i]]->evaluated_positions();
        sampled_positions[i] = sample_with_lookup(lookup, evaluated_positions);
      }
    }

    if (!sampled_tangents.is_empty()) {
      for (const int i : mask) {
        const Spline::LookupResult &lookup = lookups[i];
        const Span<float3> evaluated_tangents = splines[spline_indices[i]]->evaluated_tangents();
        sampled_tangents[i] = sample_with_lookup(lookup, evaluated_tangents).normalized();
      }
    }

    if (!sampled_normals.is_empty()) {
      for (const int i : mask) {
        const Spline::LookupResult &lookup = lookups[i];
        const Span<float3> evaluated_normals = splines[spline_indices[i]]->evaluated_normals();
        sampled_normals[i] = sample_with_lookup(lookup, evaluated_normals).normalized();
      }
    }
  }
};

/**
 * Pre-process the lengths or factors used for the sampling, turning factors into lengths, and
 * clamping between zero and the total length of the curve. Do this as a separate operation in the
 * field tree to make the sampling simpler, and to let the evaluator optimize better.
 *
 * \todo Use a mutable single input instead when they are supported.
 */
static Field<float> get_length_input_field(const GeoNodeExecParams &params,
                                           const float curve_total_length)
{
  const NodeGeometryCurveSample &storage = node_storage(params.node());
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)storage.mode;

  if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
    /* Just make sure the length is in bounds of the curve. */
    Field<float> length_field = params.get_input<Field<float>>("Length");
    auto clamp_fn = std::make_unique<fn::CustomMF_SI_SO<float, float>>(
        __func__, [curve_total_length](float length) {
          return std::clamp(length, 0.0f, curve_total_length);
        });
    auto clamp_op = std::make_shared<FieldOperation>(
        FieldOperation(std::move(clamp_fn), {std::move(length_field)}));

    return Field<float>(std::move(clamp_op), 0);
  }

  /* Convert the factor to a length and clamp it to the bounds of the curve. */
  Field<float> factor_field = params.get_input<Field<float>>("Factor");
  auto clamp_fn = std::make_unique<fn::CustomMF_SI_SO<float, float>>(
      __func__, [curve_total_length](float factor) {
        const float length = factor * curve_total_length;
        return std::clamp(length, 0.0f, curve_total_length);
      });
  auto process_op = std::make_shared<FieldOperation>(
      FieldOperation(std::move(clamp_fn), {std::move(factor_field)}));

  return Field<float>(std::move(process_op), 0);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  const CurveComponent *component = geometry_set.get_component_for_read<CurveComponent>();
  if (component == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }

  const CurveEval *curve = component->get_for_read();
  if (curve == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }

  if (curve->splines().is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  Array<float> spline_lengths = curve->accumulated_spline_lengths();
  const float total_length = spline_lengths.last();
  if (total_length == 0.0f) {
    params.set_default_remaining_outputs();
    return;
  }

  Field<float> length_field = get_length_input_field(params, total_length);

  auto sample_fn = std::make_unique<SampleCurveFunction>(std::move(geometry_set),
                                                         std::move(spline_lengths));
  auto sample_op = std::make_shared<FieldOperation>(
      FieldOperation(std::move(sample_fn), {length_field}));

  params.set_output("Position", Field<float3>(sample_op, 0));
  params.set_output("Tangent", Field<float3>(sample_op, 1));
  params.set_output("Normal", Field<float3>(sample_op, 2));
}

}  // namespace blender::nodes::node_geo_curve_sample_cc

void register_node_type_geo_curve_sample()
{
  namespace file_ns = blender::nodes::node_geo_curve_sample_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SAMPLE_CURVE, "Sample Curve", NODE_CLASS_GEOMETRY, 0);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  node_type_init(&ntype, file_ns::node_type_init);
  node_type_update(&ntype, file_ns::node_update);
  node_type_storage(
      &ntype, "NodeGeometryCurveSample", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;

  nodeRegisterType(&ntype);
}
