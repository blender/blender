/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 *
 * This file mainly converts a #bNodeTree into a lazy-function graph. This generally works by
 * creating a lazy-function for every node, which is then put into the lazy-function graph. Then
 * the nodes in the new graph are linked based on links in the original #bNodeTree. Some additional
 * nodes are inserted for things like type conversions and multi-input sockets.
 *
 * Currently, lazy-functions are even created for nodes that don't strictly require it, like
 * reroutes or muted nodes. In the future we could avoid that at the cost of additional code
 * complexity. So far, this does not seem to be a performance issue.
 */

#include "NOD_geometry_exec.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_multi_function.hh"
#include "NOD_node_declaration.hh"

#include "BLI_bit_group_vector.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_cpp_types.hh"
#include "BLI_dot_export.hh"
#include "BLI_hash.h"
#include "BLI_hash_md5.h"
#include "BLI_lazy_threading.hh"
#include "BLI_map.hh"

#include "DNA_ID.h"

#include "BKE_compute_contexts.hh"
#include "BKE_geometry_set.hh"
#include "BKE_type_conversions.hh"

#include "FN_field_cpp_type.hh"
#include "FN_lazy_function_graph_executor.hh"

#include "DEG_depsgraph_query.h"

namespace blender::nodes {

using fn::ValueOrField;
using fn::ValueOrFieldCPPType;

static const CPPType *get_socket_cpp_type(const bNodeSocketType &typeinfo)
{
  const CPPType *type = typeinfo.geometry_nodes_cpp_type;
  if (type == nullptr) {
    return nullptr;
  }
  BLI_assert(type->has_special_member_functions());
  return type;
}

static const CPPType *get_socket_cpp_type(const bNodeSocket &socket)
{
  return get_socket_cpp_type(*socket.typeinfo);
}

static const CPPType *get_vector_type(const CPPType &type)
{
  const VectorCPPType *vector_type = VectorCPPType::get_from_value(type);
  if (vector_type == nullptr) {
    return nullptr;
  }
  return &vector_type->self;
}

/**
 * Checks which sockets of the node are available and creates corresponding inputs/outputs on the
 * lazy-function.
 */
static void lazy_function_interface_from_node(const bNode &node,
                                              Vector<lf::Input> &r_inputs,
                                              Vector<lf::Output> &r_outputs,
                                              MutableSpan<int> r_lf_index_by_bsocket)
{
  const bool is_muted = node.is_muted();
  const lf::ValueUsage input_usage = lf::ValueUsage::Used;
  for (const bNodeSocket *socket : node.input_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    const CPPType *type = get_socket_cpp_type(*socket);
    if (type == nullptr) {
      continue;
    }
    if (socket->is_multi_input() && !is_muted) {
      type = get_vector_type(*type);
    }
    r_lf_index_by_bsocket[socket->index_in_tree()] = r_inputs.append_and_get_index_as(
        socket->identifier, *type, input_usage);
  }
  for (const bNodeSocket *socket : node.output_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    const CPPType *type = get_socket_cpp_type(*socket);
    if (type == nullptr) {
      continue;
    }
    r_lf_index_by_bsocket[socket->index_in_tree()] = r_outputs.append_and_get_index_as(
        socket->identifier, *type);
  }
}

NodeAnonymousAttributeID::NodeAnonymousAttributeID(const Object &object,
                                                   const ComputeContext &compute_context,
                                                   const bNode &bnode,
                                                   const StringRef identifier,
                                                   const StringRef name)
    : socket_name_(name)
{
  const ComputeContextHash &hash = compute_context.hash();
  {
    std::stringstream ss;
    ss << hash << "_" << object.id.name << "_" << bnode.identifier << "_" << identifier;
    long_name_ = ss.str();
  }
  {
    uint64_t hash_result[2];
    BLI_hash_md5_buffer(long_name_.data(), long_name_.size(), hash_result);
    std::stringstream ss;
    ss << ".a_" << std::hex << hash_result[0] << hash_result[1];
    name_ = ss.str();
    BLI_assert(name_.size() < MAX_CUSTOMDATA_LAYER_NAME);
  }
}

std::string NodeAnonymousAttributeID::user_name() const
{
  return socket_name_;
}

/**
 * Used for most normal geometry nodes like Subdivision Surface and Set Position.
 */
class LazyFunctionForGeometryNode : public LazyFunction {
 private:
  const bNode &node_;
  const GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info_;
  /**
   * A bool for every output bsocket. If true, the socket just outputs a field containing an
   * anonymous attribute id. If only such outputs are requested by other nodes, the node itself
   * does not have to execute.
   */
  Vector<bool> is_attribute_output_bsocket_;

  struct OutputAttributeID {
    int bsocket_index;
    AnonymousAttributeIDPtr attribute_id;
  };

  struct Storage {
    Vector<OutputAttributeID, 1> attributes;
  };

 public:
  LazyFunctionForGeometryNode(const bNode &node,
                              GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
      : node_(node),
        own_lf_graph_info_(own_lf_graph_info),
        is_attribute_output_bsocket_(node.output_sockets().size(), false)
  {
    BLI_assert(node.typeinfo->geometry_node_execute != nullptr);
    debug_name_ = node.name;
    lazy_function_interface_from_node(
        node, inputs_, outputs_, own_lf_graph_info.mapping.lf_index_by_bsocket);

    const NodeDeclaration &node_decl = *node.declaration();
    const aal::RelationsInNode *relations = node_decl.anonymous_attribute_relations();
    if (relations == nullptr) {
      return;
    }
    if (!relations->available_relations.is_empty()) {
      /* Inputs are only used when an output is used that is not just outputting an anonymous
       * attribute field. */
      for (lf::Input &input : inputs_) {
        input.usage = lf::ValueUsage::Maybe;
      }
      for (const aal::AvailableRelation &relation : relations->available_relations) {
        is_attribute_output_bsocket_[relation.field_output] = true;
      }
    }
    Vector<const bNodeSocket *> handled_field_outputs;
    for (const aal::AvailableRelation &relation : relations->available_relations) {
      const bNodeSocket &output_bsocket = node.output_socket(relation.field_output);
      if (output_bsocket.is_available() && !handled_field_outputs.contains(&output_bsocket)) {
        handled_field_outputs.append(&output_bsocket);
        const int lf_index = inputs_.append_and_get_index_as("Output Used", CPPType::get<bool>());
        own_lf_graph_info.mapping
            .lf_input_index_for_output_bsocket_usage[output_bsocket.index_in_all_outputs()] =
            lf_index;
      }
    }

    Vector<const bNodeSocket *> handled_geometry_outputs;
    for (const aal::PropagateRelation &relation : relations->propagate_relations) {
      const bNodeSocket &output_bsocket = node.output_socket(relation.to_geometry_output);
      if (output_bsocket.is_available() && !handled_geometry_outputs.contains(&output_bsocket)) {
        handled_geometry_outputs.append(&output_bsocket);
        const int lf_index = inputs_.append_and_get_index_as(
            "Propagate to Output", CPPType::get<bke::AnonymousAttributeSet>());
        own_lf_graph_info.mapping.lf_input_index_for_attribute_propagation_to_output
            [output_bsocket.index_in_all_outputs()] = lf_index;
      }
    }
  }

  void *init_storage(LinearAllocator<> &allocator) const override
  {
    return allocator.construct<Storage>().release();
  }

  void destruct_storage(void *storage) const override
  {
    Storage *s = static_cast<Storage *>(storage);
    std::destroy_at(s);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    Storage *storage = static_cast<Storage *>(context.storage);
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);
    const auto &local_user_data = *static_cast<GeoNodesLFLocalUserData *>(context.local_user_data);

    /* Lazily create the required anonymous attribute ids. */
    auto get_output_attribute_id = [&](const int output_bsocket_index) -> AnonymousAttributeIDPtr {
      for (const OutputAttributeID &node_output_attribute : storage->attributes) {
        if (node_output_attribute.bsocket_index == output_bsocket_index) {
          return node_output_attribute.attribute_id;
        }
      }
      const bNodeSocket &bsocket = node_.output_socket(output_bsocket_index);
      AnonymousAttributeIDPtr attribute_id = MEM_new<NodeAnonymousAttributeID>(
          __func__,
          *user_data->modifier_data->self_object,
          *user_data->compute_context,
          node_,
          bsocket.identifier,
          bsocket.name);
      storage->attributes.append({output_bsocket_index, attribute_id});
      return attribute_id;
    };

    bool used_non_attribute_output_exists = false;
    for (const int output_bsocket_index : node_.output_sockets().index_range()) {
      const bNodeSocket &output_bsocket = node_.output_socket(output_bsocket_index);
      const int lf_index =
          own_lf_graph_info_.mapping.lf_index_by_bsocket[output_bsocket.index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      const lf::ValueUsage output_usage = params.get_output_usage(lf_index);
      if (output_usage == lf::ValueUsage::Unused) {
        continue;
      }
      if (is_attribute_output_bsocket_[output_bsocket_index]) {
        if (params.output_was_set(lf_index)) {
          continue;
        }
        this->output_anonymous_attribute_field(
            params, lf_index, get_output_attribute_id(output_bsocket_index));
      }
      else {
        if (output_usage == lf::ValueUsage::Used) {
          used_non_attribute_output_exists = true;
        }
      }
    }

    if (!used_non_attribute_output_exists) {
      /* Only attribute outputs are used currently, no need to evaluate the full node and its
       * inputs. */
      return;
    }

    bool missing_input = false;
    for (const int lf_index : inputs_.index_range()) {
      if (params.try_get_input_data_ptr_or_request(lf_index) == nullptr) {
        missing_input = true;
      }
    }
    if (missing_input) {
      /* Wait until all inputs are available. */
      return;
    }

    GeoNodeExecParams geo_params{
        node_,
        params,
        context,
        own_lf_graph_info_.mapping.lf_input_index_for_output_bsocket_usage,
        own_lf_graph_info_.mapping.lf_input_index_for_attribute_propagation_to_output,
        get_output_attribute_id};

    geo_eval_log::TimePoint start_time = geo_eval_log::Clock::now();
    node_.typeinfo->geometry_node_execute(geo_params);
    geo_eval_log::TimePoint end_time = geo_eval_log::Clock::now();

    if (local_user_data.tree_logger) {
      local_user_data.tree_logger->node_execution_times.append(
          {node_.identifier, start_time, end_time});
    }
  }

  /**
   * Output the given anonymous attribute id as a field.
   */
  void output_anonymous_attribute_field(lf::Params &params,
                                        const int lf_index,
                                        AnonymousAttributeIDPtr attribute_id) const
  {
    const ValueOrFieldCPPType &value_or_field_cpp_type = *ValueOrFieldCPPType::get_from_self(
        *outputs_[lf_index].type);
    GField output_field{
        std::make_shared<AnonymousAttributeFieldInput>(std::move(attribute_id),
                                                       value_or_field_cpp_type.value,
                                                       node_.label_or_name() + TIP_(" node"))};
    void *r_value = params.get_output_data_ptr(lf_index);
    value_or_field_cpp_type.construct_from_field(r_value, std::move(output_field));
    params.output_set(lf_index);
  }

  std::string input_name(const int index) const override
  {
    for (const bNodeSocket *bsocket : node_.output_sockets()) {
      {
        const int lf_index =
            own_lf_graph_info_.mapping
                .lf_input_index_for_output_bsocket_usage[bsocket->index_in_all_outputs()];
        if (index == lf_index) {
          return StringRef("Use Output '") + bsocket->identifier + "'";
        }
      }
      {
        const int lf_index =
            own_lf_graph_info_.mapping.lf_input_index_for_attribute_propagation_to_output
                [bsocket->index_in_all_outputs()];
        if (index == lf_index) {
          return StringRef("Propagate to '") + bsocket->identifier + "'";
        }
      }
    }
    return inputs_[index].debug_name;
  }

  std::string output_name(const int index) const override
  {
    return outputs_[index].debug_name;
  }
};

/**
 * Used to gather all inputs of a multi-input socket. A separate node is necessary because
 * multi-inputs are not supported in lazy-function graphs.
 */
class LazyFunctionForMultiInput : public LazyFunction {
 private:
  const CPPType *base_type_;

