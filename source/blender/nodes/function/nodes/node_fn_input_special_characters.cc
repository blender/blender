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

#include "node_function_util.hh"

namespace blender::nodes::node_fn_input_special_characters_cc {

static void fn_node_input_special_characters_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::String>(N_("Line Break"));
  b.add_output<decl::String>(N_("Tab"));
}

class MF_SpecialCharacters : public fn::MultiFunction {
 public:
  MF_SpecialCharacters()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Special Characters"};
    signature.single_output<std::string>("Line Break");
    signature.single_output<std::string>("Tab");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    MutableSpan<std::string> lb = params.uninitialized_single_output<std::string>(0, "Line Break");
    MutableSpan<std::string> tab = params.uninitialized_single_output<std::string>(1, "Tab");

    for (const int i : mask) {
      new (&lb[i]) std::string("\n");
      new (&tab[i]) std::string("\t");
    }
  }
};

static void fn_node_input_special_characters_build_multi_function(
    NodeMultiFunctionBuilder &builder)
{
  static MF_SpecialCharacters special_characters_fn;
  builder.set_matching_fn(special_characters_fn);
}

}  // namespace blender::nodes::node_fn_input_special_characters_cc

void register_node_type_fn_input_special_characters()
{
  namespace file_ns = blender::nodes::node_fn_input_special_characters_cc;

  static bNodeType ntype;

  fn_node_type_base(
      &ntype, FN_NODE_INPUT_SPECIAL_CHARACTERS, "Special Characters", NODE_CLASS_INPUT);
  ntype.declare = file_ns::fn_node_input_special_characters_declare;
  ntype.build_multi_function = file_ns::fn_node_input_special_characters_build_multi_function;
  nodeRegisterType(&ntype);
}
