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

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_id_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Int>(N_("ID")).implicit_field();
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void set_id_in_component(GeometryComponent &component,
                                const Field<bool> &selection_field,
                                const Field<int> &id_field)
{
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_POINT);
  if (domain_size == 0) {
    return;
  }

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);

  /* Since adding the ID attribute can change the result of the field evaluation (the random value
   * node uses the index if the ID is unavailable), make sure that it isn't added before evaluating
   * the field. However, as an optimization, use a faster code path when it already exists. */
  if (component.attribute_exists("id")) {
    OutputAttribute_Typed<int> id_attribute = component.attribute_try_get_for_output_only<int>(
        "id", ATTR_DOMAIN_POINT);
    evaluator.add_with_destination(id_field, id_attribute.varray());
    evaluator.evaluate();
    id_attribute.save();
  }
  else {
    evaluator.add(id_field);
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
    const VArray<int> &result_ids = evaluator.get_evaluated<int>(0);
    OutputAttribute_Typed<int> id_attribute = component.attribute_try_get_for_output_only<int>(
        "id", ATTR_DOMAIN_POINT);
    result_ids.materialize(selection, id_attribute.as_span());
    id_attribute.save();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<int> id_field = params.extract_input<Field<int>>("ID");

  for (const GeometryComponentType type : {GEO_COMPONENT_TYPE_INSTANCES,
                                           GEO_COMPONENT_TYPE_MESH,
                                           GEO_COMPONENT_TYPE_POINT_CLOUD,
                                           GEO_COMPONENT_TYPE_CURVE}) {
    if (geometry_set.has(type)) {
      set_id_in_component(geometry_set.get_component_for_write(type), selection_field, id_field);
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_id_cc

void register_node_type_geo_set_id()
{
  namespace file_ns = blender::nodes::node_geo_set_id_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_ID, "Set ID", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
