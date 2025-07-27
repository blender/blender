/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "NOD_geometry_nodes_closure_eval.hh"
#include "NOD_geometry_nodes_lazy_function.hh"

#include "BKE_compute_contexts.hh"
#include "BKE_geometry_nodes_reference_set.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_node_tree_reference_lifetimes.hh"

#include "NOD_geo_closure.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_values.hh"

#include "DEG_depsgraph_query.hh"

#include "FN_lazy_function_execute.hh"

namespace blender::nodes {

using bke::node_tree_reference_lifetimes::ReferenceSetInfo;
using bke::node_tree_reference_lifetimes::ReferenceSetType;

/**
 * Evaluating a closure lazy function creates a wrapper lazy function graph around it which handles
 * things like type conversion and missing inputs. This side effect provider is used to make sure
 * that if the closure itself contains a side-effect node (e.g. a viewer), the wrapper graph will
 * also have a side-effect node. Otherwise, the inner side-effect node will not be executed in some
 * cases.
 */
class ClosureIntermediateGraphSideEffectProvider : public lf::GraphExecutorSideEffectProvider {
 private:
  /**
   * The node that is wrapped and should be marked as having side effects if the closure
   * itself has side effects.
   */
  const lf::FunctionNode *body_node_;

 public:
  ClosureIntermediateGraphSideEffectProvider(const lf::FunctionNode &body_node)
      : body_node_(&body_node)
  {
  }

  Vector<const lf::FunctionNode *> get_nodes_with_side_effects(
      const lf::Context &context) const override
  {
    const GeoNodesUserData &user_data = *dynamic_cast<GeoNodesUserData *>(context.user_data);
    const ComputeContextHash &context_hash = user_data.compute_context->hash();
    if (!user_data.call_data->side_effect_nodes) {
      /* There are no requested side effect nodes at all. */
      return {};
    }
    const Span<const lf::FunctionNode *> side_effect_nodes_in_closure =
        user_data.call_data->side_effect_nodes->nodes_by_context.lookup(context_hash);
    if (side_effect_nodes_in_closure.is_empty()) {
      /* The closure does not have any side effect nodes, so the wrapper also does not have any. */
      return {};
    }
    return {body_node_};
  }
};

/**
 * A lazy function that internally has a lazy-function graph that mimics the "body" of the closure
 * zone.
 */
class LazyFunctionForClosureZone : public LazyFunction {
 private:
  const bNodeTree &btree_;
  const bke::bNodeTreeZone &zone_;
  const bNode &output_bnode_;
  const ZoneBuildInfo &zone_info_;
  const ZoneBodyFunction &body_fn_;
  std::shared_ptr<ClosureSignature> closure_signature_;

