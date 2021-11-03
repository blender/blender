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

#include "BLI_hash.h"

namespace blender::nodes {

static void fn_node_legacy_random_float_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("Min")).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("Max")).default_value(1.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Int>(N_("Seed")).min(-10000).max(10000);
  b.add_output<decl::Float>(N_("Value"));
};

}  // namespace blender::nodes

class RandomFloatFunction : public blender::fn::MultiFunction {
 public:
  RandomFloatFunction()
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Random float"};
    signature.single_input<float>("Min");
    signature.single_input<float>("Max");
    signature.single_input<int>("Seed");
    signature.single_output<float>("Value");
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &min_values = params.readonly_single_input<float>(0, "Min");
    const blender::VArray<float> &max_values = params.readonly_single_input<float>(1, "Max");
    const blender::VArray<int> &seeds = params.readonly_single_input<int>(2, "Seed");
    blender::MutableSpan<float> values = params.uninitialized_single_output<float>(3, "Value");

    for (int64_t i : mask) {
      const float min_value = min_values[i];
      const float max_value = max_values[i];
      const int seed = seeds[i];
      const float value = BLI_hash_int_01(static_cast<uint32_t>(seed));
      values[i] = value * (max_value - min_value) + min_value;
    }
  }
};

static void fn_node_legacy_random_float_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static RandomFloatFunction fn;
  builder.set_matching_fn(fn);
}

void register_node_type_fn_legacy_random_float()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_LEGACY_RANDOM_FLOAT, "Random Float", 0, 0);
  ntype.declare = blender::nodes::fn_node_legacy_random_float_declare;
  ntype.build_multi_function = fn_node_legacy_random_float_build_multi_function;
  nodeRegisterType(&ntype);
}
