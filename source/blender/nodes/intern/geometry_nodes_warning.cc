/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLT_translation.hh"

#include "NOD_geometry_nodes_warning.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "UI_resources.hh"

namespace blender::nodes {

int node_warning_type_icon(const NodeWarningType type)
{
  switch (type) {
    case NodeWarningType::Error:
      return ICON_CANCEL;
    case NodeWarningType::Warning:
      return ICON_ERROR;
    case NodeWarningType::Info:
      return ICON_INFO;
  }
  BLI_assert_unreachable();
  return ICON_ERROR;
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

}  // namespace blender::nodes
