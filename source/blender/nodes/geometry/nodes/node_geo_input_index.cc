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

namespace blender::nodes {

static void geo_node_input_index_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Index").field_source();
}

class IndexFieldInput final : public fn::FieldInput {
 public:
  IndexFieldInput() : FieldInput(CPPType::get<int>(), "Index")
  {
  }

  const GVArray *get_varray_for_context(const fn::FieldContext &UNUSED(context),
                                        IndexMask mask,
                                        ResourceScope &scope) const final
  {
    /* TODO: Investigate a similar method to IndexRange::as_span() */
    auto index_func = [](int i) { return i; };
    return &scope.construct<
        fn::GVArray_For_EmbeddedVArray<int, VArray_For_Func<int, decltype(index_func)>>>(
        mask.min_array_size(), mask.min_array_size(), index_func);
  }
};

static void geo_node_input_index_exec(GeoNodeExecParams params)
{
  Field<int> index_field{std::make_shared<IndexFieldInput>()};
  params.set_output("Index", std::move(index_field));
}

}  // namespace blender::nodes

void register_node_type_geo_input_index()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_INDEX, "Index", NODE_CLASS_INPUT, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_input_index_exec;
  ntype.declare = blender::nodes::geo_node_input_index_declare;
  nodeRegisterType(&ntype);
}