 public:
  LazyFunctionForClosureZone(const bNodeTree &btree,
                             const bke::bNodeTreeZone &zone,
                             ZoneBuildInfo &zone_info,
                             const ZoneBodyFunction &body_fn)
      : btree_(btree),
        zone_(zone),
        output_bnode_(*zone.output_node()),
        zone_info_(zone_info),
        body_fn_(body_fn)
  {
    debug_name_ = "Closure Zone";

    initialize_zone_wrapper(zone, zone_info, body_fn, false, inputs_, outputs_);
    for (const auto item : body_fn.indices.inputs.reference_sets.items()) {
      const ReferenceSetInfo &reference_set =
          btree.runtime->reference_lifetimes_info->reference_sets[item.key];
      if (reference_set.type == ReferenceSetType::ClosureInputReferenceSet) {
        BLI_assert(&reference_set.socket->owner_node() != zone_.input_node());
      }
      if (reference_set.type == ReferenceSetType::ClosureOutputData) {
        if (&reference_set.socket->owner_node() == zone_.output_node()) {
          /* This reference set comes from the caller of the closure and is not captured at the
           * place where the closure is created. */
          continue;
        }
      }
      zone_info.indices.inputs.reference_sets.add_new(
          item.key,
          inputs_.append_and_get_index_as("Reference Set",
                                          CPPType::get<bke::GeometryNodesReferenceSet>()));
    }

    /* All border links are used. */
    for (const int i : zone_.border_links.index_range()) {
      inputs_[zone_info.indices.inputs.border_links[i]].usage = lf::ValueUsage::Used;
    }

    const auto &storage = *static_cast<const NodeGeometryClosureOutput *>(output_bnode_.storage);

    closure_signature_ = std::make_shared<ClosureSignature>();

    for (const int i : IndexRange(storage.input_items.items_num)) {
      const bNodeSocket &bsocket = zone_.input_node()->output_socket(i);
      closure_signature_->inputs.append({bsocket.name, bsocket.typeinfo});
    }
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const bNodeSocket &bsocket = zone_.output_node()->input_socket(i);
      closure_signature_->outputs.append({bsocket.name, bsocket.typeinfo});
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    auto &user_data = *static_cast<GeoNodesUserData *>(context.user_data);

    /* All border links are captured currently. */
    for (const int i : zone_.border_links.index_range()) {
      params.set_output(zone_info_.indices.outputs.border_link_usages[i], true);
    }
    if (!U.experimental.use_bundle_and_closure_nodes) {
      params.set_output(zone_info_.indices.outputs.main[0],
                        bke::SocketValueVariant::From(ClosurePtr()));
      return;
    }

    const auto &storage = *static_cast<const NodeGeometryClosureOutput *>(output_bnode_.storage);

    std::unique_ptr<ResourceScope> closure_scope = std::make_unique<ResourceScope>();

    lf::Graph &lf_graph = closure_scope->construct<lf::Graph>("Closure Graph");
    lf::FunctionNode &lf_body_node = lf_graph.add_function(*body_fn_.function);
    ClosureFunctionIndices closure_indices;
    Vector<const void *> default_input_values;

    for (const int i : IndexRange(storage.input_items.items_num)) {
      const NodeGeometryClosureInputItem &item = storage.input_items.items[i];
      const bNodeSocket &bsocket = zone_.input_node()->output_socket(i);
      const CPPType &cpp_type = *bsocket.typeinfo->geometry_nodes_cpp_type;

      lf::GraphInputSocket &lf_graph_input = lf_graph.add_input(cpp_type, item.name);
      lf_graph.add_link(lf_graph_input, lf_body_node.input(body_fn_.indices.inputs.main[i]));

      lf::GraphOutputSocket &lf_graph_input_usage = lf_graph.add_output(
          CPPType::get<bool>(), "Usage: " + StringRef(item.name));
      lf_graph.add_link(lf_body_node.output(body_fn_.indices.outputs.input_usages[i]),
                        lf_graph_input_usage);

      void *default_value = closure_scope->allocate_owned(cpp_type);
      construct_socket_default_value(*bsocket.typeinfo, default_value);
      default_input_values.append(default_value);
    }
    closure_indices.inputs.main = lf_graph.graph_inputs().index_range().take_back(
        storage.input_items.items_num);
    closure_indices.outputs.input_usages = lf_graph.graph_outputs().index_range().take_back(
        storage.input_items.items_num);

    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeGeometryClosureOutputItem &item = storage.output_items.items[i];
      const bNodeSocket &bsocket = zone_.output_node()->input_socket(i);
      const CPPType &cpp_type = *bsocket.typeinfo->geometry_nodes_cpp_type;

      lf::GraphOutputSocket &lf_graph_output = lf_graph.add_output(cpp_type, item.name);
      lf_graph.add_link(lf_body_node.output(body_fn_.indices.outputs.main[i]), lf_graph_output);

      lf::GraphInputSocket &lf_graph_output_usage = lf_graph.add_input(
          CPPType::get<bool>(), "Usage: " + StringRef(item.name));
      lf_graph.add_link(lf_graph_output_usage,
                        lf_body_node.input(body_fn_.indices.inputs.output_usages[i]));
    }
    closure_indices.outputs.main = lf_graph.graph_outputs().index_range().take_back(
        storage.output_items.items_num);
    closure_indices.inputs.output_usages = lf_graph.graph_inputs().index_range().take_back(
        storage.output_items.items_num);

    for (const int i : zone_.border_links.index_range()) {
      const CPPType &cpp_type = *zone_.border_links[i]->tosock->typeinfo->geometry_nodes_cpp_type;
      void *input_ptr = params.try_get_input_data_ptr(zone_info_.indices.inputs.border_links[i]);
      void *stored_ptr = closure_scope->allocate_owned(cpp_type);
      cpp_type.move_construct(input_ptr, stored_ptr);
      lf_body_node.input(body_fn_.indices.inputs.border_links[i]).set_default_value(stored_ptr);
    }

    for (const auto &item : body_fn_.indices.inputs.reference_sets.items()) {
      const ReferenceSetInfo &reference_set =
          btree_.runtime->reference_lifetimes_info->reference_sets[item.key];
      if (reference_set.type == ReferenceSetType::ClosureOutputData) {
        const bNodeSocket &socket = *reference_set.socket;
        const bNode &node = socket.owner_node();
        if (&node == zone_.output_node()) {
          /* This reference set is passed in by the code that invokes the closure. */
          lf::GraphInputSocket &lf_graph_input = lf_graph.add_input(
              CPPType::get<bke::GeometryNodesReferenceSet>(),
              StringRef("Reference Set: ") + reference_set.socket->name);
          lf_graph.add_link(
              lf_graph_input,
              lf_body_node.input(body_fn_.indices.inputs.reference_sets.lookup(item.key)));
          closure_indices.inputs.output_data_reference_sets.add_new(reference_set.socket->index(),
                                                                    lf_graph_input.index());
          continue;
        }
      }

      auto &input_reference_set = *params.try_get_input_data_ptr<bke::GeometryNodesReferenceSet>(
          zone_info_.indices.inputs.reference_sets.lookup(item.key));
      auto &stored = closure_scope->construct<bke::GeometryNodesReferenceSet>(
          std::move(input_reference_set));
      lf_body_node.input(body_fn_.indices.inputs.reference_sets.lookup(item.key))
          .set_default_value(&stored);
    }

