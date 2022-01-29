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

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_curve_handles_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>(N_("Relative"))
      .default_value(false)
      .supports_field()
      .description(N_("Output the handle positions relative to the corresponding control point "
                      "instead of in the local space of the geometry"));
  b.add_output<decl::Vector>(N_("Left")).field_source();
  b.add_output<decl::Vector>(N_("Right")).field_source();
}

class HandlePositionFieldInput final : public GeometryFieldInput {
  Field<bool> relative_;
  bool left_;

 public:
  HandlePositionFieldInput(Field<bool> relative, bool left)
      : GeometryFieldInput(CPPType::get<float3>(), "Handle"), relative_(relative), left_(left)
  {
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask mask) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_CURVE) {
      return {};
    }

    GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
    fn::FieldEvaluator evaluator(field_context, &mask);
    evaluator.add(relative_);
    evaluator.evaluate();
    const VArray<bool> &relative = evaluator.get_evaluated<bool>(0);

    VArray<float3> positions = component.attribute_get_for_read<float3>(
        "position", ATTR_DOMAIN_POINT, {0, 0, 0});

    StringRef side = left_ ? "handle_left" : "handle_right";
    VArray<float3> handles = component.attribute_get_for_read<float3>(
        side, ATTR_DOMAIN_POINT, {0, 0, 0});

    if (relative.is_single()) {
      if (relative.get_internal_single()) {
        Array<float3> output(positions.size());
        for (const int i : positions.index_range()) {
          output[i] = handles[i] - positions[i];
        }
        return component.attribute_try_adapt_domain<float3>(
            VArray<float3>::ForContainer(std::move(output)), ATTR_DOMAIN_POINT, domain);
      }
      return component.attribute_try_adapt_domain<float3>(handles, ATTR_DOMAIN_POINT, domain);
    }

    Array<float3> output(positions.size());
    for (const int i : positions.index_range()) {
      if (relative[i]) {
        output[i] = handles[i] - positions[i];
      }
      else {
        output[i] = handles[i];
      }
    }
    return component.attribute_try_adapt_domain<float3>(
        VArray<float3>::ForContainer(std::move(output)), ATTR_DOMAIN_POINT, domain);
  }

  uint64_t hash() const override
  {
    return get_default_hash_2(relative_, left_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const HandlePositionFieldInput *other_handle =
            dynamic_cast<const HandlePositionFieldInput *>(&other)) {
      return relative_ == other_handle->relative_ && left_ == other_handle->left_;
    }
    return false;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> relative = params.extract_input<Field<bool>>("Relative");
  Field<float3> left_field{std::make_shared<HandlePositionFieldInput>(relative, true)};
  Field<float3> right_field{std::make_shared<HandlePositionFieldInput>(relative, false)};

  params.set_output("Left", std::move(left_field));
  params.set_output("Right", std::move(right_field));
}

}  // namespace blender::nodes::node_geo_input_curve_handles_cc

void register_node_type_geo_input_curve_handles()
{
  namespace file_ns = blender::nodes::node_geo_input_curve_handles_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_CURVE_HANDLES, "Curve Handle Positions", NODE_CLASS_INPUT);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