 public:
  LazyFunctionForMultiInput(const bNodeSocket &socket)
  {
    debug_name_ = "Multi Input";
    base_type_ = get_socket_cpp_type(socket);
    BLI_assert(base_type_ != nullptr);
    BLI_assert(socket.is_multi_input());
    const bNodeTree &btree = socket.owner_tree();
    for (const bNodeLink *link : socket.directly_linked_links()) {
      if (link->is_muted() || !link->fromsock->is_available() ||
          bke::nodeIsDanglingReroute(&btree, link->fromnode))
      {
        continue;
      }
      inputs_.append({"Input", *base_type_});
    }
    const CPPType *vector_type = get_vector_type(*base_type_);
    BLI_assert(vector_type != nullptr);
    outputs_.append({"Output", *vector_type});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    /* Currently we only have multi-inputs for geometry and string sockets. This could be
     * generalized in the future. */
    base_type_->to_static_type_tag<GeometrySet, ValueOrField<std::string>>([&](auto type_tag) {
      using T = typename decltype(type_tag)::type;
      if constexpr (std::is_void_v<T>) {
        /* This type is not supported in this node for now. */
        BLI_assert_unreachable();
      }
      else {
        void *output_ptr = params.get_output_data_ptr(0);
        Vector<T> &values = *new (output_ptr) Vector<T>();
        for (const int i : inputs_.index_range()) {
          values.append(params.extract_input<T>(i));
        }
        params.output_set(0);
      }
    });
  }
};

/**
 * Simple lazy-function that just forwards the input.
 */
class LazyFunctionForRerouteNode : public LazyFunction {
 public:
  LazyFunctionForRerouteNode(const CPPType &type)
  {
    debug_name_ = "Reroute";
    inputs_.append({"Input", type});
    outputs_.append({"Output", type});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    void *input_value = params.try_get_input_data_ptr(0);
    void *output_value = params.get_output_data_ptr(0);
    BLI_assert(input_value != nullptr);
    BLI_assert(output_value != nullptr);
    const CPPType &type = *inputs_[0].type;
    type.move_construct(input_value, output_value);
    params.output_set(0);
  }
};

/**
 * Lazy functions for nodes whose type cannot be found. An undefined function just outputs default
 * values. It's useful to have so other parts of the conversion don't have to care about undefined
 * nodes.
 */
class LazyFunctionForUndefinedNode : public LazyFunction {
 public:
  LazyFunctionForUndefinedNode(const bNode &node, MutableSpan<int> r_lf_index_by_bsocket)
  {
    debug_name_ = "Undefined";
    Vector<lf::Input> dummy_inputs;
    lazy_function_interface_from_node(node, dummy_inputs, outputs_, r_lf_index_by_bsocket);
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    params.set_default_remaining_outputs();
  }
};

/**
 * Executes a multi-function. If all inputs are single values, the results will also be single
 * values. If any input is a field, the outputs will also be fields.
 */
static void execute_multi_function_on_value_or_field(
    const MultiFunction &fn,
    const std::shared_ptr<MultiFunction> &owned_fn,
    const Span<const ValueOrFieldCPPType *> input_types,
    const Span<const ValueOrFieldCPPType *> output_types,
    const Span<const void *> input_values,
    const Span<void *> output_values)
{
  BLI_assert(fn.param_amount() == input_types.size() + output_types.size());
  BLI_assert(input_types.size() == input_values.size());
  BLI_assert(output_types.size() == output_values.size());

  /* Check if any input is a field. */
  bool any_input_is_field = false;
  for (const int i : input_types.index_range()) {
    const ValueOrFieldCPPType &type = *input_types[i];
    const void *value_or_field = input_values[i];
    if (type.is_field(value_or_field)) {
      any_input_is_field = true;
      break;
    }
  }

  if (any_input_is_field) {
    /* Convert all inputs into fields, so that they can be used as input in the new field. */
    Vector<GField> input_fields;
    for (const int i : input_types.index_range()) {
      const ValueOrFieldCPPType &type = *input_types[i];
      const void *value_or_field = input_values[i];
      input_fields.append(type.as_field(value_or_field));
    }

    /* Construct the new field node. */
    std::shared_ptr<fn::FieldOperation> operation;
    if (owned_fn) {
      operation = fn::FieldOperation::Create(owned_fn, std::move(input_fields));
    }
    else {
      operation = fn::FieldOperation::Create(fn, std::move(input_fields));
    }

    /* Store the new fields in the output. */
    for (const int i : output_types.index_range()) {
      const ValueOrFieldCPPType &type = *output_types[i];
      void *value_or_field = output_values[i];
      type.construct_from_field(value_or_field, GField{operation, i});
    }
  }
  else {
    /* In this case, the multi-function is evaluated directly. */
    mf::ParamsBuilder params{fn, 1};
    mf::ContextBuilder context;

    for (const int i : input_types.index_range()) {
      const ValueOrFieldCPPType &type = *input_types[i];
      const void *value_or_field = input_values[i];
      const void *value = type.get_value_ptr(value_or_field);
      params.add_readonly_single_input(GPointer{type.value, value});
    }
    for (const int i : output_types.index_range()) {
      const ValueOrFieldCPPType &type = *output_types[i];
      void *value_or_field = output_values[i];
      type.self.default_construct(value_or_field);
      void *value = type.get_value_ptr(value_or_field);
      type.value.destruct(value);
      params.add_uninitialized_single_output(GMutableSpan{type.value, value, 1});
    }
    fn.call(IndexRange(1), params, context);
  }
}

/**
 * Behavior of muted nodes:
 * - Some inputs are forwarded to outputs without changes.
 * - Some inputs are converted to a different type which becomes the output.
 * - Some outputs are value initialized because they don't have a corresponding input.
 */
class LazyFunctionForMutedNode : public LazyFunction {
 private:
  Array<int> input_by_output_index_;

 public:
  LazyFunctionForMutedNode(const bNode &node, MutableSpan<int> r_lf_index_by_bsocket)
  {
    debug_name_ = "Muted";
    lazy_function_interface_from_node(node, inputs_, outputs_, r_lf_index_by_bsocket);
    for (lf::Input &fn_input : inputs_) {
      fn_input.usage = lf::ValueUsage::Maybe;
    }

    for (lf::Input &fn_input : inputs_) {
      fn_input.usage = lf::ValueUsage::Unused;
    }

    input_by_output_index_.reinitialize(outputs_.size());
    input_by_output_index_.fill(-1);
    for (const bNodeLink &internal_link : node.internal_links()) {
      const int input_i = r_lf_index_by_bsocket[internal_link.fromsock->index_in_tree()];
      const int output_i = r_lf_index_by_bsocket[internal_link.tosock->index_in_tree()];
      if (ELEM(-1, input_i, output_i)) {
        continue;
      }
      input_by_output_index_[output_i] = input_i;
      inputs_[input_i].usage = lf::ValueUsage::Maybe;
    }
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    for (const int output_i : outputs_.index_range()) {
      if (params.output_was_set(output_i)) {
        continue;
      }
      if (params.get_output_usage(output_i) != lf::ValueUsage::Used) {
        continue;
      }
      const CPPType &output_type = *outputs_[output_i].type;
      void *output_value = params.get_output_data_ptr(output_i);
      const int input_i = input_by_output_index_[output_i];
      if (input_i == -1) {
        /* The output does not have a corresponding input. */
        output_type.value_initialize(output_value);
        params.output_set(output_i);
        continue;
      }
      const void *input_value = params.try_get_input_data_ptr_or_request(input_i);
      if (input_value == nullptr) {
        continue;
      }
      const CPPType &input_type = *inputs_[input_i].type;
      if (input_type == output_type) {
        /* Forward the value as is. */
        input_type.copy_construct(input_value, output_value);
        params.output_set(output_i);
        continue;
      }
      /* Perform a type conversion and then format the value. */
      const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
      const auto *from_type = ValueOrFieldCPPType::get_from_self(input_type);
      const auto *to_type = ValueOrFieldCPPType::get_from_self(output_type);
      if (from_type != nullptr && to_type != nullptr) {
        if (conversions.is_convertible(from_type->value, to_type->value)) {
          const MultiFunction &multi_fn = *conversions.get_conversion_multi_function(
              mf::DataType::ForSingle(from_type->value), mf::DataType::ForSingle(to_type->value));
          execute_multi_function_on_value_or_field(
              multi_fn, {}, {from_type}, {to_type}, {input_value}, {output_value});
        }
        params.output_set(output_i);
        continue;
      }
      /* Use a value initialization if the conversion does not work. */
      output_type.value_initialize(output_value);
      params.output_set(output_i);
    }
  }
};

/**
 * Type conversions are generally implemented as multi-functions. This node checks if the input is
 * a field or single value and outputs a field or single value respectively.
 */
class LazyFunctionForMultiFunctionConversion : public LazyFunction {
 private:
  const MultiFunction &fn_;
  const ValueOrFieldCPPType &from_type_;
  const ValueOrFieldCPPType &to_type_;

 public:
  LazyFunctionForMultiFunctionConversion(const MultiFunction &fn,
                                         const ValueOrFieldCPPType &from,
                                         const ValueOrFieldCPPType &to)
      : fn_(fn), from_type_(from), to_type_(to)
  {
    debug_name_ = "Convert";
    inputs_.append({"From", from.self});
    outputs_.append({"To", to.self});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const void *from_value = params.try_get_input_data_ptr(0);
    void *to_value = params.get_output_data_ptr(0);
    BLI_assert(from_value != nullptr);
    BLI_assert(to_value != nullptr);

    execute_multi_function_on_value_or_field(
        fn_, {}, {&from_type_}, {&to_type_}, {from_value}, {to_value});

    params.output_set(0);
  }
};

/**
 * This lazy-function wraps nodes that are implemented as multi-function (mostly math nodes).
 */
class LazyFunctionForMultiFunctionNode : public LazyFunction {
 private:
  const NodeMultiFunctions::Item fn_item_;
  Vector<const ValueOrFieldCPPType *> input_types_;
  Vector<const ValueOrFieldCPPType *> output_types_;

 public:
  LazyFunctionForMultiFunctionNode(const bNode &node,
                                   NodeMultiFunctions::Item fn_item,
                                   MutableSpan<int> r_lf_index_by_bsocket)
      : fn_item_(std::move(fn_item))
  {
    BLI_assert(fn_item_.fn != nullptr);
    debug_name_ = node.name;
    lazy_function_interface_from_node(node, inputs_, outputs_, r_lf_index_by_bsocket);
    for (const lf::Input &fn_input : inputs_) {
      input_types_.append(ValueOrFieldCPPType::get_from_self(*fn_input.type));
    }
    for (const lf::Output &fn_output : outputs_) {
      output_types_.append(ValueOrFieldCPPType::get_from_self(*fn_output.type));
    }
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    Vector<const void *> input_values(inputs_.size());
    Vector<void *> output_values(outputs_.size());
    for (const int i : inputs_.index_range()) {
      input_values[i] = params.try_get_input_data_ptr(i);
    }
    for (const int i : outputs_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    execute_multi_function_on_value_or_field(
        *fn_item_.fn, fn_item_.owned_fn, input_types_, output_types_, input_values, output_values);
    for (const int i : outputs_.index_range()) {
      params.output_set(i);
    }
  }
};

/**
 * Some sockets have non-trivial implicit inputs (e.g. the Position input of the Set Position
 * node). Those are implemented as a separate node that outputs the value.
 */
class LazyFunctionForImplicitInput : public LazyFunction {
 private:
  /**
   * The function that generates the implicit input. The passed in memory is uninitialized.
   */
  std::function<void(void *)> init_fn_;

 public:
  LazyFunctionForImplicitInput(const CPPType &type, std::function<void(void *)> init_fn)
      : init_fn_(std::move(init_fn))
  {
    debug_name_ = "Input";
    outputs_.append({"Output", type});
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    void *value = params.get_output_data_ptr(0);
    init_fn_(value);
    params.output_set(0);
  }
};

/**
 * The viewer node does not have outputs. Instead it is executed because the executor knows that it
 * has side effects. The side effect is that the inputs to the viewer are logged.
 */
class LazyFunctionForViewerNode : public LazyFunction {
 private:
  const bNode &bnode_;
  /** The field is only logged when it is linked. */
  bool use_field_input_ = true;

 public:
  LazyFunctionForViewerNode(const bNode &bnode, MutableSpan<int> r_lf_index_by_bsocket)
      : bnode_(bnode)
  {
    debug_name_ = "Viewer";
    lazy_function_interface_from_node(bnode, inputs_, outputs_, r_lf_index_by_bsocket);

    /* Remove field input if it is not used. */
    for (const bNodeSocket *bsocket : bnode.input_sockets().drop_front(1)) {
      if (!bsocket->is_available()) {
        continue;
      }
      const Span<const bNodeLink *> links = bsocket->directly_linked_links();
      if (links.is_empty() ||
          bke::nodeIsDanglingReroute(&bnode.owner_tree(), links.first()->fromnode)) {
        use_field_input_ = false;
        inputs_.pop_last();
        r_lf_index_by_bsocket[bsocket->index_in_tree()] = -1;
      }
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    const auto &local_user_data = *static_cast<GeoNodesLFLocalUserData *>(context.local_user_data);
    if (local_user_data.tree_logger == nullptr) {
      return;
    }

    GeometrySet geometry = params.extract_input<GeometrySet>(0);
    const NodeGeometryViewer *storage = static_cast<NodeGeometryViewer *>(bnode_.storage);

    if (use_field_input_) {
      const void *value_or_field = params.try_get_input_data_ptr(1);
      BLI_assert(value_or_field != nullptr);
      const auto &value_or_field_type = *ValueOrFieldCPPType::get_from_self(*inputs_[1].type);
      GField field = value_or_field_type.as_field(value_or_field);
      const eAttrDomain domain = eAttrDomain(storage->domain);
      const StringRefNull viewer_attribute_name = ".viewer";
      if (domain == ATTR_DOMAIN_INSTANCE) {
        if (geometry.has_instances()) {
          GeometryComponent &component = geometry.get_component_for_write(
              GEO_COMPONENT_TYPE_INSTANCES);
          bke::try_capture_field_on_geometry(
              component, viewer_attribute_name, ATTR_DOMAIN_INSTANCE, field);
        }
      }
      else {
        geometry.modify_geometry_sets([&](GeometrySet &geometry) {
          for (const GeometryComponentType type :
               {GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE})
          {
            if (geometry.has(type)) {
              GeometryComponent &component = geometry.get_component_for_write(type);
              eAttrDomain used_domain = domain;
              if (used_domain == ATTR_DOMAIN_AUTO) {
                if (const std::optional<eAttrDomain> detected_domain =
                        bke::try_detect_field_domain(component, field))
                {
                  used_domain = *detected_domain;
                }
                else {
                  used_domain = ATTR_DOMAIN_POINT;
                }
              }
              bke::try_capture_field_on_geometry(
                  component, viewer_attribute_name, used_domain, field);
            }
          }
        });
      }
    }

    local_user_data.tree_logger->log_viewer_node(bnode_, std::move(geometry));
  }
};

/**
 * Outputs true when a specific viewer node is used in the current context and false otherwise.
 */
class LazyFunctionForViewerInputUsage : public LazyFunction {
 private:
  const lf::FunctionNode &lf_viewer_node_;

 public:
  LazyFunctionForViewerInputUsage(const lf::FunctionNode &lf_viewer_node)
      : lf_viewer_node_(lf_viewer_node)
  {
    debug_name_ = "Viewer Input Usage";
    outputs_.append_as("Viewer is Used", CPPType::get<bool>());
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);
    const ComputeContextHash &context_hash = user_data->compute_context->hash();
    const GeoNodesModifierData &modifier_data = *user_data->modifier_data;
    const Span<const lf::FunctionNode *> nodes_with_side_effects =
        modifier_data.side_effect_nodes->lookup(context_hash);

    const bool viewer_is_used = nodes_with_side_effects.contains(&lf_viewer_node_);
    params.set_output(0, viewer_is_used);
  }
};

class LazyFunctionForSimulationInputsUsage : public LazyFunction {

 public:
  LazyFunctionForSimulationInputsUsage()
  {
    debug_name_ = "Simulation Inputs Usage";
    outputs_.append_as("Is Initialization", CPPType::get<bool>());
    outputs_.append_as("Do Simulation Step", CPPType::get<bool>());
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    const GeoNodesLFUserData &user_data = *static_cast<GeoNodesLFUserData *>(context.user_data);
    const GeoNodesModifierData &modifier_data = *user_data.modifier_data;

    params.set_output(0,
                      modifier_data.current_simulation_state_for_write != nullptr &&
                          modifier_data.prev_simulation_state == nullptr);
    params.set_output(1, modifier_data.current_simulation_state_for_write != nullptr);
  }
};

/**
 * This lazy-function wraps a group node. Internally it just executes the lazy-function graph of
 * the referenced group.
 */
class LazyFunctionForGroupNode : public LazyFunction {
 private:
  const bNode &group_node_;
  const GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info_;
  bool has_many_nodes_ = false;
  std::optional<GeometryNodesLazyFunctionLogger> lf_logger_;
  std::optional<GeometryNodesLazyFunctionSideEffectProvider> lf_side_effect_provider_;
  std::optional<lf::GraphExecutor> graph_executor_;

  struct Storage {
    void *graph_executor_storage = nullptr;
    /* To avoid computing the hash more than once. */
    std::optional<ComputeContextHash> context_hash_cache;
  };

 public:
  /**
   * For every input bsocket there is a corresponding boolean output that indicates whether that
   * input is used.
   */
  Map<int, int> lf_output_for_input_bsocket_usage_;

