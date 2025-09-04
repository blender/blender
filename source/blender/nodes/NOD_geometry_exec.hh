/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_color_types.hh"
#include "BLI_math_quaternion_types.hh"

#include "FN_field.hh"
#include "FN_lazy_function.hh"
#include "FN_multi_function_builder.hh"

#include "BKE_attribute_filter.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_nodes_reference_set.hh"
#include "BKE_geometry_set.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_volume_grid_fwd.hh"
#include "NOD_geometry_nodes_bundle_fwd.hh"
#include "NOD_geometry_nodes_closure_fwd.hh"
#include "NOD_geometry_nodes_list_fwd.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_geometry_nodes_values.hh"
#include "NOD_menu_value.hh"

namespace blender::nodes {

using bke::AttrDomain;
using bke::AttributeAccessor;
using bke::AttributeDomainAndType;
using bke::AttributeFieldInput;
using bke::AttributeFilter;
using bke::AttributeIter;
using bke::AttributeMetaData;
using bke::AttributeReader;
using bke::AttributeWriter;
using bke::CurveComponent;
using bke::GAttributeReader;
using bke::GAttributeWriter;
using bke::GeometryComponent;
using bke::GeometryComponentEditData;
using bke::GeometryNodesReferenceSet;
using bke::GeometrySet;
using bke::GreasePencilComponent;
using bke::GSpanAttributeWriter;
using bke::InstancesComponent;
using bke::MeshComponent;
using bke::MutableAttributeAccessor;
using bke::PointCloudComponent;
using bke::SocketValueVariant;
using bke::SpanAttributeWriter;
using bke::VolumeComponent;
using fn::Field;
using fn::FieldContext;
using fn::FieldEvaluator;
using fn::FieldInput;
using fn::FieldOperation;
using fn::GField;
using geo_eval_log::NamedAttributeUsage;

class NodeAttributeFilter : public AttributeFilter {
 private:
  const GeometryNodesReferenceSet &set_;

 public:
  NodeAttributeFilter(const GeometryNodesReferenceSet &set) : set_(set) {}

  Result filter(StringRef attribute_name) const override;
};

class GeoNodeExecParams {
 private:
  const bNode &node_;
  lf::Params &params_;
  const lf::Context &lf_context_;
  const Span<int> lf_input_for_output_bsocket_usage_;
  const Span<int> lf_input_for_attribute_propagation_to_output_;
  const FunctionRef<std::string(int)> get_output_attribute_id_;

 public:
  GeoNodeExecParams(const bNode &node,
                    lf::Params &params,
                    const lf::Context &lf_context,
                    const Span<int> lf_input_for_output_bsocket_usage,
                    const Span<int> lf_input_for_attribute_propagation_to_output,
                    const FunctionRef<std::string(int)> get_output_attribute_id)
      : node_(node),
        params_(params),
        lf_context_(lf_context),
        lf_input_for_output_bsocket_usage_(lf_input_for_output_bsocket_usage),
        lf_input_for_attribute_propagation_to_output_(
            lf_input_for_attribute_propagation_to_output),
        get_output_attribute_id_(get_output_attribute_id)
  {
  }

  /**
   * Get the input value for the input socket with the given identifier.
   *
   * This method can only be called once for each identifier.
   */
  template<typename T> T extract_input(StringRef identifier)
  {
#ifndef NDEBUG
    this->check_input_access(identifier);
#endif
    const int index = this->get_input_index(identifier);
    if constexpr (is_GeoNodesMultiInput_v<T>) {
      using ValueT = typename T::value_type;
      BLI_assert(node_.input_by_identifier(identifier)->is_multi_input());
      if constexpr (std::is_same_v<ValueT, SocketValueVariant>) {
        return params_.extract_input<T>(index);
      }
      else {
        auto values_variants = params_.extract_input<GeoNodesMultiInput<SocketValueVariant>>(
            index);
        GeoNodesMultiInput<ValueT> values;
        values.values.reserve(values_variants.values.size());
        for (const int i : values_variants.values.index_range()) {
          values.values.append(values_variants.values[i].extract<ValueT>());
        }
        return values;
      }
    }
    else {
      SocketValueVariant value_variant = params_.extract_input<SocketValueVariant>(index);
      if constexpr (std::is_same_v<T, SocketValueVariant>) {
        return value_variant;
      }
      else if constexpr (std::is_enum_v<T>) {
        return T(value_variant.extract<MenuValue>().value);
      }
      else {
        T value = value_variant.extract<T>();
        if constexpr (std::is_same_v<T, GeometrySet>) {
          this->check_input_geometry_set(identifier, value);
        }
        return value;
      }
    }
  }

  void check_input_geometry_set(StringRef identifier, const GeometrySet &geometry_set) const;
  void check_output_geometry_set(const GeometrySet &geometry_set) const;

