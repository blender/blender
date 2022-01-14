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

#pragma once

#include "FN_field.hh"
#include "FN_multi_function_builder.hh"

#include "BKE_attribute_access.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry_nodes_eval_log.hh"

#include "GEO_realize_instances.hh"

struct Depsgraph;
struct ModifierData;

namespace blender::nodes {

using bke::AnonymousAttributeFieldInput;
using bke::AttributeFieldInput;
using bke::AttributeIDRef;
using bke::GeometryComponentFieldContext;
using bke::GeometryFieldInput;
using bke::OutputAttribute;
using bke::OutputAttribute_Typed;
using bke::ReadAttributeLookup;
using bke::StrongAnonymousAttributeID;
using bke::WeakAnonymousAttributeID;
using bke::WriteAttributeLookup;
using fn::CPPType;
using fn::Field;
using fn::FieldContext;
using fn::FieldEvaluator;
using fn::FieldInput;
using fn::FieldOperation;
using fn::GField;
using fn::GMutablePointer;
using fn::GMutableSpan;
using fn::GPointer;
using fn::GSpan;
using fn::GVArray;
using fn::GVArray_GSpan;
using fn::GVMutableArray;
using fn::GVMutableArray_GSpan;
using fn::ValueOrField;
using geometry_nodes_eval_log::NodeWarningType;

/**
 * This class exists to separate the memory management details of the geometry nodes evaluator
 * from the node execution functions and related utilities.
 */
class GeoNodeExecParamsProvider {
 public:
  DNode dnode;
  const Object *self_object = nullptr;
  const ModifierData *modifier = nullptr;
  Depsgraph *depsgraph = nullptr;
  geometry_nodes_eval_log::GeoLogger *logger = nullptr;

  /**
   * Returns true when the node is allowed to get/extract the input value. The identifier is
   * expected to be valid. This may return false if the input value has been consumed already.
   */
  virtual bool can_get_input(StringRef identifier) const = 0;

  /**
   * Returns true when the node is allowed to set the output value. The identifier is expected to
   * be valid. This may return false if the output value has been set already.
   */
  virtual bool can_set_output(StringRef identifier) const = 0;

  /**
   * Take ownership of an input value. The caller is responsible for destructing the value. It does
   * not have to be freed, because the memory is managed by the geometry nodes evaluator.
   */
  virtual GMutablePointer extract_input(StringRef identifier) = 0;

  /**
   * Similar to #extract_input, but has to be used for multi-input sockets.
   */
  virtual Vector<GMutablePointer> extract_multi_input(StringRef identifier) = 0;

  /**
   * Get the input value for the identifier without taking ownership of it.
   */
  virtual GPointer get_input(StringRef identifier) const = 0;

  /**
   * Prepare a memory buffer for an output value of the node. The returned memory has to be
   * initialized by the caller. The identifier and type are expected to be correct.
   */
  virtual GMutablePointer alloc_output_value(const CPPType &type) = 0;

  /**
   * The value has been allocated with #alloc_output_value.
   */
  virtual void set_output(StringRef identifier, GMutablePointer value) = 0;

  /* A description for these methods is provided in GeoNodeExecParams. */
  virtual void set_input_unused(StringRef identifier) = 0;
  virtual bool output_is_required(StringRef identifier) const = 0;
  virtual bool lazy_require_input(StringRef identifier) = 0;
  virtual bool lazy_output_is_required(StringRef identifier) const = 0;

  virtual void set_default_remaining_outputs() = 0;
};

class GeoNodeExecParams {
 private:
  GeoNodeExecParamsProvider *provider_;

 public:
  GeoNodeExecParams(GeoNodeExecParamsProvider &provider) : provider_(&provider)
  {
  }

  template<typename T>
  static inline constexpr bool is_field_base_type_v =
      is_same_any_v<T, float, int, bool, ColorGeometry4f, float3, std::string>;

