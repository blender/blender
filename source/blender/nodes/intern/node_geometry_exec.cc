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

#include "DNA_modifier_types.h"

#include "BKE_node_ui_storage.hh"

#include "DEG_depsgraph_query.h"

#include "NOD_geometry_exec.hh"
#include "NOD_type_callbacks.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

void GeoNodeExecParams::error_message_add(const NodeWarningType type, std::string message) const
{
  bNodeTree *btree_cow = node_->btree();
  BLI_assert(btree_cow != nullptr);
  if (btree_cow == nullptr) {
    return;
  }
  bNodeTree *btree_original = (bNodeTree *)DEG_get_original_id((ID *)btree_cow);

  const NodeTreeEvaluationContext context(*self_object_, *modifier_);

  BKE_nodetree_error_message_add(
      *btree_original, context, *node_->bnode(), type, std::move(message));
}

const bNodeSocket *GeoNodeExecParams::find_available_socket(const StringRef name) const
{
  for (const InputSocketRef *socket : node_->inputs()) {
    if (socket->is_available() && socket->name() == name) {
      return socket->bsocket();
    }
  }

  return nullptr;
}

ReadAttributePtr GeoNodeExecParams::get_input_attribute(const StringRef name,
                                                        const GeometryComponent &component,
                                                        const AttributeDomain domain,
                                                        const CustomDataType type,
                                                        const void *default_value) const
{
  const bNodeSocket *found_socket = this->find_available_socket(name);
  BLI_assert(found_socket != nullptr); /* There should always be available socket for the name. */
  if (found_socket == nullptr) {
    return component.attribute_get_constant_for_read(domain, type, default_value);
  }

  if (found_socket->type == SOCK_STRING) {
    const std::string name = this->get_input<std::string>(found_socket->identifier);
    /* Try getting the attribute without the default value. */
    ReadAttributePtr attribute = component.attribute_try_get_for_read(name, domain, type);
    if (attribute) {
      return attribute;
    }

    /* If the attribute doesn't exist, use the default value and output an error message
     * (except when the field is empty, to avoid spamming error messages, and not when
     * the domain is empty and we don't expect an attribute anyway). */
    if (!name.empty() && component.attribute_domain_size(domain) != 0) {
      this->error_message_add(NodeWarningType::Error,
                              TIP_("No attribute with name \"") + name + "\"");
    }
    return component.attribute_get_constant_for_read(domain, type, default_value);
  }
  if (found_socket->type == SOCK_FLOAT) {
    const float value = this->get_input<float>(found_socket->identifier);
    return component.attribute_get_constant_for_read_converted(
        domain, CD_PROP_FLOAT, type, &value);
  }
  if (found_socket->type == SOCK_VECTOR) {
    const float3 value = this->get_input<float3>(found_socket->identifier);
    return component.attribute_get_constant_for_read_converted(
        domain, CD_PROP_FLOAT3, type, &value);
  }
  if (found_socket->type == SOCK_RGBA) {
    const Color4f value = this->get_input<Color4f>(found_socket->identifier);
    return component.attribute_get_constant_for_read_converted(
        domain, CD_PROP_COLOR, type, &value);
  }
  BLI_assert(false);
  return component.attribute_get_constant_for_read(domain, type, default_value);
}

CustomDataType GeoNodeExecParams::get_input_attribute_data_type(
    const StringRef name,
    const GeometryComponent &component,
    const CustomDataType default_type) const
{
  const bNodeSocket *found_socket = this->find_available_socket(name);
  BLI_assert(found_socket != nullptr); /* There should always be available socket for the name. */
  if (found_socket == nullptr) {
    return default_type;
  }

  if (found_socket->type == SOCK_STRING) {
    const std::string name = this->get_input<std::string>(found_socket->identifier);
    ReadAttributePtr attribute = component.attribute_try_get_for_read(name);
    if (!attribute) {
      return default_type;
    }
    return attribute->custom_data_type();
  }
  if (found_socket->type == SOCK_FLOAT) {
    return CD_PROP_FLOAT;
  }
  if (found_socket->type == SOCK_VECTOR) {
    return CD_PROP_FLOAT3;
  }
  if (found_socket->type == SOCK_RGBA) {
    return CD_PROP_COLOR;
  }
  if (found_socket->type == SOCK_BOOLEAN) {
    return CD_PROP_BOOL;
  }

  BLI_assert(false);
  return default_type;
}