  LazyFunctionForGroupNode(const bNode &group_node,
                           const GeometryNodesLazyFunctionGraphInfo &group_lf_graph_info,
                           GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
      : group_node_(group_node), own_lf_graph_info_(own_lf_graph_info)
  {
    debug_name_ = group_node.name;
    allow_missing_requested_inputs_ = true;

    lazy_function_interface_from_node(
        group_node, inputs_, outputs_, own_lf_graph_info.mapping.lf_index_by_bsocket);
    for (lf::Input &input : inputs_) {
      input.usage = lf::ValueUsage::Maybe;
    }

    has_many_nodes_ = group_lf_graph_info.num_inline_nodes_approximate > 1000;

    Vector<const lf::OutputSocket *> graph_inputs;
    /* Add inputs that also exist on the bnode. */
    graph_inputs.extend(group_lf_graph_info.mapping.group_input_sockets);

    /* Add a boolean input for every output bsocket that indicates whether that socket is used. */
    for (const int i : group_node.output_sockets().index_range()) {
      own_lf_graph_info.mapping.lf_input_index_for_output_bsocket_usage
          [group_node.output_socket(i).index_in_all_outputs()] = graph_inputs.append_and_get_index(
          group_lf_graph_info.mapping.group_output_used_sockets[i]);
      inputs_.append_as("Output is Used", CPPType::get<bool>(), lf::ValueUsage::Maybe);
    }
    graph_inputs.extend(group_lf_graph_info.mapping.group_output_used_sockets);

    /* Add an attribute set input for every output geometry socket that can propagate attributes
     * from inputs. */
    for (auto [output_index, lf_socket] :
         group_lf_graph_info.mapping.attribute_set_by_geometry_output.items())
    {
      const int lf_index = inputs_.append_and_get_index_as(
          "Attribute Set", CPPType::get<bke::AnonymousAttributeSet>(), lf::ValueUsage::Maybe);
      graph_inputs.append(lf_socket);
      own_lf_graph_info.mapping.lf_input_index_for_attribute_propagation_to_output
          [group_node_.output_socket(output_index).index_in_all_outputs()] = lf_index;
    }

    Vector<const lf::InputSocket *> graph_outputs;
    /* Add outputs that also exist on the bnode. */
    graph_outputs.extend(group_lf_graph_info.mapping.standard_group_output_sockets);
    /* Add a boolean output for every input bsocket that indicates whether that socket is used. */
    for (const int i : group_node.input_sockets().index_range()) {
      const InputUsageHint &input_usage_hint =
          group_lf_graph_info.mapping.group_input_usage_hints[i];
      if (input_usage_hint.type == InputUsageHintType::DynamicSocket) {
        const lf::InputSocket *lf_socket =
            group_lf_graph_info.mapping.group_input_usage_sockets[i];
        lf_output_for_input_bsocket_usage_.add_new(i,
                                                   graph_outputs.append_and_get_index(lf_socket));
        outputs_.append_as("Input is Used", CPPType::get<bool>());
      }
    }

    lf_logger_.emplace(group_lf_graph_info);
    lf_side_effect_provider_.emplace();
    graph_executor_.emplace(group_lf_graph_info.graph,
                            std::move(graph_inputs),
                            std::move(graph_outputs),
                            &*lf_logger_,
                            &*lf_side_effect_provider_);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
    BLI_assert(user_data != nullptr);

    if (has_many_nodes_) {
      /* If the called node group has many nodes, it's likely that executing it takes a while even
       * if every individual node is very small. */
      lazy_threading::send_hint();
    }

    Storage *storage = static_cast<Storage *>(context.storage);

    /* The compute context changes when entering a node group. */
    bke::NodeGroupComputeContext compute_context{
        user_data->compute_context, group_node_.identifier, storage->context_hash_cache};
    storage->context_hash_cache = compute_context.hash();

    GeoNodesLFUserData group_user_data = *user_data;
    group_user_data.compute_context = &compute_context;
    if (user_data->modifier_data->socket_log_contexts) {
      group_user_data.log_socket_values = user_data->modifier_data->socket_log_contexts->contains(
          compute_context.hash());
    }

    GeoNodesLFLocalUserData group_local_user_data{group_user_data};

    lf::Context group_context{
        storage->graph_executor_storage, &group_user_data, &group_local_user_data};

    graph_executor_->execute(params, group_context);
  }

  void *init_storage(LinearAllocator<> &allocator) const override
  {
    Storage *s = allocator.construct<Storage>().release();
    s->graph_executor_storage = graph_executor_->init_storage(allocator);
    return s;
  }

  void destruct_storage(void *storage) const override
  {
    Storage *s = static_cast<Storage *>(storage);
    graph_executor_->destruct_storage(s->graph_executor_storage);
    std::destroy_at(s);
  }

  std::string name() const override
  {
    std::stringstream ss;
    ss << "Group '" << (group_node_.id->name + 2) << "' (" << group_node_.name << ")";
    return ss.str();
  }

  std::string input_name(const int i) const override
  {
    if (i < group_node_.input_sockets().size()) {
      return group_node_.input_socket(i).name;
    }
    for (const bNodeSocket *bsocket : group_node_.output_sockets()) {
      {
        const int lf_index =
            own_lf_graph_info_.mapping
                .lf_input_index_for_output_bsocket_usage[bsocket->index_in_all_outputs()];
        if (i == lf_index) {
          return StringRef("Use Output '") + bsocket->identifier + "'";
        }
      }
      {
        const int lf_index =
            own_lf_graph_info_.mapping.lf_input_index_for_attribute_propagation_to_output
                [bsocket->index_in_all_outputs()];
        if (i == lf_index) {
          return StringRef("Propagate to '") + bsocket->identifier + "'";
        }
      }
    }
    return inputs_[i].debug_name;
  }

  std::string output_name(const int i) const override
  {
    if (i < group_node_.output_sockets().size()) {
      return group_node_.output_socket(i).name;
    }
    for (const auto [bsocket_index, lf_socket_index] : lf_output_for_input_bsocket_usage_.items())
    {
      if (i == lf_socket_index) {
        std::stringstream ss;
        ss << "'" << group_node_.input_socket(bsocket_index).name << "' input is used";
        return ss.str();
      }
    }
    return outputs_[i].debug_name;
  }
};

static GMutablePointer get_socket_default_value(LinearAllocator<> &allocator,
                                                const bNodeSocket &bsocket)
{
  const bNodeSocketType &typeinfo = *bsocket.typeinfo;
  const CPPType *type = get_socket_cpp_type(typeinfo);
  if (type == nullptr) {
    return {};
  }
  void *buffer = allocator.allocate(type->size(), type->alignment());
  typeinfo.get_geometry_nodes_cpp_value(bsocket, buffer);
  return {type, buffer};
}

class GroupInputDebugInfo : public lf::DummyDebugInfo {
 public:
  Vector<StringRef> socket_names;

  std::string node_name() const override
  {
    return "Group Input";
  }

  std::string output_name(const int i) const override
  {
    return this->socket_names[i];
  }
};

class GroupOutputDebugInfo : public lf::DummyDebugInfo {
 public:
  Vector<StringRef> socket_names;

  std::string node_name() const override
  {
    return "Group Output";
  }

  std::string input_name(const int i) const override
  {
    return this->socket_names[i];
  }
};

/**
 * Computes the logical or of the inputs and supports short-circuit evaluation (i.e. if the first
 * input is true already, the other inputs are not checked).
 */
class LazyFunctionForLogicalOr : public lf::LazyFunction {
 public:
  LazyFunctionForLogicalOr(const int inputs_num)
  {
    debug_name_ = "Logical Or";
    for ([[maybe_unused]] const int i : IndexRange(inputs_num)) {
      inputs_.append_as("Input", CPPType::get<bool>(), lf::ValueUsage::Maybe);
    }
    outputs_.append_as("Output", CPPType::get<bool>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    int first_unavailable_input = -1;
    for (const int i : inputs_.index_range()) {
      if (const bool *value = params.try_get_input_data_ptr<bool>(i)) {
        if (*value) {
          params.set_output(0, true);
          return;
        }
      }
      else {
        first_unavailable_input = i;
      }
    }
    if (first_unavailable_input == -1) {
      params.set_output(0, false);
      return;
    }
    params.try_get_input_data_ptr_or_request(first_unavailable_input);
  }
};

/**
 * Outputs booleans that indicate which inputs of a switch node are used. Note that it's possible
 * that both inputs are used when the condition is a field.
 */
class LazyFunctionForSwitchSocketUsage : public lf::LazyFunction {
 public:
  LazyFunctionForSwitchSocketUsage()
  {
    debug_name_ = "Switch Socket Usage";
    inputs_.append_as("Condition", CPPType::get<ValueOrField<bool>>());
    outputs_.append_as("False", CPPType::get<bool>());
    outputs_.append_as("True", CPPType::get<bool>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const ValueOrField<bool> &condition = params.get_input<ValueOrField<bool>>(0);
    if (condition.is_field()) {
      params.set_output(0, true);
      params.set_output(1, true);
    }
    else {
      const bool value = condition.as_value();
      params.set_output(0, !value);
      params.set_output(1, value);
    }
  }
};

/**
 * Takes a field as input and extracts the set of anonymous attributes that it references.
 */
class LazyFunctionForAnonymousAttributeSetExtract : public lf::LazyFunction {
 private:
  const ValueOrFieldCPPType &type_;

 public:
  LazyFunctionForAnonymousAttributeSetExtract(const ValueOrFieldCPPType &type) : type_(type)
  {
    debug_name_ = "Extract Attribute Set";
    inputs_.append_as("Field", type.self);
    outputs_.append_as("Attributes", CPPType::get<bke::AnonymousAttributeSet>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const void *value_or_field = params.try_get_input_data_ptr(0);
    bke::AnonymousAttributeSet attributes;
    if (type_.is_field(value_or_field)) {
      const GField &field = *type_.get_field_ptr(value_or_field);
      field.node().for_each_field_input_recursive([&](const FieldInput &field_input) {
        if (const auto *attr_field_input = dynamic_cast<const AnonymousAttributeFieldInput *>(
                &field_input))
        {
          if (!attributes.names) {
            attributes.names = std::make_shared<Set<std::string>>();
          }
          attributes.names->add_as(attr_field_input->anonymous_id()->name());
        }
      });
    }
    params.set_output(0, std::move(attributes));
  }
};

/**
 * Conditionally joins multiple attribute sets. Each input attribute set can be disabled with a
 * corresponding boolean input.
 */
class LazyFunctionForAnonymousAttributeSetJoin : public lf::LazyFunction {
  const int amount_;

 public:
  LazyFunctionForAnonymousAttributeSetJoin(const int amount) : amount_(amount)
  {
    debug_name_ = "Join Attribute Sets";
    for ([[maybe_unused]] const int i : IndexRange(amount)) {
      inputs_.append_as("Use", CPPType::get<bool>());
      inputs_.append_as(
          "Attribute Set", CPPType::get<bke::AnonymousAttributeSet>(), lf::ValueUsage::Maybe);
    }
    outputs_.append_as("Attribute Set", CPPType::get<bke::AnonymousAttributeSet>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    Vector<bke::AnonymousAttributeSet *> sets;
    bool set_is_missing = false;
    for (const int i : IndexRange(amount_)) {
      if (params.get_input<bool>(this->get_use_input(i))) {
        if (bke::AnonymousAttributeSet *set =
                params.try_get_input_data_ptr_or_request<bke::AnonymousAttributeSet>(
                    this->get_attribute_set_input(i)))
        {
          sets.append(set);
        }
        else {
          set_is_missing = true;
        }
      }
    }
    if (set_is_missing) {
      return;
    }
    bke::AnonymousAttributeSet joined_set;
    if (sets.is_empty()) {
      /* Nothing to do. */
    }
    else if (sets.size() == 1) {
      joined_set.names = std::move(sets[0]->names);
    }
    else {
      joined_set.names = std::make_shared<Set<std::string>>();
      for (const bke::AnonymousAttributeSet *set : sets) {
        if (set->names) {
          for (const std::string &name : *set->names) {
            joined_set.names->add(name);
          }
        }
      }
    }
    params.set_output(0, std::move(joined_set));
  }

  int get_use_input(const int i) const
  {
    return 2 * i;
  }

  int get_attribute_set_input(const int i) const
  {
    return 2 * i + 1;
  }

  /**
   * Cache for functions small amounts to avoid to avoid building them many times.
   */
  static const LazyFunctionForAnonymousAttributeSetJoin &get_cached(
      const int amount, Vector<std::unique_ptr<LazyFunction>> &r_functions)
  {
    constexpr int cache_amount = 16;
    static std::array<LazyFunctionForAnonymousAttributeSetJoin, cache_amount> cached_functions =
        get_cache(std::make_index_sequence<cache_amount>{});
    if (amount < cached_functions.size()) {
      return cached_functions[amount];
    }

    auto fn = std::make_unique<LazyFunctionForAnonymousAttributeSetJoin>(amount);
    const auto &fn_ref = *fn;
    r_functions.append(std::move(fn));
    return fn_ref;
  }

 private:
  template<size_t... I>
  static std::array<LazyFunctionForAnonymousAttributeSetJoin, sizeof...(I)> get_cache(
      std::index_sequence<I...> /*indices*/)
  {
    return {LazyFunctionForAnonymousAttributeSetJoin(I)...};
  }
};

enum class AttributeReferenceKeyType {
  /** Attribute referenced by a field passed into the group. */
  InputField,
  /** Attributes referenced on the output geometry outside of the current group. */
  OutputGeometry,
  /** Attribute referenced by a field created within the current group. */
  Socket,
};

/**
 * Identifier for something that can reference anonymous attributes that should be propagated.
 */
struct AttributeReferenceKey {
  AttributeReferenceKeyType type;
  /* Used when type is InputField or OutputGeometry. */
  int index = 0;
  /* Used when type is Socket. */
  const bNodeSocket *bsocket = nullptr;

  uint64_t hash() const
  {
    return get_default_hash_3(this->type, this->bsocket, this->index);
  }

  friend bool operator==(const AttributeReferenceKey &a, const AttributeReferenceKey &b)
  {
    return a.type == b.type && a.bsocket == b.bsocket && a.index == b.index;
  }

