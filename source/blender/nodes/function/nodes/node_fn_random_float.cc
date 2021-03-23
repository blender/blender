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

static bNodeSocketTemplate fn_node_random_float_in[] = {
    {SOCK_FLOAT, N_("Min"), 0.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Max"), 1.0f, 0.0f, 0.0f, 0.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_INT, N_("Seed"), 0, 0, 0, 0, -10000, 10000},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_random_float_out[] = {
    {SOCK_FLOAT, N_("Value")},
    {-1, ""},
};

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

static void fn_node_random_float_expand_in_mf_network(
    blender::nodes::NodeMFNetworkBuilder &builder)
{
  builder.construct_and_set_matching_fn<RandomFloatFunction>();
}

void register_node_type_fn_random_float()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_RANDOM_FLOAT, "Random Float", 0, 0);
  node_type_socket_templates(&ntype, fn_node_random_float_in, fn_node_random_float_out);
  ntype.expand_in_mf_network = fn_node_random_float_expand_in_mf_network;
  nodeRegisterType(&ntype);
}