    const bNodeTree &btree_orig = *DEG_get_original(&btree_);
    if (btree_orig.runtime->logged_zone_graphs) {
      std::lock_guard lock{btree_orig.runtime->logged_zone_graphs->mutex};
      btree_orig.runtime->logged_zone_graphs->graph_by_zone_id.lookup_or_add_cb(
          output_bnode_.identifier, [&]() { return lf_graph.to_dot(); });
    }

    lf_graph.update_node_indices();

    const auto &side_effect_provider =
        closure_scope->construct<ClosureIntermediateGraphSideEffectProvider>(lf_body_node);
    lf::GraphExecutor &lf_graph_executor = closure_scope->construct<lf::GraphExecutor>(
        lf_graph, nullptr, &side_effect_provider, nullptr);
    ClosureSourceLocation source_location{
        &btree_,
        output_bnode_.identifier,
        user_data.compute_context->hash(),
    };
    ClosurePtr closure{MEM_new<Closure>(__func__,
                                        closure_signature_,
                                        std::move(closure_scope),
                                        lf_graph_executor,
                                        closure_indices,
                                        std::move(default_input_values),
                                        source_location,
                                        std::make_shared<ClosureEvalLog>())};

    params.set_output(zone_info_.indices.outputs.main[0],
                      bke::SocketValueVariant::From(std::move(closure)));
  }
};

struct EvaluateClosureEvalStorage {
  ResourceScope scope;
  ClosurePtr closure;
  lf::Graph graph;
  std::optional<lf::GraphExecutor> graph_executor;
  std::optional<ClosureIntermediateGraphSideEffectProvider> side_effect_provider;
  void *graph_executor_storage = nullptr;
};

/**
 * A lazy function that is used to evaluate a passed in closure. Internally that has to build
 * another lazy-function graph, which "fixes" different orderings of inputs/outputs, handles
 * missing sockets and type conversions.
 */
class LazyFunctionForEvaluateClosureNode : public LazyFunction {
 private:
  const bNodeTree &btree_;
  const bNode &bnode_;
  EvaluateClosureFunctionIndices indices_;

 public:
  LazyFunctionForEvaluateClosureNode(const bNode &bnode)
      : btree_(bnode.owner_tree()), bnode_(bnode)
  {
    debug_name_ = bnode.name;
    for (const int i : bnode.input_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      indices_.inputs.main.append(inputs_.append_and_get_index_as(
          bsocket.name, *bsocket.typeinfo->geometry_nodes_cpp_type, lf::ValueUsage::Maybe));
      indices_.outputs.input_usages.append(
          outputs_.append_and_get_index_as("Usage", CPPType::get<bool>()));
    }
    /* The closure input is always used. */
    inputs_[indices_.inputs.main[0]].usage = lf::ValueUsage::Used;
    for (const int i : bnode.output_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      indices_.outputs.main.append(outputs_.append_and_get_index_as(
          bsocket.name, *bsocket.typeinfo->geometry_nodes_cpp_type));
      indices_.inputs.output_usages.append(
          inputs_.append_and_get_index_as("Usage", CPPType::get<bool>(), lf::ValueUsage::Maybe));
      if (bke::node_tree_reference_lifetimes::can_contain_referenced_data(
              eNodeSocketDatatype(bsocket.type)))
      {
        const int input_i = inputs_.append_and_get_index_as(
            "Reference Set",
            CPPType::get<bke::GeometryNodesReferenceSet>(),
            lf::ValueUsage::Maybe);
        indices_.inputs.reference_set_by_output.add(i, input_i);
      }
    }
  }

  EvaluateClosureFunctionIndices indices() const
  {
    return indices_;
  }

  void *init_storage(LinearAllocator<> &allocator) const override
  {
    return allocator.construct<EvaluateClosureEvalStorage>().release();
  }