/**
 * If any of the corresponding input sockets are attributes instead of single values,
 * use the highest priority attribute domain from among them.
 * Otherwise return the default domain.
 */
AttributeDomain GeoNodeExecParams::get_highest_priority_input_domain(
    Span<std::string> names,
    const GeometryComponent &component,
    const AttributeDomain default_domain) const
{
  Vector<AttributeDomain, 8> input_domains;
  for (const std::string &name : names) {
    const bNodeSocket *found_socket = this->find_available_socket(name);
    BLI_assert(found_socket != nullptr); /* A socket should be available socket for the name. */
    if (found_socket == nullptr) {
      continue;
    }

    if (found_socket->type == SOCK_STRING) {
      const std::string name = this->get_input<std::string>(found_socket->identifier);
      ReadAttributePtr attribute = component.attribute_try_get_for_read(name);
      if (attribute) {
        input_domains.append(attribute->domain());
      }
    }
  }

  if (input_domains.size() > 0) {
    return bke::attribute_domain_highest_priority(input_domains);
  }

  return default_domain;
}

void GeoNodeExecParams::check_extract_input(StringRef identifier,
                                            const CPPType *requested_type) const
{
  bNodeSocket *found_socket = nullptr;
  for (const InputSocketRef *socket : node_->inputs()) {
    if (socket->identifier() == identifier) {
      found_socket = socket->bsocket();
      break;
    }
  }

  if (found_socket == nullptr) {
    std::cout << "Did not find an input socket with the identifier '" << identifier << "'.\n";
    std::cout << "Possible identifiers are: ";
    for (const InputSocketRef *socket : node_->inputs()) {
      if (socket->is_available()) {
        std::cout << "'" << socket->identifier() << "', ";
      }
    }
    std::cout << "\n";
    BLI_assert(false);
  }
  else if (found_socket->flag & SOCK_UNAVAIL) {
    std::cout << "The socket corresponding to the identifier '" << identifier
              << "' is disabled.\n";
    BLI_assert(false);
  }
  else if (!input_values_.contains(identifier)) {
    std::cout << "The identifier '" << identifier
              << "' is valid, but there is no value for it anymore.\n";
    std::cout << "Most likely it has been extracted before.\n";
    BLI_assert(false);
  }
  else if (requested_type != nullptr) {
    const CPPType &expected_type = *socket_cpp_type_get(*found_socket->typeinfo);
    if (*requested_type != expected_type) {
      std::cout << "The requested type '" << requested_type->name() << "' is incorrect. Expected '"
                << expected_type.name() << "'.\n";
      BLI_assert(false);
    }
  }
}

void GeoNodeExecParams::check_set_output(StringRef identifier, const CPPType &value_type) const
{
  bNodeSocket *found_socket = nullptr;
  for (const OutputSocketRef *socket : node_->outputs()) {
    if (socket->identifier() == identifier) {
      found_socket = socket->bsocket();
      break;
    }
  }

  if (found_socket == nullptr) {
    std::cout << "Did not find an output socket with the identifier '" << identifier << "'.\n";
    std::cout << "Possible identifiers are: ";
    for (const OutputSocketRef *socket : node_->outputs()) {
      if (socket->is_available()) {
        std::cout << "'" << socket->identifier() << "', ";
      }
    }
    std::cout << "\n";
    BLI_assert(false);
  }
  else if (found_socket->flag & SOCK_UNAVAIL) {
    std::cout << "The socket corresponding to the identifier '" << identifier
              << "' is disabled.\n";
    BLI_assert(false);
  }
  else if (output_values_.contains(identifier)) {
    std::cout << "The identifier '" << identifier << "' has been set already.\n";
    BLI_assert(false);
  }
  else {
    const CPPType &expected_type = *socket_cpp_type_get(*found_socket->typeinfo);
    if (value_type != expected_type) {
      std::cout << "The value type '" << value_type.name() << "' is incorrect. Expected '"
                << expected_type.name() << "'.\n";
      BLI_assert(false);
    }
  }
}

}  // namespace blender::nodes
