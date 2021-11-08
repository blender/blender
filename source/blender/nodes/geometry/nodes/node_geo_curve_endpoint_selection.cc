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

#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_curve_endpoint_selection_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Start Size"))
      .min(0)
      .default_value(1)
      .supports_field()
      .description(N_("The amount of points to select from the start of each spline"));
  b.add_input<decl::Int>(N_("End Size"))
      .min(0)
      .default_value(1)
      .supports_field()
      .description(N_("The amount of points to select from the end of each spline"));
  b.add_output<decl::Bool>(N_("Selection"))
      .field_source()
      .description(
          N_("The selection from the start and end of the splines based on the input sizes"));
}

static void select_by_spline(const int start, const int end, MutableSpan<bool> r_selection)
{
  const int size = r_selection.size();
  const int start_use = std::min(start, size);
  const int end_use = std::min(end, size);

  r_selection.slice(0, start_use).fill(true);
  r_selection.slice(size - end_use, end_use).fill(true);
}

class EndpointFieldInput final : public fn::FieldInput {
  Field<int> start_size_;
  Field<int> end_size_;

 public:
  EndpointFieldInput(Field<int> start_size, Field<int> end_size)
      : FieldInput(CPPType::get<bool>(), "Endpoint Selection node"),
        start_size_(start_size),
        end_size_(end_size)
  {
    category_ = Category::Generated;
  }

  const GVArray *get_varray_for_context(const fn::FieldContext &context,
                                        IndexMask UNUSED(mask),
                                        ResourceScope &scope) const final
  {
    if (const GeometryComponentFieldContext *geometry_context =
            dynamic_cast<const GeometryComponentFieldContext *>(&context)) {

      const GeometryComponent &component = geometry_context->geometry_component();
      const AttributeDomain domain = geometry_context->domain();
      if (component.type() != GEO_COMPONENT_TYPE_CURVE || domain != ATTR_DOMAIN_POINT) {
        return nullptr;
      }

      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      const CurveEval *curve = curve_component.get_for_read();

      Array<int> control_point_offsets = curve->control_point_offsets();

      if (curve == nullptr || control_point_offsets.last() == 0) {
        return nullptr;
      }

      GeometryComponentFieldContext size_context{curve_component, ATTR_DOMAIN_CURVE};
      fn::FieldEvaluator evaluator{size_context, curve->splines().size()};
      evaluator.add(start_size_);
      evaluator.add(end_size_);
      evaluator.evaluate();
      const VArray<int> &start_size = evaluator.get_evaluated<int>(0);
      const VArray<int> &end_size = evaluator.get_evaluated<int>(1);

      const int point_size = control_point_offsets.last();
      Array<bool> selection(point_size, false);
      int current_point = 0;
      MutableSpan<bool> selection_span = selection.as_mutable_span();
      for (int i : IndexRange(curve->splines().size())) {
        const SplinePtr &spline = curve->splines()[i];
        if (start_size[i] <= 0 && end_size[i] <= 0) {
          selection_span.slice(current_point, spline->size()).fill(false);
        }
        else {
          int start_use = std::max(start_size[i], 0);
          int end_use = std::max(end_size[i], 0);
          select_by_spline(
              start_use, end_use, selection_span.slice(current_point, spline->size()));
        }
        current_point += spline->size();
      }
      return &scope.construct<fn::GVArray_For_ArrayContainer<Array<bool>>>(std::move(selection));
    }
    return nullptr;
  };

  uint64_t hash() const override
  {
    return get_default_hash_2(start_size_, end_size_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const EndpointFieldInput *other_endpoint = dynamic_cast<const EndpointFieldInput *>(
            &other)) {
      return start_size_ == other_endpoint->start_size_ && end_size_ == other_endpoint->end_size_;
    }
    return false;
  }
};

static void geo_node_curve_endpoint_selection_exec(GeoNodeExecParams params)
{
  Field<int> start_size = params.extract_input<Field<int>>("Start Size");
  Field<int> end_size = params.extract_input<Field<int>>("End Size");
  Field<bool> selection_field{std::make_shared<EndpointFieldInput>(start_size, end_size)};
  params.set_output("Selection", std::move(selection_field));
}
}  // namespace blender::nodes

void register_node_type_geo_curve_endpoint_selection()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_ENDPOINT_SELECTION, "Endpoint Selection", NODE_CLASS_INPUT, 0);
  ntype.declare = blender::nodes::geo_node_curve_endpoint_selection_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_endpoint_selection_exec;

  nodeRegisterType(&ntype);
}