  void destruct_storage(void *storage) const override
  {
    auto *s = static_cast<EvaluateClosureEvalStorage *>(storage);
    if (s->graph_executor_storage) {
      s->graph_executor->destruct_storage(s->graph_executor_storage);
    }
    std::destroy_at(s);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    const ScopedNodeTimer node_timer{context, bnode_};

    auto &user_data = *static_cast<GeoNodesUserData *>(context.user_data);
    auto &eval_storage = *static_cast<EvaluateClosureEvalStorage *>(context.storage);
    auto local_user_data = *static_cast<GeoNodesLocalUserData *>(context.local_user_data);

    if (!eval_storage.graph_executor) {
      if (this->is_recursive_call(user_data)) {
        if (geo_eval_log::GeoTreeLogger *tree_logger = local_user_data.try_get_tree_logger(
                user_data))
        {
          tree_logger->node_warnings.append(
              *tree_logger->allocator,
              {bnode_.identifier,
               {NodeWarningType::Error, TIP_("Recursive closure is not allowed")}});
        }
        this->set_default_outputs(params);
        return;
      }

      eval_storage.closure = params.extract_input<bke::SocketValueVariant>(indices_.inputs.main[0])
                                 .extract<ClosurePtr>();
      if (eval_storage.closure) {
        this->generate_closure_compatibility_warnings(*eval_storage.closure, context);
        this->initialize_execution_graph(eval_storage);

        const bNodeTree &btree_orig = *DEG_get_original(&btree_);
        ClosureEvalLocation eval_location{
            btree_orig.id.session_uid, bnode_.identifier, user_data.compute_context->hash()};
        eval_storage.closure->log_evaluation(eval_location);
      }
      else {
        /* If no closure is provided, the Evaluate Closure node behaves as if it was muted. So some
         * values may be passed through if there are internal links. */
        this->initialize_pass_through_graph(eval_storage);
      }
    }

    const std::optional<ClosureSourceLocation> closure_source_location =
        eval_storage.closure ? eval_storage.closure->source_location() : std::nullopt;

    bke::EvaluateClosureComputeContext closure_compute_context{
        user_data.compute_context, bnode_.identifier, &btree_, closure_source_location};
    GeoNodesUserData closure_user_data = user_data;
    closure_user_data.compute_context = &closure_compute_context;
    closure_user_data.log_socket_values = should_log_socket_values_for_context(
        user_data, closure_compute_context.hash());
    GeoNodesLocalUserData closure_local_user_data{closure_user_data};

    lf::Context eval_graph_context{
        eval_storage.graph_executor_storage, &closure_user_data, &closure_local_user_data};
    eval_storage.graph_executor->execute(params, eval_graph_context);
  }

  bool is_recursive_call(const GeoNodesUserData &user_data) const
  {
    for (const ComputeContext *context = user_data.compute_context; context;
         context = context->parent())
    {
      if (const auto *closure_context = dynamic_cast<const bke::EvaluateClosureComputeContext *>(
              context))
      {
        if (closure_context->node() == &bnode_) {
          return true;
        }
      }
    }
    return false;
  }

  void set_default_outputs(lf::Params &params) const
  {
    for (const bNodeSocket *bsocket : bnode_.output_sockets().drop_back(1)) {
      const int index = bsocket->index();
      set_default_value_for_output_socket(params, indices_.outputs.main[index], *bsocket);
    }
    for (const bNodeSocket *bsocket : bnode_.input_sockets().drop_back(1)) {
      params.set_output(indices_.outputs.input_usages[bsocket->index()], false);
    }
  }

  void generate_closure_compatibility_warnings(const Closure &closure,
                                               const lf::Context &context) const
  {
    const auto &node_storage = *static_cast<const NodeGeometryEvaluateClosure *>(bnode_.storage);
    const auto &user_data = *static_cast<GeoNodesUserData *>(context.user_data);
    const auto &local_user_data = *static_cast<GeoNodesLocalUserData *>(context.local_user_data);
    geo_eval_log::GeoTreeLogger *tree_logger = local_user_data.try_get_tree_logger(user_data);
    if (tree_logger == nullptr) {
      return;
    }
    const ClosureSignature &signature = closure.signature();
    for (const NodeGeometryEvaluateClosureInputItem &item :
         Span{node_storage.input_items.items, node_storage.input_items.items_num})
    {
      if (const std::optional<int> i = signature.find_input_index(item.name)) {
        const ClosureSignature::Item &closure_item = signature.inputs[*i];
        if (!btree_.typeinfo->validate_link(eNodeSocketDatatype(item.socket_type),
                                            eNodeSocketDatatype(closure_item.type->type)))
        {
          tree_logger->node_warnings.append(
              *tree_logger->allocator,
              {bnode_.identifier,
               {NodeWarningType::Error,
                fmt::format(fmt::runtime(TIP_("Closure input has incompatible type: \"{}\"")),
                            item.name)}});
        }
      }
      else {
        tree_logger->node_warnings.append(
            *tree_logger->allocator,
            {bnode_.identifier,
             {
                 NodeWarningType::Error,
                 fmt::format(fmt::runtime(TIP_("Closure does not have input: \"{}\"")), item.name),
             }});
      }
    }
    for (const NodeGeometryEvaluateClosureOutputItem &item :
         Span{node_storage.output_items.items, node_storage.output_items.items_num})
    {
      if (const std::optional<int> i = signature.find_output_index(item.name)) {
        const ClosureSignature::Item &closure_item = signature.outputs[*i];
        if (!btree_.typeinfo->validate_link(eNodeSocketDatatype(closure_item.type->type),
                                            eNodeSocketDatatype(item.socket_type)))
        {
          tree_logger->node_warnings.append(
              *tree_logger->allocator,
              {bnode_.identifier,
               {NodeWarningType::Error,
                fmt::format(fmt::runtime(TIP_("Closure output has incompatible type: \"{}\"")),
                            item.name)}});
        }
      }
      else {
        tree_logger->node_warnings.append(
            *tree_logger->allocator,
            {bnode_.identifier,
             {NodeWarningType::Error,
              fmt::format(fmt::runtime(TIP_("Closure does not have output: \"{}\"")),
                          item.name)}});
      }
    }
  }