  friend std::ostream &operator<<(std::ostream &stream, const AttributeReferenceKey &value)
  {
    if (value.type == AttributeReferenceKeyType::InputField) {
      stream << "Input Field: " << value.index;
    }
    else if (value.type == AttributeReferenceKeyType::OutputGeometry) {
      stream << "Output Geometry: " << value.index;
    }
    else {
      stream << "Socket: " << value.bsocket->owner_node().name << " -> " << value.bsocket->name;
    }
    return stream;
  }
};

/**
 * Additional information that corresponds to an #AttributeReferenceKey.
 */
struct AttributeReferenceInfo {
  /** Output socket that contains an attribute set containing the referenced attributes. */
  lf::OutputSocket *lf_attribute_set_socket = nullptr;
  /** Geometry sockets that contain the referenced attributes. */
  Vector<const bNodeSocket *> initial_geometry_sockets;
};

/**
 * Utility class to build a lazy-function graph based on a geometry nodes tree.
 * This is mainly a separate class because it makes it easier to have variables that can be
 * accessed by many functions.
 */
struct GeometryNodesLazyFunctionGraphBuilder {
 private:
  const bNodeTree &btree_;
  GeometryNodesLazyFunctionGraphInfo *lf_graph_info_;
  lf::Graph *lf_graph_;
  GeometryNodeLazyFunctionGraphMapping *mapping_;
  MultiValueMap<const bNodeSocket *, lf::InputSocket *> input_socket_map_;
  Map<const bNodeSocket *, lf::OutputSocket *> output_socket_map_;
  Map<const bNodeSocket *, lf::Node *> multi_input_socket_nodes_;
  const bke::DataTypeConversions *conversions_;
  /**
   * Maps bsockets to boolean sockets in the graph whereby each boolean socket indicates whether
   * the bsocket is used. Sockets not contained in this map are not used.
   * This is indexed by `bNodeSocket::index_in_tree()`.
   */
  Array<lf::OutputSocket *> socket_is_used_map_;
  /**
   * Some built-in nodes get additional boolean inputs that indicate whether certain outputs are
   * used (field output sockets that contain new anonymous attribute references).
   */
  Vector<std::pair<const bNodeSocket *, lf::InputSocket *>> output_used_sockets_for_builtin_nodes_;
  /**
   * Maps from output geometry sockets to corresponding attribute set inputs.
   */
  Map<const bNodeSocket *, lf::InputSocket *> attribute_set_propagation_map_;
  /**
   * Boolean inputs that tell a node if some socket (of the same or another node) is used. If this
   * socket is in a link-cycle, its input can become a constant true.
   */
  Set<const lf::InputSocket *> socket_usage_inputs_;

  /**
   * All group input nodes are combined into one dummy node in the lazy-function graph.
   */
  lf::DummyNode *group_input_lf_node_;
  /**
   * A #LazyFunctionForSimulationInputsUsage for each simulation zone.
   */
  Map<const bNode *, lf::Node *> simulation_inputs_usage_nodes_;

  friend class UsedSocketVisualizeOptions;

 public:
  GeometryNodesLazyFunctionGraphBuilder(const bNodeTree &btree,
                                        GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
      : btree_(btree), lf_graph_info_(&lf_graph_info)
  {
  }

  void build()
  {
    btree_.ensure_topology_cache();

    lf_graph_ = &lf_graph_info_->graph;
    mapping_ = &lf_graph_info_->mapping;
    conversions_ = &bke::get_implicit_type_conversions();

    socket_is_used_map_.reinitialize(btree_.all_sockets().size());
    socket_is_used_map_.fill(nullptr);
    mapping_->lf_input_index_for_output_bsocket_usage.reinitialize(
        btree_.all_output_sockets().size());
    mapping_->lf_input_index_for_output_bsocket_usage.fill(-1);
    mapping_->lf_input_index_for_attribute_propagation_to_output.reinitialize(
        btree_.all_output_sockets().size());
    mapping_->lf_input_index_for_attribute_propagation_to_output.fill(-1);
    mapping_->lf_index_by_bsocket.reinitialize(btree_.all_sockets().size());
    mapping_->lf_index_by_bsocket.fill(-1);

    this->prepare_node_multi_functions();
    this->build_group_input_node();
    if (btree_.group_output_node() == nullptr) {
      this->build_fallback_output_node();
    }
    this->handle_nodes();
    this->handle_links();
    this->add_default_inputs();

    this->build_attribute_propagation_input_node();
    this->build_output_usage_input_node();
    this->build_input_usage_output_node();
    this->build_socket_usages();

    this->build_attribute_propagation_sets();
    this->fix_link_cycles();

    // this->print_graph();

    lf_graph_->update_node_indices();
    lf_graph_info_->num_inline_nodes_approximate += lf_graph_->nodes().size();
  }

 private:
  void prepare_node_multi_functions()
  {
    lf_graph_info_->node_multi_functions = std::make_unique<NodeMultiFunctions>(btree_);
  }

  void build_group_input_node()
  {
    Vector<const CPPType *, 16> input_cpp_types;
    const Span<const bNodeSocket *> interface_inputs = btree_.interface_inputs();
    for (const bNodeSocket *interface_input : interface_inputs) {
      input_cpp_types.append(interface_input->typeinfo->geometry_nodes_cpp_type);
    }

    /* Create a dummy node for the group inputs. */
    auto debug_info = std::make_unique<GroupInputDebugInfo>();
    group_input_lf_node_ = &lf_graph_->add_dummy({}, input_cpp_types, debug_info.get());

    for (const int i : interface_inputs.index_range()) {
      mapping_->group_input_sockets.append(&group_input_lf_node_->output(i));
      debug_info->socket_names.append(interface_inputs[i]->name);
    }
    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
  }

  /**
   * Build an output node that just outputs default values in the case when there is no Group
   * Output node in the tree.
   */
  void build_fallback_output_node()
  {
    Vector<const CPPType *, 16> output_cpp_types;
    auto debug_info = std::make_unique<GroupOutputDebugInfo>();
    for (const bNodeSocket *interface_output : btree_.interface_outputs()) {
      output_cpp_types.append(interface_output->typeinfo->geometry_nodes_cpp_type);
      debug_info->socket_names.append(interface_output->name);
    }

    lf::Node &lf_node = lf_graph_->add_dummy(output_cpp_types, {}, debug_info.get());
    for (lf::InputSocket *lf_socket : lf_node.inputs()) {
      const CPPType &type = lf_socket->type();
      lf_socket->set_default_value(type.default_value());
    }
    mapping_->standard_group_output_sockets = lf_node.inputs();

    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
  }

  void handle_nodes()
  {
    /* Insert all nodes into the lazy function graph. */
    for (const bNode *bnode : btree_.all_nodes()) {
      const bNodeType *node_type = bnode->typeinfo;
      if (node_type == nullptr) {
        continue;
      }
      if (bnode->is_muted()) {
        this->handle_muted_node(*bnode);
        continue;
      }
      switch (node_type->type) {
        case NODE_FRAME: {
          /* Ignored. */
          break;
        }
        case NODE_REROUTE: {
          this->handle_reroute_node(*bnode);
          break;
        }
        case NODE_GROUP_INPUT: {
          this->handle_group_input_node(*bnode);
          break;
        }
        case NODE_GROUP_OUTPUT: {
          this->handle_group_output_node(*bnode);
          break;
        }
        case NODE_CUSTOM_GROUP:
        case NODE_GROUP: {
          this->handle_group_node(*bnode);
          break;
        }
        case GEO_NODE_VIEWER: {
          this->handle_viewer_node(*bnode);
          break;
        }
        case GEO_NODE_SIMULATION_INPUT: {
          this->handle_simulation_input_node(btree_, *bnode);
          break;
        }
        case GEO_NODE_SIMULATION_OUTPUT: {
          this->handle_simulation_output_node(*bnode);
          break;
        }
        case GEO_NODE_SWITCH: {
          this->handle_switch_node(*bnode);
          break;
        }
        default: {
          if (node_type->geometry_node_execute) {
            this->handle_geometry_node(*bnode);
            break;
          }
          const NodeMultiFunctions::Item &fn_item = lf_graph_info_->node_multi_functions->try_get(
              *bnode);
          if (fn_item.fn != nullptr) {
            this->handle_multi_function_node(*bnode, fn_item);
            break;
          }
          if (node_type == &bke::NodeTypeUndefined) {
            this->handle_undefined_node(*bnode);
            break;
          }
          /* Nodes that don't match any of the criteria above are just ignored. */
          break;
        }
      }
    }
  }

  void handle_muted_node(const bNode &bnode)
  {
    auto lazy_function = std::make_unique<LazyFunctionForMutedNode>(bnode,
                                                                    mapping_->lf_index_by_bsocket);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));
    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::InputSocket &lf_socket = lf_node.input(lf_index);
      input_socket_map_.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::OutputSocket &lf_socket = lf_node.output(lf_index);
      output_socket_map_.add_new(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }
  }

  void handle_reroute_node(const bNode &bnode)
  {
    const bNodeSocket &input_bsocket = bnode.input_socket(0);
    const bNodeSocket &output_bsocket = bnode.output_socket(0);
    const CPPType *type = get_socket_cpp_type(input_bsocket);
    if (type == nullptr) {
      return;
    }

    auto lazy_function = std::make_unique<LazyFunctionForRerouteNode>(*type);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    lf::InputSocket &lf_input = lf_node.input(0);
    lf::OutputSocket &lf_output = lf_node.output(0);
    input_socket_map_.add(&input_bsocket, &lf_input);
    output_socket_map_.add_new(&output_bsocket, &lf_output);
    mapping_->bsockets_by_lf_socket_map.add(&lf_input, &input_bsocket);
    mapping_->bsockets_by_lf_socket_map.add(&lf_output, &output_bsocket);
  }

  void handle_group_input_node(const bNode &bnode)
  {
    for (const int i : btree_.interface_inputs().index_range()) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = group_input_lf_node_->output(i);
      output_socket_map_.add_new(&bsocket, &lf_socket);
      mapping_->dummy_socket_map.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_group_output_node(const bNode &bnode)
  {
    Vector<const CPPType *, 16> output_cpp_types;
    auto debug_info = std::make_unique<GroupOutputDebugInfo>();
    for (const bNodeSocket *interface_input : btree_.interface_outputs()) {
      output_cpp_types.append(interface_input->typeinfo->geometry_nodes_cpp_type);
      debug_info->socket_names.append(interface_input->name);
    }

    lf::DummyNode &group_output_lf_node = lf_graph_->add_dummy(
        output_cpp_types, {}, debug_info.get());

    for (const int i : group_output_lf_node.inputs().index_range()) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      lf::InputSocket &lf_socket = group_output_lf_node.input(i);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->dummy_socket_map.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }

    if (&bnode == btree_.group_output_node()) {
      mapping_->standard_group_output_sockets = group_output_lf_node.inputs();
    }

    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
  }

  void handle_group_node(const bNode &bnode)
  {
    const bNodeTree *group_btree = reinterpret_cast<bNodeTree *>(bnode.id);
    if (group_btree == nullptr) {
      return;
    }
    const GeometryNodesLazyFunctionGraphInfo *group_lf_graph_info =
        ensure_geometry_nodes_lazy_function_graph(*group_btree);
    if (group_lf_graph_info == nullptr) {
      return;
    }

    auto lazy_function = std::make_unique<LazyFunctionForGroupNode>(
        bnode, *group_lf_graph_info, *lf_graph_info_);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);

    for (const int i : bnode.input_sockets().index_range()) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      BLI_assert(!bsocket.is_multi_input());
      lf::InputSocket &lf_socket = lf_node.input(i);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : bnode.output_sockets().index_range()) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = lf_node.output(i);
      output_socket_map_.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    mapping_->group_node_map.add(&bnode, &lf_node);
    lf_graph_info_->num_inline_nodes_approximate +=
        group_lf_graph_info->num_inline_nodes_approximate;
    static const bool static_false = false;
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      {
        const int lf_input_index =
            mapping_->lf_input_index_for_output_bsocket_usage[bsocket->index_in_all_outputs()];
        if (lf_input_index != -1) {
          lf::InputSocket &lf_input = lf_node.input(lf_input_index);
          lf_input.set_default_value(&static_false);
          socket_usage_inputs_.add(&lf_input);
        }
      }
      {
        /* Keep track of attribute set inputs that need to be populated later. */
        const int lf_input_index = mapping_->lf_input_index_for_attribute_propagation_to_output
                                       [bsocket->index_in_all_outputs()];
        if (lf_input_index != -1) {
          lf::InputSocket &lf_input = lf_node.input(lf_input_index);
          attribute_set_propagation_map_.add(bsocket, &lf_input);
        }
      }
    }
    lf_graph_info_->functions.append(std::move(lazy_function));
  }

  void handle_geometry_node(const bNode &bnode)
  {
    auto lazy_function = std::make_unique<LazyFunctionForGeometryNode>(bnode, *lf_graph_info_);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);

    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::InputSocket &lf_socket = lf_node.input(lf_index);

      if (bsocket->is_multi_input()) {
        auto multi_input_lazy_function = std::make_unique<LazyFunctionForMultiInput>(*bsocket);
        lf::Node &lf_multi_input_node = lf_graph_->add_function(*multi_input_lazy_function);
        lf_graph_info_->functions.append(std::move(multi_input_lazy_function));
        lf_graph_->add_link(lf_multi_input_node.output(0), lf_socket);
        multi_input_socket_nodes_.add_new(bsocket, &lf_multi_input_node);
        for (lf::InputSocket *lf_multi_input_socket : lf_multi_input_node.inputs()) {
          mapping_->bsockets_by_lf_socket_map.add(lf_multi_input_socket, bsocket);
          const void *default_value = lf_multi_input_socket->type().default_value();
          lf_multi_input_socket->set_default_value(default_value);
        }
      }
      else {
        input_socket_map_.add(bsocket, &lf_socket);
        mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
      }
    }
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::OutputSocket &lf_socket = lf_node.output(lf_index);
      output_socket_map_.add_new(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }

    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      {
        const int lf_input_index =
            mapping_->lf_input_index_for_output_bsocket_usage[bsocket->index_in_all_outputs()];
        if (lf_input_index != -1) {
          output_used_sockets_for_builtin_nodes_.append_as(bsocket,
                                                           &lf_node.input(lf_input_index));
          socket_usage_inputs_.add_new(&lf_node.input(lf_input_index));
        }
      }
      {
        /* Keep track of attribute set inputs that need to be populated later. */
        const int lf_input_index = mapping_->lf_input_index_for_attribute_propagation_to_output
                                       [bsocket->index_in_all_outputs()];
        if (lf_input_index != -1) {
          attribute_set_propagation_map_.add(bsocket, &lf_node.input(lf_input_index));
        }
      }
    }

    lf_graph_info_->functions.append(std::move(lazy_function));
  }

  void handle_multi_function_node(const bNode &bnode, const NodeMultiFunctions::Item &fn_item)
  {
    auto lazy_function = std::make_unique<LazyFunctionForMultiFunctionNode>(
        bnode, fn_item, mapping_->lf_index_by_bsocket);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      BLI_assert(!bsocket->is_multi_input());
      lf::InputSocket &lf_socket = lf_node.input(lf_index);
      input_socket_map_.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::OutputSocket &lf_socket = lf_node.output(lf_index);
      output_socket_map_.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }
  }

  void handle_viewer_node(const bNode &bnode)
  {
    auto lazy_function = std::make_unique<LazyFunctionForViewerNode>(
        bnode, mapping_->lf_index_by_bsocket);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::InputSocket &lf_socket = lf_node.input(lf_index);
      input_socket_map_.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }

