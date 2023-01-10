/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_field.hh"
#include "FN_lazy_function.hh"
#include "FN_multi_function_builder.hh"

#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry_nodes_lazy_function.hh"

struct ModifierData;

namespace blender::nodes {

using bke::AnonymousAttributeFieldInput;
using bke::AnonymousAttributeID;
using bke::AnonymousAttributePropagationInfo;
using bke::AttributeAccessor;
using bke::AttributeFieldInput;
using bke::AttributeIDRef;
using bke::AttributeKind;
using bke::AttributeMetaData;
using bke::AttributeReader;
using bke::AttributeWriter;
using bke::AutoAnonymousAttributeID;
using bke::GAttributeReader;
using bke::GAttributeWriter;
using bke::GSpanAttributeWriter;
using bke::MutableAttributeAccessor;
using bke::SpanAttributeWriter;
using fn::Field;
using fn::FieldContext;
using fn::FieldEvaluator;
using fn::FieldInput;
using fn::FieldOperation;
using fn::GField;
using fn::ValueOrField;
using geo_eval_log::NamedAttributeUsage;
using geo_eval_log::NodeWarningType;

/**
 * An anonymous attribute created by a node.
 */
class NodeAnonymousAttributeID : public AnonymousAttributeID {
  std::string long_name_;
  std::string socket_name_;

 public:
  NodeAnonymousAttributeID(const Object &object,
                           const ComputeContext &compute_context,
                           const bNode &bnode,
                           const StringRef identifier,
                           const StringRef name);

  std::string user_name() const override;
};

class GeoNodeExecParams {
 private:
  const bNode &node_;
  lf::Params &params_;
  const lf::Context &lf_context_;
  const Map<StringRef, int> &lf_input_for_output_bsocket_usage_;
  const Map<StringRef, int> &lf_input_for_attribute_propagation_to_output_;

 public:
  GeoNodeExecParams(const bNode &node,
                    lf::Params &params,
                    const lf::Context &lf_context,
                    const Map<StringRef, int> &lf_input_for_output_bsocket_usage,
                    const Map<StringRef, int> &lf_input_for_attribute_propagation_to_output)
      : node_(node),
        params_(params),
        lf_context_(lf_context),
        lf_input_for_output_bsocket_usage_(lf_input_for_output_bsocket_usage),
        lf_input_for_attribute_propagation_to_output_(lf_input_for_attribute_propagation_to_output)
  {
  }

  template<typename T>
  static inline constexpr bool is_field_base_type_v =
      is_same_any_v<T, float, int, bool, ColorGeometry4f, float3, std::string>;

  /**
   * Get the input value for the input socket with the given identifier.
   *
   * This method can only be called once for each identifier.
   */
  template<typename T> T extract_input(StringRef identifier)
  {
    if constexpr (is_field_base_type_v<T>) {
      ValueOrField<T> value_or_field = this->extract_input<ValueOrField<T>>(identifier);
      return value_or_field.as_value();
    }
    else if constexpr (fn::is_field_v<T>) {
      using BaseType = typename T::base_type;
      ValueOrField<BaseType> value_or_field = this->extract_input<ValueOrField<BaseType>>(
          identifier);
      return value_or_field.as_field();
    }
    else {
#ifdef DEBUG
      this->check_input_access(identifier, &CPPType::get<T>());
#endif
      const int index = this->get_input_index(identifier);
      T value = params_.extract_input<T>(index);
      if constexpr (std::is_same_v<T, GeometrySet>) {
        this->check_input_geometry_set(identifier, value);
      }
      return value;
    }
  }

  void check_input_geometry_set(StringRef identifier, const GeometrySet &geometry_set) const;
  void check_output_geometry_set(const GeometrySet &geometry_set) const;

  /**
   * Get the input value for the input socket with the given identifier.
   */
  template<typename T> T get_input(StringRef identifier) const
  {
    if constexpr (is_field_base_type_v<T>) {
      ValueOrField<T> value_or_field = this->get_input<ValueOrField<T>>(identifier);
      return value_or_field.as_value();
    }
    else if constexpr (fn::is_field_v<T>) {
      using BaseType = typename T::base_type;
      ValueOrField<BaseType> value_or_field = this->get_input<ValueOrField<BaseType>>(identifier);
      return value_or_field.as_field();
    }
    else {
#ifdef DEBUG
      this->check_input_access(identifier, &CPPType::get<T>());
#endif
      const int index = this->get_input_index(identifier);
      const T &value = params_.get_input<T>(index);
      if constexpr (std::is_same_v<T, GeometrySet>) {
        this->check_input_geometry_set(identifier, value);
      }
      return value;
    }
  }

  /**
   * Store the output value for the given socket identifier.
   */
  template<typename T> void set_output(StringRef identifier, T &&value)
  {
    using StoredT = std::decay_t<T>;
    if constexpr (is_field_base_type_v<StoredT>) {
      this->set_output(identifier, ValueOrField<StoredT>(std::forward<T>(value)));
    }
    else if constexpr (fn::is_field_v<StoredT>) {
      using BaseType = typename StoredT::base_type;
      this->set_output(identifier, ValueOrField<BaseType>(std::forward<T>(value)));
    }
    else {
#ifdef DEBUG
      const CPPType &type = CPPType::get<StoredT>();
      this->check_output_access(identifier, type);
#endif
      if constexpr (std::is_same_v<StoredT, GeometrySet>) {
        this->check_output_geometry_set(value);
      }
      const int index = this->get_output_index(identifier);
      params_.set_output(index, std::forward<T>(value));
    }
  }