  void initialize_execution_graph(EvaluateClosureEvalStorage &eval_storage) const
  {
    const auto &node_storage = *static_cast<const NodeGeometryEvaluateClosure *>(bnode_.storage);

    lf::Graph &lf_graph = eval_storage.graph;

    for (const lf::Input &input : inputs_) {
      lf_graph.add_input(*input.type, input.debug_name);
    }
    for (const lf::Output &output : outputs_) {
      lf_graph.add_output(*output.type, output.debug_name);
    }
    const Span<lf::GraphInputSocket *> lf_graph_inputs = lf_graph.graph_inputs();
    const Span<lf::GraphOutputSocket *> lf_graph_outputs = lf_graph.graph_outputs();

    const Closure &closure = *eval_storage.closure;
    const ClosureSignature &closure_signature = closure.signature();
    const ClosureFunctionIndices &closure_indices = closure.indices();

    Array<std::optional<int>> inputs_map(node_storage.input_items.items_num);
    for (const int i : inputs_map.index_range()) {
      inputs_map[i] = closure_signature.find_input_index(node_storage.input_items.items[i].name);
    }
    Array<std::optional<int>> outputs_map(node_storage.output_items.items_num);
    for (const int i : outputs_map.index_range()) {
      outputs_map[i] = closure_signature.find_output_index(
          node_storage.output_items.items[i].name);
    }

    lf::FunctionNode &lf_closure_node = lf_graph.add_function(closure.function());

    static constexpr bool static_true = true;
    static constexpr bool static_false = false;
    /* The closure input is always used. */
    lf_graph_outputs[indices_.outputs.input_usages[0]]->set_default_value(&static_true);

    for (const int input_item_i : IndexRange(node_storage.input_items.items_num)) {
      lf::GraphOutputSocket &lf_usage_output =
          *lf_graph_outputs[indices_.outputs.input_usages[input_item_i + 1]];
      if (const std::optional<int> mapped_i = inputs_map[input_item_i]) {
        const bke::bNodeSocketType &from_type = *bnode_.input_socket(input_item_i + 1).typeinfo;
        const bke::bNodeSocketType &to_type = *closure_signature.inputs[*mapped_i].type;
        lf::OutputSocket *lf_from = lf_graph_inputs[indices_.inputs.main[input_item_i + 1]];
        lf::InputSocket &lf_to = lf_closure_node.input(closure_indices.inputs.main[*mapped_i]);
        if (&from_type != &to_type) {
          if (const LazyFunction *conversion_fn = build_implicit_conversion_lazy_function(
                  from_type, to_type, eval_storage.scope))
          {
            /* The provided type when evaluating the closure may be different from what the closure
             * expects exactly, so do an implicit conversion. */
            lf::Node &conversion_node = lf_graph.add_function(*conversion_fn);
            lf_graph.add_link(*lf_from, conversion_node.input(0));
            lf_from = &conversion_node.output(0);
          }
          else {
            /* Use the default value if the provided input value is not compatible with what the
             * closure expects. */
            const void *default_value = closure.default_input_value(*mapped_i);
            BLI_assert(default_value);
            lf_to.set_default_value(default_value);
            lf_usage_output.set_default_value(&static_false);
            continue;
          }
        }
        lf_graph.add_link(*lf_from, lf_to);
        lf_graph.add_link(lf_closure_node.output(closure_indices.outputs.input_usages[*mapped_i]),
                          lf_usage_output);
      }
      else {
        lf_usage_output.set_default_value(&static_false);
      }
    }

    auto get_output_default_value = [&](const bke::bNodeSocketType &type) {
      void *fallback_value = eval_storage.scope.allocate_owned(*type.geometry_nodes_cpp_type);
      construct_socket_default_value(type, fallback_value);
      return fallback_value;
    };

    for (const int output_item_i : IndexRange(node_storage.output_items.items_num)) {
      lf::GraphOutputSocket &lf_main_output =
          *lf_graph_outputs[indices_.outputs.main[output_item_i]];
      const bke::bNodeSocketType &main_output_type = *bnode_.output_socket(output_item_i).typeinfo;
      if (const std::optional<int> mapped_i = outputs_map[output_item_i]) {
        const bke::bNodeSocketType &closure_output_type =
            *closure_signature.outputs[*mapped_i].type;
        lf::OutputSocket *lf_from = &lf_closure_node.output(
            closure_indices.outputs.main[*mapped_i]);
        if (&closure_output_type != &main_output_type) {
          if (const LazyFunction *conversion_fn = build_implicit_conversion_lazy_function(
                  closure_output_type, main_output_type, eval_storage.scope))
          {
            /* Convert the type of the value coming out of the closure to the output socket type of
             * the evaluation. */
            lf::Node &conversion_node = lf_graph.add_function(*conversion_fn);
            lf_graph.add_link(*lf_from, conversion_node.input(0));
            lf_from = &conversion_node.output(0);
          }
          else {
            /* The socket types are not compatible, so use the default value. */
            void *fallback_value = get_output_default_value(main_output_type);
            lf_main_output.set_default_value(fallback_value);
            continue;
          }
        }
        /* Link the output of the closure to the output of the entire evaluation. */
        lf_graph.add_link(*lf_from, lf_main_output);
        lf_graph.add_link(*lf_graph_inputs[indices_.inputs.output_usages[output_item_i]],
                          lf_closure_node.input(closure_indices.inputs.output_usages[*mapped_i]));
      }
      else {
        void *fallback_value = get_output_default_value(main_output_type);
        lf_main_output.set_default_value(fallback_value);
      }
    }

    for (const int i : closure_indices.inputs.main.index_range()) {
      lf::InputSocket &lf_closure_input = lf_closure_node.input(closure_indices.inputs.main[i]);
      if (lf_closure_input.origin()) {
        /* Handled already. */
        continue;
      }
      const void *default_value = closure.default_input_value(i);
      lf_closure_input.set_default_value(default_value);
    }

    static const bke::GeometryNodesReferenceSet static_empty_reference_set;
    for (const int i : closure_indices.outputs.main.index_range()) {
      lf::OutputSocket &lf_closure_output = lf_closure_node.output(
          closure_indices.outputs.main[i]);
      if (const std::optional<int> lf_reference_set_input_i =
              closure_indices.inputs.output_data_reference_sets.lookup_try(i))
      {
        lf::InputSocket &lf_reference_set_input = lf_closure_node.input(*lf_reference_set_input_i);
        const int node_output_i = outputs_map.as_span().first_index_try(i);
        if (node_output_i == -1) {
          lf_reference_set_input.set_default_value(&static_empty_reference_set);
        }
        else {
          if (const std::optional<int> lf_evaluate_node_reference_set_input_i =
                  indices_.inputs.reference_set_by_output.lookup_try(node_output_i))
          {
            lf_graph.add_link(*lf_graph_inputs[*lf_evaluate_node_reference_set_input_i],
                              lf_reference_set_input);
          }
          else {
            lf_reference_set_input.set_default_value(&static_empty_reference_set);
          }
        }
      }
      if (!lf_closure_output.targets().is_empty()) {
        /* Handled already. */
        continue;
      }
      lf_closure_node.input(closure_indices.inputs.output_usages[i])
          .set_default_value(&static_false);
    }

    lf_graph.update_node_indices();
    eval_storage.side_effect_provider.emplace(lf_closure_node);
    eval_storage.graph_executor.emplace(
        lf_graph, nullptr, &*eval_storage.side_effect_provider, nullptr);
    eval_storage.graph_executor_storage = eval_storage.graph_executor->init_storage(
        eval_storage.scope.allocator());

    /* Log graph for debugging purposes. */
    const bNodeTree &btree_orig = *DEG_get_original(&btree_);
    if (btree_orig.runtime->logged_zone_graphs) {
      std::lock_guard lock{btree_orig.runtime->logged_zone_graphs->mutex};
      btree_orig.runtime->logged_zone_graphs->graph_by_zone_id.lookup_or_add_cb(
          bnode_.identifier, [&]() { return lf_graph.to_dot(); });
    }
  }