    mapping_->viewer_node_map.add(&bnode, &lf_node);
  }

  void handle_simulation_input_node(const bNodeTree &node_tree, const bNode &bnode)
  {
    const NodeGeometrySimulationInput *storage = static_cast<const NodeGeometrySimulationInput *>(
        bnode.storage);
    if (node_tree.node_by_id(storage->output_node_id) == nullptr) {
      return;
    }

    std::unique_ptr<LazyFunction> lazy_function = get_simulation_input_lazy_function(
        node_tree, bnode, *lf_graph_info_);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const int i : bnode.input_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      lf::InputSocket &lf_socket = lf_node.input(
          mapping_->lf_index_by_bsocket[bsocket.index_in_tree()]);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : bnode.output_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = lf_node.output(
          mapping_->lf_index_by_bsocket[bsocket.index_in_tree()]);
      output_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void handle_simulation_output_node(const bNode &bnode)
  {
    std::unique_ptr<LazyFunction> lazy_function = get_simulation_output_lazy_function(
        bnode, *lf_graph_info_);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const int i : bnode.input_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      lf::InputSocket &lf_socket = lf_node.input(
          mapping_->lf_index_by_bsocket[bsocket.index_in_tree()]);
      input_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : bnode.output_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = lf_node.output(
          mapping_->lf_index_by_bsocket[bsocket.index_in_tree()]);
      output_socket_map_.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }

    mapping_->sim_output_node_map.add(&bnode, &lf_node);
  }

  void handle_switch_node(const bNode &bnode)
  {
    std::unique_ptr<LazyFunction> lazy_function = get_switch_node_lazy_function(bnode);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    int input_index = 0;
    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      if (bsocket->is_available()) {
        lf::InputSocket &lf_socket = lf_node.input(input_index);
        input_socket_map_.add(bsocket, &lf_socket);
        mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
        input_index++;
      }
    }
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      if (bsocket->is_available()) {
        lf::OutputSocket &lf_socket = lf_node.output(0);
        output_socket_map_.add(bsocket, &lf_socket);
        mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
        break;
      }
    }
  }

  void handle_undefined_node(const bNode &bnode)
  {
    auto lazy_function = std::make_unique<LazyFunctionForUndefinedNode>(
        bnode, mapping_->lf_index_by_bsocket);
    lf::FunctionNode &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::OutputSocket &lf_socket = lf_node.output(lf_index);
      output_socket_map_.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }
  }

  void handle_links()
  {
    for (const auto item : output_socket_map_.items()) {
      this->insert_links_from_socket(*item.key, *item.value);
    }
  }

  void insert_links_from_socket(const bNodeSocket &from_bsocket, lf::OutputSocket &from_lf_socket)
  {
    if (bke::nodeIsDanglingReroute(&btree_, &from_bsocket.owner_node())) {
      return;
    }

    const Span<const bNodeLink *> links_from_bsocket = from_bsocket.directly_linked_links();

    struct TypeWithLinks {
      const CPPType *type;
      Vector<const bNodeLink *> links;
    };

    /* Group available target sockets by type so that they can be handled together. */
    Vector<TypeWithLinks> types_with_links;
    for (const bNodeLink *link : links_from_bsocket) {
      if (link->is_muted()) {
        continue;
      }
      if (!link->is_available()) {
        continue;
      }
      const bNodeSocket &to_bsocket = *link->tosock;
      const CPPType *to_type = get_socket_cpp_type(to_bsocket);
      if (to_type == nullptr) {
        continue;
      }
      bool inserted = false;
      for (TypeWithLinks &types_with_links : types_with_links) {
        if (types_with_links.type == to_type) {
          types_with_links.links.append(link);
          inserted = true;
          break;
        }
      }
      if (inserted) {
        continue;
      }
      types_with_links.append({to_type, {link}});
    }

    for (const TypeWithLinks &type_with_links : types_with_links) {
      const CPPType &to_type = *type_with_links.type;
      const Span<const bNodeLink *> links = type_with_links.links;

      lf::OutputSocket *converted_from_lf_socket = this->insert_type_conversion_if_necessary(
          from_lf_socket, to_type);

      auto make_input_link_or_set_default = [&](lf::InputSocket &to_lf_socket) {
        if (converted_from_lf_socket == nullptr) {
          const void *default_value = to_type.default_value();
          to_lf_socket.set_default_value(default_value);
        }
        else {
          lf_graph_->add_link(*converted_from_lf_socket, to_lf_socket);
        }
      };

      for (const bNodeLink *link : links) {
        const bNodeSocket &to_bsocket = *link->tosock;
        if (to_bsocket.is_multi_input()) {
          /* TODO: Cache this index on the link. */
          int link_index = 0;
          for (const bNodeLink *multi_input_link : to_bsocket.directly_linked_links()) {
            if (multi_input_link == link) {
              break;
            }
            if (multi_input_link->is_muted() || !multi_input_link->fromsock->is_available() ||
                bke::nodeIsDanglingReroute(&btree_, multi_input_link->fromnode))
            {
              continue;
            }
            link_index++;
          }
          if (to_bsocket.owner_node().is_muted()) {
            if (link_index == 0) {
              for (lf::InputSocket *to_lf_socket : input_socket_map_.lookup(&to_bsocket)) {
                make_input_link_or_set_default(*to_lf_socket);
              }
            }
          }
          else {
            lf::Node *multi_input_lf_node = multi_input_socket_nodes_.lookup_default(&to_bsocket,
                                                                                     nullptr);
            if (multi_input_lf_node == nullptr) {
              continue;
            }
            make_input_link_or_set_default(multi_input_lf_node->input(link_index));
          }
        }
        else {
          for (lf::InputSocket *to_lf_socket : input_socket_map_.lookup(&to_bsocket)) {
            make_input_link_or_set_default(*to_lf_socket);
          }
        }
      }
    }
  }

  lf::OutputSocket *insert_type_conversion_if_necessary(lf::OutputSocket &from_socket,
                                                        const CPPType &to_type)
  {
    const CPPType &from_type = from_socket.type();
    if (from_type == to_type) {
      return &from_socket;
    }
    const auto *from_field_type = ValueOrFieldCPPType::get_from_self(from_type);
    const auto *to_field_type = ValueOrFieldCPPType::get_from_self(to_type);
    if (from_field_type != nullptr && to_field_type != nullptr) {
      if (conversions_->is_convertible(from_field_type->value, to_field_type->value)) {
        const MultiFunction &multi_fn = *conversions_->get_conversion_multi_function(
            mf::DataType::ForSingle(from_field_type->value),
            mf::DataType::ForSingle(to_field_type->value));
        auto fn = std::make_unique<LazyFunctionForMultiFunctionConversion>(
            multi_fn, *from_field_type, *to_field_type);
        lf::Node &conversion_node = lf_graph_->add_function(*fn);
        lf_graph_info_->functions.append(std::move(fn));
        lf_graph_->add_link(from_socket, conversion_node.input(0));
        return &conversion_node.output(0);
      }
    }
    return nullptr;
  }

  void add_default_inputs()
  {
    for (auto item : input_socket_map_.items()) {
      const bNodeSocket &bsocket = *item.key;
      const Span<lf::InputSocket *> lf_sockets = item.value;
      for (lf::InputSocket *lf_socket : lf_sockets) {
        if (lf_socket->origin() != nullptr) {
          /* Is linked already. */
          continue;
        }
        this->add_default_input(bsocket, *lf_socket);
      }
    }
  }

  void add_default_input(const bNodeSocket &input_bsocket, lf::InputSocket &input_lf_socket)
  {
    if (this->try_add_implicit_input(input_bsocket, input_lf_socket)) {
      return;
    }
    GMutablePointer value = get_socket_default_value(lf_graph_info_->allocator, input_bsocket);
    if (value.get() == nullptr) {
      /* Not possible to add a default value. */
      return;
    }
    input_lf_socket.set_default_value(value.get());
    if (!value.type()->is_trivially_destructible()) {
      lf_graph_info_->values_to_destruct.append(value);
    }
  }

  bool try_add_implicit_input(const bNodeSocket &input_bsocket, lf::InputSocket &input_lf_socket)
  {
    const bNode &bnode = input_bsocket.owner_node();
    const SocketDeclaration *socket_decl = input_bsocket.runtime->declaration;
    if (socket_decl == nullptr) {
      return false;
    }
    if (socket_decl->input_field_type != InputSocketFieldType::Implicit) {
      return false;
    }
    const ImplicitInputValueFn *implicit_input_fn = socket_decl->implicit_input_fn();
    if (implicit_input_fn == nullptr) {
      return false;
    }
    std::function<void(void *)> init_fn = [&bnode, implicit_input_fn](void *r_value) {
      (*implicit_input_fn)(bnode, r_value);
    };
    const CPPType &type = input_lf_socket.type();
    auto lazy_function = std::make_unique<LazyFunctionForImplicitInput>(type, std::move(init_fn));
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));
    lf_graph_->add_link(lf_node.output(0), input_lf_socket);
    return true;
  }

  /**
   * Every output geometry socket that may propagate attributes has to know which attributes should
   * be propagated. Therefore, every one of these outputs gets a corresponding attribute set input.
   */
  void build_attribute_propagation_input_node()
  {
    const aal::RelationsInNode &tree_relations = *btree_.runtime->anonymous_attribute_relations;
    Vector<int> output_indices;
    for (const aal::PropagateRelation &relation : tree_relations.propagate_relations) {
      output_indices.append_non_duplicates(relation.to_geometry_output);
    }
    Vector<const CPPType *> cpp_types;
    auto debug_info = std::make_unique<lf::SimpleDummyDebugInfo>();
    debug_info->name = "Attributes to Propagate to Output";
    cpp_types.append_n_times(&CPPType::get<bke::AnonymousAttributeSet>(), output_indices.size());
    lf::Node &lf_node = lf_graph_->add_dummy({}, cpp_types, debug_info.get());
    for (const int i : output_indices.index_range()) {
      const int output_index = output_indices[i];
      mapping_->attribute_set_by_geometry_output.add(output_index, &lf_node.output(i));
      debug_info->output_names.append(btree_.interface_outputs()[output_index]->name);
    }
    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
  }

  /**
   * Build new boolean group inputs that indicate which group outputs are used.
   */
  void build_output_usage_input_node()
  {
    const Span<const bNodeSocket *> interface_outputs = btree_.interface_outputs();

    Vector<const CPPType *> cpp_types;
    cpp_types.append_n_times(&CPPType::get<bool>(), interface_outputs.size());
    auto debug_info = std::make_unique<lf::SimpleDummyDebugInfo>();
    debug_info->name = "Output Socket Usage";
    lf::Node &lf_node = lf_graph_->add_dummy({}, cpp_types, debug_info.get());
    for (const int i : interface_outputs.index_range()) {
      mapping_->group_output_used_sockets.append(&lf_node.output(i));
      debug_info->output_names.append(interface_outputs[i]->name);
    }
    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
  }

  /**
   * Build new boolean group outputs that indicate which group inputs are used depending on other
   * group inputs.
   */
  void build_input_usage_output_node()
  {
    const Span<const bNodeSocket *> interface_inputs = btree_.interface_inputs();

    Vector<const CPPType *> cpp_types;
    cpp_types.append_n_times(&CPPType::get<bool>(), interface_inputs.size());
    auto debug_info = std::make_unique<lf::SimpleDummyDebugInfo>();
    debug_info->name = "Input Socket Usage";
    lf::Node &lf_node = lf_graph_->add_dummy(cpp_types, {}, debug_info.get());
    for (const int i : interface_inputs.index_range()) {
      mapping_->group_input_usage_sockets.append(&lf_node.input(i));
      debug_info->input_names.append(interface_inputs[i]->name);
    }
    lf_graph_info_->dummy_debug_infos_.append(std::move(debug_info));
  }

  /**
   * For every socket we want to determine if it will be used depending on the inputs of the node
   * group (just static analysis is not enough when there are e.g. Switch nodes). This function
   * populates #socket_is_used_map_ with that information.
   */
  void build_socket_usages()
  {
    OrSocketUsagesCache or_socket_usages_cache;

    if (const bNode *group_output_bnode = btree_.group_output_node()) {
      /* Whether a group output is used is determined by a group input that has been created
       * exactly for this purpose. */
      for (const bNodeSocket *bsocket : group_output_bnode->input_sockets().drop_back(1)) {
        const int index = bsocket->index();
        socket_is_used_map_[bsocket->index_in_tree()] = const_cast<lf::OutputSocket *>(
            mapping_->group_output_used_sockets[index]);
      }
    }

    /* Iterate over all nodes from right to left to determine when which sockets are used. */
    for (const bNode *bnode : btree_.toposort_right_to_left()) {
      const bNodeType *node_type = bnode->typeinfo;
      if (node_type == nullptr) {
        /* Ignore. */
        continue;
      }

      this->build_output_socket_usages(*bnode, or_socket_usages_cache);

      if (bnode->is_muted()) {
        this->build_muted_node_usages(*bnode, or_socket_usages_cache);
        continue;
      }

      switch (node_type->type) {
        case NODE_GROUP_OUTPUT: {
          /* Handled before this loop already. */
          break;
        }
        case NODE_GROUP_INPUT: {
          /* Handled after this loop. */
          break;
        }
        case NODE_FRAME: {
          /* Ignored. */
          break;
        }
        case NODE_REROUTE: {
          /* The input is used exactly when the output is used. */
          socket_is_used_map_[bnode->input_socket(0).index_in_tree()] =
              socket_is_used_map_[bnode->output_socket(0).index_in_tree()];
          break;
        }
        case GEO_NODE_SWITCH: {
          this->build_switch_node_socket_usage(*bnode);
          break;
        }
        case GEO_NODE_VIEWER: {
          this->build_viewer_node_socket_usage(*bnode);
          break;
        }
        case GEO_NODE_SIMULATION_INPUT: {
          this->build_simulation_input_socket_usage(*bnode);
          break;
        }
        case GEO_NODE_SIMULATION_OUTPUT: {
          this->build_simulation_output_socket_usage(*bnode);
          break;
        }
        case NODE_GROUP:
        case NODE_CUSTOM_GROUP: {
          this->build_group_node_socket_usage(*bnode, or_socket_usages_cache);
          break;
        }
        default: {
          this->build_standard_node_input_socket_usage(*bnode, or_socket_usages_cache);
          break;
        }
      }
    }

    this->build_group_input_usages(or_socket_usages_cache);
    this->link_output_used_sockets_for_builtin_nodes();
  }

  using OrSocketUsagesCache = Map<Vector<lf::OutputSocket *>, lf::OutputSocket *>;

  /**
   * Combine multiple socket usages with a logical or. Inserts a new node for that purpose if
   * necessary.
   */
  lf::OutputSocket *or_socket_usages(MutableSpan<lf::OutputSocket *> usages,
                                     OrSocketUsagesCache &cache)
  {
    if (usages.is_empty()) {
      return nullptr;
    }
    if (usages.size() == 1) {
      return usages[0];
    }

    std::sort(usages.begin(), usages.end());
    return cache.lookup_or_add_cb_as(usages, [&]() {
      auto logical_or_fn = std::make_unique<LazyFunctionForLogicalOr>(usages.size());
      lf::Node &logical_or_node = lf_graph_->add_function(*logical_or_fn);
      lf_graph_info_->functions.append(std::move(logical_or_fn));

      for (const int i : usages.index_range()) {
        lf_graph_->add_link(*usages[i], logical_or_node.input(i));
      }
      return &logical_or_node.output(0);
    });
  }

  void build_output_socket_usages(const bNode &bnode, OrSocketUsagesCache &or_socket_usages_cache)
  {
    /* Output sockets are used when any of their linked inputs are used. */
    for (const bNodeSocket *socket : bnode.output_sockets()) {
      if (!socket->is_available()) {
        continue;
      }
      /* Determine when linked target sockets are used. */
      Vector<lf::OutputSocket *> target_usages;
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (!link->is_used()) {
          continue;
        }
        const bNodeSocket &target_socket = *link->tosock;
        if (lf::OutputSocket *is_used_socket = socket_is_used_map_[target_socket.index_in_tree()])
        {
          target_usages.append_non_duplicates(is_used_socket);
        }
      }
      /* Combine target socket usages into the usage of the current socket. */
      socket_is_used_map_[socket->index_in_tree()] = this->or_socket_usages(
          target_usages, or_socket_usages_cache);
    }
  }

  /**
   * An input of a muted node is used when any of its internally linked outputs is used.
   */
  void build_muted_node_usages(const bNode &bnode, OrSocketUsagesCache &or_socket_usages_cache)
  {
    /* Find all outputs that use a specific input. */
    MultiValueMap<const bNodeSocket *, const bNodeSocket *> outputs_by_input;
    for (const bNodeLink &blink : bnode.internal_links()) {
      outputs_by_input.add(blink.fromsock, blink.tosock);
    }
    for (const auto item : outputs_by_input.items()) {
      const bNodeSocket &input_bsocket = *item.key;
      const Span<const bNodeSocket *> output_bsockets = item.value;

      /* The input is used if any of the internally linked outputs is used. */
      Vector<lf::OutputSocket *> lf_socket_usages;
      for (const bNodeSocket *output_bsocket : output_bsockets) {
        if (lf::OutputSocket *lf_socket = socket_is_used_map_[output_bsocket->index_in_tree()]) {
          lf_socket_usages.append(lf_socket);
        }
      }
      socket_is_used_map_[input_bsocket.index_in_tree()] = this->or_socket_usages(
          lf_socket_usages, or_socket_usages_cache);
    }
  }

  void build_switch_node_socket_usage(const bNode &bnode)
  {
    const bNodeSocket *switch_input_bsocket = nullptr;
    const bNodeSocket *false_input_bsocket = nullptr;
    const bNodeSocket *true_input_bsocket = nullptr;
    const bNodeSocket *output_bsocket = nullptr;
    for (const bNodeSocket *socket : bnode.input_sockets()) {
      if (!socket->is_available()) {
        continue;
      }
      if (socket->name == StringRef("Switch")) {
        switch_input_bsocket = socket;
      }
      else if (socket->name == StringRef("False")) {
        false_input_bsocket = socket;
      }
      else if (socket->name == StringRef("True")) {
        true_input_bsocket = socket;
      }
    }
    for (const bNodeSocket *socket : bnode.output_sockets()) {
      if (socket->is_available()) {
        output_bsocket = socket;
        break;
      }
    }
    lf::OutputSocket *output_is_used_socket = socket_is_used_map_[output_bsocket->index_in_tree()];
    if (output_is_used_socket == nullptr) {
      return;
    }
    socket_is_used_map_[switch_input_bsocket->index_in_tree()] = output_is_used_socket;
    lf::InputSocket *lf_switch_input = input_socket_map_.lookup(switch_input_bsocket)[0];
    if (lf::OutputSocket *lf_switch_origin = lf_switch_input->origin()) {
      /* The condition input is dynamic, so the usage of the other inputs is as well. */
      static const LazyFunctionForSwitchSocketUsage switch_socket_usage_fn;
      lf::Node &lf_node = lf_graph_->add_function(switch_socket_usage_fn);
      lf_graph_->add_link(*lf_switch_origin, lf_node.input(0));
      socket_is_used_map_[false_input_bsocket->index_in_tree()] = &lf_node.output(0);
      socket_is_used_map_[true_input_bsocket->index_in_tree()] = &lf_node.output(1);
    }
    else {
      if (switch_input_bsocket->default_value_typed<bNodeSocketValueBoolean>()->value) {
        socket_is_used_map_[true_input_bsocket->index_in_tree()] = output_is_used_socket;
      }
      else {
        socket_is_used_map_[false_input_bsocket->index_in_tree()] = output_is_used_socket;
      }
    }
  }

  void build_viewer_node_socket_usage(const bNode &bnode)
  {
    const lf::FunctionNode &lf_viewer_node = *mapping_->viewer_node_map.lookup(&bnode);
    auto lazy_function = std::make_unique<LazyFunctionForViewerInputUsage>(lf_viewer_node);
    lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
    lf_graph_info_->functions.append(std::move(lazy_function));

    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      if (bsocket->is_available()) {
        socket_is_used_map_[bsocket->index_in_tree()] = &lf_node.output(0);
      }
    }
  }

  void build_simulation_input_socket_usage(const bNode &bnode)
  {
    const NodeGeometrySimulationInput *storage = static_cast<const NodeGeometrySimulationInput *>(
        bnode.storage);
    const bNode *sim_output_node = btree_.node_by_id(storage->output_node_id);
    if (sim_output_node == nullptr) {
      return;
    }
    lf::Node &lf_node = this->get_simulation_inputs_usage_node(*sim_output_node);
    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      if (bsocket->is_available()) {
        socket_is_used_map_[bsocket->index_in_tree()] = &lf_node.output(0);
      }
    }
  }

  void build_simulation_output_socket_usage(const bNode &bnode)
  {
    lf::Node &lf_node = this->get_simulation_inputs_usage_node(bnode);
    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      if (bsocket->is_available()) {
        socket_is_used_map_[bsocket->index_in_tree()] = &lf_node.output(1);
      }
    }
  }

  lf::Node &get_simulation_inputs_usage_node(const bNode &sim_output_bnode)
  {
    BLI_assert(sim_output_bnode.type == GEO_NODE_SIMULATION_OUTPUT);
    return *simulation_inputs_usage_nodes_.lookup_or_add_cb(&sim_output_bnode, [&]() {
      auto lazy_function = std::make_unique<LazyFunctionForSimulationInputsUsage>();
      lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
      lf_graph_info_->functions.append(std::move(lazy_function));
      return &lf_node;
    });
  }

  void build_group_node_socket_usage(const bNode &bnode,
                                     OrSocketUsagesCache &or_socket_usages_cache)
  {
    const bNodeTree *bgroup = reinterpret_cast<const bNodeTree *>(bnode.id);
    if (bgroup == nullptr) {
      return;
    }
    const GeometryNodesLazyFunctionGraphInfo *group_lf_graph_info =
        ensure_geometry_nodes_lazy_function_graph(*bgroup);
    if (group_lf_graph_info == nullptr) {
      return;
    }
    lf::FunctionNode &lf_group_node = const_cast<lf::FunctionNode &>(
        *mapping_->group_node_map.lookup(&bnode));
    const auto &fn = static_cast<const LazyFunctionForGroupNode &>(lf_group_node.function());

    for (const bNodeSocket *input_bsocket : bnode.input_sockets()) {
      const int input_index = input_bsocket->index();
      const InputUsageHint &input_usage_hint =
          group_lf_graph_info->mapping.group_input_usage_hints[input_index];
      switch (input_usage_hint.type) {
        case InputUsageHintType::Never: {
          /* Nothing to do. */
          break;
        }
        case InputUsageHintType::DependsOnOutput: {
          Vector<lf::OutputSocket *> output_usages;
          for (const int i : input_usage_hint.output_dependencies) {
            if (lf::OutputSocket *lf_socket =
                    socket_is_used_map_[bnode.output_socket(i).index_in_tree()]) {
              output_usages.append(lf_socket);
            }
          }
          socket_is_used_map_[input_bsocket->index_in_tree()] = this->or_socket_usages(
              output_usages, or_socket_usages_cache);
          break;
        }
        case InputUsageHintType::DynamicSocket: {
          socket_is_used_map_[input_bsocket->index_in_tree()] = &const_cast<lf::OutputSocket &>(
              lf_group_node.output(fn.lf_output_for_input_bsocket_usage_.lookup(input_index)));
          break;
        }
      }
    }

    for (const bNodeSocket *output_bsocket : bnode.output_sockets()) {
      const int lf_input_index =
          mapping_
              ->lf_input_index_for_output_bsocket_usage[output_bsocket->index_in_all_outputs()];
      BLI_assert(lf_input_index >= 0);
      lf::InputSocket &lf_socket = lf_group_node.input(lf_input_index);
      if (lf::OutputSocket *lf_output_is_used =
              socket_is_used_map_[output_bsocket->index_in_tree()]) {
        lf_graph_->add_link(*lf_output_is_used, lf_socket);
      }
      else {
        static const bool static_false = false;
        lf_socket.set_default_value(&static_false);
      }
    }
  }

  void build_standard_node_input_socket_usage(const bNode &bnode,
                                              OrSocketUsagesCache &or_socket_usages_cache)
  {
    if (bnode.input_sockets().is_empty()) {
      return;
    }

    Vector<lf::OutputSocket *> output_usages;
    for (const bNodeSocket *output_socket : bnode.output_sockets()) {
      if (!output_socket->is_available()) {
        continue;
      }
      if (lf::OutputSocket *is_used_socket = socket_is_used_map_[output_socket->index_in_tree()]) {
        output_usages.append_non_duplicates(is_used_socket);
      }
    }

    /* Assume every input is used when any output is used. */
    lf::OutputSocket *lf_usage = this->or_socket_usages(output_usages, or_socket_usages_cache);
    if (lf_usage == nullptr) {
      return;
    }

    for (const bNodeSocket *input_socket : bnode.input_sockets()) {
      if (input_socket->is_available()) {
        socket_is_used_map_[input_socket->index_in_tree()] = lf_usage;
      }
    }
  }

  void build_group_input_usages(OrSocketUsagesCache &or_socket_usages_cache)
  {
    const Span<const bNode *> group_input_nodes = btree_.group_input_nodes();
    for (const int i : btree_.interface_inputs().index_range()) {
      Vector<lf::OutputSocket *> target_usages;
      for (const bNode *group_input_node : group_input_nodes) {
        if (lf::OutputSocket *lf_socket =
                socket_is_used_map_[group_input_node->output_socket(i).index_in_tree()])
        {
          target_usages.append_non_duplicates(lf_socket);
        }
      }

      lf::OutputSocket *lf_socket = this->or_socket_usages(target_usages, or_socket_usages_cache);
      lf::InputSocket *lf_group_output = const_cast<lf::InputSocket *>(
          mapping_->group_input_usage_sockets[i]);
      InputUsageHint input_usage_hint;
      if (lf_socket == nullptr) {
        static const bool static_false = false;
        lf_group_output->set_default_value(&static_false);
        input_usage_hint.type = InputUsageHintType::Never;
      }
      else {
        lf_graph_->add_link(*lf_socket, *lf_group_output);
        if (lf_socket->node().is_dummy()) {
          /* Can support slightly more complex cases where it depends on more than one output in
           * the future. */
          input_usage_hint.type = InputUsageHintType::DependsOnOutput;
          input_usage_hint.output_dependencies = {
              mapping_->group_output_used_sockets.first_index_of(lf_socket)};
        }
        else {
          input_usage_hint.type = InputUsageHintType::DynamicSocket;
        }
      }
      lf_graph_info_->mapping.group_input_usage_hints.append(std::move(input_usage_hint));
    }
  }

  void link_output_used_sockets_for_builtin_nodes()
  {
    for (const auto &[output_bsocket, lf_input] : output_used_sockets_for_builtin_nodes_) {
      if (lf::OutputSocket *lf_is_used = socket_is_used_map_[output_bsocket->index_in_tree()]) {
        lf_graph_->add_link(*lf_is_used, *lf_input);
      }
      else {
        static const bool static_false = false;
        lf_input->set_default_value(&static_false);
      }
    }
  }

  void build_attribute_propagation_sets()
  {
    ResourceScope scope;
    const Array<const aal::RelationsInNode *> relations_by_node =
        bke::anonymous_attribute_inferencing::get_relations_by_node(btree_, scope);

    VectorSet<AttributeReferenceKey> attribute_reference_keys;
    /* Indexed by reference key index. */
    Vector<AttributeReferenceInfo> attribute_reference_infos;
    this->build_attribute_references(
        relations_by_node, attribute_reference_keys, attribute_reference_infos);

    const int sockets_num = btree_.all_sockets().size();
    const int attribute_references_num = attribute_reference_keys.size();

    /* The code below uses #BitGroupVector to store a set of attribute references per socket. Each
     * socket has a bit span where each bit corresponds to one attribute reference. */
    BitGroupVector<> referenced_by_field_socket(sockets_num, attribute_references_num, false);
    BitGroupVector<> propagated_to_geometry_socket(sockets_num, attribute_references_num, false);
    this->gather_referenced_and_potentially_propagated_data(relations_by_node,
                                                            attribute_reference_keys,
                                                            attribute_reference_infos,
                                                            referenced_by_field_socket,
                                                            propagated_to_geometry_socket);

    BitGroupVector<> required_propagated_to_geometry_socket(
        sockets_num, attribute_references_num, false);
    this->gather_required_propagated_data(relations_by_node,
                                          attribute_reference_keys,
                                          referenced_by_field_socket,
                                          propagated_to_geometry_socket,
                                          required_propagated_to_geometry_socket);

    this->build_attribute_sets_to_propagate(attribute_reference_keys,
                                            attribute_reference_infos,
                                            required_propagated_to_geometry_socket);
  }

  void build_attribute_references(const Span<const aal::RelationsInNode *> relations_by_node,
                                  VectorSet<AttributeReferenceKey> &r_attribute_reference_keys,
                                  Vector<AttributeReferenceInfo> &r_attribute_reference_infos)
  {
    auto add_get_attributes_node = [&](lf::OutputSocket &lf_field_socket) -> lf::OutputSocket & {
      const ValueOrFieldCPPType &type = *ValueOrFieldCPPType::get_from_self(
          lf_field_socket.type());
      auto lazy_function = std::make_unique<LazyFunctionForAnonymousAttributeSetExtract>(type);
      lf::Node &lf_node = lf_graph_->add_function(*lazy_function);
      lf_graph_->add_link(lf_field_socket, lf_node.input(0));
      lf_graph_info_->functions.append(std::move(lazy_function));
      return lf_node.output(0);
    };

    /* Find nodes that create new anonymous attributes. */
    for (const bNode *node : btree_.all_nodes()) {
      const aal::RelationsInNode &relations = *relations_by_node[node->index()];
      for (const aal::AvailableRelation &relation : relations.available_relations) {
        const bNodeSocket &geometry_bsocket = node->output_socket(relation.geometry_output);
        const bNodeSocket &field_bsocket = node->output_socket(relation.field_output);
        if (!field_bsocket.is_available()) {
          continue;
        }
        if (!field_bsocket.is_directly_linked()) {
          continue;
        }
        AttributeReferenceKey key;
        key.type = AttributeReferenceKeyType::Socket;
        key.bsocket = &field_bsocket;
        const int key_index = r_attribute_reference_keys.index_of_or_add(key);
        if (key_index >= r_attribute_reference_infos.size()) {
          AttributeReferenceInfo info;
          lf::OutputSocket &lf_field_socket = *output_socket_map_.lookup(&field_bsocket);
          info.lf_attribute_set_socket = &add_get_attributes_node(lf_field_socket);
          r_attribute_reference_infos.append(info);
        }
        AttributeReferenceInfo &info = r_attribute_reference_infos[key_index];
        if (geometry_bsocket.is_available()) {
          info.initial_geometry_sockets.append(&geometry_bsocket);
        }
      }
    }

    /* Find field group inputs that are evaluated within this node tree. */
    const aal::RelationsInNode &tree_relations = *btree_.runtime->anonymous_attribute_relations;
    for (const aal::EvalRelation &relation : tree_relations.eval_relations) {
      AttributeReferenceKey key;
      key.type = AttributeReferenceKeyType::InputField;
      key.index = relation.field_input;
      const int key_index = r_attribute_reference_keys.index_of_or_add(key);
      if (key_index >= r_attribute_reference_infos.size()) {
        AttributeReferenceInfo info;
        lf::OutputSocket &lf_field_socket = *const_cast<lf::OutputSocket *>(
            mapping_->group_input_sockets[relation.field_input]);
        info.lf_attribute_set_socket = &add_get_attributes_node(lf_field_socket);
        r_attribute_reference_infos.append(info);
      }
      AttributeReferenceInfo &info = r_attribute_reference_infos[key_index];
      for (const bNode *bnode : btree_.group_input_nodes()) {
        info.initial_geometry_sockets.append(&bnode->output_socket(relation.geometry_input));
      }
    }
    /* Find group outputs that attributes need to be propagated to. */
    for (const aal::PropagateRelation &relation : tree_relations.propagate_relations) {
      AttributeReferenceKey key;
      key.type = AttributeReferenceKeyType::OutputGeometry;
      key.index = relation.to_geometry_output;
      const int key_index = r_attribute_reference_keys.index_of_or_add(key);
      if (key_index >= r_attribute_reference_infos.size()) {
        AttributeReferenceInfo info;
        info.lf_attribute_set_socket = const_cast<lf::OutputSocket *>(
            mapping_->attribute_set_by_geometry_output.lookup(relation.to_geometry_output));
        r_attribute_reference_infos.append(info);
      }
      AttributeReferenceInfo &info = r_attribute_reference_infos[key_index];
      for (const bNode *bnode : btree_.group_input_nodes()) {
        info.initial_geometry_sockets.append(&bnode->output_socket(relation.from_geometry_input));
      }
    }
  }

  /**
   * For every field socket, figure out which anonymous attributes it may reference.
   * For every geometry socket, figure out which anonymous attributes may be propagated to it.
   */
  void gather_referenced_and_potentially_propagated_data(
      const Span<const aal::RelationsInNode *> relations_by_node,
      const Span<AttributeReferenceKey> attribute_reference_keys,
      const Span<AttributeReferenceInfo> attribute_reference_infos,
      BitGroupVector<> &r_referenced_by_field_socket,
      BitGroupVector<> &r_propagated_to_geometry_socket)
  {
    /* Insert initial referenced/propagated attributes. */
    for (const int key_index : attribute_reference_keys.index_range()) {
      const AttributeReferenceKey &key = attribute_reference_keys[key_index];
      const AttributeReferenceInfo &info = attribute_reference_infos[key_index];
      switch (key.type) {
        case AttributeReferenceKeyType::InputField: {
          for (const bNode *bnode : btree_.group_input_nodes()) {
            const bNodeSocket &bsocket = bnode->output_socket(key.index);
            r_referenced_by_field_socket[bsocket.index_in_tree()][key_index].set();
          }
          break;
        }
        case AttributeReferenceKeyType::OutputGeometry: {
          break;
        }
        case AttributeReferenceKeyType::Socket: {
          r_referenced_by_field_socket[key.bsocket->index_in_tree()][key_index].set();
          break;
        }
      }
      for (const bNodeSocket *geometry_bsocket : info.initial_geometry_sockets) {
        r_propagated_to_geometry_socket[geometry_bsocket->index_in_tree()][key_index].set();
      }
    }
    /* Propagate attribute usages from left to right. */
    for (const bNode *bnode : btree_.toposort_left_to_right()) {
      for (const bNodeSocket *bsocket : bnode->input_sockets()) {
        if (bsocket->is_available()) {
          const int dst_index = bsocket->index_in_tree();
          MutableBoundedBitSpan referenced_dst = r_referenced_by_field_socket[dst_index];
          MutableBoundedBitSpan propagated_dst = r_propagated_to_geometry_socket[dst_index];
          for (const bNodeLink *blink : bsocket->directly_linked_links()) {
            if (blink->is_used()) {
              const int src_index = blink->fromsock->index_in_tree();
              referenced_dst |= r_referenced_by_field_socket[src_index];
              propagated_dst |= r_propagated_to_geometry_socket[src_index];
            }
          }
        }
      }
      const aal::RelationsInNode &relations = *relations_by_node[bnode->index()];
      for (const aal::ReferenceRelation &relation : relations.reference_relations) {
        const bNodeSocket &input_bsocket = bnode->input_socket(relation.from_field_input);
        const bNodeSocket &output_bsocket = bnode->output_socket(relation.to_field_output);
        if (!input_bsocket.is_available() || !output_bsocket.is_available()) {
          continue;
        }
        r_referenced_by_field_socket[output_bsocket.index_in_tree()] |=
            r_referenced_by_field_socket[input_bsocket.index_in_tree()];
      }
      for (const aal::PropagateRelation &relation : relations.propagate_relations) {
        const bNodeSocket &input_bsocket = bnode->input_socket(relation.from_geometry_input);
        const bNodeSocket &output_bsocket = bnode->output_socket(relation.to_geometry_output);
        if (!input_bsocket.is_available() || !output_bsocket.is_available()) {
          continue;
        }
        r_propagated_to_geometry_socket[output_bsocket.index_in_tree()] |=
            r_propagated_to_geometry_socket[input_bsocket.index_in_tree()];
      }
    }
  }

  /**
   * Determines which anonymous attributes should be propagated to which geometry sockets.
   */
  void gather_required_propagated_data(
      const Span<const aal::RelationsInNode *> relations_by_node,
      const VectorSet<AttributeReferenceKey> &attribute_reference_keys,
      const BitGroupVector<> &referenced_by_field_socket,
      const BitGroupVector<> &propagated_to_geometry_socket,
      BitGroupVector<> &r_required_propagated_to_geometry_socket)
  {
    const aal::RelationsInNode &tree_relations = *btree_.runtime->anonymous_attribute_relations;
    const int sockets_num = btree_.all_sockets().size();
    const int attribute_references_num = referenced_by_field_socket.group_size();
    BitGroupVector<> required_by_geometry_socket(sockets_num, attribute_references_num, false);

    /* Initialize required attributes at group output. */
    if (const bNode *group_output_bnode = btree_.group_output_node()) {
      for (const aal::PropagateRelation &relation : tree_relations.propagate_relations) {
        AttributeReferenceKey key;
        key.type = AttributeReferenceKeyType::OutputGeometry;
        key.index = relation.to_geometry_output;
        const int key_index = attribute_reference_keys.index_of(key);
        required_by_geometry_socket[group_output_bnode->input_socket(relation.to_geometry_output)
                                        .index_in_tree()][key_index]
            .set();
      }
      for (const aal::AvailableRelation &relation : tree_relations.available_relations) {
        const bNodeSocket &geometry_bsocket = group_output_bnode->input_socket(
            relation.geometry_output);
        const bNodeSocket &field_bsocket = group_output_bnode->input_socket(relation.field_output);
        required_by_geometry_socket[geometry_bsocket.index_in_tree()] |=
            referenced_by_field_socket[field_bsocket.index_in_tree()];
      }
    }

    /* Propagate attribute usages from right to left. */
    BitVector<> required_attributes(attribute_references_num);
    for (const bNode *bnode : btree_.toposort_right_to_left()) {
      const aal::RelationsInNode &relations = *relations_by_node[bnode->index()];
      for (const bNodeSocket *bsocket : bnode->output_sockets()) {
        if (!bsocket->is_available()) {
          continue;
        }
        required_attributes.fill(false);
        for (const bNodeLink *blink : bsocket->directly_linked_links()) {
          if (blink->is_used()) {
            const bNodeSocket &to_socket = *blink->tosock;
            required_attributes |= required_by_geometry_socket[to_socket.index_in_tree()];
          }
        }
        required_attributes &= propagated_to_geometry_socket[bsocket->index_in_tree()];
        required_by_geometry_socket[bsocket->index_in_tree()] |= required_attributes;
        bits::foreach_1_index(required_attributes, [&](const int key_index) {
          const AttributeReferenceKey &key = attribute_reference_keys[key_index];
          if (key.type != AttributeReferenceKeyType::Socket || &key.bsocket->owner_node() != bnode)
          {
            r_required_propagated_to_geometry_socket[bsocket->index_in_tree()][key_index].set();
          }
        });
      }

      for (const bNodeSocket *bsocket : bnode->input_sockets()) {
        if (!bsocket->is_available()) {
          continue;
        }
        required_attributes.fill(false);
        for (const aal::PropagateRelation &relation : relations.propagate_relations) {
          if (relation.from_geometry_input == bsocket->index()) {
            const bNodeSocket &output_bsocket = bnode->output_socket(relation.to_geometry_output);
            required_attributes |= required_by_geometry_socket[output_bsocket.index_in_tree()];
          }
        }
        for (const aal::EvalRelation &relation : relations.eval_relations) {
          if (relation.geometry_input == bsocket->index()) {
            const bNodeSocket &field_bsocket = bnode->input_socket(relation.field_input);
            if (field_bsocket.is_available()) {
              required_attributes |= referenced_by_field_socket[field_bsocket.index_in_tree()];
            }
          }
        }
        required_attributes &= propagated_to_geometry_socket[bsocket->index_in_tree()];
        required_by_geometry_socket[bsocket->index_in_tree()] |= required_attributes;
      }
    }
  }

  /**
   * For every node that propagates attributes, prepare an attribute set containing information
   * about which attributes should be propagated.
   */
  void build_attribute_sets_to_propagate(
      const Span<AttributeReferenceKey> attribute_reference_keys,
      const Span<AttributeReferenceInfo> attribute_reference_infos,
      const BitGroupVector<> &required_propagated_to_geometry_socket)
  {
    JoinAttibuteSetsCache join_attribute_sets_cache;

    for (const auto [geometry_output_bsocket, lf_attribute_set_input] :
         attribute_set_propagation_map_.items())
    {
      const BoundedBitSpan required =
          required_propagated_to_geometry_socket[geometry_output_bsocket->index_in_tree()];

      Vector<lf::OutputSocket *> attribute_set_sockets;
      Vector<lf::OutputSocket *> used_sockets;

      bits::foreach_1_index(required, [&](const int key_index) {
        const AttributeReferenceKey &key = attribute_reference_keys[key_index];
        const AttributeReferenceInfo &info = attribute_reference_infos[key_index];
        lf::OutputSocket *lf_socket_usage = nullptr;
        switch (key.type) {
          case AttributeReferenceKeyType::InputField: {
            lf_socket_usage = const_cast<lf::InputSocket *>(
                                  mapping_->group_input_usage_sockets[key.index])
                                  ->origin();
            break;
          }
          case AttributeReferenceKeyType::OutputGeometry: {
            lf_socket_usage = const_cast<lf::OutputSocket *>(
                mapping_->group_output_used_sockets[key.index]);
            break;
          }
          case AttributeReferenceKeyType::Socket: {
            lf_socket_usage = socket_is_used_map_[key.bsocket->index_in_tree()];
            break;
          }
        }
        if (lf_socket_usage) {
          attribute_set_sockets.append(info.lf_attribute_set_socket);
          used_sockets.append(lf_socket_usage);
        }
      });
      if (lf::OutputSocket *joined_attribute_set = this->join_attribute_sets(
              attribute_set_sockets, used_sockets, join_attribute_sets_cache))
      {
        lf_graph_->add_link(*joined_attribute_set, *lf_attribute_set_input);
      }
      else {
        static const bke::AnonymousAttributeSet empty_set;
        lf_attribute_set_input->set_default_value(&empty_set);
      }
    }
  }

  using JoinAttibuteSetsCache = Map<Vector<lf::OutputSocket *>, lf::OutputSocket *>;

  /**
   * Join multiple attributes set into a single attribute set that can be passed into a node.
   */
  lf::OutputSocket *join_attribute_sets(const Span<lf::OutputSocket *> attribute_set_sockets,
                                        const Span<lf::OutputSocket *> used_sockets,
                                        JoinAttibuteSetsCache &cache)
  {
    BLI_assert(attribute_set_sockets.size() == used_sockets.size());
    if (attribute_set_sockets.is_empty()) {
      return nullptr;
    }

    Vector<lf::OutputSocket *, 16> key;
    key.extend(attribute_set_sockets);
    key.extend(used_sockets);
    std::sort(key.begin(), key.end());
    return cache.lookup_or_add_cb(key, [&]() {
      const auto &lazy_function = LazyFunctionForAnonymousAttributeSetJoin::get_cached(
          attribute_set_sockets.size(), lf_graph_info_->functions);
      lf::Node &lf_node = lf_graph_->add_function(lazy_function);
      for (const int i : attribute_set_sockets.index_range()) {
        lf::InputSocket &lf_use_input = lf_node.input(lazy_function.get_use_input(i));
        socket_usage_inputs_.add(&lf_use_input);
        lf::InputSocket &lf_attributes_input = lf_node.input(
            lazy_function.get_attribute_set_input(i));
        lf_graph_->add_link(*used_sockets[i], lf_use_input);
        lf_graph_->add_link(*attribute_set_sockets[i], lf_attributes_input);
      }
      return &lf_node.output(0);
    });
  }

  /**
   * By depending on "the future" (whether a specific socket is used in the future), it is possible
   * to introduce cycles in the graph. This function finds those cycles and breaks them by removing
   * specific links.
   *
   * Example for a cycle: There is a `Distribute Points on Faces` node and its `Normal` output is
   * only used when the number of generated points is larger than 1000 because of some switch node
   * later in the tree. In this case, to know whether the `Normal` output is needed, one first has
   * to compute the points, but for that one has to know whether the normal information has to be
   * added to the points. The fix is to always add the normal information in this case.
   */
  void fix_link_cycles()
  {
    lf_graph_->update_socket_indices();
    const int sockets_num = lf_graph_->socket_num();

    struct SocketState {
      bool done = false;
      bool in_stack = false;
    };

    Array<SocketState> socket_states(sockets_num);

    Vector<lf::Socket *> lf_sockets_to_check;
    for (lf::Node *lf_node : lf_graph_->nodes()) {
      if (lf_node->is_function()) {
        for (lf::OutputSocket *lf_socket : lf_node->outputs()) {
          if (lf_socket->targets().is_empty()) {
            lf_sockets_to_check.append(lf_socket);
          }
        }
      }
      if (lf_node->outputs().is_empty()) {
        for (lf::InputSocket *lf_socket : lf_node->inputs()) {
          lf_sockets_to_check.append(lf_socket);
        }
      }
    }
    Vector<lf::Socket *> lf_socket_stack;
    while (!lf_sockets_to_check.is_empty()) {
      lf::Socket *lf_inout_socket = lf_sockets_to_check.last();
      lf::Node &lf_node = lf_inout_socket->node();
      SocketState &state = socket_states[lf_inout_socket->index_in_graph()];

      if (!state.in_stack) {
        lf_socket_stack.append(lf_inout_socket);
        state.in_stack = true;
      }

      Vector<lf::Socket *, 16> lf_origin_sockets;
      if (lf_inout_socket->is_input()) {
        lf::InputSocket &lf_input_socket = lf_inout_socket->as_input();
        if (lf::OutputSocket *lf_origin_socket = lf_input_socket.origin()) {
          lf_origin_sockets.append(lf_origin_socket);
        }
      }
      else {
        lf::OutputSocket &lf_output_socket = lf_inout_socket->as_output();
        if (lf_node.is_function()) {
          lf::FunctionNode &lf_function_node = static_cast<lf::FunctionNode &>(lf_node);
          const lf::LazyFunction &fn = lf_function_node.function();
          fn.possible_output_dependencies(
              lf_output_socket.index(), [&](const Span<int> input_indices) {
                for (const int input_index : input_indices) {
                  lf_origin_sockets.append(&lf_node.input(input_index));
                }
              });
        }
      }

      bool pushed_socket = false;
      bool detected_cycle = false;
      for (lf::Socket *lf_origin_socket : lf_origin_sockets) {
        if (socket_states[lf_origin_socket->index_in_graph()].in_stack) {
          /* A cycle has been detected. The cycle is broken by removing a link and replacing it
           * with a constant "true" input. This can only affect inputs which determine whether a
           * specific value is used. Therefore, setting it to a constant true can result in more
           * computation later, but does not change correctness.
           *
           * After the cycle is broken, the cycle-detection is "rolled back" to the socket where
           * the first socket of the cycle was found. This is necessary in case another cycle goes
           * through this socket. */

          detected_cycle = true;
          const int index_in_socket_stack = lf_socket_stack.first_index_of(lf_origin_socket);
          const int index_in_sockets_to_check = lf_sockets_to_check.first_index_of(
              lf_origin_socket);
          const Span<lf::Socket *> cycle = lf_socket_stack.as_span().drop_front(
              index_in_socket_stack);

          bool broke_cycle = false;
          for (lf::Socket *lf_cycle_socket : cycle) {
            if (lf_cycle_socket->is_input() &&
                socket_usage_inputs_.contains(&lf_cycle_socket->as_input())) {
              lf::InputSocket &lf_cycle_input_socket = lf_cycle_socket->as_input();
              lf_graph_->clear_origin(lf_cycle_input_socket);
              static const bool static_true = true;
              lf_cycle_input_socket.set_default_value(&static_true);
              broke_cycle = true;
            }
            /* This is actually removed from the stack when it is resized below. */
            SocketState &lf_cycle_socket_state = socket_states[lf_cycle_socket->index_in_graph()];
            lf_cycle_socket_state.in_stack = false;
          }
          if (!broke_cycle) {
            BLI_assert_unreachable();
          }
          /* Roll back algorithm by removing the sockets that corresponded to the cycle from the
           * stacks. */
          lf_socket_stack.resize(index_in_socket_stack);
          /* The +1 is there so that the socket itself is not removed. */
          lf_sockets_to_check.resize(index_in_sockets_to_check + 1);
          break;
        }
        else if (!socket_states[lf_origin_socket->index_in_graph()].done) {
          lf_sockets_to_check.append(lf_origin_socket);
          pushed_socket = true;
        }
      }
      if (detected_cycle) {
        continue;
      }
      if (pushed_socket) {
        continue;
      }

      state.done = true;
      state.in_stack = false;
      lf_sockets_to_check.pop_last();
      lf_socket_stack.pop_last();
    }
  }

  void print_graph();
};

