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

#include "NOD_geometry_exec.hh"
#include "NOD_type_callbacks.hh"

namespace blender::nodes {

ReadAttributePtr GeoNodeExecParams::get_input_attribute(const StringRef name,
                                                        const GeometryComponent &component,
                                                        const AttributeDomain domain,
                                                        const CustomDataType type,
                                                        const void *default_value) const
{
  const bNodeSocket *found_socket = nullptr;
  LISTBASE_FOREACH (const bNodeSocket *, socket, &node_.inputs) {
    if ((socket->flag & SOCK_UNAVAIL) != 0) {
      continue;
    }
    if (name == socket->name) {
      found_socket = socket;
      break;
    }
  }
  BLI_assert(found_socket != nullptr);

  if (found_socket->type == SOCK_STRING) {
    const std::string name = this->get_input<std::string>(found_socket->identifier);
    return component.attribute_get_for_read(name, domain, type, default_value);
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

void GeoNodeExecParams::check_extract_input(StringRef identifier,
                                            const CPPType *requested_type) const
{
  bNodeSocket *found_socket = nullptr;
  LISTBASE_FOREACH (bNodeSocket *, socket, &node_.inputs) {
    if (identifier == socket->identifier) {
      found_socket = socket;
      break;
    }
  }
  if (found_socket == nullptr) {
    std::cout << "Did not find an input socket with the identifier '" << identifier << "'.\n";
    std::cout << "Possible identifiers are: ";
    LISTBASE_FOREACH (bNodeSocket *, socket, &node_.inputs) {
      if ((socket->flag & SOCK_UNAVAIL) == 0) {
        std::cout << "'" << socket->identifier << "', ";
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
  LISTBASE_FOREACH (bNodeSocket *, socket, &node_.outputs) {
    if (identifier == socket->identifier) {
      found_socket = socket;
      break;
    }
  }
  if (found_socket == nullptr) {
    std::cout << "Did not find an output socket with the identifier '" << identifier << "'.\n";
    std::cout << "Possible identifiers are: ";
    LISTBASE_FOREACH (bNodeSocket *, socket, &node_.outputs) {
      if ((socket->flag & SOCK_UNAVAIL) == 0) {
        std::cout << "'" << socket->identifier << "', ";
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