  /**
   * Get the input value for the input socket with the given identifier.
   *
   * The node calling becomes responsible for destructing the value before it is done
   * executing. This method can only be called once for each identifier.
   */
  GMutablePointer extract_input(StringRef identifier)
  {
#ifdef DEBUG
    this->check_input_access(identifier);
#endif
    return provider_->extract_input(identifier);
  }

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
      GMutablePointer gvalue = this->extract_input(identifier);
      T value = gvalue.relocate_out<T>();
      if constexpr (std::is_same_v<T, GeometrySet>) {
        this->check_input_geometry_set(identifier, value);
      }
      return value;
    }
  }

  void check_input_geometry_set(StringRef identifier, const GeometrySet &geometry_set) const;

  /**
   * Get input as vector for multi input socket with the given identifier.
   *
   * This method can only be called once for each identifier.
   */
  template<typename T> Vector<T> extract_multi_input(StringRef identifier)
  {
    Vector<GMutablePointer> gvalues = provider_->extract_multi_input(identifier);
    Vector<T> values;
    for (GMutablePointer gvalue : gvalues) {
      if constexpr (is_field_base_type_v<T>) {
        const ValueOrField<T> value_or_field = gvalue.relocate_out<ValueOrField<T>>();
        values.append(value_or_field.as_value());
      }
      else {
        values.append(gvalue.relocate_out<T>());
      }
    }
    return values;
  }

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
      GPointer gvalue = provider_->get_input(identifier);
      BLI_assert(gvalue.is_type<T>());
      const T &value = *(const T *)gvalue.get();
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
      const CPPType &type = CPPType::get<StoredT>();
#ifdef DEBUG
      this->check_output_access(identifier, type);
#endif
      GMutablePointer gvalue = provider_->alloc_output_value(type);
      new (gvalue.get()) StoredT(std::forward<T>(value));
      provider_->set_output(identifier, gvalue);
    }
  }

  /**
   * Tell the evaluator that a specific input won't be used anymore.
   */
  void set_input_unused(StringRef identifier)
  {
    provider_->set_input_unused(identifier);
  }

  /**
   * Returns true when the output has to be computed.
   * Nodes that support laziness could use the #lazy_output_is_required variant to possibly avoid
   * some computations.
   */
  bool output_is_required(StringRef identifier) const
  {
    return provider_->output_is_required(identifier);
  }

  /**
   * Tell the evaluator that a specific input is required.
   * This returns true when the input will only be available in the next execution.
   * False is returned if the input is available already.
   * This can only be used when the node supports laziness.
   */
  bool lazy_require_input(StringRef identifier)
  {
    return provider_->lazy_require_input(identifier);
  }

  /**
   * Asks the evaluator if a specific output is required right now. If this returns false, the
   * value might still need to be computed later.
   * This can only be used when the node supports laziness.
   */
  bool lazy_output_is_required(StringRef identifier)
  {
    return provider_->lazy_output_is_required(identifier);
  }

  /**
   * Get the node that is currently being executed.
   */
  const bNode &node() const
  {
    return *provider_->dnode->bnode();
  }

  const Object *self_object() const
  {
    return provider_->self_object;
  }

  Depsgraph *depsgraph() const
  {
    return provider_->depsgraph;
  }

  /**
   * Add an error message displayed at the top of the node when displaying the node tree,
   * and potentially elsewhere in Blender.
   */
  void error_message_add(const NodeWarningType type, std::string message) const;

  /**
   * Creates a read-only attribute based on node inputs. The method automatically detects which
   * input socket with the given name is available.
   *
   * \note This will add an error message if the string socket is active and
   * the input attribute does not exist.
   */
  GVArray get_input_attribute(const StringRef name,
                              const GeometryComponent &component,
                              AttributeDomain domain,
                              const CustomDataType type,
                              const void *default_value) const;

  template<typename T>
  VArray<T> get_input_attribute(const StringRef name,
                                const GeometryComponent &component,
                                const AttributeDomain domain,
                                const T &default_value) const
  {
    const CustomDataType type = bke::cpp_type_to_custom_data_type(CPPType::get<T>());
    GVArray varray = this->get_input_attribute(name, component, domain, type, &default_value);
    return varray.typed<T>();
  }

  /**
   * Get the type of an input property or the associated constant socket types with the
   * same names. Fall back to the default value if no attribute exists with the name.
   */
  CustomDataType get_input_attribute_data_type(const StringRef name,
                                               const GeometryComponent &component,
                                               const CustomDataType default_type) const;

  /**
   * If any of the corresponding input sockets are attributes instead of single values,
   * use the highest priority attribute domain from among them.
   * Otherwise return the default domain.
   */
  AttributeDomain get_highest_priority_input_domain(Span<std::string> names,
                                                    const GeometryComponent &component,
                                                    AttributeDomain default_domain) const;

  std::string attribute_producer_name() const;

  void set_default_remaining_outputs();

 private:
  /* Utilities for detecting common errors at when using this class. */
  void check_input_access(StringRef identifier, const CPPType *requested_type = nullptr) const;
  void check_output_access(StringRef identifier, const CPPType &value_type) const;

  /* Find the active socket with the input name (not the identifier). */
  const bNodeSocket *find_available_socket(const StringRef name) const;
};

}  // namespace blender::nodes
