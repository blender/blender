/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include "BLT_translation.hh"

#include "BKE_report.hh"

#include "DNA_node_types.h"

#include "NOD_warning.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "UI_resources.hh"

namespace blender::nodes {

NodeWarning::NodeWarning(const Report &report)
{
  switch (report.type) {
    case RPT_ERROR:
      this->type = NodeWarningType::Error;
      break;
    default:
      this->type = NodeWarningType::Info;
      break;
  }
  this->message = report.message;
}

int node_warning_type_icon(const NodeWarningType type)
{
  switch (type) {
    case NodeWarningType::Error:
      return ICON_STATUS_ERROR_FILLED;
    case NodeWarningType::Warning:
      return ICON_STATUS_WARNING_FILLED;
    case NodeWarningType::Info:
      return ICON_STATUS_INFO_FILLED;
  }
  BLI_assert_unreachable();
  return ICON_STATUS_ERROR_FILLED;
}

int node_warning_type_severity(const NodeWarningType type)
{
  switch (type) {
    case NodeWarningType::Error:
      return 3;
    case NodeWarningType::Warning:
      return 2;
    case NodeWarningType::Info:
      return 1;
  }
  BLI_assert_unreachable();
  return 0;
}

StringRefNull node_warning_type_name(const NodeWarningType type)
{
  const char *name = nullptr;
  RNA_enum_name_gettexted(
      rna_enum_node_warning_type_items, int(type), BLT_I18NCONTEXT_DEFAULT, &name);
  BLI_assert(name);
  return name;
}

bool warning_is_propagated(const NodeWarningPropagation propagation,
                           const NodeWarningType warning_type)
{
  switch (propagation) {
    case NODE_WARNING_PROPAGATION_ALL:
      return true;
    case NODE_WARNING_PROPAGATION_NONE:
      return false;
    case NODE_WARNING_PROPAGATION_ONLY_ERRORS:
      return warning_type == NodeWarningType::Error;
    case NODE_WARNING_PROPAGATION_ONLY_ERRORS_AND_WARNINGS:
      return ELEM(warning_type, NodeWarningType::Error, NodeWarningType::Warning);
  }
  BLI_assert_unreachable();
  return true;
}

}  // namespace blender::nodes