class UsedSocketVisualizeOptions : public lf::Graph::ToDotOptions {
 private:
  const GeometryNodesLazyFunctionGraphBuilder &builder_;
  Map<const lf::Socket *, std::string> socket_font_colors_;
  Map<const lf::Socket *, std::string> socket_name_suffixes_;

 public:
  UsedSocketVisualizeOptions(const GeometryNodesLazyFunctionGraphBuilder &builder)
      : builder_(builder)
  {
    VectorSet<lf::OutputSocket *> found;
    for (const int bsocket_index : builder_.socket_is_used_map_.index_range()) {
      const bNodeSocket *bsocket = builder_.btree_.all_sockets()[bsocket_index];
      lf::OutputSocket *lf_used_socket = builder_.socket_is_used_map_[bsocket_index];
      if (lf_used_socket == nullptr) {
        continue;
      }
      const float hue = BLI_hash_int_01(uintptr_t(lf_used_socket));
      std::stringstream ss;
      ss.precision(3);
      ss << hue << " 0.9 0.5";
      const std::string color_str = ss.str();
      const std::string suffix = " (" + std::to_string(found.index_of_or_add(lf_used_socket)) +
                                 ")";
      socket_font_colors_.add(lf_used_socket, color_str);
      socket_name_suffixes_.add(lf_used_socket, suffix);

      if (bsocket->is_input()) {
        for (const lf::InputSocket *lf_socket : builder_.input_socket_map_.lookup(bsocket)) {
          socket_font_colors_.add(lf_socket, color_str);
          socket_name_suffixes_.add(lf_socket, suffix);
        }
      }
      else if (lf::OutputSocket *lf_socket = builder_.output_socket_map_.lookup_default(bsocket,
                                                                                        nullptr))
      {
        socket_font_colors_.add(lf_socket, color_str);
        socket_name_suffixes_.add(lf_socket, suffix);
      }
    }
  }

