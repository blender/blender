/* SPDX-License-Identifier: GPL-2.0-or-later */

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

}  // namespace blender::nodes::node_fn_input_special_characters_cc

void register_node_type_fn_input_special_characters()
{
  namespace file_ns = blender::nodes::node_fn_input_special_characters_cc;

  static bNodeType ntype;

  fn_node_type_base(
      &ntype, FN_NODE_INPUT_SPECIAL_CHARACTERS, "Special Characters", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  nodeRegisterType(&ntype);
}