  /**
   * Get the input value for the input socket with the given identifier.
   */
  template<typename T> T get_input(StringRef identifier) const
  {
#ifndef NDEBUG
    this->check_input_access(identifier);
#endif
    const int index = this->get_input_index(identifier);
    if constexpr (is_GeoNodesMultiInput_v<T>) {
      using ValueT = typename T::value_type;
      BLI_assert(node_.input_by_identifier(identifier)->is_multi_input());
      if constexpr (std::is_same_v<ValueT, SocketValueVariant>) {
        return params_.get_input<T>(index);
      }
      else {
        auto values_variants = params_.get_input<GeoNodesMultiInput<SocketValueVariant>>(index);
        Vector<ValueT> values(values_variants.values.size());
        for (const int i : values_variants.values.index_range()) {
          values[i] = values_variants.values[i].extract<ValueT>();
        }
        return values;
      }
    }
    else {
      const SocketValueVariant &value_variant = params_.get_input<SocketValueVariant>(index);
      if constexpr (std::is_same_v<T, SocketValueVariant>) {
        return value_variant;
      }
      else if constexpr (std::is_enum_v<T>) {
        return T(value_variant.get<MenuValue>().value);
      }
      else {
        T value = value_variant.get<T>();
        if constexpr (std::is_same_v<T, GeometrySet>) {
          this->check_input_geometry_set(identifier, value);
        }
        return value;
      }
    }
  }

  /**
   * Low level access to the parameters. Usually, it's better to use #get_input, #extract_input and
   * #set_output instead because they are easier to use and more safe. Sometimes it can be
   * beneficial to have more direct access to the raw values though and avoid the indirection.
   */
  lf::Params &low_level_lazy_function_params()
  {
    return params_;
  }

  /**
   * Store the output value for the given socket identifier.
   */
  template<typename T> void set_output(StringRef identifier, T &&value)
  {
    using StoredT = std::decay_t<T>;
#ifndef NDEBUG
    this->check_output_access(identifier);
#endif
    if constexpr (std::is_same_v<StoredT, GeometrySet>) {
      this->check_output_geometry_set(value);
    }
    const int index = this->get_output_index(identifier);
    if constexpr (std::is_same_v<StoredT, SocketValueVariant>) {
      params_.set_output(index, std::forward<T>(value));
    }
    else {
      params_.set_output(index, SocketValueVariant::From(std::forward<T>(value)));
    }
  }

  geo_eval_log::GeoTreeLogger *get_local_tree_logger() const
  {
    return this->local_user_data()->try_get_tree_logger(*this->user_data());
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
      return data->call_data->self_object();
    }
    return nullptr;
  }

  const Depsgraph *depsgraph() const
  {
    if (const auto *data = this->user_data()) {
      if (data->call_data->modifier_data) {
        return data->call_data->modifier_data->depsgraph;
      }
      if (data->call_data->operator_data) {
        return data->call_data->operator_data->depsgraphs->active;
      }
    }
    return nullptr;
  }

  Main *bmain() const;

  GeoNodesUserData *user_data() const
  {
    return static_cast<GeoNodesUserData *>(lf_context_.user_data);
  }

  GeoNodesLocalUserData *local_user_data() const
  {
    return static_cast<GeoNodesLocalUserData *>(lf_context_.local_user_data);
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
                                               ->index_in_all_outputs()];
    return params_.get_input<bool>(lf_index);
  }

  /**
   * Return a new anonymous attribute id for the given output. None is returned if the anonymous
   * attribute is not needed.
   */
  std::optional<std::string> get_output_anonymous_attribute_id_if_needed(
      const StringRef output_identifier, const bool force_create = false)
  {
    if (!this->anonymous_attribute_output_is_required(output_identifier) && !force_create) {
      return std::nullopt;
    }
    const bNodeSocket &output_socket = *node_.output_by_identifier(output_identifier);
    return get_output_attribute_id_(output_socket.index());
  }

  /**
   * Get information about which attributes should be propagated to the given output.
   */
  NodeAttributeFilter get_attribute_filter(const StringRef output_identifier) const
  {
    const int lf_index = lf_input_for_attribute_propagation_to_output_
        [node_.output_by_identifier(output_identifier)->index_in_all_outputs()];
    const GeometryNodesReferenceSet &set = params_.get_input<GeometryNodesReferenceSet>(lf_index);
    return NodeAttributeFilter(set);
  }

  /**
   * If the path is relative, attempt to make it absolute. If the current node tree is linked,
   * the path is relative to the linked file. Otherwise, the path is relative to the current file.
   */
  std::optional<std::string> ensure_absolute_path(StringRefNull path) const;

 private:
  /* Utilities for detecting common errors at when using this class. */
  void check_input_access(StringRef identifier) const;
  void check_output_access(StringRef identifier) const;

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
