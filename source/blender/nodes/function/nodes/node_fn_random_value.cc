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

// #include "BLI_hash.h"
#include "BLI_noise.hh"

#include "node_function_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

static void fn_node_random_value_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Min")).supports_field();
  b.add_input<decl::Vector>(N_("Max")).default_value({1.0f, 1.0f, 1.0f}).supports_field();
  b.add_input<decl::Float>(N_("Min"), "Min_001").supports_field();
  b.add_input<decl::Float>(N_("Max"), "Max_001").default_value(1.0f).supports_field();
  b.add_input<decl::Int>(N_("Min"), "Min_002").min(-100000).max(100000).supports_field();
  b.add_input<decl::Int>(N_("Max"), "Max_002")
      .default_value(100)
      .min(-100000)
      .max(100000)
      .supports_field();
  b.add_input<decl::Float>(N_("Probability"))
      .min(0.0f)
      .max(1.0f)
      .default_value(0.5f)
      .subtype(PROP_FACTOR)
      .supports_field();
  b.add_input<decl::Int>(N_("ID")).implicit_field();
  b.add_input<decl::Int>(N_("Seed")).default_value(0).min(-10000).max(10000).supports_field();

  b.add_output<decl::Vector>(N_("Value")).dependent_field();
  b.add_output<decl::Float>(N_("Value"), "Value_001").dependent_field();
  b.add_output<decl::Int>(N_("Value"), "Value_002").dependent_field();
  b.add_output<decl::Bool>(N_("Value"), "Value_003").dependent_field();
}

static void fn_node_random_value_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
}

static void fn_node_random_value_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeRandomValue *data = (NodeRandomValue *)MEM_callocN(sizeof(NodeRandomValue), __func__);
  data->data_type = CD_PROP_FLOAT;
  node->storage = data;
}

static void fn_node_random_value_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeRandomValue &storage = *(const NodeRandomValue *)node->storage;
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);

  bNodeSocket *sock_min_vector = (bNodeSocket *)node->inputs.first;
  bNodeSocket *sock_max_vector = sock_min_vector->next;
  bNodeSocket *sock_min_float = sock_max_vector->next;
  bNodeSocket *sock_max_float = sock_min_float->next;
  bNodeSocket *sock_min_int = sock_max_float->next;
  bNodeSocket *sock_max_int = sock_min_int->next;
  bNodeSocket *sock_probability = sock_max_int->next;

  bNodeSocket *sock_out_vector = (bNodeSocket *)node->outputs.first;
  bNodeSocket *sock_out_float = sock_out_vector->next;
  bNodeSocket *sock_out_int = sock_out_float->next;
  bNodeSocket *sock_out_bool = sock_out_int->next;

  nodeSetSocketAvailability(sock_min_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_max_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_min_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_max_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_min_int, data_type == CD_PROP_INT32);
  nodeSetSocketAvailability(sock_max_int, data_type == CD_PROP_INT32);
  nodeSetSocketAvailability(sock_probability, data_type == CD_PROP_BOOL);

  nodeSetSocketAvailability(sock_out_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(sock_out_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(sock_out_int, data_type == CD_PROP_INT32);
  nodeSetSocketAvailability(sock_out_bool, data_type == CD_PROP_BOOL);
}

class RandomVectorFunction : public fn::MultiFunction {
 public:
  RandomVectorFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Random Value"};
    signature.single_input<float3>("Min");
    signature.single_input<float3>("Max");
    signature.single_input<int>("ID");
    signature.single_input<int>("Seed");
    signature.single_output<float3>("Value");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &min_values = params.readonly_single_input<float3>(0, "Min");
    const VArray<float3> &max_values = params.readonly_single_input<float3>(1, "Max");
    const VArray<int> &ids = params.readonly_single_input<int>(2, "ID");
    const VArray<int> &seeds = params.readonly_single_input<int>(3, "Seed");
    MutableSpan<float3> values = params.uninitialized_single_output<float3>(4, "Value");

    for (int64_t i : mask) {
      const float3 min_value = min_values[i];
      const float3 max_value = max_values[i];
      const int seed = seeds[i];
      const int id = ids[i];

      const float x = noise::hash_to_float(seed, id, 0);
      const float y = noise::hash_to_float(seed, id, 1);
      const float z = noise::hash_to_float(seed, id, 2);

      values[i] = float3(x, y, z) * (max_value - min_value) + min_value;
    }
  }
};