  void initialize_pass_through_graph(EvaluateClosureEvalStorage &eval_storage) const
  {
    const auto &node_storage = *static_cast<const NodeGeometryEvaluateClosure *>(bnode_.storage);
    lf::Graph &lf_graph = eval_storage.graph;
    for (const lf::Input &input : inputs_) {
      lf_graph.add_input(*input.type, input.debug_name);
    }
    for (const lf::Output &output : outputs_) {
      lf_graph.add_output(*output.type, output.debug_name);
    }
    const Span<lf::GraphInputSocket *> lf_graph_inputs = lf_graph.graph_inputs();
    const Span<lf::GraphOutputSocket *> lf_graph_outputs = lf_graph.graph_outputs();

    for (const int output_item_i : IndexRange(node_storage.output_items.items_num)) {
      const bNodeSocket &output_bsocket = bnode_.output_socket(output_item_i);
      const bNodeSocket *input_bsocket = evaluate_closure_node_internally_linked_input(
          output_bsocket);
      lf::GraphOutputSocket &lf_main_output =
          *lf_graph_outputs[indices_.outputs.main[output_item_i]];
      lf::GraphInputSocket &lf_usage_input =
          *lf_graph_inputs[indices_.inputs.output_usages[output_item_i]];
      const bke::bNodeSocketType &output_type = *output_bsocket.typeinfo;
      if (input_bsocket) {
        lf::OutputSocket &lf_main_input =
            *lf_graph_inputs[indices_.inputs.main[input_bsocket->index()]];
        lf::GraphOutputSocket &lf_usage_output =
            *lf_graph_outputs[indices_.outputs.input_usages[input_bsocket->index()]];
        const bke::bNodeSocketType &input_type = *input_bsocket->typeinfo;
        if (&input_type == &output_type) {
          lf_graph.add_link(lf_main_input, lf_main_output);
          lf_graph.add_link(lf_usage_input, lf_usage_output);
          continue;
        }
        if (const LazyFunction *conversion_fn = build_implicit_conversion_lazy_function(
                input_type, output_type, eval_storage.scope))
        {
          lf::Node &conversion_node = lf_graph.add_function(*conversion_fn);
          lf_graph.add_link(lf_main_input, conversion_node.input(0));
          lf_graph.add_link(conversion_node.output(0), lf_main_output);
          lf_graph.add_link(lf_usage_input, lf_usage_output);
          continue;
        }
      }
      void *default_output_value = eval_storage.scope.allocate_owned(
          *output_type.geometry_nodes_cpp_type);
      construct_socket_default_value(output_type, default_output_value);
      lf_main_output.set_default_value(default_output_value);
    }

    static constexpr bool static_false = false;
    for (const int usage_i : indices_.outputs.input_usages) {
      lf::GraphOutputSocket &lf_usage_output = *lf_graph_outputs[usage_i];
      if (!lf_usage_output.origin()) {
        lf_usage_output.set_default_value(&static_false);
      }
    }

    lf_graph.update_node_indices();
    eval_storage.graph_executor.emplace(lf_graph, nullptr, nullptr, nullptr);
    eval_storage.graph_executor_storage = eval_storage.graph_executor->init_storage(
        eval_storage.scope.allocator());
  }
};