  std::optional<std::string> socket_font_color(const lf::Socket &socket) const override
  {
    if (const std::string *color = socket_font_colors_.lookup_ptr(&socket)) {
      return *color;
    }
    return std::nullopt;
  }

  std::string socket_name(const lf::Socket &socket) const override
  {
    return socket.name() + socket_name_suffixes_.lookup_default(&socket, "");
  }

  void add_edge_attributes(const lf::OutputSocket & /*from*/,
                           const lf::InputSocket &to,
                           dot::DirectedEdge &dot_edge) const override
  {
    if (builder_.socket_usage_inputs_.contains_as(&to)) {
      // dot_edge.attributes.set("constraint", "false");
      dot_edge.attributes.set("color", "#00000055");
    }
  }
};

void GeometryNodesLazyFunctionGraphBuilder::print_graph()
{
  UsedSocketVisualizeOptions options{*this};
  std::cout << "\n\n" << lf_graph_->to_dot(options) << "\n\n";
}

const GeometryNodesLazyFunctionGraphInfo *ensure_geometry_nodes_lazy_function_graph(
    const bNodeTree &btree)
{
  btree.ensure_topology_cache();
  if (btree.has_available_link_cycle()) {
    return nullptr;
  }
  if (const ID *id_orig = DEG_get_original_id(const_cast<ID *>(&btree.id))) {
    if (id_orig->tag & LIB_TAG_MISSING) {
      return nullptr;
    }
  }
  for (const bNodeSocket *interface_bsocket : btree.interface_inputs()) {
    if (interface_bsocket->typeinfo->geometry_nodes_cpp_type == nullptr) {
      return nullptr;
    }
  }
  for (const bNodeSocket *interface_bsocket : btree.interface_outputs()) {
    if (interface_bsocket->typeinfo->geometry_nodes_cpp_type == nullptr) {
      return nullptr;
    }
  }

  std::unique_ptr<GeometryNodesLazyFunctionGraphInfo> &lf_graph_info_ptr =
      btree.runtime->geometry_nodes_lazy_function_graph_info;

  if (lf_graph_info_ptr) {
    return lf_graph_info_ptr.get();
  }
  std::lock_guard lock{btree.runtime->geometry_nodes_lazy_function_graph_info_mutex};
  if (lf_graph_info_ptr) {
    return lf_graph_info_ptr.get();
  }

  auto lf_graph_info = std::make_unique<GeometryNodesLazyFunctionGraphInfo>();
  GeometryNodesLazyFunctionGraphBuilder builder{btree, *lf_graph_info};
  builder.build();

  lf_graph_info_ptr = std::move(lf_graph_info);
  return lf_graph_info_ptr.get();
}

