/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_quaternion_types.hh"

#include "FN_field.hh"
#include "FN_lazy_function.hh"
#include "FN_multi_function_builder.hh"

#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry_nodes_lazy_function.hh"

namespace blender::nodes {

using bke::AnonymousAttributeFieldInput;
using bke::AnonymousAttributeID;
using bke::AnonymousAttributeIDPtr;
using bke::AnonymousAttributePropagationInfo;
using bke::AttributeAccessor;
using bke::AttributeFieldInput;
using bke::AttributeIDRef;
using bke::AttributeKind;
using bke::AttributeMetaData;
using bke::AttributeReader;
using bke::AttributeWriter;
using bke::CurveComponent;
using bke::GAttributeReader;
using bke::GAttributeWriter;
using bke::GeometryComponent;
using bke::GeometryComponentEditData;
using bke::GeometrySet;
using bke::GSpanAttributeWriter;
using bke::InstancesComponent;
using bke::MeshComponent;
using bke::MutableAttributeAccessor;
using bke::PointCloudComponent;
using bke::SpanAttributeWriter;
using bke::VolumeComponent;
using fn::Field;
using fn::FieldContext;
using fn::FieldEvaluator;
using fn::FieldInput;
using fn::FieldOperation;
using fn::GField;
using fn::ValueOrField;
using geo_eval_log::NamedAttributeUsage;
using geo_eval_log::NodeWarningType;

class GeoNodeExecParams {
 private:
  const bNode &node_;
  lf::Params &params_;
  const lf::Context &lf_context_;
  const Span<int> lf_input_for_output_bsocket_usage_;
  const Span<int> lf_input_for_attribute_propagation_to_output_;
  const FunctionRef<AnonymousAttributeIDPtr(int)> get_output_attribute_id_;

 public:
  GeoNodeExecParams(const bNode &node,
                    lf::Params &params,
                    const lf::Context &lf_context,
                    const Span<int> lf_input_for_output_bsocket_usage,
                    const Span<int> lf_input_for_attribute_propagation_to_output,
                    const FunctionRef<AnonymousAttributeIDPtr(int)> get_output_attribute_id)
      : node_(node),
        params_(params),
        lf_context_(lf_context),
        lf_input_for_output_bsocket_usage_(lf_input_for_output_bsocket_usage),
        lf_input_for_attribute_propagation_to_output_(
            lf_input_for_attribute_propagation_to_output),
        get_output_attribute_id_(get_output_attribute_id)
  {
  }

  template<typename T>
  static inline constexpr bool is_field_base_type_v =
      is_same_any_v<T, float, int, bool, ColorGeometry4f, float3, std::string, math::Quaternion>;

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
    return this->local_user_data()->tree_logger;
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
   */
  bool output_is_required(StringRef identifier) const
  {
    const int index = this->get_output_index(identifier);
    return params_.get_output_usage(index) != lf::ValueUsage::Unused;
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
      if (data->operator_data) {
        return data->operator_data->self_object;
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
      if (data->operator_data) {
        return data->operator_data->depsgraph;
      }
    }
    return nullptr;
  }

  GeoNodesLFUserData *user_data() const
  {
    return static_cast<GeoNodesLFUserData *>(lf_context_.user_data);
  }

  GeoNodesLFLocalUserData *local_user_data() const
  {
    return static_cast<GeoNodesLFLocalUserData *>(lf_context_.local_user_data);
  }

  /**
   * Add an error message displayed at the top of the node when displaying the node tree,
   * and potentially elsewhere in Blender.
   */
  void error_message_add(const NodeWarningType type, StringRef message) const;

  void set_default_remaining_outputs();

  void used_named_attribute(StringRef attribute_name, NamedAttributeUsage usage);

  /**
   * Return true when the anonymous attribute referenced by the given output should be created.
   */
  bool anonymous_attribute_output_is_required(const StringRef output_identifier)
  {
    const int lf_index =
        lf_input_for_output_bsocket_usage_[node_.output_by_identifier(output_identifier)
                                               .index_in_all_outputs()];
    return params_.get_input<bool>(lf_index);
  }

  /**
   * Return a new anonymous attribute id for the given output. None is returned if the anonymous
   * attribute is not needed.
   */
  AnonymousAttributeIDPtr get_output_anonymous_attribute_id_if_needed(
      const StringRef output_identifier, const bool force_create = false)
  {
    if (!this->anonymous_attribute_output_is_required(output_identifier) && !force_create) {
      return {};
    }
    const bNodeSocket &output_socket = node_.output_by_identifier(output_identifier);
    return get_output_attribute_id_(output_socket.index());
  }

  /**
   * Get information about which anonymous attributes should be propagated to the given output.
   */
  AnonymousAttributePropagationInfo get_output_propagation_info(
      const StringRef output_identifier) const
  {
    const int lf_index =
        lf_input_for_attribute_propagation_to_output_[node_.output_by_identifier(output_identifier)
                                                          .index_in_all_outputs()];
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