void evaluate_closure_eagerly(const Closure &closure, ClosureEagerEvalParams &params)
{
  const LazyFunction &fn = closure.function();
  const ClosureFunctionIndices &indices = closure.indices();
  const ClosureSignature &signature = closure.signature();
  const int fn_inputs_num = fn.inputs().size();
  const int fn_outputs_num = fn.outputs().size();

  ResourceScope scope;
  LinearAllocator<> &allocator = scope.allocator();

  GeoNodesLocalUserData local_user_data(*params.user_data);
  void *storage = fn.init_storage(allocator);
  lf::Context lf_context{storage, params.user_data, &local_user_data};

  Array<GMutablePointer> lf_input_values(fn_inputs_num);
  Array<GMutablePointer> lf_output_values(fn_outputs_num);
  Array<std::optional<lf::ValueUsage>> lf_input_usages(fn_inputs_num);
  Array<lf::ValueUsage> lf_output_usages(fn_outputs_num, lf::ValueUsage::Unused);
  Array<bool> lf_set_outputs(fn_outputs_num, false);

  Array<std::optional<int>> inputs_map(params.inputs.size());
  for (const int i : inputs_map.index_range()) {
    inputs_map[i] = signature.find_input_index(params.inputs[i].key);
  }
  Array<std::optional<int>> outputs_map(params.outputs.size());
  for (const int i : outputs_map.index_range()) {
    outputs_map[i] = signature.find_output_index(params.outputs[i].key);
  }

  for (const int input_item_i : params.inputs.index_range()) {
    ClosureEagerEvalParams::InputItem &item = params.inputs[input_item_i];
    if (const std::optional<int> mapped_i = inputs_map[input_item_i]) {
      const bke::bNodeSocketType &from_type = *item.type;
      const bke::bNodeSocketType &to_type = *signature.inputs[*mapped_i].type;
      const CPPType &to_cpp_type = *to_type.geometry_nodes_cpp_type;
      void *value = allocator.allocate(to_cpp_type);
      if (&from_type == &to_type) {
        to_cpp_type.copy_construct(item.value, value);
      }
      else {
        if (!implicitly_convert_socket_value(from_type, item.value, to_type, value)) {
          const void *default_value = closure.default_input_value(*mapped_i);
          to_cpp_type.copy_construct(default_value, value);
        }
      }
      lf_input_values[indices.inputs.main[*mapped_i]] = {to_cpp_type, value};
    }
    else {
      /* Provided input value is ignored. */
    }
  }
  for (const int output_item_i : params.outputs.index_range()) {
    if (const std::optional<int> mapped_i = outputs_map[output_item_i]) {
      /* Tell the closure that this output is used. */
      lf_input_values[indices.inputs.output_usages[*mapped_i]] = {
          CPPType::get<bool>(), allocator.construct<bool>(true).release()};
      lf_output_usages[indices.outputs.main[*mapped_i]] = lf::ValueUsage::Used;
    }
  }

  /* Set remaining main inputs to their default values. */
  for (const int main_input_i : indices.inputs.main.index_range()) {
    const int lf_input_i = indices.inputs.main[main_input_i];
    if (!lf_input_values[lf_input_i]) {
      const bke::bNodeSocketType &type = *signature.inputs[main_input_i].type;
      const CPPType &cpp_type = *type.geometry_nodes_cpp_type;
      const void *default_value = closure.default_input_value(main_input_i);
      void *value = allocator.allocate(cpp_type);
      cpp_type.copy_construct(default_value, value);
      lf_input_values[lf_input_i] = {cpp_type, value};
    }
    lf_output_values[indices.outputs.input_usages[main_input_i]] = allocator.allocate<bool>();
  }
  /* Set remaining output usages to false. */
  for (const int output_usage_i : indices.inputs.output_usages.index_range()) {
    const int lf_input_i = indices.inputs.output_usages[output_usage_i];
    if (!lf_input_values[lf_input_i]) {
      lf_input_values[lf_input_i] = {CPPType::get<bool>(),
                                     allocator.construct<bool>(false).release()};
    }
  }
  /** Set output data reference sets. */
  for (auto &&[main_output_i, lf_input_i] : indices.inputs.output_data_reference_sets.items()) {
    /* TODO: Propagate all attributes or let the caller decide. */
    auto *value = &scope.construct<bke::GeometryNodesReferenceSet>();
    lf_input_values[lf_input_i] = {value};
  }
  /** Set main outputs. */
  for (const int main_output_i : indices.outputs.main.index_range()) {
    const bke::bNodeSocketType &type = *signature.outputs[main_output_i].type;
    const CPPType &cpp_type = *type.geometry_nodes_cpp_type;
    lf_output_values[indices.outputs.main[main_output_i]] = {cpp_type,
                                                             allocator.allocate(cpp_type)};
  }

  lf::BasicParams lf_params{
      fn, lf_input_values, lf_output_values, lf_input_usages, lf_output_usages, lf_set_outputs};
  fn.execute(lf_params, lf_context);
  fn.destruct_storage(storage);

  for (const int output_item_i : params.outputs.index_range()) {
    ClosureEagerEvalParams::OutputItem &item = params.outputs[output_item_i];
    if (const std::optional<int> mapped_i = outputs_map[output_item_i]) {
      const bke::bNodeSocketType &from_type = *signature.outputs[*mapped_i].type;
      const bke::bNodeSocketType &to_type = *item.type;
      const CPPType &to_cpp_type = *to_type.geometry_nodes_cpp_type;
      void *computed_value = lf_output_values[indices.outputs.main[*mapped_i]].get();
      if (&from_type == &to_type) {
        to_cpp_type.move_construct(computed_value, item.value);
      }
      else {
        if (!implicitly_convert_socket_value(from_type, computed_value, to_type, item.value)) {
          construct_socket_default_value(to_type, item.value);
        }
      }
    }
    else {
      /* This output item is not computed by the closure, so set it to the default value. */
      construct_socket_default_value(*item.type, item.value);
    }
  }

  for (GMutablePointer value : lf_input_values) {
    if (value) {
      value.destruct();
    }
  }
  for (const int i : lf_output_values.index_range()) {
    if (lf_set_outputs[i]) {
      lf_output_values[i].destruct();
    }
  }
}

LazyFunction &build_closure_zone_lazy_function(ResourceScope &scope,
                                               const bNodeTree &btree,
                                               const bke::bNodeTreeZone &zone,
                                               ZoneBuildInfo &zone_info,
                                               const ZoneBodyFunction &body_fn)
{
  return scope.construct<LazyFunctionForClosureZone>(btree, zone, zone_info, body_fn);
}

EvaluateClosureFunction build_evaluate_closure_node_lazy_function(ResourceScope &scope,
                                                                  const bNode &bnode)
{
  EvaluateClosureFunction info;
  auto &fn = scope.construct<LazyFunctionForEvaluateClosureNode>(bnode);
  info.lazy_function = &fn;
  info.indices = fn.indices();
  return info;
}

}  // namespace blender::nodes