GeometryNodesLazyFunctionLogger::GeometryNodesLazyFunctionLogger(
    const GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
    : lf_graph_info_(lf_graph_info)
{
}

void GeometryNodesLazyFunctionLogger::log_socket_value(
    const fn::lazy_function::Socket &lf_socket,
    const GPointer value,
    const fn::lazy_function::Context &context) const
{
  auto &user_data = *static_cast<GeoNodesLFUserData *>(context.user_data);
  if (!user_data.log_socket_values) {
    return;
  }
  auto &local_user_data = *static_cast<GeoNodesLFLocalUserData *>(context.local_user_data);
  if (local_user_data.tree_logger == nullptr) {
    return;
  }

  const Span<const bNodeSocket *> bsockets =
      lf_graph_info_.mapping.bsockets_by_lf_socket_map.lookup(&lf_socket);
  if (bsockets.is_empty()) {
    return;
  }

  for (const bNodeSocket *bsocket : bsockets) {
    /* Avoid logging to some sockets when the same value will also be logged to a linked socket.
     * This reduces the number of logged values without losing information. */
    if (bsocket->is_input() && bsocket->is_directly_linked()) {
      continue;
    }
    const bNode &bnode = bsocket->owner_node();
    if (bnode.is_reroute()) {
      continue;
    }
    local_user_data.tree_logger->log_value(bsocket->owner_node(), *bsocket, value);
  }
}

static std::mutex dump_error_context_mutex;

void GeometryNodesLazyFunctionLogger::dump_when_outputs_are_missing(
    const lf::FunctionNode &node,
    Span<const lf::OutputSocket *> missing_sockets,
    const lf::Context &context) const
{
  std::lock_guard lock{dump_error_context_mutex};

  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  user_data->compute_context->print_stack(std::cout, node.name());
  std::cout << "Missing outputs:\n";
  for (const lf::OutputSocket *socket : missing_sockets) {
    std::cout << "  " << socket->name() << "\n";
  }
}

void GeometryNodesLazyFunctionLogger::dump_when_input_is_set_twice(
    const lf::InputSocket &target_socket,
    const lf::OutputSocket &from_socket,
    const lf::Context &context) const
{
  std::lock_guard lock{dump_error_context_mutex};

  std::stringstream ss;
  ss << from_socket.node().name() << ":" << from_socket.name() << " -> "
     << target_socket.node().name() << ":" << target_socket.name();

  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  user_data->compute_context->print_stack(std::cout, ss.str());
}

Vector<const lf::FunctionNode *> GeometryNodesLazyFunctionSideEffectProvider::
    get_nodes_with_side_effects(const lf::Context &context) const
{
  GeoNodesLFUserData *user_data = dynamic_cast<GeoNodesLFUserData *>(context.user_data);
  BLI_assert(user_data != nullptr);
  const ComputeContextHash &context_hash = user_data->compute_context->hash();
  const GeoNodesModifierData &modifier_data = *user_data->modifier_data;
  return modifier_data.side_effect_nodes->lookup(context_hash);
}

GeometryNodesLazyFunctionGraphInfo::GeometryNodesLazyFunctionGraphInfo() = default;
GeometryNodesLazyFunctionGraphInfo::~GeometryNodesLazyFunctionGraphInfo()
{
  for (GMutablePointer &p : this->values_to_destruct) {
    p.destruct();
  }
}

[[maybe_unused]] static void add_thread_id_debug_message(
    const GeometryNodesLazyFunctionGraphInfo &lf_graph_info,
    const lf::FunctionNode &node,
    const lf::Context &context)
{
  static std::atomic<int> thread_id_source = 0;
  static thread_local const int thread_id = thread_id_source.fetch_add(1);
  static thread_local const std::string thread_id_str = "Thread: " + std::to_string(thread_id);

  const auto &local_user_data = *static_cast<GeoNodesLFLocalUserData *>(context.local_user_data);
  if (local_user_data.tree_logger == nullptr) {
    return;
  }

  /* Find corresponding node based on the socket mapping. */
  auto check_sockets = [&](const Span<const lf::Socket *> lf_sockets) {
    for (const lf::Socket *lf_socket : lf_sockets) {
      const Span<const bNodeSocket *> bsockets =
          lf_graph_info.mapping.bsockets_by_lf_socket_map.lookup(lf_socket);
      if (!bsockets.is_empty()) {
        const bNodeSocket &bsocket = *bsockets[0];
        const bNode &bnode = bsocket.owner_node();
        local_user_data.tree_logger->debug_messages.append({bnode.identifier, thread_id_str});
        return true;
      }
    }
    return false;
  };

  if (check_sockets(node.inputs().cast<const lf::Socket *>())) {
    return;
  }
  check_sockets(node.outputs().cast<const lf::Socket *>());
}

void GeometryNodesLazyFunctionLogger::log_before_node_execute(const lf::FunctionNode &node,
                                                              const lf::Params & /*params*/,
                                                              const lf::Context &context) const
{
  /* Enable this to see the threads that invoked a node. */
  if constexpr (false) {
    add_thread_id_debug_message(lf_graph_info_, node, context);
  }
}

destruct_ptr<lf::LocalUserData> GeoNodesLFUserData::get_local(LinearAllocator<> &allocator)
{
  return allocator.construct<GeoNodesLFLocalUserData>(*this);
}

GeoNodesLFLocalUserData::GeoNodesLFLocalUserData(GeoNodesLFUserData &user_data)
{
  if (user_data.modifier_data->eval_log != nullptr) {
    this->tree_logger = &user_data.modifier_data->eval_log->get_local_tree_logger(
        *user_data.compute_context);
  }
}

}  // namespace blender::nodes
