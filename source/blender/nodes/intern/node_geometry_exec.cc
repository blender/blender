/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_modifier_types.h"

#include "DEG_depsgraph_query.h"

#include "BKE_type_conversions.hh"

#include "NOD_geometry_exec.hh"

#include "node_geometry_util.hh"

using blender::nodes::geometry_nodes_eval_log::LocalGeoLogger;

namespace blender::nodes {

void GeoNodeExecParams::error_message_add(const NodeWarningType type, std::string message) const
{
  if (provider_->logger == nullptr) {
    return;
  }
  LocalGeoLogger &local_logger = provider_->logger->local();
  local_logger.log_node_warning(provider_->dnode, type, std::move(message));
}

void GeoNodeExecParams::used_named_attribute(std::string attribute_name,
                                             const NamedAttributeUsage usage)
{
  if (provider_->logger == nullptr) {
    return;
  }
  LocalGeoLogger &local_logger = provider_->logger->local();
  local_logger.log_used_named_attribute(provider_->dnode, std::move(attribute_name), usage);
}

void GeoNodeExecParams::check_input_geometry_set(StringRef identifier,
                                                 const GeometrySet &geometry_set) const
{
  const SocketDeclaration &decl =
      *provider_->dnode->input_by_identifier(identifier).bsocket()->declaration;
  const decl::Geometry *geo_decl = dynamic_cast<const decl::Geometry *>(&decl);
  if (geo_decl == nullptr) {
    return;
  }

  const bool only_realized_data = geo_decl->only_realized_data();
  const bool only_instances = geo_decl->only_instances();
  const Span<GeometryComponentType> supported_types = geo_decl->supported_types();

  if (only_realized_data) {
    if (geometry_set.has_instances()) {
      this->error_message_add(NodeWarningType::Info,
                              TIP_("Instances in input geometry are ignored"));
    }
  }
  if (only_instances) {
    if (geometry_set.has_realized_data()) {
      this->error_message_add(NodeWarningType::Info,
                              TIP_("Realized data in input geometry is ignored"));
    }
  }
  if (supported_types.is_empty()) {
    /* Assume all types are supported. */
    return;
  }
  const Vector<GeometryComponentType> types_in_geometry = geometry_set.gather_component_types(
      true, true);
  for (const GeometryComponentType type : types_in_geometry) {
    if (type == GEO_COMPONENT_TYPE_INSTANCES) {
      continue;
    }
    if (supported_types.contains(type)) {
      continue;
    }
    std::string message = TIP_("Input geometry has unsupported type: ");
    switch (type) {
      case GEO_COMPONENT_TYPE_MESH: {
        message += TIP_("Mesh");
        break;
      }
      case GEO_COMPONENT_TYPE_POINT_CLOUD: {
        message += TIP_("Point Cloud");
        break;
      }
      case GEO_COMPONENT_TYPE_INSTANCES: {
        BLI_assert_unreachable();
        break;
      }
      case GEO_COMPONENT_TYPE_VOLUME: {
        message += TIP_("Volume");
        break;
      }
      case GEO_COMPONENT_TYPE_CURVE: {
        message += TIP_("Curve");
        break;
      }
    }
    this->error_message_add(NodeWarningType::Info, std::move(message));
  }
}

const bNodeSocket *GeoNodeExecParams::find_available_socket(const StringRef name) const
{
  for (const InputSocketRef *socket : provider_->dnode->inputs()) {
    if (socket->is_available() && socket->name() == name) {
      return socket->bsocket();
    }
  }

  return nullptr;
}

GVArray GeoNodeExecParams::get_input_attribute(const StringRef name,
                                               const GeometryComponent &component,
                                               const AttributeDomain domain,
                                               const CustomDataType type,
                                               const void *default_value) const
{
  const bNodeSocket *found_socket = this->find_available_socket(name);
  BLI_assert(found_socket != nullptr); /* There should always be available socket for the name. */
  const CPPType *cpp_type = bke::custom_data_type_to_cpp_type(type);
  const int64_t domain_num = component.attribute_domain_num(domain);

  if (default_value == nullptr) {
    default_value = cpp_type->default_value();
  }

  if (found_socket == nullptr) {
    return GVArray::ForSingle(*cpp_type, domain_num, default_value);
  }

  if (found_socket->type == SOCK_STRING) {
    const std::string name = this->get_input<std::string>(found_socket->identifier);
    /* Try getting the attribute without the default value. */
    GVArray attribute = component.attribute_try_get_for_read(name, domain, type);
    if (attribute) {
      return attribute;
    }

    /* If the attribute doesn't exist, use the default value and output an error message
     * (except when the field is empty, to avoid spamming error messages, and not when
     * the domain is empty and we don't expect an attribute anyway). */
    if (!name.empty() && component.attribute_domain_num(domain) != 0) {
      this->error_message_add(NodeWarningType::Error,
                              TIP_("No attribute with name \"") + name + "\"");
    }
    return GVArray::ForSingle(*cpp_type, domain_num, default_value);
  }
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  if (found_socket->type == SOCK_FLOAT) {
    const float value = this->get_input<float>(found_socket->identifier);
    BUFFER_FOR_CPP_TYPE_VALUE(*cpp_type, buffer);
    conversions.convert_to_uninitialized(CPPType::get<float>(), *cpp_type, &value, buffer);
    return GVArray::ForSingle(*cpp_type, domain_num, buffer);
  }
  if (found_socket->type == SOCK_INT) {
    const int value = this->get_input<int>(found_socket->identifier);
    BUFFER_FOR_CPP_TYPE_VALUE(*cpp_type, buffer);
    conversions.convert_to_uninitialized(CPPType::get<int>(), *cpp_type, &value, buffer);
    return GVArray::ForSingle(*cpp_type, domain_num, buffer);
  }
  if (found_socket->type == SOCK_VECTOR) {
    const float3 value = this->get_input<float3>(found_socket->identifier);
    BUFFER_FOR_CPP_TYPE_VALUE(*cpp_type, buffer);
    conversions.convert_to_uninitialized(CPPType::get<float3>(), *cpp_type, &value, buffer);
    return GVArray::ForSingle(*cpp_type, domain_num, buffer);
  }
  if (found_socket->type == SOCK_RGBA) {
    const ColorGeometry4f value = this->get_input<ColorGeometry4f>(found_socket->identifier);
    BUFFER_FOR_CPP_TYPE_VALUE(*cpp_type, buffer);
    conversions.convert_to_uninitialized(
        CPPType::get<ColorGeometry4f>(), *cpp_type, &value, buffer);
    return GVArray::ForSingle(*cpp_type, domain_num, buffer);
  }
  BLI_assert(false);
  return GVArray::ForSingle(*cpp_type, domain_num, default_value);
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
    std::optional<AttributeMetaData> info = component.attribute_get_meta_data(name);
    if (info) {
      return info->data_type;
    }
    return default_type;
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
      std::optional<AttributeMetaData> info = component.attribute_get_meta_data(name);
      if (info) {
        input_domains.append(info->domain);
      }
    }
  }

  if (input_domains.size() > 0) {
    return bke::attribute_domain_highest_priority(input_domains);
  }

  return default_domain;
}