  geo_eval_log::GeoTreeLogger *get_local_tree_logger() const
  {
    GeoNodesLFUserData *user_data = this->user_data();
    BLI_assert(user_data != nullptr);
    const ComputeContext *compute_context = user_data->compute_context;
    BLI_assert(compute_context != nullptr);
    if (user_data->modifier_data->eval_log == nullptr) {
      return nullptr;
    }
    return &user_data->modifier_data->eval_log->get_local_tree_logger(*compute_context);
  }

  /**
   * Tell the evaluator that a specific input won't be used anymore.
   */
  void set_input_unused(StringRef identifier)
  {
    const int index = this->get_input_index(identifier);
    params_.set_input_unused(index);
  }

  /**
   * Returns true when the output has to be computed.
   * Nodes that support laziness could use the #lazy_output_is_required variant to possibly avoid
   * some computations.
   */
  bool output_is_required(StringRef identifier) const
  {
    const int index = this->get_output_index(identifier);
    return params_.get_output_usage(index) != lf::ValueUsage::Unused;
  }

  /**
   * Tell the evaluator that a specific input is required.
   * This returns true when the input will only be available in the next execution.
   * False is returned if the input is available already.
   * This can only be used when the node supports laziness.
   */
  bool lazy_require_input(StringRef identifier)
  {
    const int index = this->get_input_index(identifier);
    return params_.try_get_input_data_ptr_or_request(index) == nullptr;
  }

  /**
   * Asks the evaluator if a specific output is required right now. If this returns false, the
   * value might still need to be computed later.
   * This can only be used when the node supports laziness.
   */
  bool lazy_output_is_required(StringRef identifier)
  {
    const int index = this->get_output_index(identifier);
    return params_.get_output_usage(index) == lf::ValueUsage::Used;
  }

  /**
   * Get the node that is currently being executed.
   */
  const bNode &node() const
  {
    return node_;
  }

  const Object *self_object() const
  {
    if (const auto *data = this->user_data()) {
      if (data->modifier_data) {
        return data->modifier_data->self_object;
      }
    }
    return nullptr;
  }

  Depsgraph *depsgraph() const
  {
    if (const auto *data = this->user_data()) {
      if (data->modifier_data) {
        return data->modifier_data->depsgraph;
      }
    }
    return nullptr;
  }

  GeoNodesLFUserData *user_data() const
  {
    return dynamic_cast<GeoNodesLFUserData *>(lf_context_.user_data);
  }

  /**
   * Add an error message displayed at the top of the node when displaying the node tree,
   * and potentially elsewhere in Blender.
   */
  void error_message_add(const NodeWarningType type, StringRef message) const;

  std::string attribute_producer_name() const;

  void set_default_remaining_outputs();

  void used_named_attribute(StringRef attribute_name, NamedAttributeUsage usage);

  /**
   * Return true when the anonymous attribute referenced by the given output should be created.
   */
  bool anonymous_attribute_output_is_required(const StringRef output_identifier)
  {
    const int lf_index = lf_input_for_output_bsocket_usage_.lookup(output_identifier);
    return params_.get_input<bool>(lf_index);
  }

  /**
   * Return a new anonymous attribute id for the given output. None is returned if the anonymous
   * attribute is not needed.
   */
  AutoAnonymousAttributeID get_output_anonymous_attribute_id_if_needed(
      const StringRef output_identifier, const bool force_create = false)
  {
    if (!this->anonymous_attribute_output_is_required(output_identifier) && !force_create) {
      return {};
    }
    const bNodeSocket &output_socket = node_.output_by_identifier(output_identifier);
    const GeoNodesLFUserData &user_data = *this->user_data();
    const ComputeContext &compute_context = *user_data.compute_context;
    return MEM_new<NodeAnonymousAttributeID>(__func__,
                                             *user_data.modifier_data->self_object,
                                             compute_context,
                                             node_,
                                             output_identifier,
                                             output_socket.name);
  }

  /**
   * Get information about which anonymous attributes should be propagated to the given output.
   */
  AnonymousAttributePropagationInfo get_output_propagation_info(
      const StringRef output_identifier) const
  {
    const int lf_index = lf_input_for_attribute_propagation_to_output_.lookup(output_identifier);
    const bke::AnonymousAttributeSet &set = params_.get_input<bke::AnonymousAttributeSet>(
        lf_index);
    AnonymousAttributePropagationInfo info;
    info.names = set.names;
    info.propagate_all = false;
    return info;
  }

 private:
  /* Utilities for detecting common errors at when using this class. */
  void check_input_access(StringRef identifier, const CPPType *requested_type = nullptr) const;
  void check_output_access(StringRef identifier, const CPPType &value_type) const;

  /* Find the active socket with the input name (not the identifier). */
  const bNodeSocket *find_available_socket(const StringRef name) const;

  int get_input_index(const StringRef identifier) const
  {
    int counter = 0;
    for (const bNodeSocket *socket : node_.input_sockets()) {
      if (!socket->is_available()) {
        continue;
      }
      if (socket->identifier == identifier) {
        return counter;
      }
      counter++;
    }
    BLI_assert_unreachable();
    return -1;
  }

  int get_output_index(const StringRef identifier) const
  {
    int counter = 0;
    for (const bNodeSocket *socket : node_.output_sockets()) {
      if (!socket->is_available()) {
        continue;
      }
      if (socket->identifier == identifier) {
        return counter;
      }
      counter++;
    }
    BLI_assert_unreachable();
    return -1;
  }
};

}  // namespace blender::nodes
