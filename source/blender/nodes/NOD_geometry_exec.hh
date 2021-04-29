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

#include "FN_generic_value_map.hh"

#include "BKE_attribute_access.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_node_ui_storage.hh"
#include "BKE_persistent_data_handle.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

struct Depsgraph;
struct ModifierData;

namespace blender::nodes {

using bke::geometry_set_realize_instances;
using bke::OutputAttribute;
using bke::OutputAttribute_Typed;
using bke::PersistentDataHandleMap;
using bke::PersistentObjectHandle;
using bke::ReadAttributeLookup;
using bke::WriteAttributeLookup;
using fn::CPPType;
using fn::GMutablePointer;
using fn::GMutableSpan;
using fn::GPointer;
using fn::GSpan;
using fn::GValueMap;
using fn::GVArray;
using fn::GVArray_GSpan;
using fn::GVArray_Span;
using fn::GVArray_Typed;
using fn::GVArrayPtr;
using fn::GVMutableArray;
using fn::GVMutableArray_GSpan;
using fn::GVMutableArray_Typed;
using fn::GVMutableArrayPtr;

/**
 * This class exists to separate the memory management details of the geometry nodes evaluator from
 * the node execution functions and related utilities.
 */
class GeoNodeExecParamsProvider {
 public:
  DNode dnode;
  const PersistentDataHandleMap *handle_map = nullptr;
  const Object *self_object = nullptr;
  const ModifierData *modifier = nullptr;
  Depsgraph *depsgraph = nullptr;

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
  virtual GMutablePointer alloc_output_value(StringRef identifier, const CPPType &type) = 0;
};

class GeoNodeExecParams {
 private:
  GeoNodeExecParamsProvider *provider_;

 public:
  GeoNodeExecParams(GeoNodeExecParamsProvider &provider) : provider_(&provider)
  {
  }

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
#ifdef DEBUG
    this->check_input_access(identifier, &CPPType::get<T>());
#endif
    GMutablePointer gvalue = this->extract_input(identifier);
    return gvalue.relocate_out<T>();
  }

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
      values.append(gvalue.relocate_out<T>());
    }
    return values;
  }

  /**
   * Get the input value for the input socket with the given identifier.
   */
  template<typename T> const T &get_input(StringRef identifier) const
  {
#ifdef DEBUG
    this->check_input_access(identifier, &CPPType::get<T>());
#endif
    GPointer gvalue = provider_->get_input(identifier);
    BLI_assert(gvalue.is_type<T>());
    return *(const T *)gvalue.get();
  }

  /**
   * Store the output value for the given socket identifier.
   */
  template<typename T> void set_output(StringRef identifier, T &&value)
  {
    using StoredT = std::decay_t<T>;
    const CPPType &type = CPPType::get<std::decay_t<T>>();
#ifdef DEBUG
    this->check_output_access(identifier, type);
#endif
    GMutablePointer gvalue = provider_->alloc_output_value(identifier, type);
    new (gvalue.get()) StoredT(std::forward<T>(value));
  }

  /**
   * Get the node that is currently being executed.
   */
  const bNode &node() const
  {
    return *provider_->dnode->bnode();
  }

  const PersistentDataHandleMap &handle_map() const
  {
    return *provider_->handle_map;
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
  GVArrayPtr get_input_attribute(const StringRef name,
                                 const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 const CustomDataType type,
                                 const void *default_value) const;

  template<typename T>
  GVArray_Typed<T> get_input_attribute(const StringRef name,
                                       const GeometryComponent &component,
                                       const AttributeDomain domain,
                                       const T &default_value) const
  {
    const CustomDataType type = bke::cpp_type_to_custom_data_type(CPPType::get<T>());
    GVArrayPtr varray = this->get_input_attribute(name, component, domain, type, &default_value);
    return GVArray_Typed<T>(std::move(varray));
  }

  /**
   * Get the type of an input property or the associated constant socket types with the
   * same names. Fall back to the default value if no attribute exists with the name.
   */
  CustomDataType get_input_attribute_data_type(const StringRef name,
                                               const GeometryComponent &component,
                                               const CustomDataType default_type) const;

  AttributeDomain get_highest_priority_input_domain(Span<std::string> names,
                                                    const GeometryComponent &component,
                                                    const AttributeDomain default_domain) const;

 private:
  /* Utilities for detecting common errors at when using this class. */
  void check_input_access(StringRef identifier, const CPPType *requested_type = nullptr) const;
  void check_output_access(StringRef identifier, const CPPType &value_type) const;

  /* Find the active socket socket with the input name (not the identifier). */
  const bNodeSocket *find_available_socket(const StringRef name) const;
};

}  // namespace blender::nodes