class RandomFloatFunction : public fn::MultiFunction {
 public:
  RandomFloatFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Random Value"};
    signature.single_input<float>("Min");
    signature.single_input<float>("Max");
    signature.single_input<int>("ID");
    signature.single_input<int>("Seed");
    signature.single_output<float>("Value");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float> &min_values = params.readonly_single_input<float>(0, "Min");
    const VArray<float> &max_values = params.readonly_single_input<float>(1, "Max");
    const VArray<int> &ids = params.readonly_single_input<int>(2, "ID");
    const VArray<int> &seeds = params.readonly_single_input<int>(3, "Seed");
    MutableSpan<float> values = params.uninitialized_single_output<float>(4, "Value");

    for (int64_t i : mask) {
      const float min_value = min_values[i];
      const float max_value = max_values[i];
      const int seed = seeds[i];
      const int id = ids[i];

      const float value = noise::hash_to_float(seed, id);
      values[i] = value * (max_value - min_value) + min_value;
    }
  }
};

class RandomIntFunction : public fn::MultiFunction {
 public:
  RandomIntFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Random Value"};
    signature.single_input<int>("Min");
    signature.single_input<int>("Max");
    signature.single_input<int>("ID");
    signature.single_input<int>("Seed");
    signature.single_output<int>("Value");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<int> &min_values = params.readonly_single_input<int>(0, "Min");
    const VArray<int> &max_values = params.readonly_single_input<int>(1, "Max");
    const VArray<int> &ids = params.readonly_single_input<int>(2, "ID");
    const VArray<int> &seeds = params.readonly_single_input<int>(3, "Seed");
    MutableSpan<int> values = params.uninitialized_single_output<int>(4, "Value");

    for (int64_t i : mask) {
      const float min_value = min_values[i];
      const float max_value = max_values[i];
      const int seed = seeds[i];
      const int id = ids[i];

      const float value = noise::hash_to_float(id, seed);
      values[i] = round_fl_to_int(value * (max_value - min_value) + min_value);
    }
  }
};

class RandomBoolFunction : public fn::MultiFunction {
 public:
  RandomBoolFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Random Value"};
    signature.single_input<float>("Probability");
    signature.single_input<int>("ID");
    signature.single_input<int>("Seed");
    signature.single_output<bool>("Value");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float> &probabilities = params.readonly_single_input<float>(0, "Probability");
    const VArray<int> &ids = params.readonly_single_input<int>(1, "ID");
    const VArray<int> &seeds = params.readonly_single_input<int>(2, "Seed");
    MutableSpan<bool> values = params.uninitialized_single_output<bool>(3, "Value");

    for (int64_t i : mask) {
      const int seed = seeds[i];
      const int id = ids[i];
      const float probability = probabilities[i];
      values[i] = noise::hash_to_float(id, seed) <= probability;
    }
  }
};

static void fn_node_random_value_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeRandomValue &storage = *(const NodeRandomValue *)builder.node().storage;
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);

  switch (data_type) {
    case CD_PROP_FLOAT3: {
      static RandomVectorFunction fn;
      builder.set_matching_fn(fn);
      break;
    }
    case CD_PROP_FLOAT: {
      static RandomFloatFunction fn;
      builder.set_matching_fn(fn);
      break;
    }
    case CD_PROP_INT32: {
      static RandomIntFunction fn;
      builder.set_matching_fn(fn);
      break;
    }
    case CD_PROP_BOOL: {
      static RandomBoolFunction fn;
      builder.set_matching_fn(fn);
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
}

}  // namespace blender::nodes

void register_node_type_fn_random_value()
{
  static bNodeType ntype;
  fn_node_type_base(&ntype, FN_NODE_RANDOM_VALUE, "Random Value", NODE_CLASS_CONVERTER, 0);
  node_type_init(&ntype, blender::nodes::fn_node_random_value_init);
  node_type_update(&ntype, blender::nodes::fn_node_random_value_update);
  ntype.draw_buttons = blender::nodes::fn_node_random_value_layout;
  ntype.declare = blender::nodes::fn_node_random_value_declare;
  ntype.build_multi_function = blender::nodes::fn_node_random_value_build_multi_function;
  node_type_storage(
      &ntype, "NodeRandomValue", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
