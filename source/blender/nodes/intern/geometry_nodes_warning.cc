/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_warning.hh"
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

}  // namespace blender::nodes
