/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_modifier_types.h"

#include "DEG_depsgraph_query.h"

#include "BKE_curves.hh"
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
                                             const eNamedAttrUsage usage)
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
      *provider_->dnode->input_by_identifier(identifier).runtime->declaration;
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
      case GEO_COMPONENT_TYPE_EDIT: {
        continue;
      }
    }
    this->error_message_add(NodeWarningType::Info, std::move(message));
  }
}

void GeoNodeExecParams::check_output_geometry_set(const GeometrySet &geometry_set) const
{
  UNUSED_VARS_NDEBUG(geometry_set);
#ifdef DEBUG
  if (const bke::CurvesEditHints *curve_edit_hints =
          geometry_set.get_curve_edit_hints_for_read()) {
    /* If this is not valid, it's likely that the number of stored deformed points does not match
     * the number of points in the original data. */
    BLI_assert(curve_edit_hints->is_valid());
  }
#endif
}

const bNodeSocket *GeoNodeExecParams::find_available_socket(const StringRef name) const
{
  for (const bNodeSocket *socket : provider_->dnode->runtime->inputs) {
    if (socket->is_available() && socket->name == name) {
      return socket;
    }
  }

  return nullptr;
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
  const bNodeSocket *found_socket = nullptr;
  for (const bNodeSocket *socket : provider_->dnode->input_sockets()) {
    if (socket->identifier == identifier) {
      found_socket = socket;
      break;
    }
  }

  if (found_socket == nullptr) {
    std::cout << "Did not find an input socket with the identifier '" << identifier << "'.\n";
    std::cout << "Possible identifiers are: ";
    for (const bNodeSocket *socket : provider_->dnode->input_sockets()) {
      if (socket->is_available()) {
        std::cout << "'" << socket->identifier << "', ";
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
  const bNodeSocket *found_socket = nullptr;
  for (const bNodeSocket *socket : provider_->dnode->output_sockets()) {
    if (socket->identifier == identifier) {
      found_socket = socket;
      break;
    }
  }

  if (found_socket == nullptr) {
    std::cout << "Did not find an output socket with the identifier '" << identifier << "'.\n";
    std::cout << "Possible identifiers are: ";
    for (const bNodeSocket *socket : provider_->dnode->output_sockets()) {
      if (!(socket->flag & SOCK_UNAVAIL)) {
        std::cout << "'" << socket->identifier << "', ";
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
