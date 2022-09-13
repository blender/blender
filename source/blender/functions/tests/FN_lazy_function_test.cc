/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "FN_lazy_function_execute.hh"
#include "FN_lazy_function_graph.hh"
#include "FN_lazy_function_graph_executor.hh"

#include "BLI_task.h"
#include "BLI_timeit.hh"

namespace blender::fn::lazy_function::tests {

class AddLazyFunction : public LazyFunction {
 public:
  AddLazyFunction()
  {
    debug_name_ = "Add";
    inputs_.append({"A", CPPType::get<int>()});
    inputs_.append({"B", CPPType::get<int>()});
    outputs_.append({"Result", CPPType::get<int>()});
  }

  void execute_impl(Params &params, const Context &UNUSED(context)) const override
  {
    const int a = params.get_input<int>(0);
    const int b = params.get_input<int>(1);
    params.set_output(0, a + b);
  }
};

class StoreValueFunction : public LazyFunction {
 private:
  int *dst1_;
  int *dst2_;

 public:
  StoreValueFunction(int *dst1, int *dst2) : dst1_(dst1), dst2_(dst2)
  {
    debug_name_ = "Store Value";
    inputs_.append({"A", CPPType::get<int>()});
    inputs_.append({"B", CPPType::get<int>(), ValueUsage::Maybe});
  }

  void execute_impl(Params &params, const Context &UNUSED(context)) const override
  {
    *dst1_ = params.get_input<int>(0);
    if (int *value = params.try_get_input_data_ptr_or_request<int>(1)) {
      *dst2_ = *value;
    }
  }
};

class SimpleSideEffectProvider : public GraphExecutor::SideEffectProvider {
 private:
  Vector<const FunctionNode *> side_effect_nodes_;

 public:
  SimpleSideEffectProvider(Span<const FunctionNode *> side_effect_nodes)
      : side_effect_nodes_(side_effect_nodes)
  {
  }

  Vector<const FunctionNode *> get_nodes_with_side_effects(
      const Context &UNUSED(context)) const override
  {
    return side_effect_nodes_;
  }
};

TEST(lazy_function, SimpleAdd)
{
  const AddLazyFunction add_fn;
  int result = 0;
  execute_lazy_function_eagerly(add_fn, nullptr, std::make_tuple(30, 5), std::make_tuple(&result));
  EXPECT_EQ(result, 35);
}

TEST(lazy_function, SideEffects)
{
  BLI_task_scheduler_init();
  int dst1 = 0;
  int dst2 = 0;

  const AddLazyFunction add_fn;
  const StoreValueFunction store_fn{&dst1, &dst2};

  Graph graph;
  FunctionNode &add_node_1 = graph.add_function(add_fn);
  FunctionNode &add_node_2 = graph.add_function(add_fn);
  FunctionNode &store_node = graph.add_function(store_fn);
  DummyNode &input_node = graph.add_dummy({}, {&CPPType::get<int>()});

  graph.add_link(input_node.output(0), add_node_1.input(0));
  graph.add_link(input_node.output(0), add_node_2.input(0));
  graph.add_link(add_node_1.output(0), store_node.input(0));
  graph.add_link(add_node_2.output(0), store_node.input(1));

  const int value_10 = 10;
  const int value_100 = 100;
  add_node_1.input(1).set_default_value(&value_10);
  add_node_2.input(1).set_default_value(&value_100);

  graph.update_node_indices();

  SimpleSideEffectProvider side_effect_provider{{&store_node}};

  GraphExecutor executor_fn{graph, {&input_node.output(0)}, {}, nullptr, &side_effect_provider};
  execute_lazy_function_eagerly(executor_fn, nullptr, std::make_tuple(5), std::make_tuple());

  EXPECT_EQ(dst1, 15);
  EXPECT_EQ(dst2, 105);
}

}  // namespace blender::fn::lazy_function::tests