std::string GeoNodeExecParams::attribute_producer_name() const
{
  return provider_->dnode->label_or_name() + TIP_(" node");
}

void GeoNodeExecParams::set_default_remaining_outputs()
{
  provider_->set_default_remaining_outputs();
}

void GeoNodeExecParams::check_input_access(StringRef identifier,
                                           const CPPType *requested_type) const
{
  bNodeSocket *found_socket = nullptr;
  for (const InputSocketRef *socket : provider_->dnode->inputs()) {
    if (socket->identifier() == identifier) {
      found_socket = socket->bsocket();
      break;
    }
  }

  if (found_socket == nullptr) {
    std::cout << "Did not find an input socket with the identifier '" << identifier << "'.\n";
    std::cout << "Possible identifiers are: ";
    for (const InputSocketRef *socket : provider_->dnode->inputs()) {
      if (socket->is_available()) {
        std::cout << "'" << socket->identifier() << "', ";
      }
    }
    std::cout << "\n";
    BLI_assert_unreachable();
  }
  else if (found_socket->flag & SOCK_UNAVAIL) {
    std::cout << "The socket corresponding to the identifier '" << identifier
              << "' is disabled.\n";
    BLI_assert_unreachable();
  }
  else if (!provider_->can_get_input(identifier)) {
    std::cout << "The identifier '" << identifier
              << "' is valid, but there is no value for it anymore.\n";
    std::cout << "Most likely it has been extracted before.\n";
    BLI_assert_unreachable();
  }
  else if (requested_type != nullptr) {
    const CPPType &expected_type = *found_socket->typeinfo->geometry_nodes_cpp_type;
    if (*requested_type != expected_type) {
      std::cout << "The requested type '" << requested_type->name() << "' is incorrect. Expected '"
                << expected_type.name() << "'.\n";
      BLI_assert_unreachable();
    }
  }
}

void GeoNodeExecParams::check_output_access(StringRef identifier, const CPPType &value_type) const
{
  bNodeSocket *found_socket = nullptr;
  for (const OutputSocketRef *socket : provider_->dnode->outputs()) {
    if (socket->identifier() == identifier) {
      found_socket = socket->bsocket();
      break;
    }
  }

  if (found_socket == nullptr) {
    std::cout << "Did not find an output socket with the identifier '" << identifier << "'.\n";
    std::cout << "Possible identifiers are: ";
    for (const OutputSocketRef *socket : provider_->dnode->outputs()) {
      if (socket->is_available()) {
        std::cout << "'" << socket->identifier() << "', ";
      }
    }
    std::cout << "\n";
    BLI_assert_unreachable();
  }
  else if (found_socket->flag & SOCK_UNAVAIL) {
    std::cout << "The socket corresponding to the identifier '" << identifier
              << "' is disabled.\n";
    BLI_assert_unreachable();
  }
  else if (!provider_->can_set_output(identifier)) {
    std::cout << "The identifier '" << identifier << "' has been set already.\n";
    BLI_assert_unreachable();
  }
  else {
    const CPPType &expected_type = *found_socket->typeinfo->geometry_nodes_cpp_type;
    if (value_type != expected_type) {
      std::cout << "The value type '" << value_type.name() << "' is incorrect. Expected '"
                << expected_type.name() << "'.\n";
      BLI_assert_unreachable();
    }
  }
}

}  // namespace blender::nodes
