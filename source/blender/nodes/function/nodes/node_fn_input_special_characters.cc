/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

namespace blender::nodes::node_fn_input_special_characters_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::String>("Line Break");
  b.add_output<decl::String>("Tab").translation_context(BLT_I18NCONTEXT_ID_TEXT);
}

class MF_SpecialCharacters : public mf::MultiFunction {
 public:
  MF_SpecialCharacters()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Special Characters", signature};
      builder.single_output<std::string>("Line Break");
      builder.single_output<std::string>("Tab");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    MutableSpan<std::string> lb = params.uninitialized_single_output<std::string>(0, "Line Break");
    MutableSpan<std::string> tab = params.uninitialized_single_output<std::string>(1, "Tab");

    mask.foreach_index([&](const int64_t i) {
      new (&lb[i]) std::string("\n");
      new (&tab[i]) std::string("\t");
    });
  }
};

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static MF_SpecialCharacters special_characters_fn;
  builder.set_matching_fn(special_characters_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(
      &ntype, "FunctionNodeInputSpecialCharacters", FN_NODE_INPUT_SPECIAL_CHARACTERS);
  ntype.ui_name = "Special Characters";
  ntype.ui_description =
      "Output string characters that cannot be typed directly with the keyboard";
  ntype.enum_name_legacy = "INPUT_SPECIAL_CHARACTERS";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.build_multi_function = node_build_multi_function;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_special_characters_cc
