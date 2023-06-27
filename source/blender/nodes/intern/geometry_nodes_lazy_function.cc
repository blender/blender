/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 *
 * This file mainly converts a #bNodeTree into a lazy-function graph, that can then be evaluated to
 * execute geometry nodes. This generally works by creating a lazy-function for every node, which
 * is then put into the lazy-function graph. Then the nodes in the new graph are linked based on
 * links in the original #bNodeTree. Some additional nodes are inserted for things like type
 * conversions and multi-input sockets.
 *
 * If the #bNodeTree contains zones, those are turned into separate lazy-functions first.
 * Essentially, a separate lazy-function graph is created for every zone that is than called by the
 * parent zone or by the root graph.
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
#include "BKE_node_tree_anonymous_attributes.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_type_conversions.hh"

#include "FN_field_cpp_type.hh"
#include "FN_lazy_function_graph_executor.hh"

#include "DEG_depsgraph_query.h"

#include <fmt/format.h>

namespace blender::nodes {

namespace aai = bke::anonymous_attribute_inferencing;
using bke::bNodeTreeZone;
using bke::bNodeTreeZones;
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
    GField output_field{std::make_shared<AnonymousAttributeFieldInput>(
        std::move(attribute_id),
        value_or_field_cpp_type.value,
        fmt::format(TIP_("{} node"), std::string_view(node_.label_or_name())))};
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
    const IndexMask mask(1);
    mf::ParamsBuilder params{fn, &mask};
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
    fn.call(mask, params, context);
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
              bke::GeometryComponent::Type::Instance);
          bke::try_capture_field_on_geometry(
              component, viewer_attribute_name, ATTR_DOMAIN_INSTANCE, field);
        }
      }
      else {
        geometry.modify_geometry_sets([&](GeometrySet &geometry) {
          for (const bke::GeometryComponent::Type type : {bke::GeometryComponent::Type::Mesh,
                                                          bke::GeometryComponent::Type::PointCloud,
                                                          bke::GeometryComponent::Type::Curve})
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
    return fmt::format(TIP_("Group '{}' ({})"), group_node_.id->name + 2, group_node_.name);
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
        return fmt::format(TIP_("'{}' input is used"),
                           group_node_.input_socket(bsocket_index).name);
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
    inputs_.append_as("Use", CPPType::get<bool>());
    inputs_.append_as("Field", type.self, lf::ValueUsage::Maybe);
    outputs_.append_as("Attributes", CPPType::get<bke::AnonymousAttributeSet>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const bool use = params.get_input<bool>(0);
    if (!use) {
      params.set_output<bke::AnonymousAttributeSet>(0, {});
      return;
    }
    const void *value_or_field = params.try_get_input_data_ptr_or_request(1);
    if (value_or_field == nullptr) {
      /* Wait until the field is computed. */
      return;
    }

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
  static const LazyFunctionForAnonymousAttributeSetJoin &get_cached(const int amount,
                                                                    ResourceScope &scope)
  {
    constexpr int cache_amount = 16;
    static std::array<LazyFunctionForAnonymousAttributeSetJoin, cache_amount> cached_functions =
        get_cache(std::make_index_sequence<cache_amount>{});
    if (amount < cached_functions.size()) {
      return cached_functions[amount];
    }

    return scope.construct<LazyFunctionForAnonymousAttributeSetJoin>(amount);
  }

 private:
  template<size_t... I>
  static std::array<LazyFunctionForAnonymousAttributeSetJoin, sizeof...(I)> get_cache(
      std::index_sequence<I...> /*indices*/)
  {
    return {LazyFunctionForAnonymousAttributeSetJoin(I)...};
  }
};

class LazyFunctionForSimulationZone : public LazyFunction {
 private:
  const bNode &sim_output_bnode_;
  const LazyFunction &fn_;

 public:
  LazyFunctionForSimulationZone(const bNode &sim_output_bnode, const LazyFunction &fn)
      : sim_output_bnode_(sim_output_bnode), fn_(fn)
  {
    debug_name_ = "Simulation Zone";
    inputs_ = fn.inputs();
    outputs_ = fn.outputs();
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const override
  {
    GeoNodesLFUserData &user_data = *static_cast<GeoNodesLFUserData *>(context.user_data);

    bke::SimulationZoneComputeContext compute_context{user_data.compute_context,
                                                      sim_output_bnode_};

    GeoNodesLFUserData zone_user_data = user_data;
    zone_user_data.compute_context = &compute_context;
    if (user_data.modifier_data->socket_log_contexts) {
      zone_user_data.log_socket_values = user_data.modifier_data->socket_log_contexts->contains(
          compute_context.hash());
    }
    GeoNodesLFLocalUserData zone_local_user_data{zone_user_data};

    lf::Context zone_context{context.storage, &zone_user_data, &zone_local_user_data};
    fn_.execute(params, zone_context);
  }

  void *init_storage(LinearAllocator<> &allocator) const override
  {
    return fn_.init_storage(allocator);
  }

  void destruct_storage(void *storage) const override
  {
    fn_.destruct_storage(storage);
  }

  std::string input_name(const int i) const override
  {
    return fn_.input_name(i);
  }

  std::string output_name(const int i) const override
  {
    return fn_.output_name(i);
  }
};

using JoinAttributeSetsCache = Map<Vector<lf::OutputSocket *>, lf::OutputSocket *>;

struct BuildGraphParams {
  /** Lazy-function graph that nodes and links should be inserted into. */
  lf::Graph &lf_graph;
  /** Map #bNodeSocket to newly generated sockets. Those maps are later used to insert links. */
  MultiValueMap<const bNodeSocket *, lf::InputSocket *> lf_inputs_by_bsocket;
  Map<const bNodeSocket *, lf::OutputSocket *> lf_output_by_bsocket;
  /**
   * Maps sockets to corresponding generated boolean sockets that indicate whether the socket is
   * used or not.
   */
  Map<const bNodeSocket *, lf::OutputSocket *> usage_by_bsocket;
  /**
   * Nodes that propagate anonymous attributes have to know which of those attributes to propagate.
   * For that they have an attribute set input for each geometry output.
   */
  Map<const bNodeSocket *, lf::InputSocket *> lf_attribute_set_input_by_output_geometry_bsocket;
  /**
   * Multi-input sockets are split into a separate node that collects all the individual values and
   * then passes them to the main node function as list.
   */
  Map<const bNodeSocket *, lf::Node *> multi_input_socket_nodes;
  /**
   * This is similar to #lf_inputs_by_bsocket but contains more relevant information when border
   * links are linked to multi-input sockets.
   */
  Map<const bNodeLink *, lf::InputSocket *> lf_input_by_border_link;
  /**
   * Keeps track of all boolean inputs that indicate whether a socket is used. Links to those
   * sockets may be replaced with a constant-true if necessary to break dependency cycles in
   * #fix_link_cycles.
   */
  Set<lf::InputSocket *> socket_usage_inputs;
  /**
   * Collect input sockets that anonymous attribute sets based on fields or group inputs have to be
   * linked to later.
   */
  MultiValueMap<int, lf::InputSocket *> lf_attribute_set_input_by_field_source_index;
  MultiValueMap<int, lf::InputSocket *> lf_attribute_set_input_by_caller_propagation_index;
  /**  */
  /** Cache to avoid building the same socket combinations multiple times. */
  Map<Vector<lf::OutputSocket *>, lf::OutputSocket *> socket_usages_combination_cache;
};

struct ZoneBuildInfo {
  /** The lazy function that contains the zone. */
  const LazyFunction *lazy_function = nullptr;

  /** Information about what the various inputs and outputs of the lazy-function are. */
  IndexRange main_input_indices;
  IndexRange main_output_indices;
  IndexRange border_link_input_indices;

  IndexRange main_input_usage_indices;
  IndexRange main_output_usage_indices;
  IndexRange border_link_input_usage_indices;

  Map<int, int> attribute_set_input_by_field_source_index;
  Map<int, int> attribute_set_input_by_caller_propagation_index;
};

/**
 * Utility class to build a lazy-function graph based on a geometry nodes tree.
 * This is mainly a separate class because it makes it easier to have variables that can be
 * accessed by many functions.
 */
struct GeometryNodesLazyFunctionGraphBuilder {
 private:
  const bNodeTree &btree_;
  const aai::AnonymousAttributeInferencingResult &attribute_inferencing_;
  ResourceScope &scope_;
  NodeMultiFunctions &node_multi_functions_;
  GeometryNodesLazyFunctionGraphInfo *lf_graph_info_;
  GeometryNodeLazyFunctionGraphMapping *mapping_;
  const bke::DataTypeConversions *conversions_;

  /**
   * All group input nodes are combined into one dummy node in the lazy-function graph.
   */
  lf::DummyNode *group_input_lf_node_;
  /**
   * A #LazyFunctionForSimulationInputsUsage for each simulation zone.
   */
  Map<const bNode *, lf::Node *> simulation_inputs_usage_nodes_;

  const bNodeTreeZones *tree_zones_;
  Array<ZoneBuildInfo> zone_build_infos_;

  friend class UsedSocketVisualizeOptions;

 public:
  GeometryNodesLazyFunctionGraphBuilder(const bNodeTree &btree,
                                        GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
      : btree_(btree),
        attribute_inferencing_(*btree.runtime->anonymous_attribute_inferencing),
        scope_(lf_graph_info.scope),
        node_multi_functions_(lf_graph_info.scope.construct<NodeMultiFunctions>(btree)),
        lf_graph_info_(&lf_graph_info)
  {
  }

  void build()
  {
    btree_.ensure_topology_cache();

    mapping_ = &lf_graph_info_->mapping;
    conversions_ = &bke::get_implicit_type_conversions();
    tree_zones_ = btree_.zones();

    this->initialize_mapping_arrays();
    this->build_zone_functions();
    this->build_root_graph();
  }

 private:
  void initialize_mapping_arrays()
  {
    mapping_->lf_input_index_for_output_bsocket_usage.reinitialize(
        btree_.all_output_sockets().size());
    mapping_->lf_input_index_for_output_bsocket_usage.fill(-1);
    mapping_->lf_input_index_for_attribute_propagation_to_output.reinitialize(
        btree_.all_output_sockets().size());
    mapping_->lf_input_index_for_attribute_propagation_to_output.fill(-1);
    mapping_->lf_index_by_bsocket.reinitialize(btree_.all_sockets().size());
    mapping_->lf_index_by_bsocket.fill(-1);
  }

  /**
   * Builds lazy-functions for all zones in the node tree.
   */
  void build_zone_functions()
  {
    zone_build_infos_.reinitialize(tree_zones_->zones.size());

    const Array<int> zone_build_order = this->compute_zone_build_order();

    for (const int zone_i : zone_build_order) {
      const bNodeTreeZone &zone = *tree_zones_->zones[zone_i];
      BLI_assert(zone.output_node->type == GEO_NODE_SIMULATION_OUTPUT);
      this->build_simulation_zone_function(zone);
    }
  }

  Array<int> compute_zone_build_order()
  {
    /* Build nested zones first. */
    Array<int> zone_build_order(tree_zones_->zones.size());
    std::iota(zone_build_order.begin(), zone_build_order.end(), 0);
    std::sort(
        zone_build_order.begin(), zone_build_order.end(), [&](const int zone_a, const int zone_b) {
          return tree_zones_->zones[zone_a]->depth > tree_zones_->zones[zone_b]->depth;
        });
    return zone_build_order;
  }

  /**
   * Builds a lazy-function for a simulation zone.
   * Internally, the generated lazy-function is just another graph.
   */
  void build_simulation_zone_function(const bNodeTreeZone &zone)
  {
    const int zone_i = zone.index;
    ZoneBuildInfo &zone_info = zone_build_infos_[zone_i];
    lf::Graph &lf_graph = scope_.construct<lf::Graph>();

    lf::Node *lf_zone_input_node = nullptr;
    lf::Node *lf_main_input_usage_node = nullptr;
    if (zone.input_node != nullptr) {
      lf_zone_input_node = &this->build_dummy_node_for_sockets(
          "Zone Input", {}, zone.input_node->input_sockets().drop_back(1), lf_graph);
      lf_main_input_usage_node = &this->build_dummy_node_for_socket_usages(
          "Input Usages", zone.input_node->input_sockets().drop_back(1), {}, lf_graph);
    }
    lf::Node &lf_border_link_input_node = this->build_zone_border_links_input_node(zone, lf_graph);
    lf::Node &lf_zone_output_node = this->build_dummy_node_for_sockets(
        "Zone Output", zone.output_node->output_sockets().drop_back(1), {}, lf_graph);
    lf::Node &lf_main_output_usage_node = this->build_dummy_node_for_socket_usages(
        "Output Usages", {}, zone.output_node->output_sockets().drop_back(1), lf_graph);
    lf::Node &lf_border_link_usage_node = this->build_border_link_input_usage_node(zone, lf_graph);

    lf::Node &lf_simulation_usage_node = [&]() -> lf::Node & {
      auto &lazy_function = scope_.construct<LazyFunctionForSimulationInputsUsage>();
      lf::Node &lf_node = lf_graph.add_function(lazy_function);

      if (lf_main_input_usage_node) {
        for (const int i : lf_main_input_usage_node->inputs().index_range()) {
          lf_graph.add_link(lf_node.output(0), lf_main_input_usage_node->input(i));
        }
      }
      return lf_node;
    }();

    BuildGraphParams graph_params{lf_graph};

    lf::FunctionNode *lf_simulation_input = nullptr;
    if (zone.input_node) {
      lf_simulation_input = this->insert_simulation_input_node(
          btree_, *zone.input_node, graph_params);
    }
    lf::FunctionNode &lf_simulation_output = this->insert_simulation_output_node(*zone.output_node,
                                                                                 graph_params);

    for (const bNodeSocket *bsocket : zone.output_node->input_sockets().drop_back(1)) {
      graph_params.usage_by_bsocket.add(bsocket, &lf_simulation_usage_node.output(1));
    }

    this->insert_nodes_and_zones(zone.child_nodes, zone.child_zones, graph_params);

    if (zone.input_node) {
      this->build_output_socket_usages(*zone.input_node, graph_params);
    }
    for (const auto item : graph_params.lf_output_by_bsocket.items()) {
      this->insert_links_from_socket(*item.key, *item.value, graph_params);
    }

    this->link_border_link_inputs_and_usages(
        zone, lf_border_link_input_node, lf_border_link_usage_node, graph_params);

    if (lf_zone_input_node != nullptr) {
      for (const int i : lf_zone_input_node->outputs().index_range()) {
        lf_graph.add_link(lf_zone_input_node->output(i), lf_simulation_input->input(i));
      }
    }
    for (const int i : lf_zone_output_node.inputs().index_range()) {
      lf_graph.add_link(lf_simulation_output.output(i), lf_zone_output_node.input(i));
    }

    this->add_default_inputs(graph_params);

    Vector<const lf::OutputSocket *, 16> lf_zone_inputs;
    if (lf_zone_input_node) {
      lf_zone_inputs.extend(lf_zone_input_node->outputs());
      zone_info.main_input_indices = lf_zone_inputs.index_range();
    }
    lf_zone_inputs.extend(lf_border_link_input_node.outputs());
    zone_info.border_link_input_indices = lf_zone_inputs.index_range().take_back(
        lf_border_link_input_node.outputs().size());

    lf_zone_inputs.extend(lf_main_output_usage_node.outputs());
    zone_info.main_output_usage_indices = lf_zone_inputs.index_range().take_back(
        lf_main_output_usage_node.outputs().size());

    Map<int, lf::OutputSocket *> lf_attribute_set_by_field_source_index;
    Map<int, lf::OutputSocket *> lf_attribute_set_by_caller_propagation_index;
    this->build_attribute_set_inputs_for_zone(graph_params,
                                              zone_info,
                                              lf_attribute_set_by_field_source_index,
                                              lf_attribute_set_by_caller_propagation_index,
                                              lf_zone_inputs);
    this->link_attribute_set_inputs(lf_graph,
                                    graph_params,
                                    lf_attribute_set_by_field_source_index,
                                    lf_attribute_set_by_caller_propagation_index);
    this->fix_link_cycles(lf_graph, graph_params.socket_usage_inputs);

    Vector<const lf::InputSocket *, 16> lf_zone_outputs;
    lf_zone_outputs.extend(lf_zone_output_node.inputs());
    zone_info.main_output_indices = lf_zone_outputs.index_range();

    if (lf_main_input_usage_node) {
      lf_zone_outputs.extend(lf_main_input_usage_node->inputs());
      zone_info.main_input_usage_indices = lf_zone_outputs.index_range().take_back(
          lf_main_input_usage_node->inputs().size());
    }

    lf_zone_outputs.extend(lf_border_link_usage_node.inputs());
    zone_info.border_link_input_usage_indices = lf_zone_outputs.index_range().take_back(
        lf_border_link_usage_node.inputs().size());

    lf_graph.update_node_indices();

    auto &logger = scope_.construct<GeometryNodesLazyFunctionLogger>(*lf_graph_info_);
    auto &side_effect_provider = scope_.construct<GeometryNodesLazyFunctionSideEffectProvider>();

    const auto &lf_graph_fn = scope_.construct<lf::GraphExecutor>(
        lf_graph, lf_zone_inputs, lf_zone_outputs, &logger, &side_effect_provider);
    const auto &zone_function = scope_.construct<LazyFunctionForSimulationZone>(*zone.output_node,
                                                                                lf_graph_fn);
    zone_info.lazy_function = &zone_function;

    // std::cout << "\n\n" << lf_graph.to_dot() << "\n\n";
  }

  lf::DummyNode &build_zone_border_links_input_node(const bNodeTreeZone &zone, lf::Graph &lf_graph)
  {
    auto &debug_info = scope_.construct<lf::SimpleDummyDebugInfo>();
    debug_info.name = "Border Links";
    Vector<const CPPType *, 16> border_link_types;
    for (const bNodeLink *border_link : zone.border_links) {
      border_link_types.append(border_link->tosock->typeinfo->geometry_nodes_cpp_type);
      debug_info.output_names.append(StringRef("Link from ") + border_link->fromsock->identifier);
    }
    lf::DummyNode &node = lf_graph.add_dummy({}, border_link_types, &debug_info);
    return node;
  }

  lf::DummyNode &build_border_link_input_usage_node(const bNodeTreeZone &zone, lf::Graph &lf_graph)
  {
    auto &debug_info = scope_.construct<lf::SimpleDummyDebugInfo>();
    debug_info.name = "Border Link Usages";
    Vector<const CPPType *, 16> types;
    types.append_n_times(&CPPType::get<bool>(), zone.border_links.size());
    debug_info.input_names.append_n_times("Usage", types.size());
    lf::DummyNode &node = lf_graph.add_dummy(types, {}, &debug_info);
    return node;
  }

  void build_attribute_set_inputs_for_zone(
      BuildGraphParams &graph_params,
      ZoneBuildInfo &zone_info,
      Map<int, lf::OutputSocket *> &lf_attribute_set_by_field_source_index,
      Map<int, lf::OutputSocket *> &lf_attribute_set_by_caller_propagation_index,
      Vector<const lf::OutputSocket *, 16> &lf_zone_inputs)
  {
    const Vector<int> all_required_field_sources = this->find_all_required_field_source_indices(
        graph_params.lf_attribute_set_input_by_output_geometry_bsocket,
        graph_params.lf_attribute_set_input_by_field_source_index);
    const Vector<int> all_required_caller_propagation_indices =
        this->find_all_required_caller_propagation_indices(
            graph_params.lf_attribute_set_input_by_output_geometry_bsocket,
            graph_params.lf_attribute_set_input_by_caller_propagation_index);

    Map<int, int> input_by_field_source_index;

    for (const int field_source_index : all_required_field_sources) {
      const aai::FieldSource &field_source =
          attribute_inferencing_.all_field_sources[field_source_index];
      if ([[maybe_unused]] const auto *input_field_source = std::get_if<aai::InputFieldSource>(
              &field_source.data))
      {
        input_by_field_source_index.add_new(field_source_index,
                                            input_by_field_source_index.size());
      }
      else {
        const auto &socket_field_source = std::get<aai::SocketFieldSource>(field_source.data);
        const bNodeSocket &bsocket = *socket_field_source.socket;
        if (lf::OutputSocket *lf_field_socket = graph_params.lf_output_by_bsocket.lookup_default(
                &bsocket, nullptr))
        {
          lf::OutputSocket *lf_usage_socket = graph_params.usage_by_bsocket.lookup_default(
              &bsocket, nullptr);
          lf::OutputSocket &lf_attribute_set_socket = this->get_extracted_attributes(
              *lf_field_socket,
              lf_usage_socket,
              graph_params.lf_graph,
              graph_params.socket_usage_inputs);
          lf_attribute_set_by_field_source_index.add(field_source_index, &lf_attribute_set_socket);
        }
        else {
          input_by_field_source_index.add_new(field_source_index,
                                              input_by_field_source_index.size());
        }
      }
    }

    {
      auto &debug_info = scope_.construct<lf::SimpleDummyDebugInfo>();
      debug_info.name = "Attribute Sets";
      Vector<const CPPType *, 16> types;
      const int num = input_by_field_source_index.size() +
                      all_required_caller_propagation_indices.size();
      types.append_n_times(&CPPType::get<bke::AnonymousAttributeSet>(), num);
      debug_info.output_names.append_n_times("Set", num);
      lf::DummyNode &node = graph_params.lf_graph.add_dummy({}, types, &debug_info);

      for (const auto item : input_by_field_source_index.items()) {
        const int field_source_index = item.key;
        const int attribute_set_index = item.value;
        lf::OutputSocket &lf_attribute_set_socket = node.output(attribute_set_index);
        lf_attribute_set_by_field_source_index.add(field_source_index, &lf_attribute_set_socket);

        const int zone_input_index = lf_zone_inputs.append_and_get_index(&lf_attribute_set_socket);
        zone_info.attribute_set_input_by_field_source_index.add(field_source_index,
                                                                zone_input_index);
      }
      for (const int i : all_required_caller_propagation_indices.index_range()) {
        const int caller_propagation_index = all_required_caller_propagation_indices[i];
        lf::OutputSocket &lf_attribute_set_socket = node.output(
            input_by_field_source_index.size() + i);
        lf_attribute_set_by_caller_propagation_index.add_new(caller_propagation_index,
                                                             &lf_attribute_set_socket);
        const int zone_input_index = lf_zone_inputs.append_and_get_index(&lf_attribute_set_socket);
        zone_info.attribute_set_input_by_caller_propagation_index.add(caller_propagation_index,
                                                                      zone_input_index);
      }
    }
  }

  /**
   * Build the graph that contains all nodes that are not contained in any zone. This graph is
   * called when this geometry nodes node group is evaluated.
   */
  void build_root_graph()
  {
    lf::Graph &lf_graph = lf_graph_info_->graph;

    this->build_group_input_node(lf_graph);
    if (btree_.group_output_node() == nullptr) {
      this->build_fallback_output_node(lf_graph);
    }

    lf::Node &lf_output_usage_node = this->build_output_usage_input_node(lf_graph);
    this->build_input_usage_output_node(lf_graph);

    BuildGraphParams graph_params{lf_graph};
    if (const bNode *group_output_bnode = btree_.group_output_node()) {
      for (const bNodeSocket *bsocket : group_output_bnode->input_sockets().drop_back(1)) {
        graph_params.usage_by_bsocket.add(bsocket, &lf_output_usage_node.output(bsocket->index()));
      }
    }

    this->insert_nodes_and_zones(
        tree_zones_->nodes_outside_zones, tree_zones_->root_zones, graph_params);

    for (const auto item : graph_params.lf_output_by_bsocket.items()) {
      this->insert_links_from_socket(*item.key, *item.value, graph_params);
    }
    this->build_group_input_usages(graph_params);
    this->add_default_inputs(graph_params);

    this->build_attribute_propagation_input_node(lf_graph);

    Map<int, lf::OutputSocket *> lf_attribute_set_by_field_source_index;
    Map<int, lf::OutputSocket *> lf_attribute_set_by_caller_propagation_index;
    this->build_attribute_set_inputs_outside_of_zones(
        graph_params,
        lf_attribute_set_by_field_source_index,
        lf_attribute_set_by_caller_propagation_index);
    this->link_attribute_set_inputs(lf_graph,
                                    graph_params,
                                    lf_attribute_set_by_field_source_index,
                                    lf_attribute_set_by_caller_propagation_index);

    this->fix_link_cycles(lf_graph, graph_params.socket_usage_inputs);

    // std::cout << "\n\n" << lf_graph.to_dot() << "\n\n";

    lf_graph.update_node_indices();
    lf_graph_info_->num_inline_nodes_approximate += lf_graph.nodes().size();
  }

  void build_attribute_set_inputs_outside_of_zones(
      BuildGraphParams &graph_params,
      Map<int, lf::OutputSocket *> &lf_attribute_set_by_field_source_index,
      Map<int, lf::OutputSocket *> &lf_attribute_set_by_caller_propagation_index)
  {
    const Vector<int> all_required_field_sources = this->find_all_required_field_source_indices(
        graph_params.lf_attribute_set_input_by_output_geometry_bsocket,
        graph_params.lf_attribute_set_input_by_field_source_index);

    for (const int field_source_index : all_required_field_sources) {
      const aai::FieldSource &field_source =
          attribute_inferencing_.all_field_sources[field_source_index];
      lf::OutputSocket *lf_attribute_set_socket;
      if (const auto *input_field_source = std::get_if<aai::InputFieldSource>(&field_source.data))
      {
        const int input_index = input_field_source->input_index;
        lf::OutputSocket &lf_field_socket = const_cast<lf::OutputSocket &>(
            *mapping_->group_input_sockets[input_index]);
        lf::OutputSocket *lf_usage_socket = const_cast<lf::OutputSocket *>(
            mapping_->group_input_usage_sockets[input_index]->origin());
        lf_attribute_set_socket = &this->get_extracted_attributes(
            lf_field_socket,
            lf_usage_socket,
            graph_params.lf_graph,
            graph_params.socket_usage_inputs);
      }
      else {
        const auto &socket_field_source = std::get<aai::SocketFieldSource>(field_source.data);
        const bNodeSocket &bsocket = *socket_field_source.socket;
        lf::OutputSocket &lf_field_socket = *graph_params.lf_output_by_bsocket.lookup(&bsocket);
        lf::OutputSocket *lf_usage_socket = graph_params.usage_by_bsocket.lookup_default(&bsocket,
                                                                                         nullptr);
        lf_attribute_set_socket = &this->get_extracted_attributes(
            lf_field_socket,
            lf_usage_socket,
            graph_params.lf_graph,
            graph_params.socket_usage_inputs);
      }
      lf_attribute_set_by_field_source_index.add_new(field_source_index, lf_attribute_set_socket);
    }

    for (const int caller_propagation_index :
         attribute_inferencing_.propagated_output_geometry_indices.index_range())
    {
      const int group_output_index =
          attribute_inferencing_.propagated_output_geometry_indices[caller_propagation_index];
      lf::OutputSocket &lf_attribute_set_socket = const_cast<lf::OutputSocket &>(
          *mapping_->attribute_set_by_geometry_output.lookup(group_output_index));
      lf_attribute_set_by_caller_propagation_index.add(caller_propagation_index,
                                                       &lf_attribute_set_socket);
    }
  }

  Vector<int> find_all_required_field_source_indices(
      const Map<const bNodeSocket *, lf::InputSocket *>
          &lf_attribute_set_input_by_output_geometry_bsocket,
      const MultiValueMap<int, lf::InputSocket *> &lf_attribute_set_input_by_field_source_index)
  {
    BitVector<> all_required_field_sources(attribute_inferencing_.all_field_sources.size(), false);
    for (const bNodeSocket *geometry_output_bsocket :
         lf_attribute_set_input_by_output_geometry_bsocket.keys())
    {
      all_required_field_sources |=
          attribute_inferencing_
              .required_fields_by_geometry_socket[geometry_output_bsocket->index_in_tree()];
    }
    for (const int field_source_index : lf_attribute_set_input_by_field_source_index.keys()) {
      all_required_field_sources[field_source_index].set();
    }

    Vector<int> indices;
    bits::foreach_1_index(all_required_field_sources, [&](const int i) { indices.append(i); });
    return indices;
  }

  Vector<int> find_all_required_caller_propagation_indices(
      const Map<const bNodeSocket *, lf::InputSocket *>
          &lf_attribute_set_input_by_output_geometry_bsocket,
      const MultiValueMap<int, lf::InputSocket *>
          &lf_attribute_set_input_by_caller_propagation_index)
  {
    BitVector<> all_required_caller_propagation_indices(
        attribute_inferencing_.propagated_output_geometry_indices.size(), false);
    for (const bNodeSocket *geometry_output_bs :
         lf_attribute_set_input_by_output_geometry_bsocket.keys())
    {
      all_required_caller_propagation_indices |=
          attribute_inferencing_
              .propagate_to_output_by_geometry_socket[geometry_output_bs->index_in_tree()];
    }
    for (const int caller_propagation_index :
         lf_attribute_set_input_by_caller_propagation_index.keys())
    {
      all_required_caller_propagation_indices[caller_propagation_index].set();
    }

    Vector<int> indices;
    bits::foreach_1_index(all_required_caller_propagation_indices,
                          [&](const int i) { indices.append(i); });
    return indices;
  }

  void link_attribute_set_inputs(
      lf::Graph &lf_graph,
      BuildGraphParams &graph_params,
      const Map<int, lf::OutputSocket *> &lf_attribute_set_by_field_source_index,
      const Map<int, lf::OutputSocket *> &lf_attribute_set_by_caller_propagation_index)
  {
    JoinAttributeSetsCache join_attribute_sets_cache;

    for (const MapItem<const bNodeSocket *, lf::InputSocket *> item :
         graph_params.lf_attribute_set_input_by_output_geometry_bsocket.items())
    {
      const bNodeSocket &geometry_output_bsocket = *item.key;
      lf::InputSocket &lf_attribute_set_input = *item.value;

      Vector<lf::OutputSocket *> lf_attribute_set_sockets;

      const BoundedBitSpan required_fields =
          attribute_inferencing_
              .required_fields_by_geometry_socket[geometry_output_bsocket.index_in_tree()];
      bits::foreach_1_index(required_fields, [&](const int field_source_index) {
        const auto &field_source = attribute_inferencing_.all_field_sources[field_source_index];
        if (const auto *socket_field_source = std::get_if<aai::SocketFieldSource>(
                &field_source.data)) {
          if (&socket_field_source->socket->owner_node() == &geometry_output_bsocket.owner_node())
          {
            return;
          }
        }
        lf_attribute_set_sockets.append(
            lf_attribute_set_by_field_source_index.lookup(field_source_index));
      });

      const BoundedBitSpan required_caller_propagations =
          attribute_inferencing_
              .propagate_to_output_by_geometry_socket[geometry_output_bsocket.index_in_tree()];
      bits::foreach_1_index(required_caller_propagations, [&](const int caller_propagation_index) {
        lf_attribute_set_sockets.append(
            lf_attribute_set_by_caller_propagation_index.lookup(caller_propagation_index));
      });

      if (lf::OutputSocket *lf_attribute_set = this->join_attribute_sets(
              lf_attribute_set_sockets,
              join_attribute_sets_cache,
              lf_graph,
              graph_params.socket_usage_inputs))
      {
        lf_graph.add_link(*lf_attribute_set, lf_attribute_set_input);
      }
      else {
        static const bke::AnonymousAttributeSet empty_set;
        lf_attribute_set_input.set_default_value(&empty_set);
      }
    }

    for (const auto item : graph_params.lf_attribute_set_input_by_field_source_index.items()) {
      const int field_source_index = item.key;
      lf::OutputSocket &lf_attribute_set_socket = *lf_attribute_set_by_field_source_index.lookup(
          field_source_index);
      for (lf::InputSocket *lf_attribute_set_input : item.value) {
        lf_graph.add_link(lf_attribute_set_socket, *lf_attribute_set_input);
      }
    }
    for (const auto item : graph_params.lf_attribute_set_input_by_caller_propagation_index.items())
    {
      const int caller_propagation_index = item.key;
      lf::OutputSocket &lf_attribute_set_socket =
          *lf_attribute_set_by_caller_propagation_index.lookup(caller_propagation_index);
      for (lf::InputSocket *lf_attribute_set_input : item.value) {
        lf_graph.add_link(lf_attribute_set_socket, *lf_attribute_set_input);
      }
    }
  }

  void insert_nodes_and_zones(const Span<const bNode *> bnodes,
                              const Span<const bNodeTreeZone *> zones,
                              BuildGraphParams &graph_params)
  {
    Vector<const bNode *> nodes_to_insert = bnodes;
    Map<const bNode *, const bNodeTreeZone *> zone_by_output;
    for (const bNodeTreeZone *zone : zones) {
      nodes_to_insert.append(zone->output_node);
      zone_by_output.add(zone->output_node, zone);
    }
    /* Insert nodes from right to left so that usage sockets can be build in the same pass. */
    std::sort(nodes_to_insert.begin(), nodes_to_insert.end(), [](const bNode *a, const bNode *b) {
      return a->runtime->toposort_right_to_left_index < b->runtime->toposort_right_to_left_index;
    });

    for (const bNode *bnode : nodes_to_insert) {
      this->build_output_socket_usages(*bnode, graph_params);
      if (const bNodeTreeZone *zone = zone_by_output.lookup_default(bnode, nullptr)) {
        this->insert_child_zone_node(*zone, graph_params);
      }
      else {
        this->insert_node_in_graph(*bnode, graph_params);
      }
    }
  }

  void link_border_link_inputs_and_usages(const bNodeTreeZone &zone,
                                          lf::Node &lf_border_link_input_node,
                                          lf::Node &lf_border_link_usage_node,
                                          BuildGraphParams &graph_params)
  {
    lf::Graph &lf_graph = graph_params.lf_graph;
    for (const int border_link_i : zone.border_links.index_range()) {
      const bNodeLink &border_link = *zone.border_links[border_link_i];
      lf::OutputSocket &lf_from = lf_border_link_input_node.output(border_link_i);
      const Vector<lf::InputSocket *> lf_link_targets = this->find_link_targets(border_link,
                                                                                graph_params);
      for (lf::InputSocket *lf_to : lf_link_targets) {
        lf_graph.add_link(lf_from, *lf_to);
      }
      lf::InputSocket &lf_usage_output = lf_border_link_usage_node.input(border_link_i);
      if (lf::OutputSocket *lf_usage = graph_params.usage_by_bsocket.lookup_default(
              border_link.tosock, nullptr))
      {
        lf_graph.add_link(*lf_usage, lf_usage_output);
      }
      else {
        static const bool static_false = false;
        lf_usage_output.set_default_value(&static_false);
      }
    }
  }

  lf::OutputSocket &get_extracted_attributes(lf::OutputSocket &lf_field_socket,
                                             lf::OutputSocket *lf_usage_socket,
                                             lf::Graph &lf_graph,
                                             Set<lf::InputSocket *> &socket_usage_inputs)
  {
    const ValueOrFieldCPPType &type = *ValueOrFieldCPPType::get_from_self(lf_field_socket.type());
    auto &lazy_function = scope_.construct<LazyFunctionForAnonymousAttributeSetExtract>(type);
    lf::Node &lf_node = lf_graph.add_function(lazy_function);
    lf::InputSocket &lf_use_input = lf_node.input(0);
    lf::InputSocket &lf_field_input = lf_node.input(1);
    socket_usage_inputs.add_new(&lf_use_input);
    if (lf_usage_socket) {
      lf_graph.add_link(*lf_usage_socket, lf_use_input);
    }
    else {
      static const bool static_false = false;
      lf_use_input.set_default_value(&static_false);
    }
    lf_graph.add_link(lf_field_socket, lf_field_input);
    return lf_node.output(0);
  }

  /**
   * Join multiple attributes set into a single attribute set that can be passed into a node.
   */
  lf::OutputSocket *join_attribute_sets(const Span<lf::OutputSocket *> lf_attribute_set_sockets,
                                        JoinAttributeSetsCache &cache,
                                        lf::Graph &lf_graph,
                                        Set<lf::InputSocket *> &socket_usage_inputs)
  {
    if (lf_attribute_set_sockets.is_empty()) {
      return nullptr;
    }
    if (lf_attribute_set_sockets.size() == 1) {
      return lf_attribute_set_sockets[0];
    }

    Vector<lf::OutputSocket *, 16> key = lf_attribute_set_sockets;
    std::sort(key.begin(), key.end());
    return cache.lookup_or_add_cb(key, [&]() {
      const auto &lazy_function = LazyFunctionForAnonymousAttributeSetJoin::get_cached(
          lf_attribute_set_sockets.size(), scope_);
      lf::Node &lf_node = lf_graph.add_function(lazy_function);
      for (const int i : lf_attribute_set_sockets.index_range()) {
        lf::OutputSocket &lf_attribute_set_socket = *lf_attribute_set_sockets[i];
        lf::InputSocket &lf_use_input = lf_node.input(lazy_function.get_use_input(i));

        /* Some attribute sets could potentially be set unused in the future based on more dynamic
         * analysis of the node tree. */
        static const bool static_true = true;
        lf_use_input.set_default_value(&static_true);

        socket_usage_inputs.add(&lf_use_input);
        lf::InputSocket &lf_attribute_set_input = lf_node.input(
            lazy_function.get_attribute_set_input(i));
        lf_graph.add_link(lf_attribute_set_socket, lf_attribute_set_input);
      }
      return &lf_node.output(0);
    });
  }

  void insert_child_zone_node(const bNodeTreeZone &child_zone, BuildGraphParams &graph_params)
  {
    const int child_zone_i = child_zone.index;
    ZoneBuildInfo &child_zone_info = zone_build_infos_[child_zone_i];
    lf::FunctionNode &child_zone_node = graph_params.lf_graph.add_function(
        *child_zone_info.lazy_function);
    mapping_->zone_node_map.add_new(&child_zone, &child_zone_node);

    for (const int i : child_zone_info.main_input_indices.index_range()) {
      const bNodeSocket &bsocket = child_zone.input_node->input_socket(i);
      lf::InputSocket &lf_input_socket = child_zone_node.input(
          child_zone_info.main_input_indices[i]);
      lf::OutputSocket &lf_usage_socket = child_zone_node.output(
          child_zone_info.main_input_usage_indices[i]);
      mapping_->bsockets_by_lf_socket_map.add(&lf_input_socket, &bsocket);
      graph_params.lf_inputs_by_bsocket.add(&bsocket, &lf_input_socket);
      graph_params.usage_by_bsocket.add(&bsocket, &lf_usage_socket);
    }
    for (const int i : child_zone_info.main_output_indices.index_range()) {
      const bNodeSocket &bsocket = child_zone.output_node->output_socket(i);
      lf::OutputSocket &lf_output_socket = child_zone_node.output(
          child_zone_info.main_output_indices[i]);
      lf::InputSocket &lf_usage_input = child_zone_node.input(
          child_zone_info.main_output_usage_indices[i]);
      mapping_->bsockets_by_lf_socket_map.add(&lf_output_socket, &bsocket);
      graph_params.lf_output_by_bsocket.add(&bsocket, &lf_output_socket);
      graph_params.socket_usage_inputs.add(&lf_usage_input);
      if (lf::OutputSocket *lf_usage = graph_params.usage_by_bsocket.lookup_default(&bsocket,
                                                                                    nullptr)) {
        graph_params.lf_graph.add_link(*lf_usage, lf_usage_input);
      }
      else {
        static const bool static_false = false;
        lf_usage_input.set_default_value(&static_false);
      }
    }

    const Span<const bNodeLink *> child_border_links = child_zone.border_links;
    for (const int child_border_link_i : child_border_links.index_range()) {
      lf::InputSocket &child_border_link_input = child_zone_node.input(
          child_zone_info.border_link_input_indices[child_border_link_i]);
      const bNodeLink &link = *child_border_links[child_border_link_i];
      graph_params.lf_input_by_border_link.add(&link, &child_border_link_input);
      lf::OutputSocket &lf_usage = child_zone_node.output(
          child_zone_info.border_link_input_usage_indices[child_border_link_i]);
      graph_params.lf_inputs_by_bsocket.add(link.tosock, &child_border_link_input);
      graph_params.usage_by_bsocket.add(link.tosock, &lf_usage);
    }

    for (const auto item : child_zone_info.attribute_set_input_by_field_source_index.items()) {
      const int field_source_index = item.key;
      const int child_zone_input_index = item.value;
      lf::InputSocket &lf_attribute_set_input = child_zone_node.input(child_zone_input_index);
      graph_params.lf_attribute_set_input_by_field_source_index.add(field_source_index,
                                                                    &lf_attribute_set_input);
    }
    for (const auto item : child_zone_info.attribute_set_input_by_caller_propagation_index.items())
    {
      const int caller_propagation_index = item.key;
      const int child_zone_input_index = item.value;
      lf::InputSocket &lf_attribute_set_input = child_zone_node.input(child_zone_input_index);
      graph_params.lf_attribute_set_input_by_caller_propagation_index.add(caller_propagation_index,
                                                                          &lf_attribute_set_input);
    }
  }

  void build_group_input_node(lf::Graph &lf_graph)
  {
    Vector<const CPPType *, 16> input_cpp_types;
    const Span<const bNodeSocket *> interface_inputs = btree_.interface_inputs();
    for (const bNodeSocket *interface_input : interface_inputs) {
      input_cpp_types.append(interface_input->typeinfo->geometry_nodes_cpp_type);
    }

    /* Create a dummy node for the group inputs. */
    auto &debug_info = scope_.construct<GroupInputDebugInfo>();
    group_input_lf_node_ = &lf_graph.add_dummy({}, input_cpp_types, &debug_info);

    for (const int i : interface_inputs.index_range()) {
      mapping_->group_input_sockets.append(&group_input_lf_node_->output(i));
      debug_info.socket_names.append(interface_inputs[i]->name);
    }
  }

  /**
   * Build an output node that just outputs default values in the case when there is no Group
   * Output node in the tree.
   */
  void build_fallback_output_node(lf::Graph &lf_graph)
  {
    Vector<const CPPType *, 16> output_cpp_types;
    auto &debug_info = scope_.construct<GroupOutputDebugInfo>();
    for (const bNodeSocket *interface_output : btree_.interface_outputs()) {
      output_cpp_types.append(interface_output->typeinfo->geometry_nodes_cpp_type);
      debug_info.socket_names.append(interface_output->name);
    }

    lf::Node &lf_node = lf_graph.add_dummy(output_cpp_types, {}, &debug_info);
    for (lf::InputSocket *lf_socket : lf_node.inputs()) {
      const CPPType &type = lf_socket->type();
      lf_socket->set_default_value(type.default_value());
    }
    mapping_->standard_group_output_sockets = lf_node.inputs();
  }

  void insert_node_in_graph(const bNode &bnode, BuildGraphParams &graph_params)
  {
    const bNodeType *node_type = bnode.typeinfo;
    if (node_type == nullptr) {
      return;
    }
    if (bnode.is_muted()) {
      this->build_muted_node(bnode, graph_params);
      return;
    }
    switch (node_type->type) {
      case NODE_FRAME: {
        /* Ignored. */
        break;
      }
      case NODE_REROUTE: {
        this->build_reroute_node(bnode, graph_params);
        break;
      }
      case NODE_GROUP_INPUT: {
        this->handle_group_input_node(bnode, graph_params);
        break;
      }
      case NODE_GROUP_OUTPUT: {
        this->build_group_output_node(bnode, graph_params);
        break;
      }
      case NODE_CUSTOM_GROUP:
      case NODE_GROUP: {
        this->build_group_node(bnode, graph_params);
        break;
      }
      case GEO_NODE_VIEWER: {
        this->build_viewer_node(bnode, graph_params);
        break;
      }
      case GEO_NODE_SWITCH: {
        this->build_switch_node(bnode, graph_params);
        break;
      }
      default: {
        if (node_type->geometry_node_execute) {
          this->build_geometry_node(bnode, graph_params);
          break;
        }
        const NodeMultiFunctions::Item &fn_item = node_multi_functions_.try_get(bnode);
        if (fn_item.fn != nullptr) {
          this->build_multi_function_node(bnode, fn_item, graph_params);
          break;
        }
        if (node_type == &bke::NodeTypeUndefined) {
          this->build_undefined_node(bnode, graph_params);
          break;
        }
        /* Nodes that don't match any of the criteria above are just ignored. */
        break;
      }
    }
  }

  void build_muted_node(const bNode &bnode, BuildGraphParams &graph_params)
  {
    auto &lazy_function = scope_.construct<LazyFunctionForMutedNode>(
        bnode, mapping_->lf_index_by_bsocket);
    lf::Node &lf_node = graph_params.lf_graph.add_function(lazy_function);
    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::InputSocket &lf_socket = lf_node.input(lf_index);
      graph_params.lf_inputs_by_bsocket.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::OutputSocket &lf_socket = lf_node.output(lf_index);
      graph_params.lf_output_by_bsocket.add_new(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }

    this->build_muted_node_usages(bnode, graph_params);
  }

  /**
   * An input of a muted node is used when any of its internally linked outputs is used.
   */
  void build_muted_node_usages(const bNode &bnode, BuildGraphParams &graph_params)
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
        if (lf::OutputSocket *lf_socket = graph_params.usage_by_bsocket.lookup_default(
                output_bsocket, nullptr))
        {
          lf_socket_usages.append(lf_socket);
        }
      }
      graph_params.usage_by_bsocket.add(&input_bsocket,
                                        this->or_socket_usages(lf_socket_usages, graph_params));
    }
  }

  void build_reroute_node(const bNode &bnode, BuildGraphParams &graph_params)
  {
    const bNodeSocket &input_bsocket = bnode.input_socket(0);
    const bNodeSocket &output_bsocket = bnode.output_socket(0);
    const CPPType *type = get_socket_cpp_type(input_bsocket);
    if (type == nullptr) {
      return;
    }

    auto &lazy_function = scope_.construct<LazyFunctionForRerouteNode>(*type);
    lf::Node &lf_node = graph_params.lf_graph.add_function(lazy_function);

    lf::InputSocket &lf_input = lf_node.input(0);
    lf::OutputSocket &lf_output = lf_node.output(0);
    graph_params.lf_inputs_by_bsocket.add(&input_bsocket, &lf_input);
    graph_params.lf_output_by_bsocket.add_new(&output_bsocket, &lf_output);
    mapping_->bsockets_by_lf_socket_map.add(&lf_input, &input_bsocket);
    mapping_->bsockets_by_lf_socket_map.add(&lf_output, &output_bsocket);

    if (lf::OutputSocket *lf_usage = graph_params.usage_by_bsocket.lookup_default(
            &bnode.output_socket(0), nullptr))
    {
      graph_params.usage_by_bsocket.add(&bnode.input_socket(0), lf_usage);
    }
  }

  void handle_group_input_node(const bNode &bnode, BuildGraphParams &graph_params)
  {
    for (const int i : btree_.interface_inputs().index_range()) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = group_input_lf_node_->output(i);
      graph_params.lf_output_by_bsocket.add_new(&bsocket, &lf_socket);
      mapping_->dummy_socket_map.add_new(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
  }

  void build_group_output_node(const bNode &bnode, BuildGraphParams &graph_params)
  {
    Vector<const CPPType *, 16> output_cpp_types;
    auto &debug_info = scope_.construct<GroupOutputDebugInfo>();
    for (const bNodeSocket *interface_input : btree_.interface_outputs()) {
      output_cpp_types.append(interface_input->typeinfo->geometry_nodes_cpp_type);
      debug_info.socket_names.append(interface_input->name);
    }

    lf::DummyNode &group_output_lf_node = graph_params.lf_graph.add_dummy(
        output_cpp_types, {}, &debug_info);

    for (const int i : group_output_lf_node.inputs().index_range()) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      lf::InputSocket &lf_socket = group_output_lf_node.input(i);
      graph_params.lf_inputs_by_bsocket.add(&bsocket, &lf_socket);
      mapping_->dummy_socket_map.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }

    if (&bnode == btree_.group_output_node()) {
      mapping_->standard_group_output_sockets = group_output_lf_node.inputs();
    }
  }

  void build_group_node(const bNode &bnode, BuildGraphParams &graph_params)
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

    auto &lazy_function = scope_.construct<LazyFunctionForGroupNode>(
        bnode, *group_lf_graph_info, *lf_graph_info_);
    lf::FunctionNode &lf_node = graph_params.lf_graph.add_function(lazy_function);

    for (const int i : bnode.input_sockets().index_range()) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      BLI_assert(!bsocket.is_multi_input());
      lf::InputSocket &lf_socket = lf_node.input(i);
      graph_params.lf_inputs_by_bsocket.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : bnode.output_sockets().index_range()) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = lf_node.output(i);
      graph_params.lf_output_by_bsocket.add_new(&bsocket, &lf_socket);
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
          graph_params.socket_usage_inputs.add(&lf_input);
        }
      }
      {
        /* Keep track of attribute set inputs that need to be populated later. */
        const int lf_input_index = mapping_->lf_input_index_for_attribute_propagation_to_output
                                       [bsocket->index_in_all_outputs()];
        if (lf_input_index != -1) {
          lf::InputSocket &lf_input = lf_node.input(lf_input_index);
          graph_params.lf_attribute_set_input_by_output_geometry_bsocket.add(bsocket, &lf_input);
        }
      }
    }

    this->build_group_node_socket_usage(bnode, lf_node, graph_params);
  }

  void build_group_node_socket_usage(const bNode &bnode,
                                     lf::FunctionNode &lf_group_node,
                                     BuildGraphParams &graph_params)
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
            if (lf::OutputSocket *lf_socket = graph_params.usage_by_bsocket.lookup_default(
                    &bnode.output_socket(i), nullptr))
            {
              output_usages.append(lf_socket);
            }
          }
          graph_params.usage_by_bsocket.add(input_bsocket,
                                            this->or_socket_usages(output_usages, graph_params));
          break;
        }
        case InputUsageHintType::DynamicSocket: {
          graph_params.usage_by_bsocket.add(
              input_bsocket,
              &lf_group_node.output(fn.lf_output_for_input_bsocket_usage_.lookup(input_index)));
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
      if (lf::OutputSocket *lf_output_is_used = graph_params.usage_by_bsocket.lookup_default(
              output_bsocket, nullptr))
      {
        graph_params.lf_graph.add_link(*lf_output_is_used, lf_socket);
      }
      else {
        static const bool static_false = false;
        lf_socket.set_default_value(&static_false);
      }
    }
  }

  void build_geometry_node(const bNode &bnode, BuildGraphParams &graph_params)
  {
    auto &lazy_function = scope_.construct<LazyFunctionForGeometryNode>(bnode, *lf_graph_info_);
    lf::Node &lf_node = graph_params.lf_graph.add_function(lazy_function);

    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::InputSocket &lf_socket = lf_node.input(lf_index);

      if (bsocket->is_multi_input()) {
        auto &multi_input_lazy_function = scope_.construct<LazyFunctionForMultiInput>(*bsocket);
        lf::Node &lf_multi_input_node = graph_params.lf_graph.add_function(
            multi_input_lazy_function);
        graph_params.lf_graph.add_link(lf_multi_input_node.output(0), lf_socket);
        graph_params.multi_input_socket_nodes.add_new(bsocket, &lf_multi_input_node);
        for (lf::InputSocket *lf_multi_input_socket : lf_multi_input_node.inputs()) {
          mapping_->bsockets_by_lf_socket_map.add(lf_multi_input_socket, bsocket);
          const void *default_value = lf_multi_input_socket->type().default_value();
          lf_multi_input_socket->set_default_value(default_value);
        }
      }
      else {
        graph_params.lf_inputs_by_bsocket.add(bsocket, &lf_socket);
        mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
      }
    }
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::OutputSocket &lf_socket = lf_node.output(lf_index);
      graph_params.lf_output_by_bsocket.add_new(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }

    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      {
        const int lf_input_index =
            mapping_->lf_input_index_for_output_bsocket_usage[bsocket->index_in_all_outputs()];
        if (lf_input_index != -1) {
          lf::InputSocket &lf_input_socket = lf_node.input(lf_input_index);
          if (lf::OutputSocket *lf_usage = graph_params.usage_by_bsocket.lookup_default(bsocket,
                                                                                        nullptr)) {
            graph_params.lf_graph.add_link(*lf_usage, lf_input_socket);
          }
          else {
            static const bool static_false = false;
            lf_input_socket.set_default_value(&static_false);
          }
          graph_params.socket_usage_inputs.add_new(&lf_node.input(lf_input_index));
        }
      }
      {
        /* Keep track of attribute set inputs that need to be populated later. */
        const int lf_input_index = mapping_->lf_input_index_for_attribute_propagation_to_output
                                       [bsocket->index_in_all_outputs()];
        if (lf_input_index != -1) {
          graph_params.lf_attribute_set_input_by_output_geometry_bsocket.add(
              bsocket, &lf_node.input(lf_input_index));
        }
      }
    }

    this->build_standard_node_input_socket_usage(bnode, graph_params);
  }

  void build_standard_node_input_socket_usage(const bNode &bnode, BuildGraphParams &graph_params)
  {
    if (bnode.input_sockets().is_empty()) {
      return;
    }

    Vector<lf::OutputSocket *> output_usages;
    for (const bNodeSocket *output_socket : bnode.output_sockets()) {
      if (!output_socket->is_available()) {
        continue;
      }
      if (lf::OutputSocket *is_used_socket = graph_params.usage_by_bsocket.lookup_default(
              output_socket, nullptr))
      {
        output_usages.append_non_duplicates(is_used_socket);
      }
    }

    /* Assume every input is used when any output is used. */
    lf::OutputSocket *lf_usage = this->or_socket_usages(output_usages, graph_params);
    if (lf_usage == nullptr) {
      return;
    }

    for (const bNodeSocket *input_socket : bnode.input_sockets()) {
      if (input_socket->is_available()) {
        graph_params.usage_by_bsocket.add(input_socket, lf_usage);
      }
    }
  }

  void build_multi_function_node(const bNode &bnode,
                                 const NodeMultiFunctions::Item &fn_item,
                                 BuildGraphParams &graph_params)
  {
    auto &lazy_function = scope_.construct<LazyFunctionForMultiFunctionNode>(
        bnode, fn_item, mapping_->lf_index_by_bsocket);
    lf::Node &lf_node = graph_params.lf_graph.add_function(lazy_function);

    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      BLI_assert(!bsocket->is_multi_input());
      lf::InputSocket &lf_socket = lf_node.input(lf_index);
      graph_params.lf_inputs_by_bsocket.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::OutputSocket &lf_socket = lf_node.output(lf_index);
      graph_params.lf_output_by_bsocket.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }

    this->build_standard_node_input_socket_usage(bnode, graph_params);
  }

  void build_viewer_node(const bNode &bnode, BuildGraphParams &graph_params)
  {
    auto &lazy_function = scope_.construct<LazyFunctionForViewerNode>(
        bnode, mapping_->lf_index_by_bsocket);
    lf::FunctionNode &lf_viewer_node = graph_params.lf_graph.add_function(lazy_function);

    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::InputSocket &lf_socket = lf_viewer_node.input(lf_index);
      graph_params.lf_inputs_by_bsocket.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }

    mapping_->viewer_node_map.add(&bnode, &lf_viewer_node);

    {
      auto &usage_lazy_function = scope_.construct<LazyFunctionForViewerInputUsage>(
          lf_viewer_node);
      lf::FunctionNode &lf_usage_node = graph_params.lf_graph.add_function(usage_lazy_function);

      for (const bNodeSocket *bsocket : bnode.input_sockets()) {
        if (bsocket->is_available()) {
          graph_params.usage_by_bsocket.add(bsocket, &lf_usage_node.output(0));
        }
      }
    }
  }

  lf::FunctionNode *insert_simulation_input_node(const bNodeTree &node_tree,
                                                 const bNode &bnode,
                                                 BuildGraphParams &graph_params)
  {
    const NodeGeometrySimulationInput *storage = static_cast<const NodeGeometrySimulationInput *>(
        bnode.storage);
    if (node_tree.node_by_id(storage->output_node_id) == nullptr) {
      return nullptr;
    }

    std::unique_ptr<LazyFunction> lazy_function = get_simulation_input_lazy_function(
        node_tree, bnode, *lf_graph_info_);
    lf::FunctionNode &lf_node = graph_params.lf_graph.add_function(*lazy_function);
    scope_.add(std::move(lazy_function));

    for (const int i : bnode.input_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      lf::InputSocket &lf_socket = lf_node.input(
          mapping_->lf_index_by_bsocket[bsocket.index_in_tree()]);
      graph_params.lf_inputs_by_bsocket.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : bnode.output_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = lf_node.output(
          mapping_->lf_index_by_bsocket[bsocket.index_in_tree()]);
      graph_params.lf_output_by_bsocket.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    return &lf_node;
  }

  lf::FunctionNode &insert_simulation_output_node(const bNode &bnode,
                                                  BuildGraphParams &graph_params)
  {
    std::unique_ptr<LazyFunction> lazy_function = get_simulation_output_lazy_function(
        bnode, *lf_graph_info_);
    lf::FunctionNode &lf_node = graph_params.lf_graph.add_function(*lazy_function);
    scope_.add(std::move(lazy_function));

    for (const int i : bnode.input_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.input_socket(i);
      lf::InputSocket &lf_socket = lf_node.input(
          mapping_->lf_index_by_bsocket[bsocket.index_in_tree()]);
      graph_params.lf_inputs_by_bsocket.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    for (const int i : bnode.output_sockets().index_range().drop_back(1)) {
      const bNodeSocket &bsocket = bnode.output_socket(i);
      lf::OutputSocket &lf_socket = lf_node.output(
          mapping_->lf_index_by_bsocket[bsocket.index_in_tree()]);
      graph_params.lf_output_by_bsocket.add(&bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, &bsocket);
    }
    return lf_node;
  }

  void build_switch_node(const bNode &bnode, BuildGraphParams &graph_params)
  {
    std::unique_ptr<LazyFunction> lazy_function = get_switch_node_lazy_function(bnode);
    lf::FunctionNode &lf_node = graph_params.lf_graph.add_function(*lazy_function);
    scope_.add(std::move(lazy_function));

    int input_index = 0;
    for (const bNodeSocket *bsocket : bnode.input_sockets()) {
      if (bsocket->is_available()) {
        lf::InputSocket &lf_socket = lf_node.input(input_index);
        graph_params.lf_inputs_by_bsocket.add(bsocket, &lf_socket);
        mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
        input_index++;
      }
    }
    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      if (bsocket->is_available()) {
        lf::OutputSocket &lf_socket = lf_node.output(0);
        graph_params.lf_output_by_bsocket.add(bsocket, &lf_socket);
        mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
        break;
      }
    }

    this->build_switch_node_socket_usage(bnode, graph_params);
  }

  void build_switch_node_socket_usage(const bNode &bnode, BuildGraphParams &graph_params)
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
    lf::OutputSocket *output_is_used_socket = graph_params.usage_by_bsocket.lookup_default(
        output_bsocket, nullptr);
    if (output_is_used_socket == nullptr) {
      return;
    }
    graph_params.usage_by_bsocket.add(switch_input_bsocket, output_is_used_socket);
    if (switch_input_bsocket->is_directly_linked()) {
      /* The condition input is dynamic, so the usage of the other inputs is as well. */
      static const LazyFunctionForSwitchSocketUsage switch_socket_usage_fn;
      lf::Node &lf_node = graph_params.lf_graph.add_function(switch_socket_usage_fn);
      graph_params.lf_inputs_by_bsocket.add(switch_input_bsocket, &lf_node.input(0));
      graph_params.usage_by_bsocket.add(false_input_bsocket, &lf_node.output(0));
      graph_params.usage_by_bsocket.add(true_input_bsocket, &lf_node.output(1));
    }
    else {
      if (switch_input_bsocket->default_value_typed<bNodeSocketValueBoolean>()->value) {
        graph_params.usage_by_bsocket.add(true_input_bsocket, output_is_used_socket);
      }
      else {
        graph_params.usage_by_bsocket.add(false_input_bsocket, output_is_used_socket);
      }
    }
  }

  void build_undefined_node(const bNode &bnode, BuildGraphParams &graph_params)
  {
    auto &lazy_function = scope_.construct<LazyFunctionForUndefinedNode>(
        bnode, mapping_->lf_index_by_bsocket);
    lf::FunctionNode &lf_node = graph_params.lf_graph.add_function(lazy_function);

    for (const bNodeSocket *bsocket : bnode.output_sockets()) {
      const int lf_index = mapping_->lf_index_by_bsocket[bsocket->index_in_tree()];
      if (lf_index == -1) {
        continue;
      }
      lf::OutputSocket &lf_socket = lf_node.output(lf_index);
      graph_params.lf_output_by_bsocket.add(bsocket, &lf_socket);
      mapping_->bsockets_by_lf_socket_map.add(&lf_socket, bsocket);
    }
  }

  lf::DummyNode &build_dummy_node_for_sockets(const StringRef name,
                                              const Span<const bNodeSocket *> input_bsockets,
                                              const Span<const bNodeSocket *> output_bsockets,
                                              lf::Graph &lf_graph)
  {
    auto &debug_info = scope_.construct<lf::SimpleDummyDebugInfo>();
    debug_info.name = name;
    Vector<const CPPType *, 16> input_types;
    Vector<const CPPType *, 16> output_types;
    for (const bNodeSocket *bsocket : input_bsockets) {
      input_types.append(bsocket->typeinfo->geometry_nodes_cpp_type);
      debug_info.input_names.append(bsocket->identifier);
    }
    for (const bNodeSocket *bsocket : output_bsockets) {
      output_types.append(bsocket->typeinfo->geometry_nodes_cpp_type);
      debug_info.output_names.append(bsocket->identifier);
    }
    lf::DummyNode &node = lf_graph.add_dummy(input_types, output_types, &debug_info);
    return node;
  }

  lf::DummyNode &build_dummy_node_for_socket_usages(
      const StringRef name,
      const Span<const bNodeSocket *> input_bsockets,
      const Span<const bNodeSocket *> output_bsockets,
      lf::Graph &lf_graph)
  {
    auto &debug_info = scope_.construct<lf::SimpleDummyDebugInfo>();
    debug_info.name = name;
    const CPPType &bool_cpp_type = CPPType::get<bool>();
    Vector<const CPPType *, 16> input_types(input_bsockets.size(), &bool_cpp_type);
    Vector<const CPPType *, 16> output_types(output_bsockets.size(), &bool_cpp_type);
    for (const bNodeSocket *bsocket : input_bsockets) {
      debug_info.input_names.append(bsocket->identifier);
    }
    for (const bNodeSocket *bsocket : output_bsockets) {
      debug_info.output_names.append(bsocket->identifier);
    }
    lf::DummyNode &node = lf_graph.add_dummy(input_types, output_types, &debug_info);
    return node;
  }

  struct TypeWithLinks {
    const CPPType *type;
    Vector<const bNodeLink *> links;
  };

  void insert_links_from_socket(const bNodeSocket &from_bsocket,
                                lf::OutputSocket &from_lf_socket,
                                BuildGraphParams &graph_params)
  {
    if (bke::nodeIsDanglingReroute(&btree_, &from_bsocket.owner_node())) {
      return;
    }

    /* Group available target sockets by type so that they can be handled together. */
    const Vector<TypeWithLinks> types_with_links = this->group_link_targets_by_type(from_bsocket);

    for (const TypeWithLinks &type_with_links : types_with_links) {
      const CPPType &to_type = *type_with_links.type;
      const Span<const bNodeLink *> links = type_with_links.links;

      lf::OutputSocket *converted_from_lf_socket = this->insert_type_conversion_if_necessary(
          from_lf_socket, to_type, graph_params.lf_graph);

      for (const bNodeLink *link : links) {
        const Vector<lf::InputSocket *> lf_link_targets = this->find_link_targets(*link,
                                                                                  graph_params);
        if (converted_from_lf_socket == nullptr) {
          const void *default_value = to_type.default_value();
          for (lf::InputSocket *to_lf_socket : lf_link_targets) {
            to_lf_socket->set_default_value(default_value);
          }
        }
        else {
          for (lf::InputSocket *to_lf_socket : lf_link_targets) {
            graph_params.lf_graph.add_link(*converted_from_lf_socket, *to_lf_socket);
          }
        }
      }
    }
  }

  Vector<TypeWithLinks> group_link_targets_by_type(const bNodeSocket &from_bsocket)
  {
    const Span<const bNodeLink *> links_from_bsocket = from_bsocket.directly_linked_links();
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
    return types_with_links;
  }

  Vector<lf::InputSocket *> find_link_targets(const bNodeLink &link,
                                              const BuildGraphParams &graph_params)
  {
    if (lf::InputSocket *lf_input_socket = graph_params.lf_input_by_border_link.lookup_default(
            &link, nullptr))
    {
      return {lf_input_socket};
    }

    const bNodeSocket &to_bsocket = *link.tosock;
    if (to_bsocket.is_multi_input()) {
      /* TODO: Cache this index on the link. */
      int link_index = 0;
      for (const bNodeLink *multi_input_link : to_bsocket.directly_linked_links()) {
        if (multi_input_link == &link) {
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
          return Vector<lf::InputSocket *>(graph_params.lf_inputs_by_bsocket.lookup(&to_bsocket));
        }
      }
      else {
        lf::Node *multi_input_lf_node = graph_params.multi_input_socket_nodes.lookup_default(
            &to_bsocket, nullptr);
        if (multi_input_lf_node == nullptr) {
          return {};
        }
        return {&multi_input_lf_node->input(link_index)};
      }
    }
    else {
      return Vector<lf::InputSocket *>(graph_params.lf_inputs_by_bsocket.lookup(&to_bsocket));
    }
    return {};
  }

  lf::OutputSocket *insert_type_conversion_if_necessary(lf::OutputSocket &from_socket,
                                                        const CPPType &to_type,
                                                        lf::Graph &lf_graph)
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
        auto &fn = scope_.construct<LazyFunctionForMultiFunctionConversion>(
            multi_fn, *from_field_type, *to_field_type);
        lf::Node &conversion_node = lf_graph.add_function(fn);
        lf_graph.add_link(from_socket, conversion_node.input(0));
        return &conversion_node.output(0);
      }
    }
    return nullptr;
  }

  void add_default_inputs(BuildGraphParams &graph_params)
  {
    for (auto item : graph_params.lf_inputs_by_bsocket.items()) {
      const bNodeSocket &bsocket = *item.key;
      const Span<lf::InputSocket *> lf_sockets = item.value;
      for (lf::InputSocket *lf_socket : lf_sockets) {
        if (lf_socket->origin() != nullptr) {
          /* Is linked already. */
          continue;
        }
        this->add_default_input(bsocket, *lf_socket, graph_params);
      }
    }
  }

  void add_default_input(const bNodeSocket &input_bsocket,
                         lf::InputSocket &input_lf_socket,
                         BuildGraphParams &graph_params)
  {
    if (this->try_add_implicit_input(input_bsocket, input_lf_socket, graph_params)) {
      return;
    }
    GMutablePointer value = get_socket_default_value(scope_.linear_allocator(), input_bsocket);
    if (value.get() == nullptr) {
      /* Not possible to add a default value. */
      return;
    }
    input_lf_socket.set_default_value(value.get());
    if (!value.type()->is_trivially_destructible()) {
      scope_.add_destruct_call([value]() mutable { value.destruct(); });
    }
  }

  bool try_add_implicit_input(const bNodeSocket &input_bsocket,
                              lf::InputSocket &input_lf_socket,
                              BuildGraphParams &graph_params)
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
    auto &lazy_function = scope_.construct<LazyFunctionForImplicitInput>(type, std::move(init_fn));
    lf::Node &lf_node = graph_params.lf_graph.add_function(lazy_function);
    graph_params.lf_graph.add_link(lf_node.output(0), input_lf_socket);
    return true;
  }

  /**
   * Every output geometry socket that may propagate attributes has to know which attributes
   * should be propagated. Therefore, every one of these outputs gets a corresponding attribute
   * set input.
   */
  void build_attribute_propagation_input_node(lf::Graph &lf_graph)
  {
    const aal::RelationsInNode &tree_relations =
        btree_.runtime->anonymous_attribute_inferencing->tree_relations;
    Vector<int> output_indices;
    for (const aal::PropagateRelation &relation : tree_relations.propagate_relations) {
      output_indices.append_non_duplicates(relation.to_geometry_output);
    }
    Vector<const CPPType *> cpp_types;
    auto &debug_info = scope_.construct<lf::SimpleDummyDebugInfo>();
    debug_info.name = "Attributes to Propagate to Output";
    cpp_types.append_n_times(&CPPType::get<bke::AnonymousAttributeSet>(), output_indices.size());
    lf::Node &lf_node = lf_graph.add_dummy({}, cpp_types, &debug_info);
    for (const int i : output_indices.index_range()) {
      const int output_index = output_indices[i];
      mapping_->attribute_set_by_geometry_output.add(output_index, &lf_node.output(i));
      debug_info.output_names.append(btree_.interface_outputs()[output_index]->name);
    }
  }

  /**
   * Build new boolean group inputs that indicate which group outputs are used.
   */
  lf::DummyNode &build_output_usage_input_node(lf::Graph &lf_graph)
  {
    const Span<const bNodeSocket *> interface_outputs = btree_.interface_outputs();

    Vector<const CPPType *> cpp_types;
    cpp_types.append_n_times(&CPPType::get<bool>(), interface_outputs.size());
    auto &debug_info = scope_.construct<lf::SimpleDummyDebugInfo>();
    debug_info.name = "Output Socket Usage";
    lf::DummyNode &lf_node = lf_graph.add_dummy({}, cpp_types, &debug_info);
    for (const int i : interface_outputs.index_range()) {
      mapping_->group_output_used_sockets.append(&lf_node.output(i));
      debug_info.output_names.append(interface_outputs[i]->name);
    }
    return lf_node;
  }

  /**
   * Build new boolean group outputs that indicate which group inputs are used depending on other
   * group inputs.
   */
  void build_input_usage_output_node(lf::Graph &lf_graph)
  {
    const Span<const bNodeSocket *> interface_inputs = btree_.interface_inputs();

    Vector<const CPPType *> cpp_types;
    cpp_types.append_n_times(&CPPType::get<bool>(), interface_inputs.size());
    auto &debug_info = scope_.construct<lf::SimpleDummyDebugInfo>();
    debug_info.name = "Input Socket Usage";
    lf::Node &lf_node = lf_graph.add_dummy(cpp_types, {}, &debug_info);
    for (const int i : interface_inputs.index_range()) {
      mapping_->group_input_usage_sockets.append(&lf_node.input(i));
      debug_info.input_names.append(interface_inputs[i]->name);
    }
  }

  /**
   * Combine multiple socket usages with a logical or. Inserts a new node for that purpose if
   * necessary.
   */
  lf::OutputSocket *or_socket_usages(MutableSpan<lf::OutputSocket *> usages,
                                     BuildGraphParams &graph_params)
  {
    if (usages.is_empty()) {
      return nullptr;
    }
    if (usages.size() == 1) {
      return usages[0];
    }

    std::sort(usages.begin(), usages.end());
    return graph_params.socket_usages_combination_cache.lookup_or_add_cb_as(usages, [&]() {
      auto &logical_or_fn = scope_.construct<LazyFunctionForLogicalOr>(usages.size());
      lf::Node &logical_or_node = graph_params.lf_graph.add_function(logical_or_fn);

      for (const int i : usages.index_range()) {
        graph_params.lf_graph.add_link(*usages[i], logical_or_node.input(i));
      }
      return &logical_or_node.output(0);
    });
  }

  void build_output_socket_usages(const bNode &bnode, BuildGraphParams &graph_params)
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
        if (lf::OutputSocket *is_used_socket = graph_params.usage_by_bsocket.lookup_default(
                &target_socket, nullptr))
        {
          target_usages.append_non_duplicates(is_used_socket);
        }
      }
      /* Combine target socket usages into the usage of the current socket. */
      graph_params.usage_by_bsocket.add(socket,
                                        this->or_socket_usages(target_usages, graph_params));
    }
  }

  void build_group_input_usages(BuildGraphParams &graph_params)
  {
    const Span<const bNode *> group_input_nodes = btree_.group_input_nodes();
    for (const int i : btree_.interface_inputs().index_range()) {
      Vector<lf::OutputSocket *> target_usages;
      for (const bNode *group_input_node : group_input_nodes) {
        if (lf::OutputSocket *lf_socket = graph_params.usage_by_bsocket.lookup_default(
                &group_input_node->output_socket(i), nullptr))
        {
          target_usages.append_non_duplicates(lf_socket);
        }
      }

      lf::OutputSocket *lf_socket = this->or_socket_usages(target_usages, graph_params);
      lf::InputSocket *lf_group_output = const_cast<lf::InputSocket *>(
          mapping_->group_input_usage_sockets[i]);
      InputUsageHint input_usage_hint;
      if (lf_socket == nullptr) {
        static const bool static_false = false;
        lf_group_output->set_default_value(&static_false);
        input_usage_hint.type = InputUsageHintType::Never;
      }
      else {
        graph_params.lf_graph.add_link(*lf_socket, *lf_group_output);
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

  /**
   * By depending on "the future" (whether a specific socket is used in the future), it is
   * possible to introduce cycles in the graph. This function finds those cycles and breaks them
   * by removing specific links.
   *
   * Example for a cycle: There is a `Distribute Points on Faces` node and its `Normal` output is
   * only used when the number of generated points is larger than 1000 because of some switch
   * node later in the tree. In this case, to know whether the `Normal` output is needed, one
   * first has to compute the points, but for that one has to know whether the normal information
   * has to be added to the points. The fix is to always add the normal information in this case.
   */
  void fix_link_cycles(lf::Graph &lf_graph, const Set<lf::InputSocket *> &socket_usage_inputs)
  {
    lf_graph.update_socket_indices();
    const int sockets_num = lf_graph.socket_num();

    struct SocketState {
      bool done = false;
      bool in_stack = false;
    };

    Array<SocketState> socket_states(sockets_num);

    Vector<lf::Socket *> lf_sockets_to_check;
    for (lf::Node *lf_node : lf_graph.nodes()) {
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
           * the first socket of the cycle was found. This is necessary in case another cycle
           * goes through this socket. */

          detected_cycle = true;
          const int index_in_socket_stack = lf_socket_stack.first_index_of(lf_origin_socket);
          const int index_in_sockets_to_check = lf_sockets_to_check.first_index_of(
              lf_origin_socket);
          const Span<lf::Socket *> cycle = lf_socket_stack.as_span().drop_front(
              index_in_socket_stack);

          bool broke_cycle = false;
          for (lf::Socket *lf_cycle_socket : cycle) {
            if (lf_cycle_socket->is_input() &&
                socket_usage_inputs.contains(&lf_cycle_socket->as_input())) {
              lf::InputSocket &lf_cycle_input_socket = lf_cycle_socket->as_input();
              lf_graph.clear_origin(lf_cycle_input_socket);
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
};

const GeometryNodesLazyFunctionGraphInfo *ensure_geometry_nodes_lazy_function_graph(
    const bNodeTree &btree)
{
  btree.ensure_topology_cache();
  if (btree.has_available_link_cycle()) {
    return nullptr;
  }
  const bNodeTreeZones *tree_zones = btree.zones();
  if (tree_zones == nullptr) {
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
